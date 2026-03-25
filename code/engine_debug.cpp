#include "engine.h"

// TODO: Delete stdlib
#include <stdio.h>

//NOTE: Intelisense helpers
internal void TiledRenderGroupToBuffer(RenderGroup& group, LoadedBitmap& dstBuffer, PlatformQueue* queue);
inline ProjectionProps GetOrtographicProjection(u32 widthPix, u32 heightPix, f32 metersToPixels);
inline RenderGroup AllocateRenderGroup(MemoryArena& arena, Assets* assets, u32 size, bool renderInBackground);
inline LoadedFont* GetOrPrefetchFont(RenderGroup& group, FontId fid);
inline FontId GetFontWithType(Assets& assets, FontType type);
inline void BeginRendering(RenderGroup& group);
inline void EndRendering(RenderGroup& group);
inline bool IsPressed(Button& button);
inline bool WasPressed(Button& button);

inline
FontDrawContext InitializeFontDrawContext(LoadedFont* font, f32 scale, V2 topline) {
	FontDrawContext context = {};
	context.font = font;
	context.scale = scale;
	context.color = V4{ 1, 1, 1, 1 };
	context.leftTopStart = context.leftTopCurrent = topline - V2{ 0, scale * context.font->metrics.ascent };
	return context;
}

inline
FontDrawContext InitializeStandardFontDrawContext(DebugState* state, V2 topline) {
	return InitializeFontDrawContext(state->font, 0.15f, topline);
}

internal
void ResetDebugCollation(DebugState* debugState, u32 frameWriteIndex) {
	EndTempMemory(debugState->scratchBuffer);
	CheckArena(debugState->arena);
	debugState->openEventFreeList = 0;
	debugState->eventStacksCount = 0;
	debugState->eventStacks = 0;
	debugState->paused = 0;
	debugState->selectedFrameIndex = U32_MAX;
	debugState->selectedRegionIndex = U32_MAX;
	debugState->selectedRecord = 0;
	debugState->frameReadIndex = (frameWriteIndex + 1) % MAX_DEBUG_FRAMES;
	debugState->scratchBuffer = BeginTempMemory(debugState->arena);
	debugState->frames = PushArray(debugState->arena, MAX_DEBUG_FRAMES, DebugFrameInfo);
}

inline
DebugState* GetDebugState() {
#if defined(INTERNAL_BUILD)
	if (debugGlobalMemory->debugMemorySize == 0) {
		return 0;
	}
	Assert(debugGlobalMemory->debugMemorySize >= sizeof(DebugState));
	DebugState* debugState = ptrcast(DebugState, debugGlobalMemory->debugMemory);
	if (!debugState->isInitialized) {
		TransientState* tranState = ptrcast(TransientState, debugGlobalMemory->transientMemory);
		Assert(tranState->isInitialized);
		InitializeArena(
			debugState->arena,
			ptrcast(u8, debugGlobalMemory->debugMemory) + sizeof(DebugState),
			debugGlobalMemory->debugMemorySize - sizeof(DebugState)
		);
		u32 frameWriteIndex = u4(debugGlobalState->frameAndEventIndex >> 32);
		debugState->renderGroup = AllocateRenderGroup(debugState->arena, &tranState->assets, MB(4), false);
		debugState->highPriorityQueue = tranState->highPriorityQueue;
		debugState->scratchBuffer = BeginTempMemory(debugState->arena);
		ResetDebugCollation(debugState, frameWriteIndex);
		debugState->isInitialized = true;
	}
	return debugState;
#else
	return 0;
#endif
}

