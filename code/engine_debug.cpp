#include "engine.h"

//NOTE: Intelisense helpers
internal void TiledRenderGroupToBuffer(RenderGroup& group, LoadedBitmap& dstBuffer, PlatformQueue* queue);
inline ProjectionProps GetOrtographicProjection(u32 widthPix, u32 heightPix, f32 metersToPixels);
inline LoadedFont* GetOrPrefetchFont(RenderGroup& group, FontId fid);
inline FontId GetFontWithType(Assets& assets, FontType type);
inline void BeginRendering(RenderGroup& group);
inline void EndRendering(RenderGroup& group);
void DebugRenderLine(LoadedFont* font, char* text, FontDrawContext& context);
extern RenderGroup debugRenderGroup;

internal
DebugEventStack* GetDebugStackForThread(DebugState* state, u32 threadId) {
	for (u32 stackIndex = 0; stackIndex < state->eventStacksCount; stackIndex++) {
		DebugEventStack* stack = state->eventStacks + stackIndex;
		if (stack->threadId == threadId) {
			Assert(stack->events);
			return stack;
		}
	}
	Assert(state->eventStacksCount < MAX_DEBUG_THREADS);
	DebugEventStack* stack = state->eventStacks + state->eventStacksCount++;
	stack->threadId = threadId;
	stack->laneId = state->eventStacksCount - 1;
	stack->events = PushStructSize(state->arena, OpenDebugEvent);
	stack->events->event = 0;
	stack->events->next = 0;
	stack->events->parent = 0;
	return stack;
}

#include <stdio.h>
void DebugCollateEvents(DebugState* debugState) {
	u32 currentFrameIndex = u4(debugGlobalState->frameAndEventIndex >> 32);
	f32 scale = 1.f / (DEBUG_TARGET_REFRESH_MS * DEBUG_CPU_FREQ);
	for (u32 frameIndex = currentFrameIndex;;) {
		frameIndex = (frameIndex + 1) % MAX_DEBUG_FRAMES;
		if (frameIndex == currentFrameIndex) {
			break;
		}
		DebugFrameInfo* frameInfo = debugState->frames + frameIndex;

		DebugEvent* eventsInFrame = debugGlobalState->debugEvents[frameIndex];
		u32 eventsInFrameCount = debugGlobalState->debugEventsCount[frameIndex];
		for (u32 eventIndex = 0;
			eventIndex < eventsInFrameCount;
			eventIndex++
			) {
			DebugEvent* event = eventsInFrame + eventIndex;
			if (event->translationUnit == 0) {
				int breakhere = 5;
			}
			if (event->type == Event_BlockBegin) {
				DebugEventStack* stack = GetDebugStackForThread(debugState, event->threadId);
				OpenDebugEvent* newOpenEvent = debugState->openEventFreeList;
				if (newOpenEvent) {
					debugState->openEventFreeList = newOpenEvent->next;
				}
				else {
					newOpenEvent = PushStructSize(debugState->arena, OpenDebugEvent);
				}
				Assert(stack->threadId == event->threadId);
				newOpenEvent->next = stack->events;
				newOpenEvent->event = event;
				newOpenEvent->parent = stack->events->event;
				stack->events = newOpenEvent;
			}
			else if (event->type == Event_BlockEnd) {
				DebugEventStack* stack = GetDebugStackForThread(debugState, event->threadId);
				OpenDebugEvent* openingDebugEvent = stack->events;
				DebugEvent* openEvent = openingDebugEvent->event;
				if (openEvent) {
					if (openEvent->debugRecordIndex == event->debugRecordIndex &&
						openEvent->translationUnit == event->translationUnit &&
						openEvent->threadId == event->threadId)
					{
						if (!openingDebugEvent->parent) {
							f32 minT = f4(openEvent->cycles - frameInfo->startCycles) * scale;
							f32 maxT = f4(event->cycles - frameInfo->startCycles) * scale;
							f32 thresholdT = 0.01f;
							if (maxT - minT > thresholdT) {
								Assert(frameInfo->regionsCount < ArrayCount(frameInfo->regions));
								DebugProfilerRegion* region = frameInfo->regions + frameInfo->regionsCount++;
								f32 frameCyclesRange = f4(frameInfo->endCycles - frameInfo->startCycles);
								// TODO: If minT < 0 -> that means openEvent started on one of the previous frames.
								// Do something about it!
								region->minT = minT;
								region->maxT = maxT;
								region->recordIndex = event->debugRecordIndex;
								region->translationUnit = event->translationUnit;
								region->laneId = stack->laneId;
							}

						}
						else {
							//TODO: Do something with regions which HAVE parent!
						}
						stack->events = openingDebugEvent->next;
						openingDebugEvent->next = debugState->openEventFreeList;
						debugState->openEventFreeList = openingDebugEvent;
					}
				}
			}
			else {
				Assert(!"Unknown event type");
			}
		}
	}
}

