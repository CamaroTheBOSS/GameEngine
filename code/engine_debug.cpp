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
inline bool WasReleased(Button& button);
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
DebugVariable* _AddDebugVariable(DebugState* state, const char* name, DebugVarType type) {
	Assert(state->variableCount < ArrayCount(state->variables));
	DebugVariable* variable = state->variables + state->variableCount++;
	u32 length = StringLength(name) + 1;
	variable->name = PushString(state->mainArena, name, length);
	variable->type = type;
	return variable;
}

inline
DebugVariableRef* _AddReferencedDebugVariable(DebugState* state, DebugVariableContext& context, const char* name, DebugVarType type) {
	DebugVariable* var = _AddDebugVariable(state, name, type);
	DebugVariableRef* ref = PushStructSize(state->mainArena, DebugVariableRef);
	ref->next = 0;
	ref->var = var;
	ref->parent = context.parent;
	if (context.parent) {
		Assert(context.parent->var->type == DebugVarType::Group);
		DebugVariableGroup* group = &context.parent->var->group;
		ref->next = group->firstChild;
		group->firstChild = ref;
	}
	return ref;
}

inline
DebugVariableRef* AddReferencedDebugVariable(DebugState* state, DebugVariableContext& context, const char* name, DebugVarType type, bool value) {
	Assert(type == DebugVarType::Bool || type == DebugVarType::CompileTimeBool);
	DebugVariableRef* ref = _AddReferencedDebugVariable(state, context, name, type);
	ref->var->boolean = value;
	return ref;
}

inline
DebugVariableRef* AddReferencedDebugVariable(DebugState* state, DebugVariableContext& context, const char* name, DebugVarType type, float value) {
	Assert(type == DebugVarType::Float || type == DebugVarType::CompileTimeFloat);
	DebugVariableRef* ref = _AddReferencedDebugVariable(state, context, name, type);
	ref->var->fl32 = value;
	return ref;
}

inline
DebugVariableRef* BeginDebugVariableGroup(DebugState* state, DebugVariableContext& context, const char* name) {
	DebugVariableRef* ref = _AddReferencedDebugVariable(state, context, name, DebugVarType::Group);
	DebugVariableGroup* group = &ref->var->group;
	group->expanded = false;
	group->firstChild = 0;
	context.parent = ref;
	return ref;
}

