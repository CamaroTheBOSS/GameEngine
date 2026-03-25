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
#define DEBUG_CONFIG_PATH "..\\code\\engine_debug_config.h"
#define DEBUG_COLLATION_SCALE (1.f / (DEBUG_TARGET_REFRESH_MS * DEBUG_CPU_FREQ));

inline
FontDrawContext InitializeFontDrawContext(LoadedFont* font, f32 scale, V2 topline) {
	FontDrawContext context = {};
	context.font = font;
	context.scale = scale;
	context.leftTopStart = context.leftTopCurrent = topline - V2{ 0, scale * context.font->metrics.ascent };
	return context;
}

inline
FontDrawContext InitializeStandardFontDrawContext(DebugState* state, V2 topline) {
	return InitializeFontDrawContext(state->font, 0.15f, topline);
}

internal
void ResetDebugCollation(DebugState* debugState, u32 frameWriteIndex) {
	EndTempMemory(debugState->collationScratchBuffer);
	CheckArena(debugState->collationArena);
	debugState->openEventFreeList = 0;
	debugState->eventStacksCount = 0;
	debugState->eventStacks = 0;
	debugState->selectedFrameIndex = U32_MAX;
	debugState->selectedRegionIndex = U32_MAX;
	debugState->selectedRecord = 0;
	debugState->frameReadIndex = (frameWriteIndex + 1) % MAX_DEBUG_FRAMES;
	debugState->collationScratchBuffer = BeginTempMemory(debugState->collationArena);
	debugState->frames = PushArray(debugState->collationArena, MAX_DEBUG_FRAMES, DebugFrameInfo);
}

internal
void ReadDebugConfig(DebugState* state) {
#if 0
	PlatformFileHandle* handle = Platform->FileOpen(DEBUG_CONFIG_PATH);
	TemporaryMemory fileDataMemory = BeginTempMemory(state->mainArena);
	char* fileData = PushArray(state->mainArena, handle->size + 1, char);
	Platform->FileRead(handle, 0, handle->size, fileData);
	fileData[handle->size] = 0;
#endif

#if 0
	// Find substring
	//TODO: UB?
	char* define = ptrcast(char, "#define ");
	char* result = 0;
	for (;;) {
		char* pattern = define;
		u32 count = 0;
		while (*fileData && *fileData++ == *pattern++) {
			count++;
			if (!*pattern) {
				result = fileData - count;
				break;
			}
		}
		if (result || !*fileData) {
			break;
		}
	}
#endif
#if 0
	//NOTE: Reading assumes that variables are IN ORDER
	u32 variableIndex = 0;
	u32 spaceCount = 0;
	while (*fileData) {
		char* start = 0;
		char* end = 0;
		while (*fileData || spaceCount == 2) {
			if (*fileData++ == ' ') {
				spaceCount++;
			}
		}
		while (*fileData) {
			if (*fileData == '\n') {
				end = fileData;
				break;
			}
			fileData++;
		};
		
		variableIndex++;
	}
#endif

#if 0
	EndTempMemory(fileDataMemory);
#endif
}

inline
DebugVariable* AddDebugVariable(DebugState* state, const char* name, DebugVarType type) {
	Assert(state->variableCount < ArrayCount(state->variables));
	DebugVariable* variable = state->variables + state->variableCount++;
	u32 length = StringLength(name) + 1;
	variable->name = PushString(state->mainArena, name, length);
	variable->type = type;
	return variable;
}

inline
DebugVariable* AddDebugVariable(DebugState* state, const char* name, DebugVarType type, bool value) {
	DebugVariable* variable = AddDebugVariable(state, name, type);
	variable->boolean = value;
	return variable;
}

