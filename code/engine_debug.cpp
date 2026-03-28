#include "engine.h"

DebugGlobalState debugGlobalState_ = {};
DebugGlobalState* debugGlobalState = &debugGlobalState_;
DebugVariable nullDebugVariable = {};

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
	context.leftTopCurrent.E[1] -= context.lineAdvance;
}

inline
Rect2 GetTextBoundingBox(DebugState* state, const char* text, FontDrawContext& context, V4 color) {
	Rect2 boundingBox = {};
	DebugRenderLine(state, text, context.leftTopCurrent, context.scale, color, false, &boundingBox);
	return boundingBox;
}

inline
FontDrawContext InitializeFontDrawContext(LoadedFont* font, f32 scale, f32 lineAdvance, V2 topline) {
	FontDrawContext context = {};
	context.font = font;
	context.scale = scale;
	context.leftTopStart = context.leftTopCurrent = topline - V2{ 0, scale * context.font->metrics.ascent };
	context.lineAdvance = lineAdvance;
	return context;
}

inline
FontDrawContext InitializeStandardFontDrawContext(DebugState* state, V2 topline) {
	f32 scale = 0.15f;
	f32 lineAdvance = scale * GetFontLineAdvance(state->font);
	return InitializeFontDrawContext(state->font, scale, lineAdvance, topline);
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
DebugVariable* QueryDebugVariable(DebugVarQueryName query) {
	if (debugGlobalMemory->debugMemorySize == 0) {
		return 0;
	}
	Assert(debugGlobalMemory->debugMemorySize >= sizeof(DebugState));
	DebugState* state = ptrcast(DebugState, debugGlobalMemory->debugMemory);
	DebugVariable* var = state->querableVariables[query];
	if (!var) {
		return &nullDebugVariable;
	}
	return var;
}

inline
DebugVariable* _AddDebugVariable(DebugState* state, const char* name, DebugVarType type) {
	static_assert((ArrayCount(state->variableHash) & (ArrayCount(state->variableHash) - 1)) == 0 &&
		"variableHash size must be a power of two");
	DebugVariable* var = PushStructSize(state->mainArena, DebugVariable);
	u32 length = StringLength(name) + 1;
	var->name = PushString(state->mainArena, name, length);
	var->type = type;

	// TODO: Better hash function!
	// TODO: Think whether this hash map has any sense, maybe it should be just straight up allocated
	// and stored just in reference
	u32 hash = u4((13 * (reinterpret_cast<uptr>(var) >> 2)) & (ArrayCount(state->variableHash) - 1));
	DebugVariableHashEntry* newEntry = PushStructSize(state->mainArena, DebugVariableHashEntry);
	newEntry->var = var;
	newEntry->next = state->variableHash[hash];
	state->variableHash[hash] = newEntry;
	return var;
}

inline
DebugVariableRef* _AddReferencedDebugVariable(DebugState* state, DebugVariableContext& context, const char* name, DebugVarType type) {
	DebugVariable* var = _AddDebugVariable(state, name, type);
	DebugVariableRef* ref = PushStructSize(state->mainArena, DebugVariableRef);
	ref->next = 0;
	ref->var = var;
	ref->parent = context.stack[context.stackCount];
	if (ref->parent) {
		Assert(ref->parent->var->type == DebugVarType::Group);
		DebugVariableGroup* group = &ref->parent->var->group;
		ref->next = group->firstChild;
		group->firstChild = ref;
	}
	else {
		context.tree->root = ref;
	}
	return ref;
}

inline
DebugVariableRef* AddReferencedDebugVariable(DebugState* state, DebugVariableContext& context, const char* name, bool value) {
	DebugVariableRef* ref = _AddReferencedDebugVariable(state, context, name, DebugVarType::Bool);
	ref->var->boolean = value;
	return ref;
}

inline
DebugVariableRef* AddReferencedDebugVariable(DebugState* state, DebugVariableContext& context, const char* name, float value) {
	DebugVariableRef* ref = _AddReferencedDebugVariable(state, context, name, DebugVarType::Float);
	ref->var->fl32 = value;
	return ref;
}

inline
DebugVariableRef* BeginDebugVariableGroup(DebugState* state, DebugVariableContext& context, const char* name) {
	DebugVariableRef* ref = _AddReferencedDebugVariable(state, context, name, DebugVarType::Group);
	ref->var->group = {};
	context.stack[++context.stackCount] = ref;
	return ref;
}

inline
DebugVariableContext BeginDebugVariableTree(DebugState* state, const char* name, V2 pos) {
	DebugVariableContext context = {};
	DebugTree* tree = PushStructSize(state->mainArena, DebugTree);
	tree->pos = pos;
	context.tree = tree;
	context.stackCount = 0;
	context.stack[0] = 0;
	return context;
}

inline
void MakeVariableCompiled(DebugState* state, DebugVariableRef* ref) {
	Assert(ref->var);
	DebugVariableRef* newRef = PushStructSize(state->mainArena, DebugVariableRef);
	newRef->var = ref->var;
	newRef->parent = 0;
	newRef->next = state->compileTimeVariables;
	state->compileTimeVariables = newRef;
}

inline
void MakeVariableQuerable(DebugState* state, DebugVariableRef* ref, DebugVarQueryName queryName) {
	Assert(ref->var);
	Assert(state->querableVariables[queryName] == 0);
	state->querableVariables[queryName] = ref->var;
}

inline
void EndDebugVariableGroup(DebugVariableContext& context) {
	Assert(context.stackCount > 0 || !"Tried to enclose group when none were opened");
	context.stackCount--;
}

inline 
DebugState* DebugBegin(LoadedBitmap& screenBitmap) {
	if (debugGlobalMemory->debugMemorySize == 0) {
		return 0;
	}
	Assert(debugGlobalMemory->debugMemorySize >= sizeof(DebugState));
	DebugState* state = ptrcast(DebugState, debugGlobalMemory->debugMemory);
	if (!state->isInitialized) {
		debugGlobalState->debugRecordsCount[0] = debugRecordsCount_Main;
		debugGlobalState->debugRecordsCount[1] = debugRecordsCount_Optimized;

		TransientState* tranState = ptrcast(TransientState, debugGlobalMemory->transientMemory);
		Assert(tranState->isInitialized);
		InitializeArena(
			state->mainArena,
			ptrcast(u8, debugGlobalMemory->debugMemory) + sizeof(DebugState),
			debugGlobalMemory->debugMemorySize - sizeof(DebugState)
		);
		SubArena(state->collationArena, state->mainArena, MB(16));
		u32 frameWriteIndex = u4(debugGlobalState->frameAndEventIndex >> 32);
		state->renderGroup = AllocateRenderGroup(state->mainArena, &tranState->assets, MB(4), false);
		state->highPriorityQueue = tranState->highPriorityQueue;
		state->collationScratchBuffer = BeginTempMemory(state->collationArena);
		ResetDebugCollation(state, frameWriteIndex);

		state->compilationHandle.state = CmdState_Completed;
		state->overlayBoundaries = GetRectFromCenterDim(V2{ 0, 0 }, V2i(screenBitmap.width, screenBitmap.height));
		V2 leftTopCorner = V2{ state->overlayBoundaries.min.X, state->overlayBoundaries.max.Y };
		DebugVariableContext context = BeginDebugVariableTree(state, "Default", leftTopCorner);
		state->UITree = context.tree;
		BeginDebugVariableGroup(state, context, "Debugging");
		BeginDebugVariableGroup(state, context, "Compile time switches");
		MakeVariableCompiled(state, AddReferencedDebugVariable(state, context, "CameraZoomout", false));
		MakeVariableCompiled(state, AddReferencedDebugVariable(state, context, "RenderFullHD", false));
		EndDebugVariableGroup(context);
		_AddReferencedDebugVariable(state, context, "Update and Compile", DebugVarType::CompilationSwitch);
		
		BeginDebugVariableGroup(state, context, "Runtime switches");
		MakeVariableQuerable(state, AddReferencedDebugVariable(state, context, "CameraZoomoutValue", 20.f), DebugVarQuery_CameraZoomoutValue);
		MakeVariableQuerable(state, AddReferencedDebugVariable(state, context, "ShowDebugInteractions", true), DebugVarQuery_ShowDebugInteractions);
		MakeVariableQuerable(state, AddReferencedDebugVariable(state, context, "ShowEventsCount", true), DebugVarQuery_ShowDebugEvents);
		EndDebugVariableGroup(context);
		EndDebugVariableGroup(context);

		context = BeginDebugVariableTree(state, "Default", leftTopCorner + V2{100.f, 0});
		state->UITree->next = context.tree;
		BeginDebugVariableGroup(state, context, "Profiler");
		state->profilerPause = AddReferencedDebugVariable(state, context, "Pause profiler", false);
		DebugVariableRef* profilerUI = _AddReferencedDebugVariable(state, context, "ProfilerUI", DebugVarType::ProfilerUI);
		profilerUI->var->profiler.rect = GetRectFromMinMax(V2{ -450, -250 }, V2{ 450, -50 });
		EndDebugVariableGroup(context);


		BeginRendering(state->renderGroup);
		state->isInitialized = true;
	}
	EndRendering(state->renderGroup);
	BeginRendering(state->renderGroup);
	state->overlayBoundaries = GetRectFromCenterDim(V2{ 0, 0 }, V2i(screenBitmap.width, screenBitmap.height));
	state->renderGroup.pushBufferSize = 0;
	state->renderGroup.projection = GetOrtographicProjection(screenBitmap.width, screenBitmap.height, 1);
	state->font = GetOrPrefetchFont(
		state->renderGroup, GetFontWithType(*state->renderGroup.assets, Font_Debug)
	);

	if (state->font) {
		f32 scale = 0.15f;
		f32 lineAdvance = scale * GetFontLineAdvance(state->font);
		state->fontContext = InitializeFontDrawContext(state->font, 0.15f, -lineAdvance, state->overlayBoundaries.min + V2{ 0, lineAdvance });
	}
	state->hotRecord = 0;
	state->hotFrameIndex = U32_MAX;
	state->hotRegionIndex = U32_MAX;
	state->nextHotInteraction = {};
	return state;
}

internal
DebugEventStack* GetDebugStackForThread(DebugState* state, u16 threadId) {
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
	DebugRecord* record = debugGlobalState->debugRecords[event->translationUnit] + event->recordIndex;
	return record;
}

internal
void DebugCollateEvents(DebugState* debugState) {
	TIMED_FUNCTION;
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
					if (openEvent->recordIndex == event->recordIndex &&
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
							if (event->recordIndex == 2 && event->translationUnit == 2) {
								int breakhere = 5;
							}
							region->recordIndex = event->recordIndex;
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
							region->durationCycles = u4(openEvent->cycles - event->cycles);
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
	case DebugVarType::Bool: {
		at += sprintf_s(at, end - at, "%s%s %d", var->name, colon, var->boolean);
	} break;
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
	for (DebugVariableRef* ref = state->compileTimeVariables; ref; ref = ref->next) {
		DebugVariable* var = ref->var;
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
						region->durationCycles
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

inline
bool IsVariableRefHot(DebugState* state, DebugVariableRef* ref) {
	return state->hotInteraction.ref == ref;
}

inline
void SetNextHotInteraction(DebugState* state, DebugVariableRef* ref, Rect2 boundingBox, DebugTree* tree) {
	state->nextHotInteraction.ref = ref;
	state->nextHotInteraction.state.startBoundingBox = boundingBox;
	state->nextHotInteraction.state.relevantTree = tree;
}

internal
void DebugRenderVariablesMenu(DebugState* state, V2 mousePos) {
	for (DebugTree* tree = state->UITree; tree; tree = tree->next) {
		FontDrawContext fontContext = InitializeStandardFontDrawContext(state, tree->pos);
		DebugVariableRef* parent = 0;
		DebugVariableRef* node = tree->root;
		u32 depth = 0;
		while (node) {
			V4 itemColor = V4{ 1, 1, 1, 1 };
			V4 hotItemColor = V4{ 0.2f, 0.5f, 1.0f, 1 };

			DebugVariable* var = node->var;
			char buffer[256] = {};
			char* end = buffer + sizeof(buffer);
			bool isHot = IsVariableRefHot(state, node);
			if (isHot) {
				itemColor = hotItemColor;
			}
			if (var->type == DebugVarType::ProfilerUI) {
				DebugRenderProfilerUI(state, var->profiler.rect, mousePos);
				Rect2 resizeAnchor = GetRectFromCenterDim(var->profiler.rect.max, V2{ 8, 8 });
				// TODO: Should this condition be included in SetNextHotInteraction?
				if (IsInRectangle(resizeAnchor, mousePos)) {
					SetNextHotInteraction(state, node, resizeAnchor, tree);
				}
				PushRect(state->renderGroup, resizeAnchor, 0, V2{ 0, 0 }, itemColor);
			}
			else {
				char* at = buffer;
				for (u32 idx = 0; idx < depth; idx++) {
					*at++ = ' ';
					*at++ = ' ';
				}
				DebugVariableToText(var, at, u4(end - at), DebugVarToText_AddColon);
				Rect2 bb = GetTextBoundingBox(state, buffer, fontContext, itemColor);
				if (IsInRectangle(bb, mousePos)) {
					SetNextHotInteraction(state, node, bb, tree);
				}
				PushRect(state->renderGroup, AddRadius(bb, V2{4.f, 4.f}), 0, V2{ 0,0 }, V4{ 0.5f, 0, 0, 1 });
				DebugRenderLine(state, buffer, fontContext, itemColor);
			}
			if (var->type == DebugVarType::Group && var->group.expanded) {
				parent = node;
				node = var->group.firstChild;
				depth += 1;
			}
			else if (node) {
				node = node->next;
			}
			if (!node && parent) {
				node = parent->next;
				if (node) {
					parent = node->parent;
					depth -= 1;
				}
			}
			if (!parent) {
				break;
			}
		}
	}
}

void DebugInteract(DebugState* state, V2 mousePos, Controller& controller) {
	// Set hot interaction
	if (state->nextHotInteraction.ref) {
		state->nextHotInteraction.state.startMousePos = mousePos;
		switch (state->nextHotInteraction.ref->var->type) {
		case DebugVarType::Bool: {
			if (WasPressed(controller.B.mouseLeft)) {
				state->nextHotInteraction.type = DebugInteract_Toggle;
				state->nextHotInteraction.state.boolean = &state->nextHotInteraction.ref->var->boolean;
			}
		} break;
		case DebugVarType::Group: {
			if (WasPressed(controller.B.mouseLeft)) {
				state->nextHotInteraction.type = DebugInteract_Toggle;
				state->nextHotInteraction.state.boolean = &state->nextHotInteraction.ref->var->group.expanded;
			}
		} break;
		case DebugVarType::Float: {
			if (WasPressed(controller.B.mouseLeft)) {
				state->nextHotInteraction.type = DebugInteract_DragIncrease;
			}
		} break;
		case DebugVarType::CompilationSwitch: {
			if (WasPressed(controller.B.mouseLeft)) {
				state->nextHotInteraction.type = DebugInteract_Compile;
			}
		} break;
		case DebugVarType::ProfilerUI: {
			if (WasPressed(controller.B.mouseLeft)) {
				state->nextHotInteraction.type = DebugInteract_Resize;
			}
		} break;
		}
		if (IsPressed(controller.B.kShift) && WasPressed(controller.B.mouseLeft)) {
			state->nextHotInteraction.type = DebugInteract_Tear;
		}
	}
	state->hotInteraction = state->nextHotInteraction;

	// What to do at the beginning of interaction
	if (!state->interacting && state->hotInteraction.type != DebugInteract_None) {
		state->interaction = state->hotInteraction;
		state->interacting = true;
		if (state->interaction.type == DebugInteract_Tear) {
			DebugVariableRef* tearPoint = state->interaction.ref;
			DebugVariableRef* oldParent = tearPoint->parent;
			V2* treePosition = &state->interaction.state.relevantTree->pos;
			if (oldParent) {
				DebugVariableRef* prevChild = 0;
				for (DebugVariableRef* child = oldParent->var->group.firstChild; child; child = child->next) {
					if (child == tearPoint) {
						if (prevChild) {
							prevChild->next = child->next;
						}
						else {
							oldParent->var->group.firstChild = oldParent->var->group.firstChild->next;
						}
						break;
					}
					prevChild = child;
				}
				DebugVariableContext context = BeginDebugVariableTree(state, "NewUserTree", mousePos);
				DebugTree* dst = context.tree;
				dst->next = state->UITree;
				state->UITree = dst;

				dst->root = tearPoint;
				tearPoint->next = 0;
				tearPoint->parent = 0;
				treePosition = &dst->pos;
			}
			state->interaction.state.pos.initial = V2{ 
				state->interaction.state.startBoundingBox.min.X, 
				state->interaction.state.startBoundingBox.max.Y 
			};
			state->interaction.state.pos.actual = treePosition;
		}
	}
	
	// What to do DURING the interaction (for interactions taking more time than one frame)
	if (state->interacting && state->interaction.ref) {
		DebugVariable* var = state->interaction.ref->var;
		V2 dMouse = mousePos - state->interaction.state.startMousePos;
		switch (state->interaction.type) {
		case DebugInteract_Compile:
		case DebugInteract_Toggle: {
			if (LengthSq(dMouse) > 5.f) {
				state->interaction.type = DebugInteract_Move;
				state->interaction.state.startMousePos = mousePos;
				state->interaction.state.pos.initial = state->interaction.state.relevantTree->pos;
				state->interaction.state.pos.actual = &state->interaction.state.relevantTree->pos;
			}
		} break;
		case DebugInteract_DragIncrease: {
			var->fl32 += 0.001f * dMouse.Y;
		} break;
		case DebugInteract_Resize: {
			f32 newMaxX = Maximum(mousePos.X, var->profiler.rect.min.X + 10.f);
			f32 newMaxY = Maximum(mousePos.Y, var->profiler.rect.min.Y + 10.f);
			var->profiler.rect.max = V2{ newMaxX, newMaxY };
		} break;
		case DebugInteract_Tear:
		case DebugInteract_Move: {
			DebugModifiedPosition& pos = state->interaction.state.pos;
			*pos.actual = pos.initial + dMouse;
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

	if (QueryDebugVariable(DebugVarQuery_ShowDebugInteractions)->boolean) {
		char buffer[256];
		const char* interaction = "Unknown";
		switch (state->interaction.type) {
		case DebugInteract_None: {
			interaction = "None";
		} break;
		case DebugInteract_Toggle: {
			interaction = "Toggle";
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
		V2 dMouse = mousePos - state->interaction.state.startMousePos;
		char* at = buffer;
		char* end = buffer + sizeof(buffer);
		at += sprintf_s(at, end - at, "%s with %s", interaction, state->interaction.ref ? state->interaction.ref->var->name : "none");
		if (state->interacting) {
			sprintf_s(at, end - at, ": dMouse: %f %f", dMouse.X, dMouse.Y);
		}
		DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
		
	}

	// What to do at the END of interaction
	bool interactionEnded = false;
	switch (state->interaction.type) {
	case DebugInteract_Resize:
	case DebugInteract_DragIncrease:
	case DebugInteract_Move:
	case DebugInteract_Tear: {
		interactionEnded = !IsPressed(controller.B.mouseLeft);
	} break;
	case DebugInteract_Toggle: {
		if (WasReleased(controller.B.mouseLeft) || !IsPressed(controller.B.mouseLeft)) {
			bool* boolean = state->interaction.state.boolean;
			*boolean = !(*boolean);
			interactionEnded = true;
		}
	} break;
	case DebugInteract_Compile: {
		if (!IsPressed(controller.B.mouseLeft)) {
			if (state->compilationHandle.state != CmdState_Running) {
				WriteDebugConfig(state);
				char cwd[] = "..\\code";
				char cmd[] = "cmd.exe /c build.bat --game_only";
				state->compilationHandle = Platform->SystemExecuteCommand(cwd, cmd);
			}
			interactionEnded = true;
		}
	} break;
	}
	if (interactionEnded) {
		state->interaction = {};
		state->interacting = false;
	}
}
	

void DebugRenderOverlay(DebugState* state, LoadedBitmap& dstBitmap, InputData& input) {
	TIMED_FUNCTION;
	if (!state->font) {
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
	if (QueryDebugVariable(DebugVarQuery_ShowDebugEvents)->boolean) {
		char buffer[256];
		sprintf_s(buffer, 256, "events in frame 0: %d", debugGlobalState->debugEventsCount[0]);
		DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
	}
	
	TiledRenderGroupToBuffer(state->renderGroup, dstBitmap, state->highPriorityQueue);
}

extern "C" DebugGlobalState* DebugInit(ProgramMemory* memory) {
	return debugGlobalState;
}

extern "C" DebugFrameInfo* DebugFinishFrame(ProgramMemory* memory, BitmapData& rawBitmap, InputData& input) {
	TIMED_FUNCTION;
	LoadedBitmap bitmap = {};
	bitmap.height = rawBitmap.height;
	bitmap.width = rawBitmap.width;
	bitmap.data = ptrcast(u32, rawBitmap.data);
	bitmap.pitch = rawBitmap.pitch;

	DebugState* state = DebugBegin(bitmap);
	if (!state) {
		return 0;
	}
	DebugRenderOverlay(state, bitmap, input);


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

u32 debugRecordsCount_Main = __COUNTER__;