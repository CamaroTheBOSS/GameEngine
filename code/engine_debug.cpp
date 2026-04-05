#include "engine.h"

/* TODO:
* Better profiler 
* - regions should be debug variables and be used in interaction system!
* - should display frames starting from the newest! The oldest should be dropped when outside the screen
* - should display frame information: duration, events count and other stuff
* - should be another tree displayed in different way -> Pause / Resize / Move / Clickable regions
* 
* Profiler for memory
* - all the stack allocators should be visible
* - memory from asset system should be visible
* 
* Entity introspection
* - entity selection
* - checking and modifing values!
*/

DebugGlobalState debugGlobalState_ = {};
DebugGlobalState* debugGlobalState = &debugGlobalState_;
#if 0
DebugVariable nullDebugVariable = {};
#endif

// TODO: Delete stdlib
#include <stdio.h>

//NOTE: Intelisense helpers
internal void TiledRenderGroupToBuffer(RenderGroup& group, LoadedBitmap& dstBuffer, PlatformQueue* queue);
inline V2 FromPixelSpaceToWorldSpace(Projection& projection, V2 pixelSpacePos, f32 atDistanceFromCamera);
inline Projection GetOrtographicProjection(u32 widthPix, u32 heightPix, f32 metersToPixels);
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
bool DebugIdsEqual(DebugId id1, DebugId id2) {
	for (i32 index = 0; index < ArrayCount(id1.val); index++) {
		if (id1.val[index] != id2.val[index]) {
			return false;
		}
	}
	return true;
}

inline
bool DebugIdIsNull(DebugId id) {
	for (i32 index = 0; index < ArrayCount(id.val); index++) {
		if (id.val[index] != 0) {
			return false;
		}
	}
	return true;
}

inline
DebugId GetDebugIdForLink(DebugVariableLink* link) {
	DebugId id = {};
	id.val[0] = link;
	return id;
}

inline
bool IsHighlighted(DebugState* state, DebugId did) {
	return DebugIdsEqual(state->nextHotInteraction.id, did);
}

inline
bool IsSelected(DebugState* state, DebugId did) {
	return DebugIdsEqual(state->selectedId, did);
}

internal DebugEvent* InitializePermanentDebugVariable(DebugEvent* subevent, DebugEventType type, const char* name, const char* file, u16 line) {
	RecordDebugEventNoBracket(0, Event_PermanentVariableDeclaration, file, name, line);
	event->data_DebugEvent = subevent;
	subevent->blockName = name;
	subevent->coreId = 0;
	subevent->cycles = 0;
	subevent->threadId = 0;
	subevent->type = type;
	subevent->file = file;
	subevent->line = line;
	return subevent;
}

inline DebugState* GetDebugState() {
	if (debugGlobalMemory->debugMemorySize == 0) {
		return 0;
	}
	Assert(debugGlobalMemory->debugMemorySize >= sizeof(DebugState));
	DebugState* state = ptrcast(DebugState, debugGlobalMemory->debugMemory);
	if (!state->isInitialized) {
		return 0;
	}
	return state;
}

inline
DebugId DEBUG_POINTER_ID(void* ptr) {
	return { ptr };
}

inline
void DEBUG_HIT(DebugId did, Rect2 boundingBox) {
	DebugState* state = GetDebugState();
	if (!state) {
		return;
	}
	DebugInteraction interaction = {};
	interaction.id = did;
	interaction.startBoundingBox = boundingBox;
	state->nextHotInteraction = interaction;
}

inline
bool DEBUG_HIGHLIGHTED(DebugId did, V4* color) {
	DebugState* state = GetDebugState();
	if (!state) {
		return false;
	}
	if (IsSelected(state, did)) {
		*color = V4{ 1, 1, 0, 1 };
	}
	else if (IsHighlighted(state, did)) {
		*color = V4{ 0, 1, 1, 1 };
	}
	else {
		return false;
	}
	return true;
}

inline bool DEBUG_DATA_BLOCK_REQUESTED(DebugId did) {
	DebugState* state = GetDebugState();
	if (!state) {
		return false;
	}
	bool result = IsSelected(state, did) || IsHighlighted(state, did);
	return result;
}