void DebugRenderOverlay(ProgramMemory* memory, LoadedBitmap& dstBitmap, InputData& input) {
	if (memory->debugMemorySize == 0) {
		return;
	}
	TransientState* tranState = ptrcast(TransientState, memory->transientMemory);
	DebugState* debugState = ptrcast(DebugState, memory->debugMemory);
	if (!debugState->isInitialized) {
		return;
	}
	debugRenderGroup.pushBufferSize = 0;
	debugRenderGroup.projection = GetOrtographicProjection(dstBitmap.width, dstBitmap.height, 1);
	BeginRendering(debugRenderGroup);
	LoadedFont* font = GetOrPrefetchFont(
		debugRenderGroup, GetFontWithType(*debugRenderGroup.assets, Font_Debug)
	);
	if (font) {
		Controller& controller = input.controllers[KB_CONTROLLER_IDX];
		V2 mousePos = { controller.mouse.X - 0.5f, controller.mouse.Y - 0.5f };
		mousePos = Hadamard(mousePos, V2i(dstBitmap.width, dstBitmap.height));

		FontDrawContext context = {};
		context.scale = 0.15f;
		context.color = V4{ 0.8f, 0.8f, 0.8f, 1 };
		context.leftEdge = context.leftCurrent = -0.5f * dstBitmap.width;
		context.topEdge = context.topCurrent = 0.5f * dstBitmap.height -
			context.scale * font->metrics.ascent;

		debugGlobalState->debugRecordsCount[0] = debugRecordsCount_Main;
		debugGlobalState->debugRecordsCount[1] = debugRecordsCount_Optimized;
		Assert(debugGlobalState->debugRecordsCount[MAX_TRANSLATION_UNIT - 1] != 0);

		// Collate debug events
		f32 profilerPosY = -200.f;
		f32 profilerPosX = -350.f;
		f32 profilerHeight = 200.f;
		f32 threadLaneWidth = 8.f;
		f32 threadLaneSpace = 2.f;
		f32 threadLaneTotalWidth = threadLaneWidth + threadLaneSpace;
		f32 frameLaneSpace = 10.f;
		V4 colors[] = {
			V4{1, 1, 1, 1},
			V4{1, 0, 0, 1},
			V4{0, 1, 0, 1},
			V4{0, 0, 1, 1},
			V4{0, 1, 1, 1},
			V4{1, 0, 1, 1},
			V4{1, 1, 0, 1},
			V4{0, 0, 0, 1},
		};
		u32 currentFrameIndex = u4(debugGlobalState->frameAndEventIndex >> 32);
		u32 frameIndexForDrawing = 0;
		f32 frameWidth = f4(debugState->eventStacksCount) * threadLaneTotalWidth + frameLaneSpace;
		for (u32 frameIndex = currentFrameIndex;;) {
			u32 colorIndexForDrawing = 0;
			frameIndex = (frameIndex + 1) % MAX_DEBUG_FRAMES;
			if (frameIndex == currentFrameIndex) {
				break;
			}
			DebugFrameInfo* frameInfo = debugState->frames + frameIndex;
			for (u32 regionIndex = 0; regionIndex < frameInfo->regionsCount; regionIndex++) {
				DebugProfilerRegion* region = frameInfo->regions + regionIndex;

				V3 spanCenter = {
					profilerPosX + frameIndexForDrawing * frameWidth + (f4(region->laneId) + 0.5f) * threadLaneTotalWidth,
					profilerPosY + 0.5f * f4(region->maxT + region->minT) * profilerHeight,
					0
				};
				V2 spanSize = {
					threadLaneWidth,
					f4(region->maxT - region->minT) * profilerHeight
				};
				Rect2 rectangle = GetRectFromCenterDim(spanCenter.XY, spanSize);
				PushRect(debugRenderGroup, rectangle, 0, V2{ 0, 0 }, colors[colorIndexForDrawing]);
				if (IsInRectangle(rectangle, mousePos)) {
					DebugRecord* record = debugGlobalState->debugRecords[region->translationUnit] + region->recordIndex;
					if (record->blockName) {
						char buffer[256];
						sprintf_s(buffer, "%s | %s:%d", 
							record->blockName,
							record->file,
							record->line
						);
						//context.color = colors[colorIndexForDrawing];
						DebugRenderLine(font, buffer, context);
					}
				}
				/*char buffer[256];
				sprintf_s(buffer, "MOUSE: %4f, %4f", mousePos.X, mousePos.Y);
				DebugRenderLine(font, buffer, context);*/
				colorIndexForDrawing = (colorIndexForDrawing + 1) % ArrayCount(colors);
			}

			frameIndexForDrawing++;
			frameInfo->regionsCount = 0;
		}
		f32 profilerWidth = f4(frameIndexForDrawing + 1) * frameWidth;
		V3 targetFrameRateCenter = {
			profilerPosX + 0.5f * profilerWidth,
			profilerPosY + profilerHeight,
			0
		};
		V2 targetFrameRateSize = { profilerWidth, 2.f };
		PushRect(debugRenderGroup, targetFrameRateCenter, targetFrameRateSize, V2{ 0, 0 }, V4{ 0, 0, 0, 1 });
		TiledRenderGroupToBuffer(debugRenderGroup, dstBitmap, tranState->highPriorityQueue);
	}
	EndRendering(debugRenderGroup);
}

