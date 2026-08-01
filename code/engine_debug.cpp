#include "engine.h"
#include "renderer_software.cpp"

#if 1
#define PRINT_DEBUGGING(format, ...) \
	{ char buffer[256]; \
	sprintf_s(buffer, 256, format, __VA_ARGS__); \
	DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 }); }
#else
#define PRINT_DEBUGGING(...)
#endif

DebugGlobalState debugGlobalState_ = {};
DebugGlobalState* debugGlobalState = &debugGlobalState_;

// TODO: Delete stdlib
#include <stdio.h>

//NOTE: Intelisense helpers
inline bool IsPressed(Button& button);
inline bool WasPressed(Button& button);
inline bool WasReleased(Button& button);
static u64 DEBUG_CPU_FREQ = 1;
static f32 DEBUG_COLLATION_SCALE = 1.f;
static u32 DEBUG_SPAN_MERGE_CYCLES_THRESHOLD = 1;

internal DebugVariable* GetOrCreateDebugVariableForGroup(DebugState* state, DebugVariableLink* link, DebugParsedGUID& guid);
debug_variable bool PROFILER_PAUSE = false;

inline
DebugVariableFrame* GetNewestFrame(DebugState* state, DebugVariable* var) {
	return var->frames + state->newestFrameOrdinal;
}

inline
DebugVariableFrame* GetCollationFrame(DebugState* state, DebugVariable* var) {
	return var->frames + state->collationFrameOrdinal;
}

inline
u32 SubtractFrameOrdinals(u32 newer, u32 older) {
	u32 result;
	if (newer > older) {
		result = newer - older;
	}
	else {
		result = MAX_COLLATION_FRAMES - older + newer;
	}
	return result;
}


inline
DebugStoredEvent* GetNewestEventSentinel(DebugVariable* var) {
	u32 index = var->newestEventFrameOrdinal;
	return &var->frames[index].eventSentinel;
}

inline
DebugStoredEvent* GetOldestEventSentinel(DebugVariable* var) {
	u32 index = var->oldestEventFrameOrdinal;
	return &var->frames[index].eventSentinel;
}

inline 
DebugStoredEvent* GetNewestEvent(DebugVariable* var) {
	DebugStoredEvent* sentinel = GetNewestEventSentinel(var);
	return sentinel->next;
}

inline
DebugStoredEvent* GetOldestEvent(DebugVariable* var) {
	DebugStoredEvent* sentinel = GetOldestEventSentinel(var);
	return sentinel->prev;
}

inline
String8 GetName(DebugParsedGUID& parsedGuid) {
	return { parsedGuid.GUID.str + parsedGuid.nameStart, parsedGuid.nameLength };
}

inline
String8 GetName(DebugVariable* var) {
	return GetName(var->guid);
}

inline
String8 GetName(DebugProfilerSpan* span) {
	return GetName(span->var->guid);
}

inline
void ExpandGroup(DebugVariableLink* link, bool expansion) {
	Assert(link->isGroup);
	GetNewestEvent(link->variable)->event.data_bool = expansion;
}

inline
bool IsExpanded(DebugVariableLink* link) {
	Assert(link->isGroup);
	return GetNewestEvent(link->variable)->event.data_bool;
}

inline
bool AreDebugIdsEqual(DebugId id1, DebugId id2) {
	bool result = id1.ptr == id2.ptr &&
		id1.index == id2.index;
	return result;
}

inline
bool IsVariableHot(DebugState* state, DebugVariableLink* link) {
	bool result = state->hotInteraction.tree.link == link;
	return result;
}

inline
bool IsVariableHot(DebugState* state, Rect2& rect2) {
	bool result = state->hotInteraction.mod_Rect2.actual == &rect2;
	return result;
}

inline
bool IsVariableHot(DebugState* state, DebugArenaView* view) {
	bool result = state->hotInteraction.arenaView == view;
	return result;
}

inline
bool SelectedByVar(DebugSelectedSpan& span) {
	return span.type == SpanSelection_ByVar;
}

inline
bool SelectedByEvent(DebugSelectedSpan& span) {
	return span.type == SpanSelection_ByEvent;
}

inline
bool IsVariableHot(DebugState* state, DebugSelectedSpan& selectedSpan) {
	if (state->hotInteraction.obj != DebugInteractionObject::ProfilerSpan) {
		return false;
	}
	DebugSelectedSpan& hotSelectedSpan = state->hotInteraction.selectedSpan;
	bool result = true;
	if (SelectedByEvent(hotSelectedSpan)) {
		result = hotSelectedSpan.byEvent == selectedSpan.byEvent;
	}
	else if (SelectedByVar(hotSelectedSpan)) {
		result = hotSelectedSpan.byVar == selectedSpan.byVar;
	}
	return result;
}

inline
bool IsProfiledEventHot(DebugState* state, DebugStoredEvent* event) {
	if (state->hotInteraction.obj != DebugInteractionObject::ProfilerSpan) {
		return false;
	}
	DebugSelectedSpan& hotSelectedSpan = state->hotInteraction.selectedSpan;
	if (SelectedByVar(hotSelectedSpan)) {
		return hotSelectedSpan.byVar == event->span.var;
	}
	if (SelectedByEvent(hotSelectedSpan)) {
		return hotSelectedSpan.byEvent == event;
	}
	return false;
}

inline
bool IsVariableHot(DebugState* state, DebugStoredEvent* event) {
	if (state->hotInteraction.obj != DebugInteractionObject::ProfilerSpan) {
		return false;
	}
	DebugSelectedSpan& hotSelectedSpan = state->hotInteraction.selectedSpan;
	bool eventsAreEqual = hotSelectedSpan.byEvent == event;
	bool varsAreEqual = hotSelectedSpan.byVar == event->span.var;
	if (SelectedByVar(hotSelectedSpan)) {
		return varsAreEqual;
	}
	else if (SelectedByEvent(hotSelectedSpan)) {
		return eventsAreEqual;
	}
	return eventsAreEqual || varsAreEqual;
}

inline
bool IsVariableHot(DebugState* state, DebugId id) {
	bool result = AreDebugIdsEqual(state->hotInteraction.id, id);
	return result;
}

inline
bool IsDebugIdNull(DebugId id) {
	bool result = id.ptr == 0 &&
		id.index == 0;
	return result;
}

inline
f32 DurationToMs(u64 durationCycles) {
	return 1000.f * f4(durationCycles) / DEBUG_CPU_FREQ;
}

inline
u64 GetSpanCyclesDuration(DebugProfilerSpan* span) {
	return span->cyclesEnd - span->cyclesStart;
}

inline
u64 GetEventCyclesDuration(DebugStoredEvent* event) {
	return GetSpanCyclesDuration(&event->span);
}

inline
f32 GetVariableFrameAvgDurationMs(DebugVariableFrame* frame) {
	return DurationToMs(frame->durationSum) / frame->eventHitSum;
}

inline
f32 GetSpanAvgDurationMs(DebugProfilerSpan* span) {
	return DurationToMs(GetSpanCyclesDuration(span)) / span->hitCount;
}

inline
f32 GetEventAvgDurationMs(DebugStoredEvent* event) {
	return GetSpanAvgDurationMs(&event->span);
}



inline
DebugId GetDebugIdForLink(DebugVariableLink* link) {
	DebugId id = {};
	id.ptr = link;
	return id;
}