inline
u32 GetFontWidthAdvanceFor(LoadedFont* font, u32 firstCodepoint, u32 secondCodepoint) {
	Assert(firstCodepoint < font->onePastMaxCodepoint && secondCodepoint < font->onePastMaxCodepoint);
	u32 firstKerningIndex = font->codepointToLogicalIndex[firstCodepoint];
	u32 secondKerningIndex = font->codepointToLogicalIndex[secondCodepoint];
	//Assert((firstKerningIndex != 0 || firstCodepoint == 0) && secondKerningIndex != 0);
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
	ResetArena(debugState->collationArena);
	debugState->openEventFreeList = 0;
	debugState->threadStacksCount = 0;
	debugState->threadStacks = 0;
	debugState->selectedFrameIndex = U32_MAX;
	debugState->selectedRegionIndex = U32_MAX;
	debugState->frameReadIndex = (frameWriteIndex + 1) % MAX_DEBUG_FRAMES;
	debugState->frames = PushArray(debugState->collationArena, MAX_DEBUG_FRAMES, DebugFrameInfo);
}

internal 
DebugState* DebugBegin(LoadedBitmap& screenBitmap) {
	if (debugGlobalMemory->debugMemorySize == 0) {
		return 0;
	}
	Assert(debugGlobalMemory->debugMemorySize >= sizeof(DebugState));
	DebugState* state = ptrcast(DebugState, debugGlobalMemory->debugMemory);
	if (!state->isInitialized) {
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
		ResetDebugCollation(state, frameWriteIndex);

		state->overlayBoundaries = GetRectFromCenterDim(V2{ 0, 0 }, V2i(screenBitmap.width, screenBitmap.height));
#if 0
		state->compilationHandle.state = CmdState_Completed;
#endif
		V2 leftTopCorner = V2{ state->overlayBoundaries.min.X, state->overlayBoundaries.max.Y };
#if 0
		DebugVariableContext context = BeginDebugVariableTree(state, state->mainArena, "Default", leftTopCorner);
		BeginDebugVariableGroup(state, context, "Debugging");
		BeginDebugVariableGroup(state, context, "Compile time switches");
		MakeVariableCompiled(state, AddReferencedDebugVariable(state, context, "CameraZoomout", false));
		MakeVariableCompiled(state, AddReferencedDebugVariable(state, context, "ShowEntityHitboxes", false));
		EndDebugVariableGroup(context);
		_AddReferencedDebugVariable(state, context, "Update and Compile", DebugVarType::CompilationSwitch);
		
		BeginDebugVariableGroup(state, context, "Runtime switches");
		MakeVariableQuerable(state, AddReferencedDebugVariable(state, context, "CameraZoomoutValue", 20.f), DebugVarQuery_CameraZoomoutValue);
		MakeVariableQuerable(state, AddReferencedDebugVariable(state, context, "ShowDebugInteractions", true), DebugVarQuery_ShowDebugInteractions);
		MakeVariableQuerable(state, AddReferencedDebugVariable(state, context, "ShowEventsCount", true), DebugVarQuery_ShowDebugEvents);
		EndDebugVariableGroup(context);
		EndDebugVariableGroup(context);

		context = BeginDebugVariableTree(state, state->mainArena, "Default", leftTopCorner + V2{100.f, 0});
		BeginDebugVariableGroup(state, context, "Profiler");
		state->profilerPause = AddReferencedDebugVariable(state, context, "Pause profiler", false);
		DebugVariableLink* profilerUI = _AddReferencedDebugVariable(state, context, "ProfilerUI", DebugVarType::ProfilerUI);
		profilerUI->var->profiler.rect = GetRectFromMinMax(V2{ -450, -250 }, V2{ 450, -50 });
		EndDebugVariableGroup(context);
#endif
		state->frameMemory = BeginTempMemory(state->collationArena);
		state->temporaryVarTree = PushStructSize(state->collationArena, DebugTree);	
		
		state->permanentVarTree = PushStructSize(state->mainArena, DebugTree);
		state->permanentVarTree->pos = leftTopCorner + V2{200, 0};
		DLINKED_LIST_INIT(&state->permanentVarTree->root);

		DLINKED_LIST_INIT(&state->UISentinel);
		DLINKED_LIST_ADD(&state->UISentinel, state->temporaryVarTree);
		DLINKED_LIST_ADD(&state->UISentinel, state->permanentVarTree);

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
		V2 leftUpCorner = V2{ state->overlayBoundaries.min.X, state->overlayBoundaries.max.Y };
		//state->fontContext = InitializeFontDrawContext(state->font, 0.15f, lineAdvance, leftUpCorner - V2{ 0, lineAdvance });
	}
	state->hotFrameIndex = U32_MAX;
	state->hotRegionIndex = U32_MAX;
	return state;
}

