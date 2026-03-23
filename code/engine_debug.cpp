#include "engine.h"

//NOTE: Intelisense helpers
internal void TiledRenderGroupToBuffer(RenderGroup& group, LoadedBitmap& dstBuffer, PlatformQueue* queue);
inline ProjectionProps GetOrtographicProjection(u32 widthPix, u32 heightPix, f32 metersToPixels);
inline LoadedFont* GetOrPrefetchFont(RenderGroup& group, FontId fid);
inline FontId GetFontWithType(Assets& assets, FontType type);
inline void BeginRendering(RenderGroup& group);
inline void EndRendering(RenderGroup& group);
inline bool IsPressed(Button& button);
inline bool WasPressed(Button& button);
extern RenderGroup debugRenderGroup;

inline
u32 GetFontWidthAdvanceFor(LoadedFont* font, u32 firstCodepoint, u32 secondCodepoint) {
	Assert(firstCodepoint < font->onePastMaxCodepoint && secondCodepoint < font->onePastMaxCodepoint);
	u32 firstKerningIndex = font->codepointToLogicalIndex[firstCodepoint];
	u32 secondKerningIndex = font->codepointToLogicalIndex[secondCodepoint];
	Assert((firstKerningIndex != 0 || firstCodepoint == 0) && secondKerningIndex != 0);
	return font->kerningTable[firstKerningIndex * font->onePastMaxLogicalIndex + secondKerningIndex];
}

inline
BitmapId GetFontGlyphBitmapIdFor(LoadedFont* font, u32 codepoint) {
	Assert(codepoint < font->onePastMaxCodepoint);
	u32 index = font->logicalIndexBaseForGlyphs + font->codepointToLogicalIndex[codepoint];
	Assert(index != 0);
	return { index };
}

inline
u32 GetFontLineAdvance(LoadedFont* font) {
	return font->metrics.ascent + font->metrics.descent + font->metrics.externalLeading;
}

inline
u32 HexToInt(char c) {
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	else if (c >= '0' && c <= '9') {
		return c - '0';
	}
	Assert(!"Wrong hex character");
	return 0;
}

internal
void DebugRenderLine(LoadedFont* font, char* text, V2 pos, f32 scale, V4 color) {
	u32 prevChar = 0;
	f32 spaceAdvance = scale * 55;
	for (char* at = text; *at; at++) {
		u32 codepoint = 0;
		if (*at == '\\' &&
			at[1] == '0' &&
			at[2] == 'x') {
			// TODO: It may cause buffer overflow if input data is incorrect
			u32 ox1 = HexToInt(at[3]);
			u32 ox2 = HexToInt(at[4]);
			u32 ox3 = HexToInt(at[5]);
			u32 ox4 = HexToInt(at[6]);
			codepoint = (ox1 << 12) +
				(ox2 << 8) +
				(ox3 << 4) +
				(ox4 << 0);
			at += 6;
		}
		else {
			codepoint = *at;
		}
		if (codepoint != ' ') {
			BitmapId bid = GetFontGlyphBitmapIdFor(font, codepoint);
			AssetMetadata* metadata = GetAssetMetadata(*debugRenderGroup.assets, bid.id);
			f32 width = f4(metadata->_bitmapInfo.width);
			f32 height = f4(metadata->_bitmapInfo.height);
			pos.X += scale * GetFontWidthAdvanceFor(font, prevChar, codepoint);
			V3 anchor = ToV3(pos, 0);
			PushBitmap(debugRenderGroup, bid, anchor, scale * height, V2{ 0, 0 }, color);
		}
		else {
			pos.X += scale * GetFontWidthAdvanceFor(font, prevChar, codepoint);
		}
		prevChar = codepoint;
	}
}

internal
void DebugRenderLine(LoadedFont* font, char* text, FontDrawContext& context) {
	DebugRenderLine(font, text, context.leftTopCurrent, context.scale, context.color);
	context.leftTopCurrent.E[1] -= context.scale * GetFontLineAdvance(font);
}

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

inline
bool IsEventRecordingNeeded(OpenDebugEvent* openingEvent, DebugRecord* record) {
	if (!record) {
		return !openingEvent->parent;
	}
	if (!openingEvent->parent) {
		return false;
	}
	DebugRecord* another = debugGlobalState->debugRecords[openingEvent->parent->translationUnit] + openingEvent->parent->debugRecordIndex;
	bool result = (another->file == record->file) && (another->line == record->line);
	return result;
}