inline 
void DebugBegin(LoadedBitmap& screenBitmap) {
#if INTERNAL_BUILD
	DebugState* state = GetDebugState();
	if (!state) {
		return;
	}
	state->overlayBoundaries = GetRectFromCenterDim(V2{ 0, 0 }, V2i(screenBitmap.width, screenBitmap.height));
	state->renderGroup.pushBufferSize = 0;
	state->renderGroup.projection = GetOrtographicProjection(screenBitmap.width, screenBitmap.height, 1);
	BeginRendering(state->renderGroup);
	state->font = GetOrPrefetchFont(
		state->renderGroup, GetFontWithType(*state->renderGroup.assets, Font_Debug)
	);
#endif
}

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
void DebugRenderLine(DebugState* state, const char* text, V2 pos, f32 scale, V4 color, bool render = true, Rect2* boundingBox = 0) {
	Rect2 resultBoundingBox = InversedInfinityRect2();
	u32 prevChar = 0;
	f32 spaceAdvance = scale * 55;
	for (const char* at = text; *at; at++) {
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
			BitmapId bid = GetFontGlyphBitmapIdFor(state->font, codepoint);
			AssetMetadata* metadata = GetAssetMetadata(*state->renderGroup.assets, bid.id);
			f32 width = f4(metadata->_bitmapInfo.width);
			f32 height = f4(metadata->_bitmapInfo.height);
			pos.X += scale * GetFontWidthAdvanceFor(state->font, prevChar, codepoint);
			V3 anchor = ToV3(pos, 0);
			if (render) {
				PushBitmap(state->renderGroup, bid, anchor, scale * height, V2{ 0, 0 }, color);
			}
			if (boundingBox) {
				Rect2 glyphRect = GetRectFromMinDim(pos, scale * V2{ width, height });
				resultBoundingBox = Union(resultBoundingBox, glyphRect);
			}
		}
		else {
			pos.X += scale * GetFontWidthAdvanceFor(state->font, prevChar, codepoint);
		}
		prevChar = codepoint;
	}
	if (boundingBox) {
		*boundingBox = resultBoundingBox;
	}
}

inline
void DebugRenderLine(DebugState* state, const char* text, FontDrawContext& context, bool render = true, Rect2* boundingBox = 0) {
	DebugRenderLine(state, text, context.leftTopCurrent, context.scale, context.color, render, boundingBox);
	context.leftTopCurrent.E[1] -= context.scale * GetFontLineAdvance(context.font);
}

inline
Rect2 GetTextBoundingBox(DebugState* state, const char* text, FontDrawContext& context) {
	Rect2 boundingBox = {};
	DebugRenderLine(state, text, context.leftTopCurrent, context.scale, context.color, false, &boundingBox);
	return boundingBox;
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
	*stack->events = {};
	return stack;
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
				newOpenEvent->childRegionCount = 0;
				stack->events = newOpenEvent;
			}
			else if (event->type == Event_BlockEnd) {
				DebugEventStack* stack = GetDebugStackForThread(debugState, event->threadId);
				OpenDebugEvent* block = stack->events;
				OpenDebugEvent* parentBlock = block->next;
				DebugEvent* openEvent = block->event;
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
							u16 regionIndex = u2(frameInfo->regionsCount++);
							DebugProfilerRegion* region = frameInfo->regions + regionIndex;
							// TODO: If minT < 0 -> that means openEvent started on one of the previous frames.
							// Do something about it!
							if (event->debugRecordIndex == 2 && event->translationUnit == 2) {
								int breakhere = 5;
							}
							region->minT = minT;
							region->maxT = maxT;
							region->recordIndex = event->debugRecordIndex;
							region->translationUnit = event->translationUnit;
							region->laneId = stack->laneId;
							region->parentRecord = 0;
							if (parentBlock->event) {
								region->parentRecord = GetDebugRecordFor(parentBlock->event);
								Assert(parentBlock->childRegionCount < ArrayCount(parentBlock->childRegionIndexes));
								parentBlock->childRegionIndexes[parentBlock->childRegionCount++] = regionIndex;
							}
							region->startCycles = openEvent->cycles;
							region->endCycles = event->cycles;
							region->frameStartCycles = frameInfo->startCycles;
							region->frameEndCycles = frameInfo->endCycles;
							region->parentRegionIndex = U32_MAX;
							for (u32 index = 0; index < block->childRegionCount; index++) {
								DebugProfilerRegion* childRegion = frameInfo->regions + block->childRegionIndexes[index];
								childRegion->parentRegionIndex = regionIndex;
							}
						}
						stack->events = parentBlock;
						block->next = debugState->openEventFreeList;
						debugState->openEventFreeList = block;
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