internal
DebugThreadStack* GetDebugStackForThread(DebugState* state, u16 threadId) {
	for (u32 stackIndex = 0; stackIndex < state->threadStacksCount; stackIndex++) {
		DebugThreadStack* stack = state->threadStacks + stackIndex;
		if (stack->threadId == threadId) {
			return stack;
		}
	}
	Assert(state->threadStacksCount < MAX_DEBUG_THREADS);
	DebugThreadStack* stack = state->threadStacks + state->threadStacksCount++;
	stack->threadId = threadId;
	stack->laneId = state->threadStacksCount - 1;
	stack->timeEvents = PushStructSize(state->collationArena, OpenDebugEvent);
	stack->dataEvents = PushStructSize(state->collationArena, OpenDebugEvent);
	*stack->timeEvents = {};
	*stack->dataEvents = {};
	return stack;
}

internal
OpenDebugEvent* PushToEventStack(DebugState* state, OpenDebugEvent** stack, DebugEvent* event) {
	OpenDebugEvent* newBlock = state->openEventFreeList;
	if (newBlock) {
		state->openEventFreeList = newBlock->next;
	}
	else {
		newBlock = PushStructSize(state->collationArena, OpenDebugEvent);
	}
	newBlock->next = *stack;
	newBlock->event = event;
	newBlock->childRegionCount = 0;
	*stack = newBlock;
	return newBlock;
}

inline
void PopFromEventStack(DebugState* state, OpenDebugEvent** stack) {
	OpenDebugEvent* block = *stack;
	*stack = block->next;
	block->next = state->openEventFreeList;
	state->openEventFreeList = block;
}

struct DebugVariableDefinitionContext {
	u32 stackDepth;
	DebugVariableLink* parentStack[64];
};

internal
DebugVariableLink* AddCollationVariableLink(DebugState* state, DebugVariableDefinitionContext& context, DebugEvent* event, bool permanent) {
	DebugVariableLink* link = PushStructSize(permanent ? state->mainArena : state->collationArena, DebugVariableLink);
	DebugVariableLink* parent = context.parentStack[context.stackDepth];
	ZeroStruct(*link);
	link->parent = parent;
	link->event = event;
	Assert(event);
	DLINKED_LIST_INIT(link);
	if (parent) {
		if (!parent->children) {
			parent->children = PushStructSize(permanent ? state->mainArena : state->collationArena, DebugVariableLink);
			ZeroStruct(*parent->children);
			DLINKED_LIST_INIT(parent->children);
		}
		DLINKED_LIST_ADD(parent->children, link);
	}
	return link;
}

internal
DebugVariableLink* BeginCollationVariableLinkGroup(DebugState* state, DebugVariableDefinitionContext& context, DebugEvent* event, bool permanent) {
	Assert(context.stackDepth < ArrayCount(context.parentStack) - 1);
	DebugVariableLink* link = AddCollationVariableLink(state, context, event, permanent);
	context.parentStack[++context.stackDepth] = link;
	return link;
}