inline
bool IsHighlighted(DebugState* state, DebugId did) {
	return AreDebugIdsEqual(state->hotInteraction.id, did);
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
DebugInteraction InteractionMoveTree(V2 mousePos, DebugTree* tree, V2 treeInitialPos) {
	DebugInteraction interaction = {};
	interaction.type = DebugInteractionType::MoveV2;
	interaction.startMousePos = mousePos;
	tree->pos = treeInitialPos;
	interaction.mod_V2.initial = tree->pos;
	interaction.mod_V2.actual = &tree->pos;
	return interaction;
}
inline
DebugInteraction InteractionMovedRect2(Rect2 bbox, Rect2* mover) {
	DebugInteraction interaction = {};
	interaction.startBoundingBox = bbox;
	interaction.obj = DebugInteractionObject::MovedRect2;
	interaction.mod_Rect2.initial = *mover;
	interaction.mod_Rect2.actual = mover;
	return interaction;
}

inline
DebugInteraction InteractionResizedRect2(Rect2 bbox, Rect2* resizable) {
	DebugInteraction interaction = {};
	interaction.startBoundingBox = bbox;
	interaction.obj = DebugInteractionObject::ResizedRect2;
	interaction.mod_Rect2.initial = *resizable;
	interaction.mod_Rect2.actual = resizable;
	return interaction;
}

inline
DebugInteraction InteractionDragIncrease(Rect2 bbox, f32* dragged, f32 amountPerPixel, DebugAxis axis) {
	DebugInteraction interaction = {};
	interaction.startBoundingBox = bbox;
	interaction.obj = DebugInteractionObject::Float;
	interaction.type = DebugInteractionType::DragIncrease;
	interaction.dragged_f32.initial = *dragged;
	interaction.dragged_f32.actual = dragged;
	interaction.dragged_f32.amountPerPixel = amountPerPixel;
	interaction.dragged_f32.axis = axis;
	return interaction;
}

inline 
DebugSelectedSpan BuildSelectedSpan(DebugVariable* var, DebugStoredEvent* event, DebugSpanSelectionType type, u32 frameOrdinal) {
	DebugSelectedSpan selected;
	selected.type = type;
	selected.byVar = var;
	selected.byEvent = event;
	selected.frameOrdinal = frameOrdinal;
	return selected;
}

inline
DebugInteraction InteractionProfilerSpan(Rect2 bbox, DebugSelectedSpan selectedSpan) {
	DebugInteraction interaction = {};
	interaction.startBoundingBox = bbox;
	interaction.obj = DebugInteractionObject::ProfilerSpan;
	interaction.selectedSpan = selectedSpan;
	return interaction;
}

inline
DebugInteraction InteractionProfilerSpan(DebugVariable* var, DebugStoredEvent* event, Rect2 bbox, DebugSpanSelectionType type, u32 frameOrdinal) {
	DebugProfilerSpan* newestSpan = &GetNewestEvent(var)->span;
	DebugSelectedSpan selected = BuildSelectedSpan(newestSpan->var, event, type, frameOrdinal);
	return InteractionProfilerSpan(bbox, selected);
}

inline
DebugInteraction InteractionArenaView(Rect2 bbox, DebugArenaView* arenaView) {
	DebugInteraction interaction = {};
	interaction.startBoundingBox = bbox;
	interaction.obj = DebugInteractionObject::ArenaView;
	interaction.arenaView = arenaView;
	return interaction;
}

inline
DebugInteraction InteractionIntrospectable(Rect2 bBox, DebugId did) {
	DebugInteraction interaction = {};
	interaction.obj = DebugInteractionObject::Introspectable;
	interaction.id = did;
	interaction.startBoundingBox = bBox;
	return interaction;
}

inline
DebugInteraction InteractionWithTree(Rect2 bBox, DebugTree* tree, DebugVariableLink* link) {
	DebugInteraction interaction = {};
	interaction.obj = DebugInteractionObject::Tree;
	interaction.tree.link = link;
	interaction.tree.tree = tree;
	interaction.startBoundingBox = bBox;
	Assert(link);
	return interaction;
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
		state->nextHotInteraction = InteractionIntrospectable(boundingBox, did);
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

inline 
bool DEBUG_DATA_BLOCK_REQUESTED(DebugId did) {
	DebugState* state = GetDebugState();
	if (!state) {
		return false;
	}
	bool result = IsSelected(state, did) || IsHighlighted(state, did);
	return result;
}

inline
u32 GetCollationFrameCount(DebugState* state) {
	u32 result = SubtractFrameOrdinals(state->newestFrameOrdinal, state->oldestFrameOrdinal);
	return result;
}

internal
DebugParsedGUID DebugParseGUID(const char* input) {
	DebugParsedGUID parsed;
	u16 totalLength = 0;
	parsed.fileStart = 0;
	parsed.fileLength = u2(FindCharacterInString(input, '|'));
	totalLength += parsed.fileLength + (parsed.fileLength != 0);
	parsed.lineStart = totalLength;
	parsed.lineLength = u2(FindCharacterInString(input + totalLength, '|'));
	totalLength += parsed.lineLength + (parsed.lineLength != 0);
	parsed.counterStart = totalLength;
	parsed.counterLength = u2(FindCharacterInString(input + totalLength, '|'));
	totalLength += parsed.counterLength + (parsed.counterLength != 0);
	parsed.nameStart = totalLength;
	parsed.nameLength = u2(StringLength(input + totalLength));
	totalLength += parsed.nameLength;

	parsed.GUID.length = totalLength;
	parsed.GUID.str = input;
	return parsed;
}

DebugParsedGUID DebugCopyGUID(MemoryArena& arena, DebugParsedGUID& src) {
	DebugParsedGUID result = src;
	result.GUID.str = PushString(arena, src.GUID.str, src.GUID.length);
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
void DebugRenderLine(DebugState* state, const char* text, V2 pos, f32 scale, V4 color, bool render = true, Rect2* boundingBox = 0, f32 Z = 0.f) {
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
			V3 anchor = ToV3(pos, Z);
			if (render) {
				PushBitmap(state->renderGroup, ScaledFlatTransform(scale * height), bid, anchor, color);
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
		Assert(*text != 0);
		if (*text != 0) {
			*boundingBox = resultBoundingBox;
		}
	}
}

inline
void DebugRenderLine(DebugState* state, const char* text, FontDrawContext& context, V4 color, bool render = true, Rect2* boundingBox = 0, f32 Z = 0.f) {
	DebugRenderLine(state, text, context.leftTopCurrent, context.scale, color, render, boundingBox, Z);
	if (render) {
		context.leftTopCurrent.E[1] -= context.lineAdvance;
	}
}

inline
void DebugRenderLineWithOutline(DebugState* state, const char* text, V2 pos, f32 scale, V4 textColor, V4 outlineColor, f32 Z = 0.f) {
	Rect2 bb = {};

	DebugRenderLine(state, text, pos, scale, outlineColor, false, &bb);
	PushRect(state->renderGroup, DefaultFlatTransform(), AddRadius(bb, V2{ 4.f, 4.f }), Z, outlineColor);
	DebugRenderLine(state, text, pos, scale, textColor, true, 0, Z);
}

inline
void DebugRenderLineWithOutline(DebugState* state, const char* text, FontDrawContext& context, V4 textColor, V4 outlineColor, f32 Z = 0.f) {
	Rect2 bb = {};

	DebugRenderLine(state, text, context, textColor, false, &bb);
	PushRect(state->renderGroup, DefaultFlatTransform(), AddRadius(bb, V2{ 4.f, 4.f }), Z, outlineColor);
	DebugRenderLine(state, text, context, textColor, true, 0, Z);
}

inline
Rect2 GetTextBoundingBox(DebugState* state, const char* text, FontDrawContext& context) {
	Rect2 boundingBox = {};
	DebugRenderLine(state, text, context.leftTopCurrent, context.scale, V4{0, 0, 0, 0}, false, &boundingBox);
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
	f32 scale = 0.12f;
	f32 lineAdvance = scale * GetFontLineAdvance(state->font);
	return InitializeFontDrawContext(state->font, scale, lineAdvance, topline);
}

internal
DebugVariableLink* AddVariableToGroup(DebugState* state, DebugVariableLink* parent, DebugVariable* var) {
	Assert(parent->isGroup);
	DebugVariableLink* link = PushStructSize(state->mainArena, DebugVariableLink);
	link->variable = var;
	link->parent = parent;
	link->nextInHash = 0;
	link->isGroup = false;
	link->next = parent->firstChild;
	parent->firstChild = link;
	return link;
}

inline
u32 GetStringHash(String8 string) {
	// TODO: Better hash function!
	internal u32 primes[] = {
		3, 5, 7, 11, 13, 17, 19, 23, 29, 31,37,	41,	43,	47,	53,	59,	61,
		67,	71, 73,	79,	83,	89,	97,	101,103,107,109,113,127,131,137,139,
		149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211,
		223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281,
		283, 293, 307, 311, 313, 317, 331, 337, 347, 349, 353, 359, 367,
		373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433, 439, 443,
		449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509, 521, 523, 541
	};
	u32 hash = 0;
	Assert(string.length < ArrayCount(primes))
		for (u32 idx = 0; idx < string.length; idx++) {
			hash += primes[idx] * (string.str[idx] - 'a');
			hash ^= 524287;
		}
	return hash;
}

inline
DebugVariable* _GetDebugVariable(DebugState* state, DebugParsedGUID& parsedGUID, u32 hashSlot) {
	DebugVariable* result = 0;
	for (DebugVariable* var = state->variableHash[hashSlot]; var; var = var->nextInHash) {
		if (StringsAreEqual(var->guid.GUID, parsedGUID.GUID)) {
			result = var;
			break;
		}
	}
	return result;
}

inline
DebugVariable* GetDebugVariable(DebugState* state, DebugParsedGUID& GUID) {
	u32 hashSlot = GetStringHash(GUID.GUID) % ArrayCount(state->variableHash);
	DebugVariable* result = _GetDebugVariable(state, GUID, hashSlot);
	return result;
}

internal
DebugVariable* GetOrCreateDebugVariable(DebugState* state, DebugVariableLink* group,
	DebugParsedGUID& guid, bool isIntrospectionGroup, bool timed) {
	u32 hashSlot = GetStringHash(guid.GUID) % ArrayCount(state->variableHash);
	DebugVariable* result = _GetDebugVariable(state, guid, hashSlot);
	if (!result) {
		result = PushStructSize(state->mainArena, DebugVariable);
		ZeroSize_(ptrcast(u8, result->frames), sizeof(result->frames));
		for (u32 frameIndex = 0; frameIndex < ArrayCount(result->frames); frameIndex++) {
			DebugVariableFrame* frame = result->frames + frameIndex;
			DebugStoredEvent* sentinel = &frame->eventSentinel;
			DLINKED_LIST_INIT(sentinel);
		}
		result->guid = DebugCopyGUID(state->mainArena, guid);
		result->nextInHash = state->variableHash[hashSlot];
		result->isIntrospectionGroup = isIntrospectionGroup;
		result->timed = timed;
		result->eventHitSum = 0;
		result->durationSum = 0;
		result->newestEventFrameOrdinal = state->collationFrameOrdinal;
		result->oldestEventFrameOrdinal = state->collationFrameOrdinal;
		state->variableHash[hashSlot] = result;
		if (group) {
			AddVariableToGroup(state, group, result);
		}
	}
	return result;
}

internal
DebugVariableLink* GetOrCreateVariableGroup(DebugState* state, DebugVariableLink* parentGroup, DebugParsedGUID& parsedGuid) {
	TIMED_FUNCTION;
	// TODO: Could I avoid hashing string?;
	u32 hashSlot = GetStringHash(parsedGuid.GUID) % ArrayCount(state->groupHash);
	DebugVariableLink* result = 0;
	for (DebugVariableLink* group = state->groupHash[hashSlot]; group; group = group->nextInHash) {
		DebugParsedGUID* candidate = &group->variable->guid;
		if (StringsAreEqual(parsedGuid.GUID, candidate->GUID)) {
			result = group;
			break;
		}
	}
	if (!result) {
		result = PushStructSize(state->mainArena, DebugVariableLink);
		result->isGroup = true;
		result->firstChild = 0;
		result->parent = parentGroup;
		result->next = parentGroup->firstChild;
		parentGroup->firstChild = result;
		result->nextInHash = state->groupHash[hashSlot];
		state->groupHash[hashSlot] = result;
		result->variable = GetOrCreateDebugVariableForGroup(state, result, parsedGuid);
	}
	return result;
}


internal
DebugTree* AddTree(DebugState* state, V2 pos, const char* name) {
	DebugTree* tree = PushStructSize(state->mainArena, DebugTree);
	tree->pos = pos;
	tree->rootGroup = {};
	tree->rootGroup.isGroup = true;
	DebugParsedGUID guid = {};
	guid.GUID.length = StringLength(name);
	guid.GUID.str = PushString(state->mainArena, name, guid.GUID.length);
	guid.nameLength = u2(guid.GUID.length);
	tree->rootGroup.variable = GetOrCreateDebugVariableForGroup(state, &tree->rootGroup, guid);
	DLINKED_LIST_ADD(&state->UISentinel, tree);
	return tree;
}

internal 
DebugState* DebugBegin(InputData& input, RenderCommandBuffer* renderCommands, u32 bitmapWidth, u32 bitmapHeight) {
	if (debugGlobalMemory->debugMemorySize == 0) {
		return 0;
	}
	Assert(debugGlobalMemory->debugMemorySize >= sizeof(DebugState));
	DebugState* state = ptrcast(DebugState, debugGlobalMemory->debugMemory);
	TransientState* tranState = ptrcast(TransientState, debugGlobalMemory->transientMemory);
	Assert(tranState->isInitialized);
	if (!state->isInitialized) {
#if 1
		DebugState* debugState = state;
		InitializeArena(
			debugState->mainArena,
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
#if 1
		SubArena(state->collationFrameArena, state->mainArena, MB(128));
#else
		SubArena(state->collationFrameArena, state->mainArena, MB(2));
#endif
		state->controller = &input.controllers[KB_CONTROLLER_IDX];
		state->renderGroup = BeginRendering(renderCommands, &tranState->assets);
		state->highPriorityQueue = tranState->highPriorityQueue;
		state->overlayBoundaries = GetRectFromCenterDim(V2{ 0, 0 }, V2i(bitmapWidth, bitmapHeight));
		
		state->cpuProfiler.view.rect = Rect2{
			V2{ state->overlayBoundaries.min + V2{ 30.f, 120.f } },
			V2{ state->overlayBoundaries.max.X - 30.f, state->overlayBoundaries.min.Y + 160.f }
		};
		state->cpuProfiler.view.offset = V2{ 0, 0 };
		state->cpuProfiler.view.projection = GetOrtographicProjection(bitmapWidth, bitmapHeight, 1);
		state->cpuProfiler.view.zoom = state->cpuProfiler.view.projection.camera.focalLength;
		state->rootCpuProfilerEvent = {};
		state->rootCpuProfilerEvent.GUID = DEBUG_NAME("[Whole Frame]");
		state->rootCpuProfilerEventGuid = DebugParseGUID(state->rootCpuProfilerEvent.GUID);
		state->frameVariable = GetOrCreateDebugVariable(state, 0, state->rootCpuProfilerEventGuid, false, true);

		state->memProfiler.view.rect = Rect2{
			V2{ state->cpuProfiler.view.rect.min.X, state->cpuProfiler.view.rect.max.Y + 30.f },
			V2{ state->cpuProfiler.view.rect.max + V2{0, 60.f + 30.f} }
		};
		state->memProfiler.view.offset = V2{ 0, 0 };
		state->memProfiler.view.projection = GetOrtographicProjection(bitmapWidth, bitmapHeight, 1);
		state->memProfiler.view.zoom = state->memProfiler.view.projection.camera.focalLength;

		state->cpuTimingsView.rect = Rect2{
			V2{ state->overlayBoundaries.min.X + 30,  state->overlayBoundaries.max.Y - 120.f },
			V2{ state->overlayBoundaries.max.X - 30.f, state->overlayBoundaries.max.Y - 20.f }
		};
		state->cpuTimingsView.offset = V2{ 0, 0 };
		state->cpuTimingsView.projection = GetOrtographicProjection(bitmapWidth, bitmapHeight, 1);
		state->cpuTimingsView.zoom = state->cpuTimingsView.projection.camera.focalLength;

		state->cpuTimingsHierarchyView.rect = Rect2{
			V2{ state->cpuProfiler.view.rect.min.X, state->cpuProfiler.view.rect.min.Y - 100.f },
			V2{ state->cpuProfiler.view.rect.max.X, state->cpuProfiler.view.rect.min.Y - 10.f }
		};
		state->cpuTimingsHierarchyView.offset = V2{ 0, 0 };
		state->cpuTimingsHierarchyView.projection = GetOrtographicProjection(bitmapWidth, bitmapHeight, 1);
		state->cpuTimingsHierarchyView.zoom = state->cpuTimingsHierarchyView.projection.camera.focalLength;

		state->threadStacks = PushArray(state->mainArena, MAX_DEBUG_THREADS, DebugThreadStack);
		DLINKED_LIST_INIT(&state->UISentinel);

		V2 leftTopCorner = V2{ state->overlayBoundaries.min.X, state->overlayBoundaries.max.Y };
		V2 rightTopCorner = V2{ state->overlayBoundaries.max.X - 400.f, state->overlayBoundaries.max.Y };
		AddTree(state, leftTopCorner, "Debugging");
		
		PlatformCpuInfo cpuInfo = Platform->SystemGetCpuInfo();
		u32 targetFrameRate = RoundF32ToU32(1.f / input.dtFrame);
		DEBUG_CPU_FREQ = cpuInfo.cpuHz;
		DEBUG_COLLATION_SCALE = f4(targetFrameRate) / DEBUG_CPU_FREQ;
		DEBUG_SPAN_MERGE_CYCLES_THRESHOLD = u4(0.001'000f * DEBUG_CPU_FREQ); // 100us

		state->isInitialized = true;
	}
	EndRendering(state->renderGroup);
	state->renderGroup = BeginRendering(renderCommands, &tranState->assets);
	state->overlayBoundaries = GetRectFromCenterDim(V2{ 0, 0 }, V2i(bitmapWidth, bitmapHeight));
	state->renderGroup.projection = GetOrtographicProjection(bitmapWidth, bitmapHeight, 1);
	state->renderGroup.projection.offset = V3{ 0, 0, 100 }; //NOTE: Put a bias for debug stuff for sorting purposes
	state->font = GetOrPrefetchFont(
		state->renderGroup, GetFontWithType(*state->renderGroup.assets, Font_Debug)
	);

	if (state->font) {
		f32 scale = 0.12f;
		f32 lineAdvance = scale * GetFontLineAdvance(state->font);
		state->fontContext = InitializeFontDrawContext(state->font, scale, -lineAdvance, state->overlayBoundaries.min + V2{ 0, lineAdvance });
		V2 leftUpCorner = V2{ state->overlayBoundaries.min.X, state->overlayBoundaries.max.Y };
	}
	debugGlobalState->swapEvent.GUID = 0;
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

inline
u32 AdvanceFrameOrdinal(u32 base, i32 advancer) {
	return (base + advancer) & (MAX_COLLATION_FRAMES - 1);
}

inline
u32 NextFrameOrdinal(u32 base) {
	return (base + 1) & (MAX_COLLATION_FRAMES - 1);
}

inline
u32 PrevFrameOrdinal(u32 base) {
	return (base - 1) & (MAX_COLLATION_FRAMES - 1);
}

internal
void FreeOldestFrame(DebugState* state) {
	TIMED_FUNCTION;
	for (u32 hashSlot = 0; hashSlot < ArrayCount(state->variableHash); hashSlot++) {
		for (DebugVariable* var = state->variableHash[hashSlot]; var; var = var->nextInHash) {
			DebugVariableFrame* frame = var->frames + state->oldestFrameOrdinal;
			DebugStoredEvent* sentinel = &frame->eventSentinel;
			if (DLINKED_LIST_IS_EMPTY(sentinel) || var->isIntrospectionGroup) {
				continue;
			}
			var->durationSum -= frame->durationSum;
			var->eventHitSum -= frame->eventHitSum;
			bool eventFound = false;
			for (u32 frameOrdinal = NextFrameOrdinal(state->oldestFrameOrdinal); frameOrdinal != state->newestFrameOrdinal; frameOrdinal = NextFrameOrdinal(frameOrdinal)) {
				DebugVariableFrame* varframe = var->frames + frameOrdinal;
				if (varframe->eventSentinel.next != &varframe->eventSentinel) {
					var->oldestEventFrameOrdinal = frameOrdinal;
					eventFound = true;
					break;
				}
			}
			if (!eventFound) {
				var->oldestEventFrameOrdinal = var->newestEventFrameOrdinal;
			}

			sentinel->prev->next = state->freeStoredEventList;
			state->freeStoredEventList = sentinel->next;
			sentinel->next->prev = 0;
			*frame = {};
			DLINKED_LIST_INIT(sentinel);
		}
	}
	state->oldestFrameOrdinal = NextFrameOrdinal(state->oldestFrameOrdinal);
}

internal
DebugStoredEvent* AllocateEvent(DebugState* state) {
	DebugStoredEvent* storedEvent = 0;
	while (!storedEvent) {
		storedEvent = state->freeStoredEventList;
		if (storedEvent) {
			state->freeStoredEventList = state->freeStoredEventList->next;
		}
		else if (HasArenaSpaceFor(state->collationFrameArena, sizeof(DebugStoredEvent))) {
			storedEvent = PushStructSize(state->collationFrameArena, DebugStoredEvent);
		}
		else if (GetCollationFrameCount(state) > 1) {
			FreeOldestFrame(state);
		}
		else {
			return 0;
		}
	}
	storedEvent->captureFrameIndex = state->totalFrameCount;
	state->allocEventsSum++;
	return storedEvent;
}

inline
DebugStoredEvent* _StoreEvent(DebugState* state, DebugVariable* var) {
	DebugStoredEvent* storedEvent = AllocateEvent(state);
	DebugStoredEvent* sentinel = &GetCollationFrame(state, var)->eventSentinel;
	DLINKED_LIST_ADD(sentinel, storedEvent);
	var->newestEventFrameOrdinal = state->collationFrameOrdinal;
	return storedEvent;
}

inline
DebugStoredEvent* StoreEvent(DebugState* state, DebugVariableLink* group, DebugParsedGUID& guid, bool isIntrospectionGroup, bool timed) {
	DebugVariable* var = GetOrCreateDebugVariable(state, group, guid, isIntrospectionGroup, timed);
	DebugStoredEvent* result = _StoreEvent(state, var);
	return result;
}

inline
DebugStoredEvent* _StoreTimedEvent(DebugState* state, DebugVariable* var, DebugVariableLink* group, DebugParsedGUID& guid, bool isIntrospectionGroup, u64 startCycles, u64 endCycles, u8 thread, u32 hitCount) {
	DebugVariableFrame* frame = GetCollationFrame(state, var);
	DebugStoredEvent* result = _StoreEvent(state, var);
	
	u64 duration = endCycles - startCycles;
	var->eventHitSum += hitCount;
	var->durationSum += duration;

	frame->eventHitSum += hitCount;
	frame->durationSum += duration;
	
	result->span.cyclesStart = startCycles;
	result->span.cyclesEnd = endCycles;
	result->span.var = var;
	result->span.sibling = 0;
	result->span.firstChild = 0;
	result->span.thread = thread;
	result->span.hitCount = hitCount;
	return result;
}

inline
DebugStoredEvent* StoreTimedEvent(DebugState* state, DebugVariableLink* group, DebugParsedGUID& guid, u64 startCycles, u64 endCycles, u8 thread) {
	DebugVariable* var = GetOrCreateDebugVariable(state, group, guid, false, true);
	DebugStoredEvent* result = _StoreTimedEvent(state, var, group, guid, false, startCycles, endCycles, thread, 1);
	return result;
}

internal
DebugVariable* GetOrCreateDebugVariableForGroup(DebugState* state, DebugVariableLink* group, DebugParsedGUID& guid) {
	DebugVariable* var = GetOrCreateDebugVariable(state, group, guid, true, false);
	DebugStoredEvent* stored = _StoreEvent(state, var);
	stored->event.GUID = guid.GUID.str;
	stored->event.type = Event_Data_bool;
	stored->event.data_bool = false;
	return var;
}

inline
bool SpansCouldBeMerged(DebugProfilerSpan* old, DebugProfilerSpan* _new) {
	return old->var == _new->var &&
		old->cyclesEnd + DEBUG_SPAN_MERGE_CYCLES_THRESHOLD > _new->cyclesStart;
}

internal DebugProfilerSpan* TryMergeChildren(DebugState* state, DebugProfilerSpan* oldParent, DebugProfilerSpan* newParent);
internal
void _MergeEvents(DebugState* state, DebugStoredEvent* oldMergedEvent, DebugStoredEvent* newMergedEvent) {
	Assert(oldMergedEvent->span.var == newMergedEvent->span.var); // Parents have to be the same var to make any sense
	Assert(oldMergedEvent->span.thread == newMergedEvent->span.thread);
	Assert(oldMergedEvent != newMergedEvent);
	Assert(&oldMergedEvent->span != &newMergedEvent->span);

	oldMergedEvent->span.hitCount += newMergedEvent->span.hitCount;
	oldMergedEvent->span.cyclesEnd = newMergedEvent->span.cyclesEnd;
	Assert(newMergedEvent->span.sibling == 0);
	TryMergeChildren(state, &oldMergedEvent->span, &newMergedEvent->span);

	DLINKED_LIST_REMOVE(newMergedEvent);
	newMergedEvent->next = state->freeStoredEventList;
	newMergedEvent->prev = 0;
	state->freeStoredEventList = newMergedEvent;
}

internal
DebugProfilerSpan* TryMergeChildren(DebugState* state, DebugProfilerSpan* oldParent, DebugProfilerSpan* newParent) {
	//NOTE In case of successful merge newParent is removed from linked list 
	// in parent call (TryMergeSibling or other TryMergeChildren)
	Assert(oldParent->var == newParent->var); // Parents have to be the same var to make any sense
	Assert(oldParent->thread == newParent->thread);
	if (!newParent->firstChild) {
		return 0;
	}
	if (!oldParent->firstChild) {
		oldParent->firstChild = newParent->firstChild;
		return 0;
	}
	DebugStoredEvent* lastNewChild = newParent->firstChild;
	DebugStoredEvent* secondLastNewChild = 0;
	while (lastNewChild->span.sibling) {
		secondLastNewChild = lastNewChild;
		lastNewChild = lastNewChild->span.sibling;
	}
	DebugStoredEvent* oldMergedEvent = oldParent->firstChild;
	DebugStoredEvent* newMergedEvent = lastNewChild;
	DebugProfilerSpan* oldMergedSpan = &oldMergedEvent->span;
	DebugProfilerSpan* newMergedSpan = &newMergedEvent->span;
	

	if (SpansCouldBeMerged(oldMergedSpan, newMergedSpan)) {
		if (secondLastNewChild) {
			secondLastNewChild->span.sibling = oldMergedEvent;
		}
		_MergeEvents(state, oldMergedEvent, newMergedEvent);
		return oldMergedSpan;
	}
	newMergedSpan->sibling = oldMergedEvent;
	return newMergedSpan;
}

internal
DebugProfilerSpan* TryMergeSibling(DebugState* state, DebugVariable* var, DebugStoredEvent* newMergedEvent, OpenDebugEvent* parentBlock) {
	DebugProfilerSpan* newMergedSpan = &newMergedEvent->span;
	DebugStoredEvent* oldMergedEvent = parentBlock->firstChild;
	if (oldMergedEvent) {
		DebugProfilerSpan* oldMergedSpan = &oldMergedEvent->span;
		Assert(oldMergedSpan->thread == newMergedSpan->thread);
		if (SpansCouldBeMerged(oldMergedSpan, newMergedSpan)) {
			_MergeEvents(state, oldMergedEvent, newMergedEvent);
			return oldMergedSpan;
		}
	}
	newMergedSpan->sibling = parentBlock->firstChild;
	parentBlock->firstChild = newMergedEvent;
	return newMergedSpan;
}

inline
DebugStoredEvent* StoreEventCopy(DebugState* state, DebugVariableLink* group, DebugEvent* event, DebugParsedGUID& guid) {
	DebugStoredEvent* result = StoreEvent(state, group, guid, false, false);
	result->event = *event;
	return result;
}

internal
void DebugCollateEvents(DebugState* state) {
	TIMED_FUNCTION;
	if (PROFILER_PAUSE) {
		return;
	}

	if (NextFrameOrdinal(state->newestFrameOrdinal) == state->oldestFrameOrdinal) {
		FreeOldestFrame(state);
	}
	
	u32 tableIndex = !debugGlobalState->currentFrameIndex;
	DebugEvent* eventsInFrame = debugGlobalState->events[tableIndex];
	u32 eventsInFrameCount = debugGlobalState->eventsCount[tableIndex];
	u64 frameStartCycles = debugGlobalState->frameStartCycles[tableIndex];
	u64 frameEndCycles = debugGlobalState->frameEndCycles[tableIndex];

	DebugStoredEvent* rootTimeEvent = StoreTimedEvent(
		state, 0, state->rootCpuProfilerEventGuid, 
		frameStartCycles, frameEndCycles, 0
	);
	for (u32 eventIndex = 0; eventIndex < eventsInFrameCount; eventIndex++) {
		DebugEvent* event = eventsInFrame + eventIndex;
		DebugThreadStack* stack = GetDebugStackForThread(state, event->threadId);
		DebugParsedGUID parsedGuid = DebugParseGUID(event->GUID);
		switch (event->type) {
		case Event_Time_BlockBegin: {
			OpenDebugEvent* block = PushToEventStack(state, &stack->timeEvents, event);
			block->parsedGuid = parsedGuid;
		} break;
		case Event_Time_BlockEnd: {
			OpenDebugEvent* block = stack->timeEvents;
			OpenDebugEvent* parentBlock = block->next;
			DebugEvent* openEvent = &block->event;
			Assert(openEvent);
			Assert(openEvent->threadId == event->threadId);
			Assert(!parentBlock || parentBlock->event.threadId == event->threadId);

			DebugVariable* var = GetOrCreateDebugVariable(state, 0, block->parsedGuid, false, true);
			DebugStoredEvent* storedEvent = _StoreTimedEvent(
				state, var, 0, block->parsedGuid, false,
				openEvent->cycles, event->cycles, stack->laneId, openEvent->hitCount
			);
			DebugProfilerSpan* span = &storedEvent->span;
			span->firstChild = block->firstChild;

			if (parentBlock) {
#if 1
				TryMergeSibling(state, var, storedEvent, parentBlock);
#else
				span->sibling = parentBlock->firstChild;
				parentBlock->firstChild = storedEvent;
#endif
			}
			else {
				span->sibling = rootTimeEvent->span.firstChild;
				rootTimeEvent->span.firstChild = storedEvent;
			}
			for (DebugStoredEvent* child = span->firstChild; child; child = child->span.sibling) {
				Assert(child->span.thread == span->thread);
			}
			PopFromEventStack(state, &stack->timeEvents);
		} break;
		case Event_Data_BlockBegin: {
			DebugVariableLink* parent = stack->dataEvents ? 
				stack->dataEvents->group : 
				&state->UISentinel.next->rootGroup;
			OpenDebugEvent* block = PushToEventStack(state, &stack->dataEvents, event);
			block->group = GetOrCreateVariableGroup(state, parent, parsedGuid);
		} break;
		case Event_Data_BlockEnd: {
			PopFromEventStack(state, &stack->dataEvents);
		} break;
#if 0
		case Event_MemoryArenaInitialize: {
			DebugArenaView* newArenaView = PushStructSize(state->mainArena, DebugArenaView);
			newArenaView->event = StoreEvent(state, 0, event, parsedGuid, newFrame->frameIndex, true);
			newArenaView->firstChild = 0;
			newArenaView->next = 0;
			newArenaView->name = "TODO";//TODO TO BE RESOLVED event->blockName;
			newArenaView->GUID = event->GUID;
			MemoryArenaSnapshot* snapshot = &event->data_MemoryArenaSnapshot;
			MemoryArena* parentArena = snapshot->parent;
			if (parentArena) {
				DebugArenaView* view = state->arenaViews;
				DebugArenaView* parentStack[16] = {};
				u32 stackDepth = 0;
				while (view) {
					if (view->event->event.data_MemoryArenaSnapshot.arena.data == parentArena->data) {
						newArenaView->next = view->firstChild;
						view->firstChild = newArenaView;
						DebugParsedGUID guidTODO = DebugParseGUID(view->event->event.GUID);
						DebugVariable* var = GetDebugVariable(state, guidTODO);
						GetNewestEvent(var)->event.data_MemoryArenaSnapshot.arena = *parentArena;
						break;
					}
					if (view->firstChild) {
						Assert(stackDepth < ArrayCount(parentStack) - 1);
						parentStack[++stackDepth] = view;
						view = view->firstChild;
					}
					else {
						view = view->next;
					}
					while (!view && stackDepth > 0) {
						view = parentStack[stackDepth--];
						if (view) {
							view = view->next;
						}
					}
				}
			}
			else {
				newArenaView->next = state->arenaViews;
				state->arenaViews = newArenaView;
			}
		} break;
		case Event_MemoryArenaUpdate: {
			DebugVariable* var = GetDebugVariable(state, parsedGuid);
			GetNewestEvent(var)->event = *event;

			// TODO: This is nasty, cause StoreEvent actually allocates stuff on the arena, so it causes
			// exponential growth of events incoming to the debug system until the memory is drained
			// StoreEvent(state, 0, event, newFrame->frameIndex);
		} break;
#else
		case Event_MemoryArenaInitialize:
		case Event_MemoryArenaUpdate:
			break;
#endif
		default: {
			StoreEventCopy(state, stack->dataEvents->group, event, parsedGuid);
		} break;
		}
	}

	state->newestFrameOrdinal = state->collationFrameOrdinal;
	state->collationFrameOrdinal = NextFrameOrdinal(state->collationFrameOrdinal);
}

enum DebugVarToTextFlags {
	DebugVarToText_ConfigPrefix = 0x1,
	DebugVarToText_AddFloatSuffix = 0x2,
	DebugVarToText_AddNewLine = 0x4,
	DebugVarToText_AddColon = 0x8,
};

u64 DebugVariableToText(DebugVariable* variable, char* buffer, u64 size, u32 flags) {
	char* at = buffer;
	char* end = buffer + size;
	DebugStoredEvent* sentinel = GetNewestEventSentinel(variable);
	DebugStoredEvent* newestEvent = sentinel->next;
	if (newestEvent != sentinel) {
		DebugEvent* event = &newestEvent->event;
		if (flags & DebugVarToText_ConfigPrefix) {
			at += sprintf_s(at, end - at, "#define CONSTANT_");
		}
		const char* colon = (flags & DebugVarToText_AddColon) ? ":" : "";
		String8 variableName = GetName(variable);
		switch (event->type) {
		case Event_Data_bool: {
			at += sprintf_s(at, end - at, "%.*s%s %d", variableName.length, variableName.str, colon, event->data_bool);
		} break;
		case Event_Data_i32: {
			at += sprintf_s(at, end - at, "%.*s%s %d", variableName.length, variableName.str, colon, event->data_i32);
		} break;
		case Event_Data_u32: {
			at += sprintf_s(at, end - at, "%.*s%s %d", variableName.length, variableName.str, colon, event->data_u32);
		} break;
		case Event_Data_f32: {
			at += sprintf_s(at, end - at, "%.*s%s %f", variableName.length, variableName.str, colon, event->data_f32);
			if (flags & DebugVarToText_AddFloatSuffix && (end - at) > 0) {
				*at++ = 'f';
			}
		} break;
		case Event_Data_V2: {
			at += sprintf_s(at, end - at, "%.*s%s {%f, %f}", variableName.length, variableName.str, colon,
				event->data_V2.X, event->data_V2.Y);
		} break;
		case Event_Data_V3: {
			at += sprintf_s(at, end - at, "%.*s%s {%f, %f, %f}", variableName.length, variableName.str, colon,
				event->data_V3.X, event->data_V3.Y, event->data_V3.Z);
		} break;
		case Event_Data_V4: {
			at += sprintf_s(at, end - at, "%.*s%s {%f, %f, %f, %f}", variableName.length, variableName.str, colon,
				event->data_V4.X, event->data_V4.Y, event->data_V4.Z, event->data_V4.W);
		} break;
		case Event_Data_BlockBegin: {
			at += sprintf_s(at, end - at, "%.*s%s", variableName.length, variableName.str, colon);
		} break;
		default: {
			at += sprintf_s(at, end - at, "Unknown: %.*s", variableName.length, variableName.str);
		} break;
		}
		if (flags & DebugVarToText_AddNewLine && (end - at) > 0) {
			*at++ = '\n';
		}
	}
	return at - buffer;
}

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
	rectangle = IntersectionInWidth(rectangle, boundaries);
	return rectangle;
}

inline
void AdvanceScroll(DebugScroll& scroll, i32 ticks) {
	if (scroll.range > 0.f) {
		scroll.value += ticks * scroll.distancePerTick / scroll.range;
		scroll.value = Clip01(scroll.value);
	}
}

inline
f32 GetScrollValue(DebugScroll& scroll) {
	f32 value = scroll.value * scroll.range + scroll.min;
	return value;
}

inline
void ChangeScrollRange(DebugScroll& scroll, f32 newRange) {
	if (newRange > 0.f) {
		scroll.value = scroll.value * SafeRatio(scroll.range, newRange);
		scroll.range = newRange;
	}
}

inline
f32 GetScrollSizeForContainer(DebugScroll& scroll, f32 container) {
	f32 size = Squared(container) / (scroll.range + 1.f);
	return size;
}

inline
f32 GetScrollValueForContainer(DebugScroll& scroll, f32 container) {
	f32 size = GetScrollSizeForContainer(scroll, container);
	f32 value = scroll.value * (container - size) + 0.5f * size;
	return value;
}

inline
void RenderScroll(DebugState* state, V2 center, V2 size, V2 mousePos, f32* data, DebugAxis axis, f32 amountPerPixel) {
	Rect2 scrollAnchor = GetRectFromCenterDim(center, size);
	V4 itemColor = V4{ 1, 1, 1, 1 };
	if (IsInRectangle(scrollAnchor, mousePos)) {
		itemColor = V4{ 0.5f, 0.5f, 0, 1 };
		state->nextHotInteraction = InteractionDragIncrease(scrollAnchor, data, amountPerPixel, axis);
	}
	PushRect(state->renderGroup, DefaultFlatTransform(), scrollAnchor, 0, itemColor);
}

inline
void RenderResizeAnchor(DebugState* state, V2 center, V2 size, V2 mousePos, Rect2* data) {
	Rect2 resizeAnchor = GetRectFromCenterDim(center, size);
	V4 itemColor = V4{ 1, 1, 1, 1 };
	if (IsInRectangle(resizeAnchor, mousePos)) {
		itemColor = V4{ 0.5f, 0.5f, 0, 1 };
		state->nextHotInteraction = InteractionResizedRect2(resizeAnchor, data);
	}
	PushRect(state->renderGroup, DefaultFlatTransform(), resizeAnchor, 0, itemColor);
}

inline
bool IsVariableTimed(DebugVariable* var) { return var->timed; }


inline
void GetVarMetricsByText(DebugState* state, DebugVariable* var, char* dst, size_t dstSize, u32 rank, DebugStoredEvent* event) {
	String8 name = GetName(var);

	f32 sumTimingMs, avgTimingMs;
	u64 callsCount, avgCycles;
	if (event) {
		u64 totalCycles = GetEventCyclesDuration(event);
		sumTimingMs = DurationToMs(totalCycles);
		avgTimingMs = sumTimingMs / event->span.hitCount;
		avgCycles = totalCycles / event->span.hitCount;
		callsCount = event->span.hitCount;
	}
	else {
		DebugVariableFrame* frame = GetNewestFrame(state, var);
		sumTimingMs = DurationToMs(frame->durationSum);
		avgTimingMs = sumTimingMs / frame->eventHitSum;
		avgCycles = frame->durationSum / frame->eventHitSum;
		callsCount = frame->eventHitSum;
	}
	
	
	if (rank > 0) {
		sprintf_s(dst, dstSize, "%-2d. %-30.*s:  AVGMS(%8.2fms)   AVGCYC(%8lld)   SUM ALL THREADS(%8.2fms)   COUNT PER FRAME(%5lld)",
			rank, name.length, name.str, avgTimingMs, avgCycles, sumTimingMs, callsCount);
	}
	else {
		sprintf_s(dst, dstSize, "###[%-30.*s:  AVGMS(%8.2fms)   AVGCYC(%8lld)   SUM ALL THREADS(%8.2fms)   COUNT PER FRAME(%5lld)",
			name.length, name.str, avgTimingMs, avgCycles, sumTimingMs, callsCount);
	}
}

internal
void DebugRenderCpuProfilerTimings(DebugState* state, Controller& controller, V2 mousePos) {
	if (!DEBUG_Profiler_CpuShowMostExpensiveFunctions) {
		return;
	}
	TIMED_FUNCTION;
	DebugVirtualView& view = state->cpuTimingsView;
	bool isHot = IsInRectangle(view.rect, mousePos);
	if (isHot) {
		state->nextHotInteraction = InteractionMovedRect2(view.rect, &view.rect);
		view.offset += V2{ 0.f, -state->controller->mouseWheelTicks * 30.f };
	}
	V4 backgroundColor = V4{ 0.03f, 0.03f, 0.03f, 0.75f };
	PushRect(state->renderGroup, DefaultFlatTransform(), view.rect, -1.f, backgroundColor);
	V2 viewDim = GetDim(view.rect);
	TemporaryMemory tempMemory = BeginTempMemory(state->mainArena);
	u32 maxElements = 1024;
	DebugVariable** variables = PushArray(state->mainArena, maxElements, DebugVariable*);
	SortElement* sortElements = PushArray(state->mainArena, maxElements, SortElement);
	SortElement* tmpBuffer = PushArray(state->mainArena, maxElements, SortElement);
	u32 elementCount = 0;
	DebugVariable** variablesIt = variables;
	for (u32 hashSlot = 0; hashSlot < ArrayCount(state->variableHash); hashSlot++) {
		for (DebugVariable* var = state->variableHash[hashSlot]; var; var = var->nextInHash) {
			if (var->eventHitSum == 0 || !IsVariableTimed(var)) { continue; }
			Assert(elementCount < (maxElements - 1));
			DebugVariableFrame* frame = GetNewestFrame(state, var);
			SortElement* sortElement = sortElements + elementCount;
			sortElement->key = -GetVariableFrameAvgDurationMs(frame);
			sortElement->offset = elementCount++;
			*variablesIt++ = var;
		}
	}
	RadixSort(sortElements, elementCount, tmpBuffer);

	f32 currentHeight = view.rect.max.Y;
	f32 currentWidth = view.rect.min.X + 10.f;
	FontDrawContext fontContext = InitializeFontDrawContext(state->font, state->fontContext.scale, -state->fontContext.lineAdvance, V2{ currentWidth, currentHeight });
	u32 currentSortIndex = u4(Maximum(0.f, view.offset.Y / fontContext.lineAdvance));
	f32 maxHeight = Maximum((elementCount + 1) * fontContext.lineAdvance, viewDim.Y);
	view.offset.Y = Clip(view.offset.Y, 0, maxHeight - viewDim.Y);

	char buffer[256];
	while (currentSortIndex < elementCount) {
		SortElement* sortElement = sortElements + currentSortIndex;
		DebugVariable* var = variables[sortElement->offset];
		if (fontContext.leftTopCurrent.Y > view.rect.min.Y) {
			V4 color = V4{ 1, 1, 1, 1 };
			GetVarMetricsByText(state, var, buffer, ArrayCount(buffer), currentSortIndex + 1, 0);
			Rect2 bb = GetTextBoundingBox(state, buffer, fontContext);
			if (IsInRectangle(bb, mousePos)) {
				state->nextHotInteraction = InteractionProfilerSpan(var, 0, bb, SpanSelection_ByVar, var->newestEventFrameOrdinal);
				DebugRenderLineWithOutline(state, buffer, fontContext, color, V4{ 0, 0, 0, 1 }, 1.f);
			}
			else {
				DebugRenderLine(state, buffer, fontContext, color);
			}
		}
		currentSortIndex++;
	}
	V2 resizeCenter = view.rect.max;
	V2 resizeSize = V2{ 8, 8 };
	RenderResizeAnchor(state, resizeCenter, resizeSize, mousePos, &view.rect);

	V2 scrollSize = V2{ 8.f, Squared(viewDim.Y) / maxHeight };
	V2 scrollCenter = V2{ view.rect.min.X, view.rect.max.Y - view.offset.Y / maxHeight * viewDim.Y - 0.5f * scrollSize.Y };
	RenderScroll(state, scrollCenter, scrollSize, mousePos, &view.offset.Y, Axis_Y, -maxHeight / viewDim.Y);

	EndTempMemory(tempMemory);
}

internal
void DebugRenderCpuProfilerTimingsHierarchy(DebugState* state, Controller& controller, V2 mousePos) {
	TIMED_FUNCTION;
	if (!DEBUG_Profiler_Cpu) {
		return;
	}
	DebugVirtualView& view = state->cpuTimingsHierarchyView;
	bool isHot = IsInRectangle(view.rect, mousePos);
	if (isHot) {
		state->nextHotInteraction = InteractionMovedRect2(view.rect, &view.rect);
		view.offset += V2{ 0.f, -state->controller->mouseWheelTicks * 30.f };
	}
	V4 backgroundColor = V4{ 0.03f, 0.03f, 0.03f, 0.75f };
	PushRect(state->renderGroup, DefaultFlatTransform(), view.rect, -1.f, backgroundColor);



	V2 viewDim = GetDim(view.rect);
	TemporaryMemory tempMemory = BeginTempMemory(state->mainArena);
	u32 maxElements = 2048;
	DebugVariable** variables = PushArray(state->mainArena, maxElements, DebugVariable*);
	DebugStoredEvent* events = PushArray(state->mainArena, maxElements, DebugStoredEvent);
	SortElement* sortElements = PushArray(state->mainArena, maxElements, SortElement);
	SortElement* tmpBuffer = PushArray(state->mainArena, maxElements, SortElement);
	u32 elementCount = 0;
	DebugVariable** variablesIt = variables;
	DebugStoredEvent* eventsIt = events;

	DebugSelectedSpan& selectedSpan = state->cpuProfiler.selectedSpans[state->cpuProfiler.selectedSpanCount];
	DebugVariable* parentVar = 0;
	DebugStoredEvent* parentEvent = 0;
	u32 selectedEventFrameOrdinal = state->newestFrameOrdinal;
	if (SelectedByEvent(selectedSpan)) {
		DebugProfilerSpan* rootSpan = &selectedSpan.byEvent->span;
		parentVar = rootSpan->var;
		parentEvent = selectedSpan.byEvent;
		selectedEventFrameOrdinal = selectedSpan.frameOrdinal;
		for (DebugStoredEvent* child = rootSpan->firstChild; child; child = child->span.sibling) {
			if (elementCount >= (maxElements - 1)) {
				break;
			}

			SortElement* sortElement = sortElements + elementCount;
			sortElement->key = -GetEventAvgDurationMs(child);
			sortElement->offset = elementCount++;
			*variablesIt++ = child->span.var;

			eventsIt->span.var = child->span.var;
			eventsIt->span.hitCount = child->span.hitCount;
			eventsIt->span.cyclesStart = child->span.cyclesStart;
			eventsIt->span.cyclesEnd = child->span.cyclesEnd;
			eventsIt->next = child;
			eventsIt++;
		}
	}
	else {
		DebugVariable* var = SelectedByVar(selectedSpan) ? 
			selectedSpan.byVar : 
			GetDebugVariable(state, state->rootCpuProfilerEventGuid);
		parentVar = var;
		u32 collectionIter = 0;
		u32 MAX_COLLECTION_ITERS = 20; //NOTE: For performance reasons just check last 20 events for children collection
		DebugVariableFrame* frame = GetNewestFrame(state, var);
		DebugStoredEvent* sentinel = &frame->eventSentinel;
		for (DebugStoredEvent* event = sentinel->next; event != sentinel; event = event->next) {
			for (DebugStoredEvent* child = event->span.firstChild; child; child = child->span.sibling) {
				DebugVariable* childVar = child->span.var;
				bool found = false;
				for (u32 existingVarIdx = 0; existingVarIdx < elementCount; existingVarIdx++) {
					DebugVariable* other = variables[existingVarIdx];
					if (childVar == other) {
						found = true;
						break;
					}
				}
				if (found) {
					continue;
				}
				DebugVariableFrame* childFrame = GetNewestFrame(state, childVar);
				Assert(elementCount < (maxElements - 1));
				SortElement* sortElement = sortElements + elementCount;
				sortElement->key = -GetVariableFrameAvgDurationMs(childFrame);
				sortElement->offset = elementCount++;
				*variablesIt++ = childVar;
			}
			collectionIter++;
			if (collectionIter >= MAX_COLLECTION_ITERS) {
				break;
			}
		}
		PRINT_DEBUGGING("Iter count: %d", collectionIter);
		PRINT_DEBUGGING("Iter count: %d", collectionIter);
	}
	RadixSort(sortElements, elementCount, tmpBuffer);

	f32 currentHeight = view.rect.max.Y;
	f32 currentWidth = view.rect.min.X + 10.f;
	FontDrawContext fontContext = InitializeFontDrawContext(state->font, state->fontContext.scale, -state->fontContext.lineAdvance, V2{ currentWidth, currentHeight });
	u32 currentSortIndex = u4(Maximum(0.f, view.offset.Y / fontContext.lineAdvance));
	f32 maxHeight = Maximum((elementCount + 1) * fontContext.lineAdvance, viewDim.Y);
	view.offset.Y = Clip(view.offset.Y, 0, maxHeight - viewDim.Y);

	char buffer[256];
	GetVarMetricsByText(state, parentVar, buffer, ArrayCount(buffer), 0, parentEvent);
	DebugRenderLine(state, buffer, fontContext, V4{ 1, 1, 1, 1 });

	u32 rank = currentSortIndex + 1;
	while (currentSortIndex < elementCount) {
		SortElement* sortElement = sortElements + currentSortIndex;
		DebugVariable* var = variables[sortElement->offset];
		f32 timingMs = -sortElement->key;
		if (fontContext.leftTopCurrent.Y > view.rect.min.Y) {
			V4 color = V4{ 1, 1, 1, 1 };
			DebugStoredEvent* aggregatedEvent = parentEvent ? events + sortElement->offset : 0;
			GetVarMetricsByText(state, var, buffer, ArrayCount(buffer), rank, aggregatedEvent);
			Rect2 spanRect = GetTextBoundingBox(state, buffer, fontContext);
			if (IsInRectangle(spanRect, mousePos)) {
				DebugStoredEvent* originalEvent = aggregatedEvent ? aggregatedEvent->next : 0;
				u32 frameOrdinal = aggregatedEvent ? selectedEventFrameOrdinal : var->newestEventFrameOrdinal;
				DebugSpanSelectionType interactionType = originalEvent ? SpanSelection_ByEvent : SpanSelection_ByVar;
				state->nextHotInteraction = InteractionProfilerSpan(var, originalEvent, spanRect, interactionType, frameOrdinal);
				DebugRenderLineWithOutline(state, buffer, fontContext, color, V4{ 0, 0, 0, 1 }, 1.f);
			}
			else {
				DebugRenderLine(state, buffer, fontContext, color);
			}
			rank++;
		}
		currentSortIndex++;
	}
	V2 resizeCenter = view.rect.max;
	V2 resizeSize = V2{ 8, 8 };
	RenderResizeAnchor(state, resizeCenter, resizeSize, mousePos, &view.rect);

	V2 scrollSize = V2{ 8.f, Squared(viewDim.Y) / maxHeight };
	V2 scrollCenter = V2{ view.rect.min.X, view.rect.max.Y - view.offset.Y / maxHeight * viewDim.Y - 0.5f * scrollSize.Y };
	RenderScroll(state, scrollCenter, scrollSize, mousePos, &view.offset.Y, Axis_Y, -maxHeight / viewDim.Y);

	EndTempMemory(tempMemory);
}


internal
void DebugRenderCpuProfiler(DebugState* state, Controller& controller, V2 mousePos) {
	TIMED_FUNCTION;
	if (!DEBUG_Profiler_Cpu) {
		return;
	}
	DebugVirtualView& view = state->cpuProfiler.view;
	bool isHot = IsInRectangle(view.rect, mousePos);
	if (isHot) {
		state->nextHotInteraction = InteractionMovedRect2(view.rect, &view.rect);
	}
	V2 viewDim = GetDim(view.rect);
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

		V4{0.5, 1, 0.5, 1},
		V4{0.5, 0.5, 1, 1},
		V4{0.5, 1, 1, 1},
		V4{1, 0.5, 1, 1},
		V4{1, 1, 0.5, 1},

		V4{1, 0.5f, 0.5f, 1},
		V4{0.5f, 0.5f, 0.5f, 1},
	};
	f32 frameWidth = f4(state->threadStacksCount) * threadLaneTotalWidth + frameLaneSpace;
	f32 maxWidth = Maximum(GetCollationFrameCount(state) * frameWidth, viewDim.X);
	if (isHot) {
		view.offset += V2{ state->controller->mouseWheelTicks * 30.f, 0.f };
	}
	view.offset.X = Clip(view.offset.X, 0.f, maxWidth - viewDim.X);

	f32 currentWidth = frameWidth - view.offset.X;
	V4 backgroundColor = V4{ 0.03f, 0.03f, 0.03f, 0.75f };
	PushRect(state->renderGroup, DefaultFlatTransform(), view.rect, -1.f, backgroundColor);

	u32 startFrameOrdinal = state->newestFrameOrdinal;
	u32 endFrameOrdinal = state->oldestFrameOrdinal;
	DebugSelectedSpan& selectedSpan = state->cpuProfiler.selectedSpans[state->cpuProfiler.selectedSpanCount];
	DebugVariable* rootVar = state->frameVariable;
	if (SelectedByEvent(selectedSpan)) {
		startFrameOrdinal = selectedSpan.frameOrdinal;
		endFrameOrdinal = PrevFrameOrdinal(startFrameOrdinal);

		i32 frameDiffCount = GetNewestEvent(state->frameVariable)->captureFrameIndex - selectedSpan.byEvent->captureFrameIndex;
		currentWidth += frameDiffCount * frameWidth;

		rootVar = selectedSpan.byEvent->span.var;
	}
	else if (SelectedByVar(selectedSpan)) {
		rootVar = selectedSpan.byVar;
	}

	for (u32 frameOrdinal = startFrameOrdinal;
		frameOrdinal != endFrameOrdinal;
		frameOrdinal = PrevFrameOrdinal(frameOrdinal)) 
	{
		DebugStoredEvent* rootSentinel = &rootVar->frames[frameOrdinal].eventSentinel;
		DebugStoredEvent* frameEvent = state->frameVariable->frames[frameOrdinal].eventSentinel.next;
		DebugStoredEvent* rootEvent = SelectedByEvent(selectedSpan) ?
			selectedSpan.byEvent :
			rootSentinel->next;
		for (; rootEvent != rootSentinel; rootEvent = rootEvent->next) {
			for (DebugStoredEvent* event = rootEvent->span.firstChild; event; event = event->span.sibling) {
				DebugProfilerSpan* span = &event->span;
				f32 minT = f4(span->cyclesStart - frameEvent->span.cyclesStart) * DEBUG_COLLATION_SCALE;
				f32 maxT = f4(span->cyclesEnd - frameEvent->span.cyclesStart) * DEBUG_COLLATION_SCALE;
				f32 threshold = 0.01f;
				if (maxT - minT < threshold) {
					continue;
				}
				Rect2 spanRect = GetCpuSpanRectangle(view.rect, currentWidth, span->thread,
					threadLaneWidth, threadLaneTotalWidth, minT, maxT
				);
				String8 name = GetName(span);
				u32 colorIndex = u4(uptr(name.str)) % ArrayCount(colors);
				V4 rectColor = colors[colorIndex];


				if (IsVariableHot(state, event)) {
					rectColor = V4{ 1, 1, 1, 1 };
				}
				if (IsInRectangle(spanRect, mousePos)) {
					rectColor = V4{ 1, 1, 1, 1 };
					if (name.str) {
						char buffer[256];
						sprintf_s(buffer, "%.*s", name.length, name.str);
						V4 color = V4{ 1, 1, 1, 1 };
						f32 lineAdvance = state->fontContext.scale * f4(GetFontLineAdvance(state->font));
						V2 textPos = mousePos + V2{ 0, lineAdvance };
						DebugRenderLineWithOutline(state, buffer, textPos, state->fontContext.scale, color, V4{ 0, 0, 0, 1 }, 1.f);
						textPos += V2{ 0, lineAdvance };
						sprintf_s(buffer, "t<%4f,%4f>, hitCount: %d", minT, maxT, span->hitCount);
						DebugRenderLineWithOutline(state, buffer, textPos, state->fontContext.scale, color, V4{ 0, 0, 0, 1 }, 1.f);
					}
					DebugSelectedSpan selectedSpanData = BuildSelectedSpan(span->var, event, SpanSelection_ByEvent, frameOrdinal);
					state->nextHotInteraction = InteractionProfilerSpan(spanRect, selectedSpanData);
				}
				if (IsValid(spanRect) && GetHeight(spanRect) > 1.0f) {
					PushRect(state->renderGroup, DefaultFlatTransform(), spanRect, 0, rectColor);
				}
			}
			if (SelectedByEvent(selectedSpan)) {
				break;
			}
		}
		currentWidth += frameWidth;
	}
	V2 resizeCenter = view.rect.max;
	V2 resizeSize = V2{ 8, 8 };
	RenderResizeAnchor(state, resizeCenter, resizeSize, mousePos, &state->cpuProfiler.view.rect);

	V2 scrollSize = V2{ Squared(viewDim.X) / maxWidth, 8.f };
	V2 scrollCenter = V2{ view.rect.max.X - view.offset.X / maxWidth * viewDim.X - 0.5f * scrollSize.X, view.rect.min.Y };
	RenderScroll(state, scrollCenter, scrollSize, mousePos, &view.offset.X, Axis_X, -maxWidth / viewDim.X);
}

inline
Rect2 ZoomTowardsDirection(DebugVirtualView& view, i32 ticks, V2 mousePos) {
	f32 prevZoomAmount = view.zoom;
	view.zoom = Clip(view.zoom - view.zoom * 0.06f * f4(ticks), 0.01f, view.projection.camera.focalLength);
	V2 viewCenter = GetCenter(view.rect);
	V2 viewDim = GetDim(view.rect);
	Rect2 zoomedView = GetRenderRectangleAtDistance(view.projection, u4(viewDim.X), u4(viewDim.Y), view.zoom);

	V2 mouseProjected = (mousePos - viewCenter); 
	mouseProjected.X = Clip(mouseProjected.X, -0.5f * viewDim.X, 0.5f * viewDim.X);
	mouseProjected.Y = Clip(mouseProjected.Y, -0.5f * viewDim.Y, 0.5f * viewDim.Y);
	
	V2 amountToMove = mouseProjected * (prevZoomAmount - view.zoom) / view.projection.camera.focalLength;
	V2 minOffset = view.rect.min - zoomedView.min - viewCenter;
	V2 maxOffset = view.rect.max - zoomedView.max - viewCenter;
	view.offset += amountToMove;
	view.offset.X = Clip(view.offset.X, minOffset.X, maxOffset.X);
	view.offset.Y = Clip(view.offset.Y, minOffset.Y, maxOffset.Y);
	V2 finalOffset = view.offset + viewCenter;
	zoomedView = MoveRectangle(zoomedView, finalOffset);
	return zoomedView;
}

internal
void DebugRenderMemoryProfiler(DebugState* state, Controller& controller, V2 mousePos) {
	TIMED_FUNCTION;
	if (!DEBUG_Profiler_Memory) {
		return;
	}
	DebugVirtualView& view = state->memProfiler.view;
	bool isHot = IsInRectangle(view.rect, mousePos);
	if (isHot) {
		state->nextHotInteraction = InteractionMovedRect2(view.rect, &view.rect);
	}

	f32 spanHeight = 40.f;
	V4 colors[] = {
		V4{1, 0, 0, 1},
		V4{0, 1, 0, 1},
		V4{0, 0, 1, 1},
		//V4{0, 1, 1, 1},
		V4{1, 0, 1, 1},
		V4{1, 1, 0, 1},
		V4{1, 0.5f, 0.5f, 1},
	};
	Rect2 zoomedView = ZoomTowardsDirection(view, isHot ? controller.mouseWheelTicks : 0, mousePos);
	V2 viewDim = GetDim(view.rect);
	V2 viewCenter = GetCenter(view.rect);
	V2 zoomedViewDim = GetDim(zoomedView);

#if 0
	char buffer[256];
	char* at = buffer;
	char* end = buffer + sizeof(buffer);
	DebugRenderLine(state, "------------------", state->fontContext, V4{ 1, 1, 1, 1 });
	sprintf_s(buffer, 256, "zoomedView.min: %f, %f", zoomedView.min.X, zoomedView.min.Y);
	DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
	sprintf_s(buffer, 256, "zoomedView.max: %f, %f", zoomedView.max.X, zoomedView.max.Y);
	DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
	sprintf_s(buffer, 256, "zoomAmount: %f offset: %f, ", view.zoom, view.offset.X);
	DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
#endif

	V4 backgroundColor = V4{ 0.03f, 0.03f, 0.03f, 0.75f };
	PushRect(state->renderGroup, DefaultFlatTransform(), view.rect, -1.f, backgroundColor);


	u64 maxSize = debugGlobalMemory->memoryBlockSize;
	u64 memoryStart = u64(debugGlobalMemory->memoryBlock);
#if 0
	f32 testData[] = {
			memoryStart + 0.f,
			memoryStart + 0.1f * maxSize,
			memoryStart + 0.2f * maxSize,
			memoryStart + 0.3f * maxSize,
			memoryStart + 0.4f * maxSize,
			memoryStart + 0.5f * maxSize,
			memoryStart + 0.6f * maxSize,
			memoryStart + 0.7f * maxSize,
			memoryStart + 0.8f * maxSize,
			memoryStart + 0.9f * maxSize,
			memoryStart + f4(maxSize)
	};
	f32 baseY = view.rect.min.Y + 30.f;
	for (u32 i = 0; i < ArrayCount(testData); i++) {
		f32 testPoint = (f4(testData[i] - memoryStart) - f4(0.5f * maxSize)) / f4(maxSize) * viewDim.X;
		f32 a = viewDim.X / zoomedViewDim.X;
		f32 b = view.rect.max.X - zoomedView.max.X * a;
		f32 center = (viewCenter.X + testPoint) * a + b;
		V4 c = V4{ i * 0.1f, 0.f, 0.f, 1 };
		if (i == (ArrayCount(testData) - 1) || i == 0) {
			c = V4{ 1, 1, 1, 1 };
		}
		PushRect(state->renderGroup, V3{ center, baseY, 0.f }, V2{ 8.f, 8.f }, V2{ 0, 0 }, c);
	}
#else
	f32 a = viewDim.X / zoomedViewDim.X;
	f32 b = view.rect.max.X - zoomedView.max.X * a;
	f32 memoryToViewSpace = viewDim.X / f4(maxSize);

	DebugArenaView* arenaView = state->arenaViews;
	if (state->selectedArenaViewsCount > 0) {
		arenaView = state->selectedArenaViews[state->selectedArenaViewsCount]->firstChild;
	}
	u32 arenaViewIndex = 0;
	while (arenaView) {
		u32 colorIndex = arenaViewIndex % ArrayCount(colors);
		V4 color = colors[colorIndex];
		
		MemoryArenaSnapshot* snapshot = &arenaView->event->event.data_MemoryArenaSnapshot;
		f32 left = (f4(u64(snapshot->arena.data) - memoryStart) - f4(0.5f * maxSize)) * memoryToViewSpace;
		f32 usage = (f4(u64(snapshot->arena.data + snapshot->arena.used) - memoryStart) - f4(0.5f * maxSize)) * memoryToViewSpace;
		f32 right = (f4(u64(snapshot->arena.data + snapshot->arena.capacity) - memoryStart) - f4(0.5f * maxSize)) * memoryToViewSpace;
		f32 leftViewSpace = (viewCenter.X + left) * a + b;
		f32 usageViewSpace = (viewCenter.X + usage) * a + b;
		f32 rightViewSpace = (viewCenter.X + right) * a + b;
		Rect2 capacityRect = GetRectFromMinMax(
			V2{ leftViewSpace, view.rect.min.Y },
			V2{ rightViewSpace, view.rect.min.Y + 0.8f * viewDim.Y }
		);
		capacityRect = Intersection(capacityRect, view.rect);
		if (IsInRectangle(capacityRect, mousePos)) {
			if (IsVariableHot(state, arenaView)) {
				color = V4{ 1, 1, 1, 1 };
				if (arenaView->name) {
					char buffer[256];
					sprintf_s(buffer, "%s", arenaView->name);
					color = V4{ 1, 1, 1, 1 };
					f32 lineAdvance = state->fontContext.scale * f4(GetFontLineAdvance(state->font));
					V2 textPos = mousePos + V2{ 0, lineAdvance };
					DebugRenderLineWithOutline(state, buffer, textPos, state->fontContext.scale, color, V4{ 0, 0, 0, 1 }, 1.f);
					textPos += V2{ 0, lineAdvance };
					MemoryArena* arena = &snapshot->arena;
					sprintf_s(buffer, "u %lld/%lld, tempcount %d", arena->used, arena->capacity, arena->tempCount);
					DebugRenderLineWithOutline(state, buffer, textPos, state->fontContext.scale, color, V4{ 0, 0, 0, 1 }, 1.f);
				}
			}
			state->nextHotInteraction = InteractionArenaView(capacityRect, arenaView);
		}
		if (IsValid(capacityRect)) {
			PushRect(state->renderGroup, DefaultFlatTransform(), capacityRect, 0, color);
		}
#if 0
		Rect2 usageRect = GetRectFromMinMax(
			V2{ leftViewSpace, view.rect.min.Y },
			V2{ usageViewSpace, view.rect.min.Y + 30.f }
		);
		PushRect(state->renderGroup, usageRect, 0, V2{ 0, 0 }, color);
#endif
		arenaView = arenaView->next;
		arenaViewIndex++;
	}
#endif
#if 0
	PushRectOutlineInside(state->renderGroup, zoomedView, 0, V4{ 1, 0, 0, 1 }, 2.f);
#endif

	V2 resizeCenter = view.rect.max;
	V2 resizeSize = V2{ 8, 8 };
	RenderResizeAnchor(state, resizeCenter, resizeSize, mousePos, &view.rect);

	V2 scrollCenter = V2{ viewCenter.X + view.offset.X, view.rect.min.Y };
	V2 scrollSize = V2{ zoomedViewDim.X, 8.f };
	RenderScroll(state, scrollCenter, scrollSize, mousePos, &view.offset.X, Axis_X, 1.f);
}

internal
void DebugRenderVariablesMenu(DebugState* state, Controller& controller, V2 mousePos) {
	for (DebugTree* tree = state->UISentinel.next; tree != &state->UISentinel; tree = tree->next) {
		FontDrawContext fontContext = InitializeStandardFontDrawContext(state, tree->pos);
		DebugVariableLink* node = &tree->rootGroup;
		u32 depth = 0;
		DebugVariableLink* parent[64] = {};
		while (node) {
			char buffer[256] = {};
			char* end = buffer + sizeof(buffer);
			V4 itemColor = V4{ 1, 1, 1, 1 };
			V4 hotItemColor = V4{ 0.2f, 0.5f, 1.0f, 1 };
			DebugStoredEvent* sentinel = GetNewestEventSentinel(node->variable);
			if (sentinel->next != sentinel) {
				bool isHot = IsVariableHot(state, node);
				if (isHot) {
					itemColor = hotItemColor;
				}

				char* at = buffer;
				for (u32 idx = 0; idx < depth; idx++) {
					*at++ = ' ';
					*at++ = ' ';
				}
				if (node->isGroup) {
					String8 name = GetName(node->variable);
					at += sprintf_s(at, end - at, "%.*s:", name.length, name.str);
				}
				else {
					DebugVariableToText(node->variable, at, u4(end - at), DebugVarToText_AddColon);
				}

				Rect2 bb = GetTextBoundingBox(state, buffer, fontContext);
				if (IsInRectangle(bb, mousePos)) {
					state->nextHotInteraction = InteractionWithTree(bb, tree, node);
				}
				V4 bbColor = V4{ 0.5f, 0, 0, 1 };
				PushRect(state->renderGroup, DefaultFlatTransform(), AddRadius(bb, V2{ 4.f, 4.f }), 0, bbColor);
				DebugRenderLine(state, buffer, fontContext, itemColor);
			}

			if (node) {
				if (node->isGroup && IsExpanded(node)) {
					// TODO: Display group names
					parent[++depth] = node;
					node = parent[depth]->firstChild;
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
		if (state->cpuProfiler.selectedSpanCount > 0) {
			if (IsPressed(controller.B.kShift)) {
				state->cpuProfiler.selectedSpanCount = 0;
			}
			else {
				state->cpuProfiler.selectedSpanCount--;
			}
		}
		
		if (state->selectedArenaViewsCount > 0) {
			if (IsPressed(controller.B.kShift)) {
				state->selectedArenaViewsCount = 0;
			}
			else {
				state->selectedArenaViewsCount--;
			}
		}
	}
	if (WasPressed(state->controller->B.kP)) {
		PROFILER_PAUSE = !PROFILER_PAUSE;
	}

	// Set hot interaction
	DebugInteractionObject nextInteractionObj = state->nextHotInteraction.obj;
	if (nextInteractionObj != DebugInteractionObject::None) {
		switch (nextInteractionObj) {
		case DebugInteractionObject::Tree: {
			DebugTree* tree = state->nextHotInteraction.tree.tree;
			DebugVariableLink* link = state->nextHotInteraction.tree.link;
			DebugEvent* event = &GetNewestEvent(state->nextHotInteraction.tree.link->variable)->event;
			DebugVariable* var = state->nextHotInteraction.tree.link->variable;
			switch (event->type) {
			case Event_Data_bool: {
				if (WasPressed(controller.B.mouseLeft)) {
					state->nextHotInteraction.type = DebugInteractionType::Toggle;
				}
			} break;
			case Event_Data_f32: {
				if (WasPressed(controller.B.mouseLeft)) {
					state->nextHotInteraction = InteractionDragIncrease(
						state->nextHotInteraction.startBoundingBox, &event->data_f32, 0.1f, Axis_Y
					);
				}
			} break;
			}
			if (IsPressed(controller.B.kShift) && WasPressed(controller.B.mouseLeft)) {
				state->nextHotInteraction = InteractionWithTree(
					state->nextHotInteraction.startBoundingBox, tree, link);
				state->nextHotInteraction.type = DebugInteractionType::Tear;
			}
			state->nextHotInteraction.var = var;
		} break;
		case DebugInteractionObject::ArenaView: {
			if (WasPressed(controller.B.mouseLeft)) {
				state->nextHotInteraction.type = DebugInteractionType::SelectArenaView;
			}
		} break;
		case DebugInteractionObject::Introspectable: {
			if (WasPressed(controller.B.mouseLeft)) {
				state->nextHotInteraction.type = DebugInteractionType::Select;
			}
		} break;
		case DebugInteractionObject::ResizedRect2: {
			if (WasPressed(controller.B.mouseLeft)) {
				state->nextHotInteraction.type = DebugInteractionType::ResizeRect2;
			}
		} break;
		case DebugInteractionObject::MovedRect2: {
			if (WasPressed(controller.B.mouseLeft)) {
				state->nextHotInteraction.type = DebugInteractionType::MoveRect2;
			}
		} break;
		case DebugInteractionObject::ProfilerSpan: {
			DebugSelectedSpan& selected = state->nextHotInteraction.selectedSpan;
			if (WasPressed(controller.B.mouseLeft) && IsPressed(controller.B.kShift) && selected.byEvent) {
				selected.type = SpanSelection_ByEvent;
				state->nextHotInteraction.type = DebugInteractionType::SelectProfilerSpan;
			} else if (WasPressed(controller.B.mouseLeft) && selected.byVar) {
				selected.type = SpanSelection_ByVar;
				state->nextHotInteraction.type = DebugInteractionType::SelectProfilerSpan;
			}
		} break;
		}
		state->nextHotInteraction.startMousePos = mousePos;
	}
	state->hotInteraction = state->nextHotInteraction;

	// What to do at the beginning of interaction
	if (!state->interacting && state->hotInteraction.obj != DebugInteractionObject::None) {
		state->interaction = state->hotInteraction;
		state->interacting = true;
		if (state->interaction.type == DebugInteractionType::Tear) {
#if 1
			DebugVariableLink* tearPoint = state->interaction.tree.link;
			DebugVariableLink* oldParentGroup = tearPoint->parent;
			DebugTree* dstTree = state->interaction.tree.tree;
			if (oldParentGroup) {
				DebugVariableLink* prevChild = 0;
				for (DebugVariableLink* child = oldParentGroup->firstChild; child; child = child->next) {
					if (child == tearPoint) {
						if (prevChild) {
							prevChild->next = child->next;
						}
						else {
							oldParentGroup->firstChild = oldParentGroup->firstChild->next;
						}
						break;
					}
					prevChild = child;
				}
				dstTree = AddTree(state, dstTree->pos, "NewUserGroup");
				dstTree->rootGroup.firstChild = tearPoint;
				ExpandGroup(&dstTree->rootGroup, true);
				tearPoint->next = 0;
				tearPoint->parent = &dstTree->rootGroup;
			}
			Rect2& bbox = state->interaction.startBoundingBox;
			V2 treePos = V2{ bbox.min.X, bbox.max.Y };
			state->interaction = InteractionMoveTree(mousePos, dstTree, treePos);
#endif
		}
	}
	
	// What to do DURING the interaction (for interactions taking more time than one frame)
	if (state->interacting && state->interaction.type != DebugInteractionType::None) {
		V2 dMouse = mousePos - state->interaction.startMousePos;
		switch (state->interaction.type) {
		case DebugInteractionType::Toggle: {
			DebugTree* tree = state->interaction.tree.tree;
			if (LengthSq(dMouse) > 5.f) {
				state->interaction = InteractionMoveTree(mousePos, tree, tree->pos);
			}
		} break;
		case DebugInteractionType::DragIncrease: {
			DebugDraggedFloat& f = state->interaction.dragged_f32;
			*f.actual = f.initial + f.amountPerPixel * dMouse.E[f.axis];
			if (state->interaction.var) {
				GetNewestEvent(state->interaction.var)->event.data_f32 = *f.actual;
			}
		} break;
		case DebugInteractionType::ResizeRect2: {
			DebugModifiedRect2& rect2 = state->interaction.mod_Rect2;
			f32 newMaxX = Maximum(mousePos.X, rect2.initial.min.X + 10.f);
			f32 newMaxY = Maximum(mousePos.Y, rect2.initial.min.Y + 10.f);
			rect2.actual->max = V2{ newMaxX, newMaxY };
			if (state->interaction.var) {
				GetNewestEvent(state->interaction.var)->event.data_Rect2 = *rect2.actual;
			}
		} break;
		case DebugInteractionType::MoveV2: {
			DebugModifiedV2& pos = state->interaction.mod_V2;
			*pos.actual = pos.initial + dMouse;
			if (state->interaction.var) {
				GetNewestEvent(state->interaction.var)->event.data_V2 = *pos.actual;
			}
		} break;
		case DebugInteractionType::MoveRect2: {
			DebugModifiedRect2& rect2 = state->interaction.mod_Rect2;
			rect2.actual->min = rect2.initial.min + dMouse;
			rect2.actual->max = rect2.initial.max + dMouse;
			if (state->interaction.var) {
				GetNewestEvent(state->interaction.var)->event.data_Rect2 = *rect2.actual;
			}
		} break;
		}
	}

	if(DEBUG_Debug_ShowInteractions) {
		const char* interaction = "Unknown";
#define CASE_VALUE_TO_STRING(value, assignable) case value: { assignable = #value; } break
		switch (state->interaction.type) {
			CASE_VALUE_TO_STRING(DebugInteractionType::None, interaction);
			CASE_VALUE_TO_STRING(DebugInteractionType::Toggle, interaction);
			CASE_VALUE_TO_STRING(DebugInteractionType::DragIncrease, interaction);
			CASE_VALUE_TO_STRING(DebugInteractionType::ResizeRect2, interaction);
			CASE_VALUE_TO_STRING(DebugInteractionType::MoveRect2, interaction);
			CASE_VALUE_TO_STRING(DebugInteractionType::MoveV2, interaction);
			CASE_VALUE_TO_STRING(DebugInteractionType::Select, interaction);
			CASE_VALUE_TO_STRING(DebugInteractionType::SelectProfilerSpan, interaction);
			CASE_VALUE_TO_STRING(DebugInteractionType::Tear, interaction);
		} 
		PRINT_DEBUGGING("%s", interaction);
	}

	// What to do at the END of interaction
	bool interactionEnded = false;
	switch (state->interaction.type) {
	case DebugInteractionType::DragIncrease:
	case DebugInteractionType::ResizeRect2:
	case DebugInteractionType::MoveRect2:
	case DebugInteractionType::MoveV2: {
		interactionEnded = !IsPressed(controller.B.mouseLeft);
	} break;
	case DebugInteractionType::Toggle: {
		if (WasReleased(controller.B.mouseLeft) || !IsPressed(controller.B.mouseLeft)) {
			DebugEvent* event = &GetNewestEvent(state->interaction.tree.link->variable)->event;
			event->data_bool = !event->data_bool;
			interactionEnded = true;
		}
	} break;
	case DebugInteractionType::Select: {
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
	case DebugInteractionType::SelectProfilerSpan: {
		if (SelectedByEvent(state->interaction.selectedSpan)) {
			PROFILER_PAUSE = true;
		}
		if (state->cpuProfiler.selectedSpanCount >= ArrayCount(state->cpuProfiler.selectedSpans) - 1) {
			state->cpuProfiler.selectedSpanCount = 0;
		}
		state->cpuProfiler.selectedSpans[++state->cpuProfiler.selectedSpanCount] = state->interaction.selectedSpan;
		interactionEnded = true;
	} break;
	case DebugInteractionType::SelectArenaView: {
		Assert(state->selectedArenaViewsCount < ArrayCount(state->selectedArenaViews) - 1);
		state->selectedArenaViews[++state->selectedArenaViewsCount] = state->interaction.arenaView;
		interactionEnded = true;
	} break;
	default: {
		interactionEnded = true;
	} break;
	}

	const char* GUID = "NOTHING";
	if (state->interaction.type != DebugInteractionType::None) {
		if (state->interaction.var) {
			debugGlobalState->swapEvent = GetNewestEvent(state->interaction.var)->event;
			GUID = debugGlobalState->swapEvent.GUID;
		}
	}
	// PRINT_DEBUGGING("SWAPPED EVENT GUID: %s", GUID);
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

void DebugRenderOverlay(DebugState* state) {
	TIMED_FUNCTION;
	if (!state->font) {
		return;
	}
	Controller& controller = *state->controller;
	V2 mousePos = FromPixelSpaceToWorldSpace(state->renderGroup.projection, controller.mouse, 0.f);
	DebugRenderVariablesMenu(state, controller, mousePos);
	DebugRenderCpuProfiler(state, controller, mousePos);
	DebugRenderCpuProfilerTimings(state, controller, mousePos);
	DebugRenderCpuProfilerTimingsHierarchy(state, controller, mousePos);
	DebugRenderMemoryProfiler(state, controller, mousePos);
	DebugInteract(state, mousePos, controller);

	if(DEBUG_Debug_ShowEventsCount) {
		u32 currentFrame = !debugGlobalState->currentFrameIndex;
		PRINT_DEBUGGING("Events in frame: %d", debugGlobalState->eventsCount[currentFrame]);
#if 0
		DebugFrameInfo* frameInfo = state->frames + state->frameReadIndex;
		for (u32 eventType = 0; eventType < ArrayCount(frameInfo->eventCount.count); eventType++) {
			sprintf_s(buffer, 256, "   (type)%d = (count)%d", eventType, frameInfo->eventCount.count[eventType]);
			DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
		}
#endif
#if 0
		{
			MemoryArena* arenas[] = { &state->collationFrameArena, &state->mainArena };
			const char* arenaNames[] = { "CollationFrame", "Main" };
			char buffer[256];
			char* at = buffer;
			char* end = buffer + sizeof(buffer);
			at += sprintf_s(at, end - at, "Arena remaining sizes:   ");
			for (u32 arenaIndex = 0; arenaIndex < ArrayCount(arenas); arenaIndex++) {
				u64 arenaRemainingSize = GetArenaFreeSpaceSize(*arenas[arenaIndex]) / 1024;
				at += sprintf_s(at, end - at, "%s: %lldkB   ", arenaNames[arenaIndex], arenaRemainingSize);
			}
			DebugRenderLine(state, buffer, state->fontContext, V4{ 1, 1, 1, 1 });
		}
#endif
		PRINT_DEBUGGING("Event Dealloc/Alloc count: %d/%d", state->deallocEventsSum, state->allocEventsSum);
#if 0
		{
			DebugCollationFrame* frame = state->framesSentinel.next;
			f32 durationMs = DurationToMs(frame->endCycles - frame->startCycles);
			f32 durationMsNoDebug = durationMs - DurationToMs(frame->endCyclesDebugFinishFrame - frame->startCyclesDebugFinishFrame);
			PRINT_DEBUGGING("Frame duration: %.2fms (%.2fms)", durationMs, durationMsNoDebug);
		}
#endif
	}
}

extern "C" DebugGlobalState* DebugInit(ProgramMemory* memory) {
	return debugGlobalState;
}

extern "C" void DebugFinishFrame(ProgramMemory* memory, RenderCommandBuffer* renderCommands, InputData& input, u32 bitmapWidth, u32 bitmapHeight) {
	TIMED_FUNCTION;
	debugGlobalState->frameStartCyclesDebugFinishFrame[debugGlobalState->currentFrameIndex] = __rdtsc();

	DebugState* state = DebugBegin(input, renderCommands, bitmapWidth, bitmapHeight);
	if (!state) {
		return;
	}
	DebugRenderOverlay(state);
	DebugCollateEvents(state);
	state->totalFrameCount++;
	debugGlobalState->frameEndCyclesDebugFinishFrame[debugGlobalState->currentFrameIndex] = __rdtsc();
	return;
}