void DebugMainMenu() {
	DebugState* debugState = GetDebugState();
	if (!debugState) {
		return;
	}
	struct MenuItem {
		const char* name;
		u32 state;
	};
	MenuItem menuItems[] = {
		{ "Show profiler", 0 },
		{ "Show hitboxes", 0 },
		{ "Show chunk borders", 0 },
	};
	f32 menuPosX = -450;
	f32 menuPosY = 250;
	for (u32 itemIndex = 0; itemIndex < ArrayCount(menuItems); itemIndex++) {
		MenuItem* item = menuItems + itemIndex;
		V2 center = {
			menuPosX ,
			menuPosY - itemIndex * 20.f
		};
		Rect2 checkbox = GetRectFromCenterHalfDim(center, V2{5, 5});
		V4 color = item->state ? V4{ 0, 1, 0, 1 } : V4{ 1, 0, 0, 1 };
		PushRect(debugState->renderGroup, checkbox, 0, V2{ 0, 0 }, color);
		center += V2{ 5.f, -5.f };
		DebugRenderLine(debugState, item->name, center, 0.15f, color);
	}
}

void WriteDebugConfig(DebugState* state) {
	const char* menuItems[] = {
		"DEBUGUI_CAMERA_ZOOMOUT",
		"DEBUGUI_CAMERA_ZOOMOUT_VALUE",
		"DEBUGUI_FULLHD",
	};

	char buffer[4096];
	char* at = buffer;
	char* end = buffer + sizeof(buffer);
	for (u32 itemIndex = 0; itemIndex < ArrayCount(menuItems); itemIndex++) {
		const char* item = menuItems[itemIndex];
		if (itemIndex == 0) {
			at += sprintf_s(at, end - at, "#define %s %d\n", item, state->debugZoomoutCamera);
		}
		else if (itemIndex == 1) {
			f32 val = state->debugCameraDistanceToTarget ? 50.f : 20.f;
			at += sprintf_s(at, end - at, "#define %s %ff\n", item, val);
		}
		else if (itemIndex == 2) {
			at += sprintf_s(at, end - at, "#define %s %d\n", item, state->debugFullHD);
		}
	}
	debugGlobalMemory->debug.WriteFile("..\\code\\engine_debug_config.h", buffer, at - buffer);
}

void DebugVariablesMenu(DebugState* state, V2 mousePos, Controller& controller) {
	const char* menuItems[] = {
		"DEBUGUI_CAMERA_ZOOMOUT",
		"DEBUGUI_CAMERA_ZOOMOUT_VALUE",
		"DEBUGUI_FULLHD",
		"Update and compile"
	};
	V2 topline = V2{ state->overlayBoundaries.min.X, state->overlayBoundaries.max.Y };
	FontDrawContext context = InitializeStandardFontDrawContext(state, topline);
	i32 hotItemIndex = -1;
	for (u32 itemIndex = 0; itemIndex < ArrayCount(menuItems); itemIndex++) {
		const char* item = menuItems[itemIndex];
		char buffer[256];
		if (itemIndex == 0) {
			sprintf_s(buffer, "%s: %d", item, state->debugZoomoutCamera);
		}
		else if (itemIndex == 1) {
			sprintf_s(buffer, "%s: %d", item, state->debugCameraDistanceToTarget);
		}
		else if (itemIndex == 2) {
			sprintf_s(buffer, "%s: %d", item, state->debugFullHD);
		}
		else {
			sprintf_s(buffer, "%s", item);
		}
		
		Rect2 bb = GetTextBoundingBox(state, buffer, context);
		if (IsInRectangle(bb, mousePos)) {
			context.color = V4{ 0, 0, 1, 1 };
			hotItemIndex = itemIndex;
		}
		else {
			context.color = V4{ 1, 1, 1, 1 };
		}
		DebugRenderLine(state, buffer, context);
	}
	if (WasPressed(controller.B.mouseLeft)) {
		if (hotItemIndex == 0) {
			state->debugZoomoutCamera = !state->debugZoomoutCamera;
		}
		else if (hotItemIndex == 1) {
			state->debugCameraDistanceToTarget = !state->debugCameraDistanceToTarget;
		}
		else if (hotItemIndex == 2) {
			state->debugFullHD = !state->debugFullHD;
		}
		else if (hotItemIndex == 3) {
			WriteDebugConfig(state);
			if (state->compilationHandle.state != CmdState_Running) {
				char cwd[] = "..\\code";
				char cmd[] = "cmd.exe /c build.bat --game_only";
				state->compilationHandle = Platform->SystemExecuteCommand(cwd, cmd);
			}
		}
	}
	
	
}