internal
void EndCollationVariableLinkGroup(DebugState* state, DebugVariableDefinitionContext& context) {
	Assert(context.stackDepth > 0 || !"Tried to enclose group when none were opened");
	context.stackDepth--;

	if (context.stackDepth == 0) {

	}
}
internal void DebugRenderVariablesMenu(DebugState*, V2);
internal
void DebugCollateEvents(DebugState* debugState) {
	TIMED_FUNCTION;

	// TODO Make this code more sane
	DLINKED_LIST_REMOVE(debugState->temporaryVarTree);
	EndTempMemory(debugState->frameMemory);
	debugState->frameMemory = BeginTempMemory(debugState->collationArena);
	debugState->temporaryVarTree = PushStructSize(debugState->collationArena, DebugTree);
	*debugState->temporaryVarTree = {};
	debugState->temporaryVarTree->pos = V2{ debugState->overlayBoundaries.min.X, debugState->overlayBoundaries.max.Y };
	DLINKED_LIST_ADD(&debugState->UISentinel, debugState->temporaryVarTree);
	DLINKED_LIST_INIT(&debugState->temporaryVarTree->root);
	DebugVariableDefinitionContext context = {};
	context.parentStack[0] = &debugState->temporaryVarTree->root;
	DebugVariableDefinitionContext context2 = {};
	context2.parentStack[0] = &debugState->permanentVarTree->root;

	debugState->threadStacks = PushArray(debugState->collationArena, MAX_DEBUG_THREADS, DebugThreadStack);
	debugState->threadStacksCount = 0;
	debugState->openEventFreeList = 0;

	debugState->frameWriteIndex = u4(debugGlobalState->frameAndEventIndex >> 32);
	for (; 
		debugState->frameReadIndex != debugState->frameWriteIndex;
		debugState->frameReadIndex = (debugState->frameReadIndex + 1) % MAX_DEBUG_FRAMES
		) {
		DebugFrameInfo* frameInfo = debugState->frames + debugState->frameReadIndex;
		frameInfo->regionsCount = 0;
		frameInfo->eventCount = {};

		f32 scale = DEBUG_COLLATION_SCALE;
		DebugEvent* eventsInFrame = debugGlobalState->debugEvents[debugState->frameReadIndex];
		u32 eventsInFrameCount = debugGlobalState->debugEventsCount[debugState->frameReadIndex];
		u64 startCycles = debugGlobalState->frameStartCycles[debugState->frameReadIndex];
		for (u32 eventIndex = 0;
			eventIndex < eventsInFrameCount;
			eventIndex++
			) {
			DebugEvent* event = eventsInFrame + eventIndex;
			DebugThreadStack* stack = GetDebugStackForThread(debugState, event->threadId);
			Assert(event->type < Event_Count);
			frameInfo->eventCount.count[event->type]++;
			frameInfo->eventCount.count[Event_Count]++;
			switch (event->type) {
			case Event_Time_BlockBegin: {
				PushToEventStack(debugState, &stack->timeEvents, event);
			} break;
			case Event_Time_BlockEnd: {
				OpenDebugEvent* block = stack->timeEvents;
				OpenDebugEvent* parentBlock = block->next;
				DebugEvent* openEvent = block->event;
				if (openEvent) {
					if (openEvent->threadId == event->threadId) {
						f32 minT = f4(openEvent->cycles - startCycles) * scale;
						f32 maxT = f4(event->cycles - startCycles) * scale;
						f32 thresholdT = 0.01f;
						if ((maxT - minT) > thresholdT) {
							Assert(frameInfo->regionsCount < ArrayCount(frameInfo->regions));
							u16 regionIndex = u2(frameInfo->regionsCount++);
							DebugProfilerRegion* region = frameInfo->regions + regionIndex;
							// TODO: If minT < 0 -> that means openEvent started on one of the previous frames.
							// Do something about it!
							region->laneId = stack->laneId;
							region->parentEventId = 0;
							if (parentBlock->event) {
								region->parentEventId = parentBlock->event->blockName;
								Assert(parentBlock->childRegionCount < ArrayCount(parentBlock->childRegionIndexes));
								parentBlock->childRegionIndexes[parentBlock->childRegionCount++] = regionIndex;
							}
							region->regionName = block->event->blockName;
							region->minT = minT;
							region->maxT = maxT;
							region->durationCycles = u4(openEvent->cycles - event->cycles);
							region->parentRegionIndex = U32_MAX;
							for (u32 index = 0; index < block->childRegionCount; index++) {
								DebugProfilerRegion* childRegion = frameInfo->regions + block->childRegionIndexes[index];
								childRegion->parentRegionIndex = regionIndex;
							}
						}
						PopFromEventStack(debugState, &stack->timeEvents);
					}
				}
			} break;
			case Event_Data_BlockBegin: {
				PushToEventStack(debugState, &stack->dataEvents, event);
				BeginCollationVariableLinkGroup(debugState, context, event, false);
			} break;
			case Event_Data_BlockEnd: {
				PopFromEventStack(debugState, &stack->dataEvents);
				EndCollationVariableLinkGroup(debugState, context);
			} break;
			case Event_PermanentVariableDeclaration: {
				AddCollationVariableLink(debugState, context2, event->data_DebugEvent, true);
			} break;
			case Event_Data_u32:
			case Event_Data_i32:
			case Event_Data_f32:
			case Event_Data_V2:
			case Event_Data_V3:
			case Event_Data_V4: {
				AddCollationVariableLink(debugState, context, event, false);
			} break;
			}
		}
	}
	f32 s = 0;
}

