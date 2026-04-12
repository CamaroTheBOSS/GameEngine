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
bool IsVariableHot(DebugState* state, DebugVariableLink* link) {
	bool result = state->hotInteraction.objType == DebugInteractObject_LinkInTree &&
		state->hotInteraction.linkInTree.link == link;
	return result;
}

inline
bool IsVariableHot(DebugState* state, Rect2& rect2) {
	bool result = state->hotInteraction.objType == DebugInteractObject_Mod_Rect2 &&
		state->hotInteraction.obj_Rect2.actual == &rect2;
	return result;
}

inline
bool AreDebugIdsEqual(DebugId id1, DebugId id2) {
	bool result = id1.ptr == id2.ptr &&
		id1.index == id2.index;
	return result;
}

inline
bool IsDebugIdNull(DebugId id) {
	bool result = id.ptr == 0 &&
		id.index == 0;
	return result;
}

inline
DebugId GetDebugIdForLink(DebugVariableLink* link) {
	DebugId id = {};
	id.ptr = link;
	return id;
}


inline
bool IsHighlighted(DebugState* state, DebugId did) {
	return AreDebugIdsEqual(state->nextHotInteraction.id, did);
}

inline
bool IsSelected(DebugState* state, DebugId did, u32* outIndex = 0) {
	if (IsDebugIdNull(did)) {
		return false;
	}
	for (u32 index = 0; index < state->selectedCount; index++) {
		DebugId id = state->selectedId[index];
		if (AreDebugIdsEqual(id, did)) {
			if (outIndex) {
				*outIndex = index;
			}
			return true;
		}
	}
	return false;
}

inline
bool IsIntrospectionGroup(DebugVariableGroup* group) {
	bool result = group->introspectionObjectIndex > 0;
	return result;
}

inline 
DebugInteraction GetMoveInteraction(V2 mousePos, DebugTree* tree, V2 treeInitialPos) {
	DebugInteraction interaction = {};
	interaction.type = DebugInteract_Move;
	interaction.objType = DebugInteractObject_Pos;
	interaction.startMousePos = mousePos;
	tree->pos = treeInitialPos;
	interaction.pos.initial = tree->pos;
	interaction.pos.actual = &tree->pos;
	return interaction;
}

inline
DebugInteraction InteractionWithIntrospectable(Rect2 bBox, DebugId did) {
	DebugInteraction interaction = {};
	interaction.objType = DebugInteractObject_Id;
	interaction.id = did;
	interaction.startBoundingBox = bBox;
	return interaction;
}

inline
DebugInteraction InteractionWithLink(DebugState* state, Rect2 boundingBox, DebugTree* tree, DebugVariableLink* link) {
	DebugInteraction interaction = {};
	interaction.objType = DebugInteractObject_LinkInTree;
	interaction.linkInTree.link = link;
	interaction.linkInTree.tree = tree;
	interaction.startBoundingBox = boundingBox;
	return interaction;
}