void DebugRenderOverlay(ProgramMemory* memory, LoadedBitmap& dstBitmap, InputData& input) {
	TIMED_FUNCTION;
	DebugState* state = GetDebugState();
	if (!state || !state->font) {
		return;
	}
	Controller& controller = input.controllers[KB_CONTROLLER_IDX];
	V2 mousePos = { controller.mouse.X - 0.5f, controller.mouse.Y - 0.5f };
	mousePos = Hadamard(mousePos, V2i(dstBitmap.width, dstBitmap.height));
	if (WasPressed(controller.B.mouseRight)) {
		state->paused = !state->paused;
	}
	DebugVariablesMenu(state, mousePos, controller);

	DebugRecord* hotRecord = 0;
	u32 hotRegionIndex = U32_MAX;
	u32 hotFrameIndex = U32_MAX;
	f32 fontScale = 0.15f;
	V4 fontColor = V4{ 0.8f, 0.8f, 0.8f, 1 };
	debugGlobalState->debugRecordsCount[0] = debugRecordsCount_Main;
	debugGlobalState->debugRecordsCount[1] = debugRecordsCount_Optimized;

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
	f32 frameWidth = f4(state->eventStacksCount) * threadLaneTotalWidth + frameLaneSpace;
	for (u32 frameIndex = state->frameWriteIndex;;) {
		frameIndex = (frameIndex + 1) % MAX_DEBUG_FRAMES;
		if (frameIndex == state->frameWriteIndex) {
			break;
		}
		DebugFrameInfo* frameInfo = state->frames + frameIndex;
		for (u32 regionIndex = 0; regionIndex < frameInfo->regionsCount; regionIndex++) {
			DebugProfilerRegion* region = frameInfo->regions + regionIndex;
			if (state->selectedRecord != region->parentRecord) {
				continue;
			}
			if (state->selectedFrameIndex != U32_MAX &&
				(state->selectedRegionIndex != region->parentRegionIndex ||
					state->selectedFrameIndex != frameIndex)) {
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
					DebugRenderLine(state, buffer, textPos, fontScale, fontColor);
					textPos = V2{ -400, 250.f };
					sprintf_s(buffer, "t<%4f,%4f>, f<%llu, %llu> (%d)",
						region->minT,
						region->maxT,
						region->frameStartCycles,
						region->frameEndCycles,
						u4(region->frameEndCycles - region->frameStartCycles)
							
					);
					DebugRenderLine(state, buffer, textPos, fontScale, fontColor);
					textPos = V2{ -400, 230.f };
					sprintf_s(buffer, "c<%llu, %llu> (%d)",
						region->startCycles,
						region->endCycles,
						u4(region->endCycles - region->startCycles)
					);
					DebugRenderLine(state, buffer, textPos, fontScale, fontColor);
				}
				hotRecord = record;
				hotRegionIndex = regionIndex;
				hotFrameIndex = frameIndex;
			}
			PushRect(state->renderGroup, rectangle, 0, V2{ 0, 0 }, isHovered ? V4{1, 1, 1, 1} : colors[colorIndex]);
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
	PushRect(state->renderGroup, targetFrameRateCenter, targetFrameRateSize, V2{ 0, 0 }, V4{ 0, 0, 0, 1 });

	if (state->compilationHandle.state == CmdState_Running) {
		DebugRenderLine(state, "Compiling", V2{ 0, 0 }, 0.15f, V4{1, 1, 1, 1});
	}

	if (WasPressed(controller.B.mouseLeft)) {
		state->selectedRecord = hotRecord;
	}
	if (WasPressed(controller.B.mouseMiddle)) {
		state->selectedRegionIndex = hotRegionIndex;
		state->selectedFrameIndex = hotFrameIndex;
		state->selectedRecord = hotRecord;
	}

	TiledRenderGroupToBuffer(state->renderGroup, dstBitmap, state->highPriorityQueue);
}

extern "C" DebugGlobalState* DebugInit(ProgramMemory* memory) {
	return debugGlobalState;
}

extern "C" DebugFrameInfo* DebugFinishFrame(ProgramMemory* memory) {
	TIMED_FUNCTION;
	DebugState* state = GetDebugState();
	if (!state) {
		return 0;
	}
	EndRendering(state->renderGroup);
	if (state->compilationHandle.state == CmdState_Running) {
		Platform->SystemGetCommandState(state->compilationHandle);
	}
	if (state->paused) {
		return 0;
	}
	DebugCollateEvents(state);
	u32 frameWriteIndex = u4(debugGlobalState->frameAndEventIndex >> 32);
	DebugFrameInfo* result = state->frames + frameWriteIndex;
	return result;
}