enum DebugVarToTextFlags {
	DebugVarToText_ConfigPrefix = 0x1,
	DebugVarToText_AddFloatSuffix = 0x2,
	DebugVarToText_AddNewLine = 0x4,
	DebugVarToText_AddColon = 0x8,
};

u64 DebugEventToText(DebugEvent* event, char* buffer, u32 size, u32 flags) {
	char* at = buffer;
	char* end = buffer + size;
	if (flags & DebugVarToText_ConfigPrefix) {
		at += sprintf_s(at, end - at, "#define DEBUGUI_");
	}
	const char* colon = (flags & DebugVarToText_AddColon) ? ":" : "";

	switch (event->type) {
	case Event_Data_bool: {
		at += sprintf_s(at, end - at, "%s%s %d", event->blockName, colon, event->data_bool);
	} break;
	case Event_Data_i32: {
		at += sprintf_s(at, end - at, "%s%s %d", event->blockName, colon, event->data_i32);
	} break;
	case Event_Data_u32: {
		at += sprintf_s(at, end - at, "%s%s %d", event->blockName, colon, event->data_u32);
	} break;
	case Event_Data_f32: {
		at += sprintf_s(at, end - at, "%s%s %f", event->blockName, colon, event->data_f32);
		if (flags & DebugVarToText_AddFloatSuffix && (end - at) > 0) {
			*at++ = 'f';
		}
	} break;
	case Event_Data_V2: {
		at += sprintf_s(at, end - at, "%s%s {%f, %f}", event->blockName, colon,
			event->data_V2.X, event->data_V2.Y);
	} break;
	case Event_Data_V3: {
		at += sprintf_s(at, end - at, "%s%s {%f, %f, %f}", event->blockName, colon,
			event->data_V3.X, event->data_V3.Y, event->data_V3.Z);
	} break;
	case Event_Data_V4: {
		at += sprintf_s(at, end - at, "%s%s {%f, %f, %f, %f}", event->blockName, colon,
			event->data_V4.X, event->data_V4.Y, event->data_V4.Z, event->data_V4.W);
	} break;
	case Event_Data_BlockBegin: {
		at += sprintf_s(at, end - at, "%s%s", event->blockName, colon);
	} break;
	}
	if (flags & DebugVarToText_AddNewLine && (end - at) > 0) {
		*at++ = '\n';
	}
	return at - buffer;
}