extern "C" DebugGlobalState* DebugInit(ProgramMemory* memory) {
	return debugGlobalState;
}

extern "C" DebugFrameInfo* DebugFinishFrame(ProgramMemory* memory) {
	TIMED_FUNCTION;
	if (memory->debugMemorySize == 0) {
		return 0;
	}
	DebugState* debugState = ptrcast(DebugState, memory->debugMemory);
	if (!debugState->isInitialized) {
		InitializeArena(
			debugState->arena,
			ptrcast(u8, memory->debugMemory) + sizeof(DebugState),
			memory->debugMemorySize - sizeof(DebugState)
		);
		debugState->openEventFreeList = 0;
		debugState->eventStacksCount = 0;
		debugState->maxFrameCount = MAX_DEBUG_FRAMES;
		debugState->frameCount = 0;
		debugState->currentFrame = 0;
		debugState->scratchBuffer = BeginTempMemory(debugState->arena);
		debugState->isInitialized = true;
	}
	EndTempMemory(debugState->scratchBuffer);
	debugState->openEventFreeList = 0;
	debugState->eventStacksCount = 0;
	debugState->currentFrame = 0;
	debugState->frameCount = 0;
	debugState->scratchBuffer = BeginTempMemory(debugState->arena);
	debugState->eventStacks = PushArray(debugState->arena, MAX_DEBUG_THREADS, DebugEventStack);
	debugState->frames = PushArray(debugState->arena, debugState->maxFrameCount, DebugFrameInfo);
	DebugCollateEvents(debugState);

	u32 frameIndex = u4(debugGlobalState->frameAndEventIndex >> 32);
	DebugFrameInfo* result = debugState->frames + frameIndex;
	//debugState->currentFrame = (debugState->currentFrame + 1) % debugState->maxFrameCount;
	return result;
}