inline
DebugVariable* AddDebugVariable(DebugState* state, const char* name, DebugVarType type, float value) {
	DebugVariable* variable = AddDebugVariable(state, name, type);
	variable->fl32 = value;
	return variable;
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
		debugGlobalState->debugRecordsCount[0] = debugRecordsCount_Main;
		debugGlobalState->debugRecordsCount[1] = debugRecordsCount_Optimized;

		TransientState* tranState = ptrcast(TransientState, debugGlobalMemory->transientMemory);
		Assert(tranState->isInitialized);
		InitializeArena(
			debugState->mainArena,
			ptrcast(u8, debugGlobalMemory->debugMemory) + sizeof(DebugState),
			debugGlobalMemory->debugMemorySize - sizeof(DebugState)
		);
		SubArena(debugState->collationArena, debugState->mainArena, MB(16));
		u32 frameWriteIndex = u4(debugGlobalState->frameAndEventIndex >> 32);
		debugState->renderGroup = AllocateRenderGroup(debugState->mainArena, &tranState->assets, MB(4), false);
		debugState->highPriorityQueue = tranState->highPriorityQueue;
		debugState->collationScratchBuffer = BeginTempMemory(debugState->collationArena);
		ResetDebugCollation(debugState, frameWriteIndex);

		debugState->variableCount = 0;
		AddDebugVariable(debugState, "CameraZoomout", DebugVarType::Bool, false);
		AddDebugVariable(debugState, "CameraZoomoutValue", DebugVarType::Float, 20.f);
		AddDebugVariable(debugState, "RenderFullHD", DebugVarType::Bool, false);
		AddDebugVariable(debugState, "Update and Compile", DebugVarType::CompilationSwitch);
		debugState->profilerSwitch = AddDebugVariable(debugState, "Expand profiler", DebugVarType::ProfilerSwitch, true);
		debugState->profilerPause = AddDebugVariable(debugState, "Pause profiler", DebugVarType::ProfilerPause, false);
		DebugVariable* profilerUI = AddDebugVariable(debugState, "ProfilerUI", DebugVarType::ProfilerUI);
		profilerUI->profiler.rect = GetRectFromMinMax(V2{ -450, -250 }, V2{ 450, -50 });

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
	if (debugGlobalMemory->executableReloaded) {

	}
	state->overlayBoundaries = GetRectFromCenterDim(V2{ 0, 0 }, V2i(screenBitmap.width, screenBitmap.height));
	state->renderGroup.pushBufferSize = 0;
	state->renderGroup.projection = GetOrtographicProjection(screenBitmap.width, screenBitmap.height, 1);
	BeginRendering(state->renderGroup);
	state->font = GetOrPrefetchFont(
		state->renderGroup, GetFontWithType(*state->renderGroup.assets, Font_Debug)
	);
	if (state->font) {
		state->fontContext = InitializeStandardFontDrawContext(state, V2{ state->overlayBoundaries.min.X, state->overlayBoundaries.max.Y });
	}
	state->hotRecord = 0;
	state->hotFrameIndex = U32_MAX;
	state->hotRegionIndex = U32_MAX;
	state->hotVariable = 0;
	state->nextHotVariable = 0;
	state->hotInteraction = DebugInteract_None;
	state->nextHotInteraction = DebugInteract_None;
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
void DebugRenderLine(DebugState* state, const char* text, FontDrawContext& context, V4 color, bool render = true, Rect2* boundingBox = 0) {
	DebugRenderLine(state, text, context.leftTopCurrent, context.scale, color, render, boundingBox);
	context.leftTopCurrent.E[1] -= context.scale * GetFontLineAdvance(context.font);
}

inline
Rect2 GetTextBoundingBox(DebugState* state, const char* text, FontDrawContext& context, V4 color) {
	Rect2 boundingBox = {};
	DebugRenderLine(state, text, context.leftTopCurrent, context.scale, color, false, &boundingBox);
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
	stack->events = PushStructSize(state->collationArena, OpenDebugEvent);
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
	TemporaryMemory stackMemory = BeginTempMemory(debugState->collationArena);
	debugState->eventStacks = PushArray(debugState->collationArena, MAX_DEBUG_THREADS, DebugEventStack);
	debugState->eventStacksCount = 0;
	debugState->openEventFreeList = 0;

	debugState->frameWriteIndex = u4(debugGlobalState->frameAndEventIndex >> 32);
	for (; 
		debugState->frameReadIndex != debugState->frameWriteIndex;
		debugState->frameReadIndex = (debugState->frameReadIndex + 1) % MAX_DEBUG_FRAMES
		) {
		DebugFrameInfo* frameInfo = debugState->frames + debugState->frameReadIndex;
		frameInfo->regionsCount = 0;
		f32 scale = DEBUG_COLLATION_SCALE;
		DebugEvent* eventsInFrame = debugGlobalState->debugEvents[debugState->frameReadIndex];
		u32 eventsInFrameCount = debugGlobalState->debugEventsCount[debugState->frameReadIndex];
		u64 startCycles = debugGlobalState->frameStartCycles[debugState->frameReadIndex];
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
					newOpenEvent = PushStructSize(debugState->collationArena, OpenDebugEvent);
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
						f32 minT = f4(openEvent->cycles - startCycles) * scale;
						f32 maxT = f4(event->cycles - startCycles) * scale;
						f32 thresholdT = 0.01f;
						if ((maxT - minT) > thresholdT) {
							Assert(frameInfo->regionsCount < ArrayCount(frameInfo->regions));
							u16 regionIndex = u2(frameInfo->regionsCount++);
							DebugProfilerRegion* region = frameInfo->regions + regionIndex;
							// TODO: If minT < 0 -> that means openEvent started on one of the previous frames.
							// Do something about it!
							if (event->debugRecordIndex == 2 && event->translationUnit == 2) {
								int breakhere = 5;
							}
							region->recordIndex = event->debugRecordIndex;
							region->translationUnit = event->translationUnit;
							region->laneId = stack->laneId;
							region->parentRecord = 0;
							if (parentBlock->event) {
								region->parentRecord = GetDebugRecordFor(parentBlock->event);
								Assert(parentBlock->childRegionCount < ArrayCount(parentBlock->childRegionIndexes));
								parentBlock->childRegionIndexes[parentBlock->childRegionCount++] = regionIndex;
							}
							region->minT = minT;
							region->maxT = maxT;
							region->startCycles = openEvent->cycles;
							region->endCycles = event->cycles;
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

void WriteDebugConfig(DebugState* state) {
	char buffer[4096];
	char* at = buffer;
	char* end = buffer + sizeof(buffer);
	for (u32 varIndex = 0; varIndex < state->variableCount; varIndex++) {
		DebugVariable* var = state->variables + varIndex;
		switch (var->type) {
		case DebugVarType::Bool: {
			at += sprintf_s(at, end - at, "#define DEBUGUI_%s %d\n", var->name, var->boolean);
		} break;
		case DebugVarType::Float: {
			at += sprintf_s(at, end - at, "#define DEBUGUI_%s %ff\n", var->name, var->fl32);
		} break;
		}
	}
	debugGlobalMemory->debug.WriteFile(DEBUG_CONFIG_PATH, buffer, at - buffer);
}

void DebugRenderProfilerUI(DebugState* state, Rect2 boundaries, V2 mousePos) {
	f32 profilerPosY = boundaries.min.Y;
	f32 profilerPosX = boundaries.min.X;
	f32 profilerHeight = GetDim(boundaries).Y;
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
		V4{1, 0.5f, 0.5f, 1},
	};
	u32 frameIndexForDrawing = 0;
	f32 frameWidth = f4(state->eventStacksCount) * threadLaneTotalWidth + frameLaneSpace;
	f32 collationScale = DEBUG_COLLATION_SCALE;
	PushRect(state->renderGroup, boundaries, 0, V2{ 0, 0 }, V4{ 0.03f, 0.03f, 0.03f, 1 });
	u32 firstFrameIndex = state->frameWriteIndex + 1;
	for (u32 frameIndex = firstFrameIndex;;) {
		frameIndex = (frameIndex + 1) % MAX_DEBUG_FRAMES;
		if (frameIndex == state->frameWriteIndex) {
			break;
		}
		bool breakAfterThisFrame = false;
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
			f32 minT = region->minT;
			f32 maxT = region->maxT;
			V3 spanCenter = {
				profilerPosX + frameIndexForDrawing * frameWidth + (f4(region->laneId) + 0.5f) * threadLaneTotalWidth,
				profilerPosY + 0.5f * (maxT + minT) * profilerHeight,
				0
			};
			V2 spanSize = {
				threadLaneWidth,
				(maxT - minT) * profilerHeight
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
					V4 color = V4{ 1, 1, 1, 1 };
					f32 lineAdvance = state->fontContext.scale * f4(GetFontLineAdvance(state->font));
					V2 textPos = mousePos + V2{ 0, lineAdvance };
					DebugRenderLine(state, buffer, textPos, state->fontContext.scale, color);
					textPos += V2{ 0, lineAdvance };
					sprintf_s(buffer, "t<%4f,%4f>, ec(%d)",
						minT,
						maxT,
						u4(region->endCycles - region->startCycles)
					);
					DebugRenderLine(state, buffer, textPos, state->fontContext.scale, color);
				}
				state->hotRecord = record;
				state->hotRegionIndex = regionIndex;
				state->hotFrameIndex = frameIndex;
			}
#if 1
			rectangle.max.X = Clip(rectangle.max.X, boundaries.min.X, boundaries.max.X);
			if (IsValid(rectangle)) {
				if (frameIndexForDrawing >= 10) {
					int breakhere = 0;
				}
				PushRect(state->renderGroup, rectangle, 0, V2{ 0, 0 }, isHovered ? V4{ 1, 1, 1, 1 } : colors[colorIndex]);
			}
			else {
				if (rectangle.min.X > 1000.f || rectangle.min.X < -1000.f) {
					int breakhere = 5;
				}
				breakAfterThisFrame = true;
			}
#else
			PushRect(state->renderGroup, rectangle, 0, V2{ 0, 0 }, isHovered ? V4{ 1, 1, 1, 1 } : colors[colorIndex]);
#endif
		}
		frameIndexForDrawing++;
		if (breakAfterThisFrame) {
			break;
		}
	}
	f32 profilerWidth = f4(frameIndexForDrawing + 1) * frameWidth;
	V3 targetFrameRateCenter = {
		profilerPosX + 0.5f * profilerWidth,
		profilerPosY + profilerHeight,
		0
	};
	V2 targetFrameRateSize = { profilerWidth, 2.f };
	PushRect(state->renderGroup, targetFrameRateCenter, targetFrameRateSize, V2{ 0, 0 }, V4{ 0, 0, 0, 1 });
}

void DebugRenderVariablesMenu(DebugState* state, V2 mousePos) {
	V2 topline = V2{ state->overlayBoundaries.min.X, state->overlayBoundaries.max.Y };
	for (u32 varIndex = 0; varIndex < state->variableCount; varIndex++) {
		DebugVariable* var = state->variables + varIndex;
		char buffer[256] = {};
		char* end = buffer + sizeof(buffer);
		switch (var->type) {
		case DebugVarType::Bool: {
			sprintf_s(buffer, end - buffer, "%s: %d", var->name, var->boolean);
		} break;
		case DebugVarType::Float: {
			sprintf_s(buffer, end - buffer, "%s: %f", var->name, var->fl32);
		} break;
		case DebugVarType::ProfilerUI: {
			DebugRenderProfilerUI(state, var->profiler.rect, mousePos);
		} break;
		default: {
			sprintf_s(buffer, end - buffer, "%s", var->name);
		} break;
		}
		V4 color = V4{ 1, 1, 1, 1 };
		Rect2 bb = GetTextBoundingBox(state, buffer, state->fontContext, color);
		if (IsInRectangle(bb, mousePos)) {
			color = V4{ 0, 0, 1, 1 };
			state->hotVariable = var;
		}
		DebugRenderLine(state, buffer, state->fontContext, color);
	}
}

void DebugInteract(DebugState* state, Controller& controller) {
	if (state->hotVariable && WasPressed(controller.B.mouseLeft)) {
		switch (state->hotVariable->type) {
		case DebugVarType::ProfilerPause:
		case DebugVarType::ProfilerSwitch:
		case DebugVarType::Bool: {
			state->hotVariable->boolean = !state->hotVariable->boolean;
		} break;
		case DebugVarType::Float: {
			state->hotVariable->fl32 += 1.f;
		} break;
		case DebugVarType::CompilationSwitch: {
			WriteDebugConfig(state);
			if (state->compilationHandle.state != CmdState_Running) {
				char cwd[] = "..\\code";
				char cmd[] = "cmd.exe /c build.bat --game_only";
				state->compilationHandle = Platform->SystemExecuteCommand(cwd, cmd);
			}
		} break;
		}
	}
	else {
		if (WasPressed(controller.B.mouseLeft)) {
			state->selectedRecord = state->hotRecord;
		}
		if (WasPressed(controller.B.mouseMiddle)) {
			state->selectedRegionIndex = state->hotRegionIndex;
			state->selectedFrameIndex = state->hotFrameIndex;
			state->selectedRecord = state->hotRecord;
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

	DebugRenderVariablesMenu(state, mousePos);
	if (state->compilationHandle.state == CmdState_Running) {
		DebugRenderLine(state, "Compiling", state->fontContext, V4{1, 1, 1, 1});
	}
	DebugInteract(state, controller);
	
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
	if (state->profilerPause->boolean) {
		return 0;
	}
	DebugCollateEvents(state);
	u32 frameWriteIndex = u4(debugGlobalState->frameAndEventIndex >> 32);
	DebugFrameInfo* result = state->frames + frameWriteIndex;
	return result;
}