inline
void EndDebugVariableGroup(DebugVariableContext& context) {
	Assert(context.parent || !"Tried to enclose group when none were opened");
	context.parent = context.parent->parent;
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
		debugState->compilationHandle.state = CmdState_Completed;
		
		DebugVariableContext context = {};
		debugState->UITree = BeginDebugVariableGroup(debugState, context, "Debugging");
			BeginDebugVariableGroup(debugState, context, "Compile time switches");
				AddReferencedDebugVariable(debugState, context, "CameraZoomout", DebugVarType::CompileTimeBool, false);
				AddReferencedDebugVariable(debugState, context, "CameraZoomoutValue", DebugVarType::CompileTimeFloat, 20.f);
				AddReferencedDebugVariable(debugState, context, "ShowDebugInteractions", DebugVarType::CompileTimeBool, true);
				AddReferencedDebugVariable(debugState, context, "ShowEventsCount", DebugVarType::CompileTimeBool, true);
				AddReferencedDebugVariable(debugState, context, "RenderFullHD", DebugVarType::CompileTimeBool, false);
			EndDebugVariableGroup(context);
			_AddReferencedDebugVariable(debugState, context, "Update and Compile", DebugVarType::CompilationSwitch);
			BeginDebugVariableGroup(debugState, context, "Profiler");
				debugState->profilerPause = AddReferencedDebugVariable(debugState, context, "Pause profiler", DebugVarType::Bool, false);
				DebugVariableRef* profilerUI = _AddReferencedDebugVariable(debugState, context, "ProfilerUI", DebugVarType::ProfilerUI);
				profilerUI->var->profiler.rect = GetRectFromMinMax(V2{ -450, -250 }, V2{ 450, -50 });
			EndDebugVariableGroup(context);
		EndDebugVariableGroup(context);

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
	state->nextInteraction = {};
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

enum DebugVarToTextFlags {
	DebugVarToText_ConfigPrefix = 0x1,
	DebugVarToText_AddFloatSuffix = 0x2,
	DebugVarToText_AddNewLine = 0x4,
	DebugVarToText_AddColon = 0x8,
};

u64 DebugVariableToText(DebugVariable* var, char* buffer, u32 size, u32 flags) {
	char* at = buffer;
	char* end = buffer + size;
	if (flags & DebugVarToText_ConfigPrefix) {
		at += sprintf_s(at, end - at, "#define DEBUGUI_");
	}
	const char* colon = (flags & DebugVarToText_AddColon) ? ":" : "";

	switch (var->type) {
	case DebugVarType::CompileTimeBool:
	case DebugVarType::Bool: {
		at += sprintf_s(at, end - at, "%s%s %d", var->name, colon, var->boolean);
	} break;
	case DebugVarType::CompileTimeFloat:
	case DebugVarType::Float: {
		at += sprintf_s(at, end - at, "%s%s %f", var->name, colon, var->fl32);
		if (flags & DebugVarToText_AddFloatSuffix && (end - at) > 0) {
			*at++ = 'f';
		}
	} break;
	case DebugVarType::Group:
	case DebugVarType::CompilationSwitch: {
		at += sprintf_s(at, end - at, "%s%s", var->name, colon);
	} break;
	}
	if (flags & DebugVarToText_AddNewLine && (end - at) > 0) {
		*at++ = '\n';
	}
	return at - buffer;
}

void WriteDebugConfig(DebugState* state) {
	char buffer[4096];
	char* at = buffer;
	char* end = buffer + sizeof(buffer);
	for (u32 varIndex = 0; varIndex < state->variableCount; varIndex++) {
		DebugVariable* var = state->variables + varIndex;
		if (var->type >= DebugVarType::CompileTimeCount) {
			continue;
		}
		at += DebugVariableToText(var, at, u4(end - at),
			DebugVarToText_ConfigPrefix |
			DebugVarToText_AddFloatSuffix |
			DebugVarToText_AddNewLine
		);
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
}

void DebugRenderVariablesMenu(DebugState* state, V2 mousePos) {
	V2 topline = V2{ state->overlayBoundaries.min.X, state->overlayBoundaries.max.Y };
#if 1
	DebugVariableRef* parent = 0;
	DebugVariableRef* node = state->UITree;
	while (node) {
		V4 itemColor = V4{ 1, 1, 1, 1 };
		V4 hotItemColor = V4{ 0.2f, 0.5f, 1.0f, 1 };

		DebugVariable* var = node->var;
		char buffer[256] = {};
		char* end = buffer + sizeof(buffer);
		if (var->type == DebugVarType::ProfilerUI) {
			DebugRenderProfilerUI(state, var->profiler.rect, mousePos);
			Rect2 anchor = GetRectFromCenterDim(var->profiler.rect.max, V2{ 8, 8 });
			bool isHot = IsInRectangle(anchor, mousePos) && state->nextInteraction.type == DebugInteract_None;
			if (isHot) {
				itemColor = hotItemColor;
				state->nextInteraction.hot = var;
			}
			PushRect(state->renderGroup, anchor, 0, V2{ 0, 0 }, itemColor);
		}
		else {
			DebugVariableToText(var, buffer, ArrayCount(buffer),
				DebugVarToText_AddColon
			);
			Rect2 bb = GetTextBoundingBox(state, buffer, state->fontContext, itemColor);
			bool isHot = IsInRectangle(bb, mousePos) && state->nextInteraction.type == DebugInteract_None;
			if (isHot) {
				itemColor = hotItemColor;
				state->nextInteraction.hot = var;
			}
			DebugRenderLine(state, buffer, state->fontContext, itemColor);
		}

		if (var->type == DebugVarType::Group && var->group.expanded) {
			parent = node;
			node = var->group.firstChild;
		}
		else if (node) {
			node = node->next;
		}
		if (!node && parent) {
			node = parent->next;
			if (node) {
				parent = node->parent;
			}
		}
	}
#else
	for (u32 varIndex = 0; varIndex < state->variableCount; varIndex++) {
		V4 itemColor = V4{ 1, 1, 1, 1 };
		V4 hotItemColor = V4{ 0.2f, 0.5f, 1.0f, 1 };
		
		DebugVariable* var = state->variables + varIndex;
		char buffer[256] = {};
		char* end = buffer + sizeof(buffer);
		if (var->type == DebugVarType::ProfilerUI) {
			DebugRenderProfilerUI(state, var->profiler.rect, mousePos);
			Rect2 anchor = GetRectFromCenterDim(var->profiler.rect.max, V2{ 8, 8 });
			bool isHot = IsInRectangle(anchor, mousePos) && state->nextHotInteraction == DebugInteract_None;
			if (isHot) {
				itemColor = hotItemColor;
				state->nextHotVariable = var;
			}
			PushRect(state->renderGroup, anchor, 0, V2{ 0, 0 }, itemColor);
		}
		else {
			DebugVariableToText(var, buffer, ArrayCount(buffer),
				DebugVarToText_AddColon
			);
			Rect2 bb = GetTextBoundingBox(state, buffer, state->fontContext, itemColor);
			bool isHot = IsInRectangle(bb, mousePos) && state->nextHotInteraction == DebugInteract_None;
			if (isHot) {
				itemColor = hotItemColor;
				state->nextHotVariable = var;
			}
			DebugRenderLine(state, buffer, state->fontContext, itemColor);
		}
		
	}
#endif
}

void DebugInteract(DebugState* state, V2 mousePos, Controller& controller) {
	// Begin interaction
	if (state->nextInteraction.hot) {
		state->nextInteraction.startMousePos = mousePos;
		switch (state->nextInteraction.hot->type) {
		case DebugVarType::CompileTimeBool:
		case DebugVarType::Bool: {
			if (WasReleased(controller.B.mouseLeft)) {
				state->nextInteraction.type = DebugInteract_Toggle;
			}
		} break;
		case DebugVarType::Group: {
			if (WasReleased(controller.B.mouseLeft)) {
				state->nextInteraction.type = DebugInteract_Expand;
			}
		} break;
		case DebugVarType::CompileTimeFloat:
		case DebugVarType::Float: {
			if (WasPressed(controller.B.mouseLeft)) {
				state->nextInteraction.type = DebugInteract_DragIncrease;
			}
		} break;
		case DebugVarType::CompilationSwitch: {
			if (WasReleased(controller.B.mouseLeft)) {
				state->nextInteraction.type = DebugInteract_Compile;
			}
		} break;
		case DebugVarType::ProfilerUI: {
			if (WasPressed(controller.B.mouseLeft)) {
				state->nextInteraction.type = DebugInteract_Resize;
			}
		} break;
		}
	}
	else {
		if (IsPressed(controller.B.mouseLeft) || IsPressed(controller.B.mouseRight)) {
			state->nextInteraction.type = DebugInteract_Noop;
		}
	}
	if (state->nextInteraction.type != DebugInteract_None) {
		state->nextInteraction.var = state->nextInteraction.hot;
	}
	if (!state->interaction.var && state->interaction.type != DebugInteract_Noop) {
		state->interaction = state->nextInteraction;
		state->nextInteraction = {};
	}
	
	
	// Actual interaction functions
	if (state->interaction.var) {
		DebugVariable* var = state->interaction.var;
		V2 dMouse = mousePos - state->interaction.startMousePos;
		switch (state->interaction.type) {
		case DebugInteract_Toggle: {
			var->boolean = !var->boolean;
		} break;
		case DebugInteract_Expand: {
			var->group.expanded = !var->group.expanded;
		} break;
		case DebugInteract_DragIncrease: {
			var->fl32 += 0.001f * dMouse.Y;
		} break;
		case DebugInteract_Compile: {
			WriteDebugConfig(state);
			if (state->compilationHandle.state != CmdState_Running) {
				char cwd[] = "..\\code";
				char cmd[] = "cmd.exe /c build.bat --game_only";
				state->compilationHandle = Platform->SystemExecuteCommand(cwd, cmd);
			}
		} break;
		case DebugInteract_Resize: {
			f32 newMaxX = Maximum(mousePos.X, var->profiler.rect.min.X + 10.f);
			f32 newMaxY = Maximum(mousePos.Y, var->profiler.rect.min.Y + 10.f);
			var->profiler.rect.max = V2{ newMaxX, newMaxY };
		} break;
		}
	}
	if (WasPressed(controller.B.mouseLeft)) {
		state->selectedRecord = state->hotRecord;
	}
	if (WasPressed(controller.B.mouseMiddle)) {
		state->selectedRegionIndex = state->hotRegionIndex;
		state->selectedFrameIndex = state->hotFrameIndex;
		state->selectedRecord = state->hotRecord;
	}

#if DEBUGUI_ShowDebugInteractions
	{
		char buffer[256];
		const char* interaction = "Unknown";
		switch (state->interaction.type) {
		case DebugInteract_None: {
			interaction = "None";
		} break;
		case DebugInteract_Noop: {
			interaction = "Noop";
		} break;
		case DebugInteract_Toggle: {
			interaction = "Toggle";
		} break;
		case DebugInteract_Expand: {
			interaction = "Expand";
		} break;
		case DebugInteract_DragIncrease: {
			interaction = "DragIncrease";
		} break;
		case DebugInteract_Compile: {
			interaction = "Compile";
		} break;
		case DebugInteract_Resize: {
			interaction = "Resize";
		} break;
		case DebugInteract_Move: {
			interaction = "Move";
		} break;
		}
		sprintf_s(buffer, sizeof(buffer), "%s with %s", interaction, state->interaction.var ? state->interaction.var->name : "none");
		DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
		sprintf_s(buffer, sizeof(buffer), "LM: WasPressed: %d", WasPressed(controller.B.mouseLeft));
		DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
		sprintf_s(buffer, sizeof(buffer), "LM: IsPressed: %d", IsPressed(controller.B.mouseLeft));
		DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
		sprintf_s(buffer, sizeof(buffer), "LM: WasReleased: %d", WasReleased(controller.B.mouseLeft));
		DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
	}
#endif

	// End interactions
		switch (state->interaction.type) {
		case DebugInteract_Resize:
		case DebugInteract_DragIncrease: {
			if (!IsPressed(controller.B.mouseLeft)) {
				state->interaction = state->nextInteraction;
			}
		} break;
		case DebugInteract_Noop: {
			if (!IsPressed(controller.B.mouseLeft) && !IsPressed(controller.B.mouseRight)) {
				state->interaction = {};
			}
		} break;
		default: {
			state->interaction = state->nextInteraction;
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
	else if (state->compilationHandle.state == CmdState_Failed) {
		DebugRenderLine(state, "Failed Compilation", state->fontContext, V4{ 1, 1, 1, 1 });
	}
	DebugInteract(state, mousePos, controller);
#if DEBUGUI_ShowEventsCount
	{
		char buffer[256];
		sprintf_s(buffer, 256, "events in frame 0: %d", debugGlobalState->debugEventsCount[0]);
		DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
	}
#endif
	
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
	if (state->profilerPause->var->boolean) {
		return 0;
	}
	DebugCollateEvents(state);
	u32 frameWriteIndex = u4(debugGlobalState->frameAndEventIndex >> 32);
	DebugFrameInfo* result = state->frames + frameWriteIndex;
	return result;
}