internal DebugEvent* InitializePermanentDebugVariable(DebugEvent* subevent, DebugEventType type, const char* name, const char* file, u16 line, const char* GUID) {
	RecordDebugEventNoBracket(0, Event_PermanentVariableDeclaration, file, name, line);
	event->data_DebugEvent = subevent;
	event->GUID = GUID;
	subevent->blockName = name;
	subevent->coreId = 0;
	subevent->cycles = 0;
	subevent->threadId = 0;
	subevent->type = type;
	subevent->file = file;
	subevent->line = line;
	subevent->GUID = GUID;
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
DebugId DEBUG_POINTER_ID(void* ptr, u32 objId) {
	return { ptr, objId };
}

inline
void DEBUG_HIT(DebugId did, Rect2 boundingBox) {
	DebugState* state = GetDebugState();
	if (!state) {
		return;
	}
	if (!IsSelected(state, did)) {
		state->nextHotInteraction = InteractionWithIntrospectable(boundingBox, did);
	}
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
DebugTree* AddTree(DebugState* state, V2 pos, const char* name) {
	DebugTree* tree = PushStructSize(state->mainArena, DebugTree);
	DebugVariableLink* link = PushStructSize(state->mainArena, DebugVariableLink);
	link->group = &tree->rootGroup;
	link->next = 0;
	link->parentGroup = 0;
	link->variable = 0;

	tree->pos = pos;
	tree->rootGroup = {};
	tree->rootGroup.expanded = true;
	tree->rootGroup.name = name;
	tree->rootGroup.nameLength = StringLength(tree->rootGroup.name);
	tree->rootGroup.containingLink = link;
	DLINKED_LIST_ADD(&state->UISentinel, tree);
	return tree;
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
#if 1
		InitializeArena(
			state->mainArena,
			ptrcast(u8, debugGlobalMemory->debugMemory) + sizeof(DebugState),
			debugGlobalMemory->debugMemorySize - sizeof(DebugState)
		);
#else
		InitializeArena(
			state->mainArena,
			ptrcast(u8, debugGlobalMemory->debugMemory) + sizeof(DebugState),
			kB(4237)
		);
#endif 
#if 0
		SubArena(state->collationFrameArena, state->mainArena, MB(16));
#else
		SubArena(state->collationFrameArena, state->mainArena, kB(1024));
#endif
		state->renderGroup = AllocateRenderGroup(state->mainArena, &tranState->assets, MB(4), false);
		state->highPriorityQueue = tranState->highPriorityQueue;
		state->overlayBoundaries = GetRectFromCenterDim(V2{ 0, 0 }, V2i(screenBitmap.width, screenBitmap.height));
		state->threadStacks = PushArray(state->mainArena, MAX_DEBUG_THREADS, DebugThreadStack);
		DLINKED_LIST_INIT(&state->UISentinel);
		DLINKED_LIST_INIT(&state->framesSentinel);

		V2 leftTopCorner = V2{ state->overlayBoundaries.min.X, state->overlayBoundaries.max.Y };
		V2 rightTopCorner = V2{ state->overlayBoundaries.max.X - 400.f, state->overlayBoundaries.max.Y };
		state->defaultMainVariablesGroup = &AddTree(state, leftTopCorner, "Debugging")->rootGroup;
		state->defaultIntrospectionGroup = &AddTree(state, rightTopCorner, "Introspection")->rootGroup;

		BeginRendering(state->renderGroup);
		state->isInitialized = true;
	}
	EndRendering(state->renderGroup);
	BeginRendering(state->renderGroup);
	state->entityIntrospectionCountInFrame = 0;
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
	stack->timeEvents = 0;
	stack->dataEvents = 0;
	return stack;
}

internal
OpenDebugEvent* PushToEventStack(DebugState* state, OpenDebugEvent** stack, DebugEvent* event) {
	OpenDebugEvent* newBlock = state->openEventFreeList;
	if (newBlock) {
		state->openEventFreeList = newBlock->next;
	}
	else {
		newBlock = PushStructSize(state->mainArena, OpenDebugEvent);
	}
	*newBlock = {};
	newBlock->next = *stack;
	newBlock->event = *event;
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
DebugVariableLink* AddVariableToGroup(DebugState* state, DebugVariableGroup* group, DebugVariable* var) {
	DebugVariableLink* link = PushStructSize(state->mainArena, DebugVariableLink);
	link->variable = var;
	link->parentGroup = group;
	link->group = 0;
	link->next = group->firstLink;
	group->firstLink = link;
	return link;
}

DebugVariableLink* AddGroupToGroup(DebugState* state, DebugVariableGroup* parent, DebugVariableGroup* group) {
	DebugVariableLink* link = PushStructSize(state->mainArena, DebugVariableLink);
	link->variable = 0;
	link->parentGroup = parent;
	link->group = group;
	group->containingLink = link;
	link->next = parent->firstLink;
	parent->firstLink = link;
	return link;
}

inline
u32 GetStringHash(const char* string, u32 length) {
	// TODO: Better hash function!
	u32 hash = 0;
	internal u32 primes[] = {
		3, 5, 7, 11, 13, 17, 19, 23, 29, 31,37,	41,	43,	47,	53,	59,	61,	
		67,	71, 73,	79,	83,	89,	97,	101,103,107,109,113,127,131,137
	};
	Assert(length < ArrayCount(primes))
	for (u32 idx = 0; idx < length; idx++) {
		hash += primes[idx] * (string[idx] - 'a');
		hash ^= 524287;
	}
	return hash;
}

internal
DebugVariableGroup* GetOrCreateVariableGroup(DebugState* state, DebugVariableGroup* parentGroup, const char* name, u32 nameLength, u32 introspectionObjectIndex = 0) {
	TIMED_FUNCTION;
	// TODO: Could I avoid hashing string?
	u32 hashSlot = (GetStringHash(name, nameLength) + 13 * introspectionObjectIndex) % ArrayCount(state->groupHash);
	DebugVariableGroup* result = 0;
	for (DebugVariableGroup* group = state->groupHash[hashSlot]; group; group = group->nextInHash) {
		if (StringsAreEqual(group->name, group->nameLength, name, nameLength) && 
			introspectionObjectIndex == group->introspectionObjectIndex) 
		{
			result = group;
			break;
		}
	}
	if (!result) {
		result = PushStructSize(state->mainArena, DebugVariableGroup);
		result->expanded = true;
		result->firstLink = 0;
		result->name = name;
		result->nameLength = nameLength;
		result->parentGroup = parentGroup;
		result->introspectionObjectIndex = introspectionObjectIndex;
		AddGroupToGroup(state, parentGroup, result);
		result->nextInHash = state->groupHash[hashSlot];
		state->groupHash[hashSlot] = result;
	}
	return result;
}

internal
DebugVariable* GetOrCreateDebugVariableForEvent(DebugState* state, DebugVariableGroup* group, 
	DebugStoredEvent* storedEvent) {
	// TODO: Verify that this is compiled as AND and not as MOD
	DebugEvent* event = &storedEvent->event;
	u32 hashSlot = (u4(uptr(event->GUID) >> 2) + 13 * group->introspectionObjectIndex) % ArrayCount(state->variableHash);
	DebugVariable* result = 0;
	for (DebugVariable* var = state->variableHash[hashSlot]; var; var = var->nextInHash) {
		if (var->GUID == event->GUID && var->introspectionObjectIndex == group->introspectionObjectIndex) {
			result = var;
			break;
		}
	}
	if (!result) {
		result = PushStructSize(state->mainArena, DebugVariable);
		result->GUID = event->GUID;
		result->name = PushString(state->mainArena, event->GUID, StringLength(event->GUID) + 1);
		result->nextInHash = state->variableHash[hashSlot];
		result->introspectionObjectIndex = group->introspectionObjectIndex;
		state->variableHash[hashSlot] = result;
		AddVariableToGroup(state, group, result);
	}
	return result;
}

internal
void FreeOldestFrame(DebugState* state) {
	TIMED_FUNCTION;
	DebugCollationFrame* frame = state->framesSentinel.prev;
	Assert(frame != &state->framesSentinel);
	for (u32 hashSlot = 0; hashSlot < ArrayCount(state->variableHash); hashSlot++) {
		for (DebugVariable* var = state->variableHash[hashSlot]; var; var = var->nextInHash) {
			if (var->permanent || !var->oldestEvent || 
				var->oldestEvent->captureFrameIndex > frame->frameIndex) {
				continue;
			}
			DebugStoredEvent* lastEventToRemove = var->oldestEvent;
			while (lastEventToRemove->next && lastEventToRemove->captureFrameIndex <= frame->frameIndex) {
				lastEventToRemove = lastEventToRemove->next;
			}
			DebugStoredEvent* newOldest = lastEventToRemove->next;
			lastEventToRemove->next = state->freeStoredEventList;
			state->freeStoredEventList = var->oldestEvent;
			if (newOldest) {
				var->oldestEvent = newOldest;
				Assert(var->newestEvent->captureFrameIndex >= newOldest->captureFrameIndex);
			}
			else {
				var->oldestEvent = var->newestEvent = 0;
			}
		}
	}


	for (u32 thread = 0; thread < frame->threadCount; thread++) {
		DebugProfilerSpan* span = (frame->cpuSpansPerThread + thread)->firstChild;
		DebugProfilerSpan* spanStack[64] = {};
		u32 spanStackCount = 0;
		while (span) {
			DebugProfilerSpan* next = span;
			while (next) {
				if (next->firstChild) {
					Assert(spanStackCount < ArrayCount(spanStack) - 1);
					spanStack[++spanStackCount] = next->firstChild;
				}
				if (!next->next) {
					break;
				}
				next = next->next;
			}
			next->next = state->spanFreeList;
			state->spanFreeList = span;

			span = spanStack[spanStackCount--];
		}
	}
	DLINKED_LIST_REMOVE(frame);
	frame->next = state->freeFrameList;
	state->freeFrameList = frame;
}

internal
DebugStoredEvent* StoreEvent(DebugState* state, DebugVariableGroup* group, DebugEvent* event,
	u32 captureFrameIndex, bool permanent = false) {
	DebugStoredEvent* storedEvent = 0;
	while (!storedEvent) {
		storedEvent = state->freeStoredEventList;
		if (storedEvent) {
			state->freeStoredEventList = state->freeStoredEventList->next;
		}
		else if (HasArenaSpaceFor(state->collationFrameArena, sizeof(DebugStoredEvent))) {
			storedEvent = PushStructSize(state->collationFrameArena, DebugStoredEvent);
		}
		else {
			FreeOldestFrame(state);
		}
	}
	storedEvent->event = *event;
	storedEvent->next = 0;
	storedEvent->captureFrameIndex = captureFrameIndex;

	DebugVariable* var = GetOrCreateDebugVariableForEvent(state, group, storedEvent);
	var->permanent = permanent;
	if (!var->oldestEvent) {
		var->oldestEvent = var->newestEvent = storedEvent;
	}
	else {
		var->newestEvent = var->newestEvent->next = storedEvent;
	}
	return storedEvent;
}

internal 
DebugProfilerSpan* AllocateSpan(DebugState* state) {
	DebugProfilerSpan* span = 0;
	while (!span) {
		span = state->spanFreeList;
		if (span) {
			state->spanFreeList = state->spanFreeList->next;
		}
		else if (HasArenaSpaceFor(state->collationFrameArena, sizeof(DebugProfilerSpan))) {
			span = PushStructSize(state->collationFrameArena, DebugProfilerSpan);
		}
		else {
			FreeOldestFrame(state);
		}
	}
	span->firstChild = 0;
	span->next = 0;
	span->maxT = 0;
	span->minT = 0;
	return span;
}

internal
DebugCollationFrame* AllocateNewDebugFrame(DebugState* state) {
	DebugCollationFrame* newFrame = 0;
	while (!newFrame) {
		newFrame = state->freeFrameList;
		if (newFrame) {
			state->freeFrameList = state->freeFrameList->next;
		}
		else if (HasArenaSpaceFor(state->collationFrameArena, sizeof(DebugCollationFrame))) {
			newFrame = PushStructSize(state->collationFrameArena, DebugCollationFrame);
		}
		else {
			FreeOldestFrame(state);
		}
	}
	u32 frameIndex = !debugGlobalState->currentFrameIndex;
	newFrame->eventsCount = debugGlobalState->eventsCount[frameIndex];
	newFrame->startCycles = debugGlobalState->frameStartCycles[frameIndex];
	newFrame->frameIndex = state->totalFrameCount;
	ZeroStruct(newFrame->cpuSpansPerThread);
	DLINKED_LIST_ADD(&state->framesSentinel, newFrame);
	return newFrame;
}

internal
DebugVariableGroup* GetGroupForHierachicalName(DebugState* state, DebugVariableGroup* group, const char* name, u32 introspectionObjectIndex = 0) {
	const char* firstUnderscore = 0;
	const char* at = name;
	while (*at != 0) {
		if (*at == '_') {
			firstUnderscore = at;
			break;
		}
		at++;
	}
	DebugVariableGroup* result = group;
	if (firstUnderscore) {
		u32 descentGroupNameLength = u4(firstUnderscore - name);
		DebugVariableGroup* descentGroup = GetOrCreateVariableGroup(state, group, name, descentGroupNameLength, introspectionObjectIndex);
		result = GetGroupForHierachicalName(state, descentGroup, name + descentGroupNameLength + 1, introspectionObjectIndex);
	}
	return result;
}

internal
DebugVariableGroup* GetGroupForObjectIntrospection(DebugState* state, DebugVariableGroup* group, const char* name, u32 introspectionObjectIndex) {
	DebugVariableGroup* parentGroup = GetGroupForHierachicalName(state, group, name);
	const char* lastGroupName = parentGroup->name + parentGroup->nameLength + 1;
	u32 lastGroupNameLength = StringLength(lastGroupName);
	DebugVariableGroup* result = GetOrCreateVariableGroup(state, parentGroup, lastGroupName, lastGroupNameLength, introspectionObjectIndex);
	return result;
}

internal
void DebugCollateEvents(DebugState* state) {
	TIMED_FUNCTION;

	u32 frameIndex = !debugGlobalState->currentFrameIndex;
	f32 scale = DEBUG_COLLATION_SCALE;
	DebugEvent* eventsInFrame = debugGlobalState->events[frameIndex];
	u32 eventsInFrameCount = debugGlobalState->eventsCount[frameIndex];
	DebugCollationFrame* newFrame = AllocateNewDebugFrame(state);
	DebugVariableGroup* introspectionGroup = state->defaultIntrospectionGroup;
	for (u32 eventIndex = 0;
		eventIndex < eventsInFrameCount;
		eventIndex++
		) {
		DebugEvent* event = eventsInFrame + eventIndex;
		DebugThreadStack* stack = GetDebugStackForThread(state, event->threadId);
		switch (event->type) {
		case Event_Time_BlockBegin: {
			PushToEventStack(state, &stack->timeEvents, event);
		} break;
		case Event_Time_BlockEnd: {
			OpenDebugEvent* block = stack->timeEvents;
			OpenDebugEvent* parentBlock = block->next;
			DebugEvent* openEvent = &block->event;
			Assert(openEvent);
			Assert(openEvent->threadId == event->threadId);
			f32 minT = f4(openEvent->cycles - newFrame->startCycles) * scale;
			f32 maxT = f4(event->cycles - newFrame->startCycles) * scale;
			f32 thresholdT = 0.01f;
			if ((maxT - minT) > thresholdT) {
				DebugProfilerSpan* span = AllocateSpan(state);
				span->minT = minT;
				span->maxT = maxT;
				span->thread = stack->laneId;
				span->name = openEvent->blockName;
				span->firstChild = block->firstChildSpan;
				if (parentBlock) {
					span->next = parentBlock->firstChildSpan;
					parentBlock->firstChildSpan = span;
				}
				else {
					Assert(stack->laneId < ArrayCount(newFrame->cpuSpansPerThread));
					DebugProfilerSpan* threadRoot = newFrame->cpuSpansPerThread + stack->laneId;
					span->next = threadRoot->firstChild;
					threadRoot->firstChild = span;
					if (newFrame->threadCount < stack->laneId + 1) {
						newFrame->threadCount = stack->laneId + 1;
					}
				}
			}
			PopFromEventStack(state, &stack->timeEvents);
		} break;
		case Event_Data_BlockBegin: {
			state->entityIntrospectionCountInFrame++;
			u32 selectedIndex = 0;
			u32 inFrameEntityIndex = IsSelected(state, event->data_DebugId, &selectedIndex) ? 99 + selectedIndex : state->entityIntrospectionCountInFrame;
			introspectionGroup = GetGroupForObjectIntrospection(state, introspectionGroup, event->blockName, inFrameEntityIndex);
			introspectionGroup->dataReceivingFrameIndex = state->totalFrameCount;
			introspectionGroup->introspectionId = event->data_DebugId;
			PushToEventStack(state, &stack->dataEvents, event);
		} break;
		case Event_Data_BlockEnd: {
			// TODO: Stack on EventStack
			introspectionGroup = state->defaultIntrospectionGroup;
			PopFromEventStack(state, &stack->dataEvents);
		} break;
		case Event_PermanentVariableDeclaration: {
			DebugVariableGroup* group = GetGroupForHierachicalName(state, state->defaultMainVariablesGroup, event->blockName);
			DebugStoredEvent* storedEvent = StoreEvent(state, group, event, newFrame->frameIndex, true);
		} break;
		case Event_Data_u32:
		case Event_Data_i32:
		case Event_Data_f32:
		case Event_Data_V2:
		case Event_Data_V3:
		case Event_Data_V4: {
			StoreEvent(state, introspectionGroup, event, newFrame->frameIndex);
		} break;
		}
	}
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
	case Event_PermanentVariableDeclaration: {
		u32 newFlags = flags;
		flags &= (~DebugVarToText_ConfigPrefix | DebugVarToText_AddNewLine);
		DebugEventToText(event->data_DebugEvent, at, u4(end - at), newFlags);
	} break;
	default: {
		at += sprintf_s(at, end - at, "Unknown: %s", event->blockName);
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

Rect2 GetCpuSpanRectangle(Rect2 boundaries, f32 currentWidth,
	u32 currentThread, f32 threadWidth, f32 threadTotalWidth, f32 minT, f32 maxT) {
	V2 dims = GetDim(boundaries);
	V3 spanCenter = {
		boundaries.min.X + dims.X - currentWidth + (f4(currentThread) + 0.5f) * threadTotalWidth,
		boundaries.min.Y + 0.5f * (maxT + minT) * dims.Y,
		0
	};
	V2 spanSize = { threadWidth, (maxT - minT) * dims.Y };

	Rect2 rectangle = GetRectFromCenterDim(spanCenter.XY, spanSize);
	rectangle.min.X = Clip(rectangle.min.X, boundaries.min.X, boundaries.max.X);
	return rectangle;
}

void DebugRenderCpuProfiler(DebugState* state, V2 mousePos) {
	Rect2 init = Rect2{
		V2{ state->overlayBoundaries.min + V2{ 30.f, 30.f } },
		V2{ state->overlayBoundaries.max.X - 30.f, state->overlayBoundaries.min.Y + 250.f }
	};
	DEFINE_DEBUG_VARIABLE_WITH_INIT(Rect2, Profiler_Boundaries, init);
	Rect2 boundaries = Profiler_Boundaries.data_Rect2;
#if 1
	f32 profilerPosY = boundaries.min.Y;
	f32 profilerPosX = boundaries.min.X;
	V2 profilerDim = GetDim(boundaries);
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
	f32 frameWidth = f4(state->threadStacksCount) * threadLaneTotalWidth + frameLaneSpace;
	f32 currentWidth = frameWidth;
	f32 collationScale = DEBUG_COLLATION_SCALE;
	V4 backgroundColor = V4{ 0.03f, 0.03f, 0.03f, 1 };
	PushRect(state->renderGroup, boundaries, 0, V2{ 0, 0 }, backgroundColor);

	DebugCollationFrame* frame = state->framesSentinel.next;
	while (currentWidth < profilerDim.X + frameWidth && frame != &state->framesSentinel) {
		for (u32 thread = 0; thread < frame->threadCount; thread++) {
			DebugProfilerSpan* span = (frame->cpuSpansPerThread + thread)->firstChild;
			Rect2 placeholderRectangle = GetCpuSpanRectangle(boundaries, currentWidth, thread,
				threadLaneWidth, threadLaneTotalWidth, 0.f, 1.f
			);
			if (IsValid(placeholderRectangle)) {
				PushRect(state->renderGroup, placeholderRectangle, 0, V2{ 0, 0 }, V4{ 0, 0, 0, 1 });
			}
			while (span) {
				Rect2 spanRect = GetCpuSpanRectangle(boundaries, currentWidth, thread,
					threadLaneWidth, threadLaneTotalWidth, span->minT, span->maxT
				);

				bool isHovered = IsInRectangle(spanRect, mousePos);
				u32 colorIndex = u4(uptr(span->name) >> 2) % ArrayCount(colors);
				V4 rectColor = colors[colorIndex];
				if (isHovered) {
					if (span->name) {
						char buffer[256];
						sprintf_s(buffer, "%s", span->name);
						V4 color = V4{ 1, 1, 1, 1 };
						f32 lineAdvance = state->fontContext.scale * f4(GetFontLineAdvance(state->font));
						V2 textPos = mousePos + V2{ 0, lineAdvance };
						DebugRenderLine(state, buffer, textPos, state->fontContext.scale, color);
						textPos += V2{ 0, lineAdvance };
						sprintf_s(buffer, "t<%4f,%4f>", span->minT, span->maxT);
						DebugRenderLine(state, buffer, textPos, state->fontContext.scale, color);
					}
					rectColor = V4{ 1, 1, 1, 1 };
				}
				if (IsValid(spanRect)) {
					PushRect(state->renderGroup, spanRect, 0, V2{ 0, 0 }, rectColor);
				}

				span = span->next;
			}
		}
		frame = frame->next;
		currentWidth += frameWidth;
	}

	Rect2 resizeAnchor = GetRectFromCenterDim(boundaries.max, V2{ 8, 8 });
	if (IsInRectangle(boundaries, mousePos)) {
		DebugInteraction interaction = {};
		interaction.startBoundingBox = resizeAnchor;
		interaction.objType = DebugInteractObject_Mod_Rect2;
		interaction.obj_Rect2.initial = Profiler_Boundaries.data_Rect2;
		interaction.obj_Rect2.actual = &Profiler_Boundaries.data_Rect2;
		state->nextHotInteraction = interaction;
	}
	V4 itemColor = V4{ 1, 1, 1, 1 };
	if (IsVariableHot(state, Profiler_Boundaries.data_Rect2)) {
		itemColor = V4{ 0.5f, 0.5f, 0, 1 };
	}
	if (IsInRectangle(resizeAnchor, mousePos)) {
		DebugInteraction interaction = {};
		interaction.startBoundingBox = resizeAnchor;
		interaction.objType = DebugInteractObject_Mod_Rect2;
		interaction.obj_Rect2.initial = Profiler_Boundaries.data_Rect2;
		interaction.obj_Rect2.actual = &Profiler_Boundaries.data_Rect2;
		state->nextHotInteraction = interaction;
	}
	PushRect(state->renderGroup, resizeAnchor, 0, V2{ 0, 0 }, itemColor);
#endif
}

inline
bool GroupShouldBeRendered(DebugState* state, DebugVariableGroup* group, DebugTree* tree) {
	bool result = group && (!IsIntrospectionGroup(group) ||
							group->dataReceivingFrameIndex == (state->totalFrameCount - 1) ||
							&tree->rootGroup != state->defaultIntrospectionGroup);
	return result;
}

internal
void DebugRenderVariablesMenu(DebugState* state, V2 mousePos) {
	for (DebugTree* tree = state->UISentinel.next; tree != &state->UISentinel; tree = tree->next) {
		FontDrawContext fontContext = InitializeStandardFontDrawContext(state, tree->pos);
		DebugVariableLink* node = tree->rootGroup.containingLink;
		u32 depth = 0;
		DebugVariableLink* parent[64] = {};
		while (node) {
			char buffer[256] = {};
			char* end = buffer + sizeof(buffer);
			V4 itemColor = V4{ 1, 1, 1, 1 };
			V4 hotItemColor = V4{ 0.2f, 0.5f, 1.0f, 1 };
			bool elementRendered = false;

			bool isGroup = node->group != 0;
			if ((!isGroup && node->variable->newestEvent) || GroupShouldBeRendered(state, node->group, tree)) {
				bool isHot = IsVariableHot(state, isGroup ? node->group->containingLink : node);
				if (isHot) {
					itemColor = hotItemColor;
				}

				char* at = buffer;
				for (u32 idx = 0; idx < depth; idx++) {
					*at++ = ' ';
					*at++ = ' ';
				}
				if (isGroup) {
					at += sprintf_s(at, end - at, "%.*s", node->group->nameLength, node->group->name);
					if (IsIntrospectionGroup(node->group)) {
						at += sprintf_s(at, end - at, " [%d]", node->group->introspectionObjectIndex);
					}
					*at++ = ':';
					*at++ = 0;
				}
				else {
					DebugEvent* event = &node->variable->newestEvent->event;
					DebugEventToText(event, at, u4(end - at), DebugVarToText_AddColon);
				}

				Rect2 bb = GetTextBoundingBox(state, buffer, fontContext, itemColor);
				if (IsInRectangle(bb, mousePos)) {
					state->nextHotInteraction = InteractionWithLink(state, bb, tree, isGroup ? node->group->containingLink : node);
				}
				DebugVariableGroup* selectedGroup = isGroup ? node->group : node->parentGroup;
				V4 bbColor = V4{ 0.5f, 0, 0, 1 };
				if (IsSelected(state, selectedGroup->introspectionId)) {
					bbColor = V4{ 0.5f, 0.5f, 0, 1 };
				}
				else {
					selectedGroup->introspectionId = {};
				}

				PushRect(state->renderGroup, AddRadius(bb, V2{ 4.f, 4.f }), 0, V2{ 0,0 }, bbColor);
				DebugRenderLine(state, buffer, fontContext, itemColor);
				elementRendered = true;
			}

			if (node) {
				if (node->group && node->group->expanded && elementRendered) {
					// TODO: Display group names
					parent[++depth] = node;
					node = parent[depth]->group->firstLink;
				}
				else {
					node = node->next;
				}
			}
			while (!node && depth > 0) {
				node = parent[depth--];
				if (node) {
					node = node->next;
				}
			}
		}
	}
}

internal
void DebugInteract(DebugState* state, V2 mousePos, Controller& controller) {
	if (WasPressed(controller.B.kEsc)) {
		state->selectedCount = 0;
	}

	// Set hot interaction
	DebugInteractionObject nextInteractionObjType = state->nextHotInteraction.objType;
	if (nextInteractionObjType != DebugInteractObject_None) {
		state->nextHotInteraction.startMousePos = mousePos;
		switch (nextInteractionObjType) {
		case DebugInteractObject_LinkInTree: {
			DebugVariableGroup* group = state->nextHotInteraction.linkInTree.link->group;
			bool isIntrospectGroup = group && IsIntrospectionGroup(group);
			bool isSelectedGroup = group && IsSelected(state, group->introspectionId);
			if (group) {
				if ((!isIntrospectGroup || isSelectedGroup) &&
					WasPressed(controller.B.mouseLeft))
				{
					state->nextHotInteraction.type = DebugInteract_Toggle;
				}
			}
			else {
				DebugEvent* event = &state->nextHotInteraction.linkInTree.link->variable->newestEvent->event;
				if (event->type == Event_PermanentVariableDeclaration) {
					event = event->data_DebugEvent;
				}
				switch (event->type) {
				case Event_Data_bool: {
					if (WasPressed(controller.B.mouseLeft)) {
						state->nextHotInteraction.type = DebugInteract_Toggle;
					}
				} break;
				case Event_Data_f32: {
					if (WasPressed(controller.B.mouseLeft)) {
						state->nextHotInteraction.type = DebugInteract_DragIncrease;
						state->nextHotInteraction.fl32.initial = event->data_f32;
						state->nextHotInteraction.fl32.actual = &event->data_f32;
					}
				} break;
				}
			}
			if (!group || !isIntrospectGroup || isSelectedGroup) {
				if (IsPressed(controller.B.kShift) && WasPressed(controller.B.mouseLeft)) {
					state->nextHotInteraction.type = DebugInteract_Tear;
				}
			}
		} break;
		case DebugInteractObject_Id: {
			if (WasPressed(controller.B.mouseLeft)) {
				state->nextHotInteraction.type = DebugInteract_Select;
			}
		} break;
		case DebugInteractObject_Mod_Rect2: {
			if (WasPressed(controller.B.mouseLeft)) {
				state->nextHotInteraction.type = DebugInteract_Resize;
			}
		}
		}
	}
	state->hotInteraction = state->nextHotInteraction;

	// What to do at the beginning of interaction
	if (!state->interacting && state->hotInteraction.type != DebugInteract_None) {
		state->interaction = state->hotInteraction;
		state->interacting = true;
		if (state->interaction.type == DebugInteract_Tear) {
#if 1
			DebugVariableLink* tearPoint = state->interaction.groupInTree.group->containingLink;
			if (state->interaction.objType == DebugInteractObject_LinkInTree) {
				tearPoint = state->interaction.linkInTree.link;
			}
			DebugVariableGroup* oldParentGroup = tearPoint->parentGroup;
			DebugTree* dstTree = state->interaction.linkInTree.tree;
			if (oldParentGroup) {
				DebugVariableLink* prevChild = 0;
				for (DebugVariableLink* child = oldParentGroup->firstLink; child; child = child->next) {
					if (child == tearPoint) {
						if (prevChild) {
							prevChild->next = child->next;
						}
						else {
							oldParentGroup->firstLink = oldParentGroup->firstLink->next;
						}
						break;
					}
					prevChild = child;
				}
				dstTree = AddTree(state, dstTree->pos, "NewUserGroup");
				dstTree->rootGroup.firstLink = tearPoint;
				tearPoint->next = 0;
				tearPoint->parentGroup = &dstTree->rootGroup;
			}
			Rect2& bbox = state->interaction.startBoundingBox;
			V2 treePos = V2{ bbox.min.X, bbox.max.Y };
			state->interaction = GetMoveInteraction(mousePos, dstTree, treePos);
#endif
		}
	}
	
	// What to do DURING the interaction (for interactions taking more time than one frame)
	if (state->interacting && state->interaction.objType != DebugInteractObject_None) {
		V2 dMouse = mousePos - state->interaction.startMousePos;
		switch (state->interaction.type) {
		case DebugInteract_Toggle: {
			Assert(state->interaction.objType == DebugInteractObject_LinkInTree);
			DebugTree* tree = state->interaction.linkInTree.tree;
			if (LengthSq(dMouse) > 5.f) {
				state->interaction = GetMoveInteraction(mousePos, tree, tree->pos);
			}
		} break;
		case DebugInteract_DragIncrease: {
			DebugModifiedFloat& f = state->interaction.fl32;
			*f.actual = f.initial + 0.1f * dMouse.Y;
		} break;
		case DebugInteract_Resize: {
			DebugModifiedRect2& rect2 = state->interaction.obj_Rect2;
			f32 newMaxX = Maximum(mousePos.X, rect2.initial.min.X + 10.f);
			f32 newMaxY = Maximum(mousePos.Y, rect2.initial.min.Y + 10.f);
			rect2.actual->max = V2{ newMaxX, newMaxY };
		} break;
		case DebugInteract_Move: {
			DebugModifiedV2& pos = state->interaction.pos;
			*pos.actual = pos.initial + dMouse;
		} break;
		}
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
		char* at = buffer;
		char* end = buffer + sizeof(buffer);
		at += sprintf_s(at, end - at, "%s", interaction);
		DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
	}

	// What to do at the END of interaction
	bool interactionEnded = false;
	switch (state->interaction.type) {
	case DebugInteract_Resize:
	case DebugInteract_DragIncrease:
	case DebugInteract_Move: {
		interactionEnded = !IsPressed(controller.B.mouseLeft);
	} break;
	case DebugInteract_Toggle: {
		Assert(state->interaction.objType == DebugInteractObject_LinkInTree);
		if (WasReleased(controller.B.mouseLeft) || !IsPressed(controller.B.mouseLeft)) {
			bool* boolean = 0;
			if (state->interaction.linkInTree.link->group) {
				boolean = &state->interaction.linkInTree.link->group->expanded;
			}
			else {
				DebugEvent* event = &state->interaction.linkInTree.link->variable->newestEvent->event;
				if (event->type == Event_PermanentVariableDeclaration) {
					event = event->data_DebugEvent;
				}
				boolean = &event->data_bool;
			}
			if (boolean) {
				*boolean = !(*boolean);
			}
			interactionEnded = true;
		}
	} break;
	case DebugInteract_Select: {
		if (WasReleased(controller.B.mouseLeft) || !IsPressed(controller.B.mouseLeft)) {
			if (IsPressed(controller.B.kShift)) {
				u32 index = state->selectedCount++;
				state->selectedCount = state->selectedCount % ArrayCount(state->selectedId);
				state->selectedId[index] = state->interaction.id;
			}
			else {
				state->selectedCount = 1;
				state->selectedId[0] = state->interaction.id;
			}
			
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
	default: {
		interactionEnded = true;
	} break;
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
	DebugRenderCpuProfiler(state, mousePos);
	DebugInteract(state, mousePos, controller);

	DEBUG_IF(Debug_ShowEventsCount) {
		char buffer[256];
		u32 currentFrame = !debugGlobalState->currentFrameIndex;
		sprintf_s(buffer, 256, "Events in frame: %d", debugGlobalState->eventsCount[currentFrame]);
		DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });

#if 0
		DebugFrameInfo* frameInfo = state->frames + state->frameReadIndex;
		for (u32 eventType = 0; eventType < ArrayCount(frameInfo->eventCount.count); eventType++) {
			sprintf_s(buffer, 256, "   (type)%d = (count)%d", eventType, frameInfo->eventCount.count[eventType]);
			DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
		}
#endif
		MemoryArena* arenas[] = { &state->collationFrameArena, &state->mainArena };
		const char* arenaNames[] = { "CollationFrame", "Main" };
		for (u32 arenaIndex = 0; arenaIndex < ArrayCount(arenas); arenaIndex++) {
			u64 arenaRemainingSize = GetArenaFreeSpaceSize(*arenas[arenaIndex]) / 1024;
			sprintf_s(buffer, 256, "%sArena remaining size: %lldkB", arenaNames[arenaIndex], arenaRemainingSize);
			DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
		}
		
	}

	TiledRenderGroupToBuffer(state->renderGroup, dstBitmap, state->highPriorityQueue);
}

extern "C" DebugGlobalState* DebugInit(ProgramMemory* memory) {
	return debugGlobalState;
}

extern "C" void DebugFinishFrame(ProgramMemory* memory, BitmapData& rawBitmap, InputData& input) {
	TIMED_FUNCTION;
	LoadedBitmap bitmap = {};
	bitmap.height = rawBitmap.height;
	bitmap.width = rawBitmap.width;
	bitmap.data = ptrcast(u32, rawBitmap.data);
	bitmap.pitch = rawBitmap.pitch;

	DebugState* state = DebugBegin(bitmap);
	if (!state) {
		return;
	}
	DebugRenderOverlay(state, bitmap, input);
	DebugCollateEvents(state);
	state->totalFrameCount++;
	return;
}