#if 0
void WriteDebugConfig(DebugState* state) {
	char buffer[4096];
	char* at = buffer;
	char* end = buffer + sizeof(buffer);
	for (DebugVariableLink* link = state->compileTimeVariables; link; link = link->next) {
		DebugVariable* var = link->var;
		at += DebugVariableToText(var, at, u4(end - at),
			DebugVarToText_ConfigPrefix |
			DebugVarToText_AddFloatSuffix |
			DebugVarToText_AddNewLine
		);
	}
	debugGlobalMemory->debug.WriteFile(DEBUG_CONFIG_PATH, buffer, at - buffer);
}
#endif

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
	f32 frameWidth = f4(state->threadStacksCount) * threadLaneTotalWidth + frameLaneSpace;
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
			if (state->selectedEventId != region->parentEventId) {
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
			u32 colorIndex = u4(13 * reinterpret_cast<uptr>(region->regionName)) % ArrayCount(colors);
			bool isHovered = IsInRectangle(rectangle, mousePos);
			if (isHovered) {
				if (region->regionName) {
					char buffer[256];
					sprintf_s(buffer, "%s", region->regionName);
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
				state->hotRegionName = region->regionName;
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
bool IsVariableHot(DebugState* state, DebugVariableLink* link) {
	return state->hotInteraction.link == link;
}

inline
void SetNextHotInteraction(DebugState* state, DebugVariableLink* link, Rect2 boundingBox, DebugTree* tree) {
	DebugInteraction interaction = {};
	interaction.id = GetDebugIdForLink(link);
	interaction.link = link;
	interaction.startBoundingBox = boundingBox;
	interaction.relevantTree = tree;
	state->nextHotInteraction = interaction;
}

internal
void DebugRenderVariablesMenu(DebugState* state, V2 mousePos) {
	for (DebugTree* tree = state->UISentinel.next; tree != &state->UISentinel; tree = tree->next) {
		FontDrawContext fontContext = InitializeStandardFontDrawContext(state, tree->pos);
		DebugVariableLink* parent = &tree->root;
		if (!parent->children) {
			continue;
		}
		DebugVariableLink* sentinel = parent->children;
		DebugVariableLink* node = sentinel->next;

		u32 depth = 0;
		while (node != sentinel) {
			V4 itemColor = V4{ 1, 1, 1, 1 };
			V4 hotItemColor = V4{ 0.2f, 0.5f, 1.0f, 1 };

			DebugEvent* event = node->event;
			char buffer[256] = {};
			char* end = buffer + sizeof(buffer);
			bool isHot = IsVariableHot(state, node);
			if (isHot) {
				itemColor = hotItemColor;
			}
			if (event->type == Event_ProfilerUI) {
				Rect2 boundaries = event->data_Rect2;
				DebugRenderProfilerUI(state, boundaries, mousePos);
				Rect2 resizeAnchor = GetRectFromCenterDim(boundaries.max, V2{ 8, 8 });
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
				DebugEventToText(event, at, u4(end - at), DebugVarToText_AddColon);
				Rect2 bb = GetTextBoundingBox(state, buffer, fontContext, itemColor);
				if (IsInRectangle(bb, mousePos)) {
					SetNextHotInteraction(state, node, bb, tree);
				}
				PushRect(state->renderGroup, AddRadius(bb, V2{ 4.f, 4.f }), 0, V2{ 0,0 }, V4{ 0.5f, 0, 0, 1 });
				DebugRenderLine(state, buffer, fontContext, itemColor);
			}
			if (node->children/*var->group.expanded*/) {
				parent = node;
				sentinel = node->children;
				node = sentinel->next;
				depth += 1;
			}
			else if (node != sentinel) {
				node = node->next;
			}
			while (node == sentinel && parent) {
				node = parent->next;
				parent = node->parent;
				if (parent) {
					sentinel = parent->children;
				}
				depth -= 1;
			}
			if (!parent) {
				break;
			}
		}
	}
}

void DebugInteract(DebugState* state, V2 mousePos, Controller& controller) {
	// Set hot interaction
	if (!DebugIdIsNull(state->nextHotInteraction.id)) {
		state->nextHotInteraction.startMousePos = mousePos;
		if (state->nextHotInteraction.link) {
			DebugEvent* event = state->nextHotInteraction.link->event;
			switch (event->type) {
			case Event_Data_bool: {
				if (WasPressed(controller.B.mouseLeft)) {
					state->nextHotInteraction.type = DebugInteract_Toggle;
					state->nextHotInteraction.boolean = &event->data_bool;
				}
			} break;
			case Event_Data_f32: {
				if (WasPressed(controller.B.mouseLeft)) {
					state->nextHotInteraction.type = DebugInteract_DragIncrease;
				}
			} break;
#if 0
			case DebugVarType::CompilationSwitch: {
				if (WasPressed(controller.B.mouseLeft)) {
					state->nextHotInteraction.type = DebugInteract_Compile;
				}
			} break;
#endif
			case Event_ProfilerUI: {
				if (WasPressed(controller.B.mouseLeft)) {
					state->nextHotInteraction.type = DebugInteract_Resize;
				}
			} break;
			}
			if (IsPressed(controller.B.kShift) && WasPressed(controller.B.mouseLeft)) {
				state->nextHotInteraction.type = DebugInteract_Tear;
			}
		}
		else {
			// NOTE: Selecting hot entity
			if (WasPressed(controller.B.mouseLeft)) {
				state->nextHotInteraction.type = DebugInteract_Select;
			}
		}
	}
	state->hotInteraction = state->nextHotInteraction;

	// What to do at the beginning of interaction
	if (!state->interacting && state->hotInteraction.type != DebugInteract_None) {
		state->interaction = state->hotInteraction;
		state->interacting = true;
		if (state->interaction.type == DebugInteract_Tear) {
#if 0
			DebugVariableLink* tearPoint = state->interaction.link;
			DebugVariableLink* oldParent = tearPoint->parent;
			V2* treePosition = &state->interaction.state.relevantTree->pos;
			if (oldParent) {
				DebugVariableLink* prevChild = 0;
				for (DebugVariableLink* child = oldParent->var->group.firstChild; child; child = child->next) {
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
				DebugVariableContext context = BeginDebugVariableTree(state, state->mainArena, "NewUserTree", mousePos);
				DebugTree* dst = context.tree;
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
#endif
		}
	}
	
	// What to do DURING the interaction (for interactions taking more time than one frame)
	if (state->interacting && state->interaction.link) {
		V2 dMouse = mousePos - state->interaction.startMousePos;
		switch (state->interaction.type) {
		case DebugInteract_Compile:
		case DebugInteract_Toggle: {
			if (LengthSq(dMouse) > 5.f) {
				state->interaction.type = DebugInteract_Move;
				state->interaction.startMousePos = mousePos;
				state->interaction.pos.initial = state->interaction.relevantTree->pos;
				state->interaction.pos.actual = &state->interaction.relevantTree->pos;
			}
		} break;
		case DebugInteract_DragIncrease: {
			DebugEvent* event = state->interaction.link->event;
			event->data_f32 += 0.001f * dMouse.Y;
		} break;
		case DebugInteract_Resize: {
			DebugEvent* event = state->interaction.link->event;
			f32 newMaxX = Maximum(mousePos.X, event->data_Rect2.min.X + 10.f);
			f32 newMaxY = Maximum(mousePos.Y, event->data_Rect2.min.Y + 10.f);
			event->data_Rect2.max = V2{ newMaxX, newMaxY };
		} break;
		case DebugInteract_Tear:
		case DebugInteract_Move: {
			DebugModifiedPosition& pos = state->interaction.pos;
			*pos.actual = pos.initial + dMouse;
		} break;
		}
	}
	if (WasPressed(controller.B.mouseLeft)) {
#if 0
		state->selected = state->hotRecord;
#endif
	}
	if (WasPressed(controller.B.mouseMiddle)) {
#if 0
		state->selectedRegionIndex = state->hotRegionIndex;
		state->selectedFrameIndex = state->hotFrameIndex;
		state->selectedRecord = state->hotRecord;
#endif
	}

	DEBUG_IF(Debug_ShowInteractions) {
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
		case DebugInteract_Select: {
			interaction = "Select";
		} break;
		}
		V2 dMouse = mousePos - state->interaction.startMousePos;
		char* at = buffer;
		char* end = buffer + sizeof(buffer);
		if (state->interaction.link) {
			at += sprintf_s(at, end - at, "%s with %s", interaction, state->interaction.link->event->blockName);
		}
		else if (!DebugIdIsNull(state->interaction.id)) {
			at += sprintf_s(at, end - at, "%s with debug id %p", interaction, state->interaction.id.val[0]);
		}
		else {
			at += sprintf_s(at, end - at, "%s with none", interaction);
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
			bool* boolean = state->interaction.boolean;
			*boolean = !(*boolean);
			interactionEnded = true;
		}
	} break;
	case DebugInteract_Select: {
		if (WasReleased(controller.B.mouseLeft) || !IsPressed(controller.B.mouseLeft)) {
			state->selectedId = state->interaction.id;
			interactionEnded = true;
		}
	} break;
#if 0
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
#endif
	}
	if (interactionEnded) {
		state->interaction = {};
		state->interacting = false;
	}
	state->nextHotInteraction = {};
}

void DebugDumpStruct(DebugState* state, MemberDefinition* memberArray, u32 memberCount, void* basePtr, u32 indentLevel = 0) {
	for (u32 memberIndex = 0; memberIndex < memberCount; memberIndex++) {
		MemberDefinition* member = memberArray + memberIndex;
		char buffer[256];
		char* at = buffer;
		char* end = buffer + sizeof(buffer);
		for (u32 indent = 0; indent < indentLevel; indent++) {
			*at++ = ' ';
			*at++ = ' ';
			*at++ = ' ';
		}
		*at = 0;
		u8* memberAddress = ptrcast(u8, basePtr) + member->offset;
		switch (member->type) {
		case MetaType_u32: {
			sprintf_s(at, end - at, "%s: %d", member->name, *ptrcast(u32, memberAddress));
			DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
		} break;
		case MetaType_i32: {
			sprintf_s(at, end - at, "%s: %d", member->name, *ptrcast(i32, memberAddress));
			DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
		} break;
		case MetaType_f32: {
			sprintf_s(at, end - at, "%s: %f", member->name, *ptrcast(f32, memberAddress));
			DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
		} break;
		case MetaType_V2: {
			V2* v = ptrcast(V2, memberAddress);
			sprintf_s(at, end - at, "%s: {%.2f, %.2f}", member->name, v->X, v->Y);
			DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
		} break;
		case MetaType_V3: {
			V3* v = ptrcast(V3, memberAddress);
			sprintf_s(at, end - at, "%s: {%.2f, %.2f, %.2f}", member->name, v->X, v->Y, v->Z);
			DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
		} break;
		case MetaType_V4: {
			V4* v = ptrcast(V4, memberAddress);
			sprintf_s(at, end - at, "%s: {%.2f, %.2f, %.2f, %.2f}", member->name, v->X, v->Y, v->Z, v->W);
			DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
		} break;
		case MetaType_CollisionVolumeGroup: {
			sprintf_s(at, end - at, "%s:", member->name);
			DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
			DebugDumpStruct(state, MembersOf_CollisionVolumeGroup, ArrayCount(MembersOf_CollisionVolumeGroup), memberAddress, indentLevel + 1);
		}
		}
	}
	
}

void DebugRenderOverlay(DebugState* state, LoadedBitmap& dstBitmap, InputData& input) {
	TIMED_FUNCTION;
	if (!state->font) {
		return;
	}
	Controller& controller = input.controllers[KB_CONTROLLER_IDX];
	V2 mousePos = FromPixelSpaceToWorldSpace(state->renderGroup.projection, controller.mouse, 0.f);
	DebugRenderVariablesMenu(state, mousePos);
#if 0
	if (state->compilationHandle.state == CmdState_Running) {
		DebugRenderLine(state, "Compiling", state->fontContext, V4{1, 1, 1, 1});
	}
	else if (state->compilationHandle.state == CmdState_Failed) {
		DebugRenderLine(state, "Failed Compilation", state->fontContext, V4{ 1, 1, 1, 1 });
	}
#endif
	DebugInteract(state, mousePos, controller);

	DEBUG_IF(Debug_ShowEventsCount) {
		char buffer[256];
		sprintf_s(buffer, 256, "Events in frame: %d", debugGlobalState->debugEventsCount[0]);
		DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });

		DebugFrameInfo* frameInfo = state->frames + state->frameReadIndex;
		for (u32 eventType = 0; eventType < ArrayCount(frameInfo->eventCount.count); eventType++) {
			sprintf_s(buffer, 256, "   (type)%d = (count)%d", eventType, frameInfo->eventCount.count[eventType]);
			DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
		}
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

#if 0
	if (state->compilationHandle.state == CmdState_Running) {
		Platform->SystemGetCommandState(state->compilationHandle);
	}
	if (state->profilerPause->var->data_bool) {
		return 0;
	}
#endif
	DebugCollateEvents(state);
	u32 frameWriteIndex = u4(debugGlobalState->frameAndEventIndex >> 32);
	DebugFrameInfo* result = state->frames + frameWriteIndex;
	return result;
}