inline
DebugRecord* GetDebugRecord(u32 tUnit, u32 recordIndex) {
	DebugRecord* record = debugGlobalState->debugRecords[tUnit] + recordIndex;
	return record;
}

inline
DebugRecord* GetDebugRecordFor(DebugEvent* event) {
	DebugRecord* record = debugGlobalState->debugRecords[event->translationUnit] + event->debugRecordIndex;
	return record;
}

#include <stdio.h>
internal
void DebugCollateEvents(DebugState* debugState) {
	TemporaryMemory stackMemory = BeginTempMemory(debugState->arena);
	debugState->eventStacks = PushArray(debugState->arena, MAX_DEBUG_THREADS, DebugEventStack);
	debugState->eventStacksCount = 0;
	debugState->openEventFreeList = 0;

	debugState->frameWriteIndex = u4(debugGlobalState->frameAndEventIndex >> 32);
	f32 scale = 1.f / (DEBUG_TARGET_REFRESH_MS * DEBUG_CPU_FREQ);
	for (; 
		debugState->frameReadIndex != debugState->frameWriteIndex;
		debugState->frameReadIndex = (debugState->frameReadIndex + 1) % MAX_DEBUG_FRAMES
		) {
		DebugFrameInfo* frameInfo = debugState->frames + debugState->frameReadIndex;
		frameInfo->regionsCount = 0;
		DebugEvent* eventsInFrame = debugGlobalState->debugEvents[debugState->frameReadIndex];
		u32 eventsInFrameCount = debugGlobalState->debugEventsCount[debugState->frameReadIndex];
		for (u32 eventIndex = 0;
			eventIndex < eventsInFrameCount;
			eventIndex++
			) {
			DebugEvent* event = eventsInFrame + eventIndex;
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
						f32 minT = f4(openEvent->cycles - frameInfo->startCycles) * scale;
						f32 maxT = f4(event->cycles - frameInfo->startCycles) * scale;
						f32 thresholdT = 0.01f;
						if (maxT - minT > thresholdT) {
							Assert(frameInfo->regionsCount < ArrayCount(frameInfo->regions));
							DebugProfilerRegion* region = frameInfo->regions + frameInfo->regionsCount++;
							// TODO: If minT < 0 -> that means openEvent started on one of the previous frames.
							// Do something about it!
							region->minT = minT;
							region->maxT = maxT;
							region->recordIndex = event->debugRecordIndex;
							region->translationUnit = event->translationUnit;
							region->laneId = stack->laneId;
							region->parentRecord = 0;
							if (openingDebugEvent->parent) {
								region->parentRecord = GetDebugRecordFor(openingDebugEvent->parent);
							}
							region->startCycles = openEvent->cycles;
							region->endCycles = event->cycles;
							region->frameStartCycles = frameInfo->startCycles;
							region->frameEndCycles = frameInfo->endCycles;
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
	EndTempMemory(stackMemory);
}

void DebugRenderOverlay(ProgramMemory* memory, LoadedBitmap& dstBitmap, InputData& input) {
	TIMED_FUNCTION;
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
		if (WasPressed(controller.B.mouseRight)) {
			debugState->paused = !debugState->paused;
		}
		DebugRecord* hotRecord = 0;
		f32 fontScale = 0.15f;
		V4 fontColor = V4{ 0.8f, 0.8f, 0.8f, 1 };
		debugGlobalState->debugRecordsCount[0] = debugRecordsCount_Main;
		debugGlobalState->debugRecordsCount[1] = debugRecordsCount_Optimized;
		Assert(debugGlobalState->debugRecordsCount[MAX_TRANSLATION_UNIT - 1] != 0);

		// Collate debug events
		f32 profilerPosY = -0.47f * f4(dstBitmap.height);
		f32 profilerPosX = -0.5f * f4(dstBitmap.width);
		f32 profilerHeight = 200.f;
		f32 threadLaneWidth = 8.f;
		f32 threadLaneSpace = 2.f;
		f32 threadLaneTotalWidth = threadLaneWidth + threadLaneSpace;
		f32 frameLaneSpace = 10.f;
		V4 colors[] = {
			V4{1, 0, 0, 1},
			V4{0, 1, 0, 1},
			V4{0, 0, 1, 1},
			V4{0, 1, 1, 1},
			V4{1, 0, 1, 1},
			V4{1, 1, 0, 1},
			V4{0, 0, 0, 1},
		};
		u32 frameIndexForDrawing = 0;
		f32 frameWidth = f4(debugState->eventStacksCount) * threadLaneTotalWidth + frameLaneSpace;
		for (u32 frameIndex = debugState->frameWriteIndex;;) {
			frameIndex = (frameIndex + 1) % MAX_DEBUG_FRAMES;
			if (frameIndex == debugState->frameWriteIndex) {
				break;
			}
			DebugFrameInfo* frameInfo = debugState->frames + frameIndex;
			for (u32 regionIndex = 0; regionIndex < frameInfo->regionsCount; regionIndex++) {
				DebugProfilerRegion* region = frameInfo->regions + regionIndex;
				if (debugState->selectedRecord != region->parentRecord) {
					continue;
				}

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
				u32 colorIndex = (13 * region->translationUnit + region->recordIndex) % ArrayCount(colors);
				bool isHovered = IsInRectangle(rectangle, mousePos);
				if (isHovered) {
					DebugRecord* record = debugGlobalState->debugRecords[region->translationUnit] + region->recordIndex;
					if (record->blockName) {
						char buffer[256];
						sprintf_s(buffer, "%s | %s:%d", 
							record->blockName,
							record->file,
							record->line
						);
						V2 textPos = mousePos + V2{ 0, 10.f };
						DebugRenderLine(font, buffer, textPos, fontScale, fontColor);
						textPos = V2{ -400, 250.f };
						sprintf_s(buffer, "t<%4f,%4f>, f<%llu, %llu> (%d)",
							region->minT,
							region->maxT,
							region->frameStartCycles,
							region->frameEndCycles,
							u4(region->frameEndCycles - region->frameStartCycles)
							
						);
						DebugRenderLine(font, buffer, textPos, fontScale, fontColor);
						textPos = V2{ -400, 230.f };
						sprintf_s(buffer, "c<%llu, %llu> (%d)",
							region->startCycles,
							region->endCycles,
							u4(region->endCycles - region->startCycles)
						);
						DebugRenderLine(font, buffer, textPos, fontScale, fontColor);
					}
					hotRecord = record;
				}
				PushRect(debugRenderGroup, rectangle, 0, V2{ 0, 0 }, isHovered ? V4{1, 1, 1, 1} : colors[colorIndex]);
			}
			frameIndexForDrawing++;
		}
		f32 profilerWidth = f4(frameIndexForDrawing + 1) * frameWidth;
		V3 targetFrameRateCenter = {
			profilerPosX + 0.5f * profilerWidth,
			profilerPosY + profilerHeight,
			0
		};
		V2 targetFrameRateSize = { profilerWidth, 2.f };
		PushRect(debugRenderGroup, targetFrameRateCenter, targetFrameRateSize, V2{ 0, 0 }, V4{ 0, 0, 0, 1 });

		if (WasPressed(controller.B.mouseLeft)) {
			debugState->selectedRecord = hotRecord;
		}

		TiledRenderGroupToBuffer(debugRenderGroup, dstBitmap, tranState->highPriorityQueue);
	}
	EndRendering(debugRenderGroup);
}

internal
void ResetDebugCollation(DebugState* debugState, u32 frameWriteIndex) {
	EndTempMemory(debugState->scratchBuffer);
	CheckArena(debugState->arena);
	debugState->openEventFreeList = 0;
	debugState->eventStacksCount = 0;
	debugState->eventStacks = 0;
	debugState->paused = 0;
	debugState->frameReadIndex = (frameWriteIndex + 1) % MAX_DEBUG_FRAMES;
	debugState->scratchBuffer = BeginTempMemory(debugState->arena);
	debugState->frames = PushArray(debugState->arena, MAX_DEBUG_FRAMES, DebugFrameInfo);
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
	u32 frameWriteIndex = u4(debugGlobalState->frameAndEventIndex >> 32);
	if (!debugState->isInitialized) {
		InitializeArena(
			debugState->arena,
			ptrcast(u8, memory->debugMemory) + sizeof(DebugState),
			memory->debugMemorySize - sizeof(DebugState)
		);
		debugState->scratchBuffer = BeginTempMemory(debugState->arena);
		ResetDebugCollation(debugState, frameWriteIndex);
		debugState->isInitialized = true;
	}
	if (debugState->paused) {
		return 0;
	}
	DebugCollateEvents(debugState);
	DebugFrameInfo* result = debugState->frames + frameWriteIndex;
	return result;
}