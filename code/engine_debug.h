#include "engine_common.h"
#include "engine_render.h"

// ------------------- EVENT PROFILER --------------------
#define MAX_DEBUG_EVENTS 900000
#define MAX_DEBUG_FRAMES 40
#define MAX_DEBUG_THREADS 64
#define MAX_STACK_REGIONS 4096
#define DEBUG_CPU_FREQ (2.9f * 1000 * 1000)
#define DEBUG_TARGET_REFRESH_MS 16.6666666f

#define MAX_CHILD_SPANS 128
#define MAX_DEPTH_SPANS 128

struct DebugId {
	void* ptr;
	u32 index;
};

enum DebugEventType : u8 {
	Event_Unknown,

	Event_Time_BlockBegin,
	Event_Time_BlockEnd,

	Event_Data_BlockBegin,
	Event_Data_BlockEnd,
	Event_Data_bool,
	Event_Data_u32,
	Event_Data_f32,
	Event_Data_i32,
	Event_Data_V2,
	Event_Data_V3,
	Event_Data_V4,
	Event_Data_Rect2,
	Event_Data_Rect3,
	
	Event_PermanentVariableDeclaration,
	Event_MemoryArenaInitialize,
	Event_MemoryArenaUpdate,
	Event_Count,
};

struct DebugEventCountMetrics {
	u32 count[Event_Count + 1];
};

struct MemoryArenaSnapshot {
	MemoryArena arena;
	MemoryArena* parent;
};

struct DebugEvent {
	DebugEventType type;
	u8 coreId;
	u16 reserved;
	u16 threadId;
	u64 cycles;
	const char* GUID;
	const char* file;
	const char* blockName;
	u32 line;
	union {
		void* generic;
		DebugId data_DebugId;
		DebugEvent* data_DebugEvent;
		MemoryArenaSnapshot data_MemoryArenaSnapshot;

		bool data_bool;
		u32 data_u32;
		f32 data_f32;
		i32 data_i32;
		V2 data_V2;
		V3 data_V3;
		V4 data_V4;
		Rect2 data_Rect2;
		Rect3 data_Rect3;
	};
};

struct DebugStoredEvent {
	u32 captureFrameIndex;

	DebugEvent event;
	DebugStoredEvent* next;
};

struct DebugVariable {
	const char* GUID;
	const char* name;
	bool permanent;
	u32 introspectionObjectIndex;

	DebugVariable* nextInHash;
	DebugStoredEvent* oldestEvent;
	DebugStoredEvent* newestEvent;
};

struct PermanentDebugVariable {
	DebugVariable* var;
	const char* blockName;
	PermanentDebugVariable* next;
};

struct DebugGlobalState {
	DebugEvent events[2][MAX_DEBUG_EVENTS];
	u32 eventsCount[2];
	u64 frameStartCycles[2];
	u32 currentFrameIndex;
	volatile u64 frameAndEventIndex;
};

// TODO: Move it down below
#if INTERNAL_BUILD 
extern DebugGlobalState* debugGlobalState;
#endif

struct DebugProfilerSpan;
struct DebugProfilerSpanChildren {
	u32 count;
	DebugProfilerSpan* children[MAX_CHILD_SPANS];
};

struct OpenDebugEvent {
	DebugEvent event;
	union {
		DebugProfilerSpanChildren childSpans;
	};
	OpenDebugEvent* next;
};

struct DebugThreadStack {
	u16 threadId;
	u8 laneId; //NOTE: de facto threadId starting from 0,1,2,3,4...N
	OpenDebugEvent* timeEvents;
	OpenDebugEvent* dataEvents;
};

#define UniqueGUID__(file, line, counter) file ":" #line " (" #counter ")"
#define UniqueGUID_(file, line, counter) UniqueGUID__(file, line, counter)
#define UniqueGUID UniqueGUID_(__FILE__, __LINE__, __COUNTER__)
#define RecordDebugEventNoBracket_(counter, eventtype, filename, blockname, linenumber) \
	u64 frameAndEventIndex12345 = AtomicAddU64(&debugGlobalState->frameAndEventIndex, 1);\
	u32 frameIndex12345 = frameAndEventIndex12345 >> 32;\
	u32 eventIndex12345 = frameAndEventIndex12345 & U32_MAX;\
	Assert(eventIndex12345 < MAX_DEBUG_EVENTS);\
	DebugEvent* event12345 = debugGlobalState->events[frameIndex12345] + eventIndex12345;\
	u32 coreId;\
	event12345->cycles = __rdtscp(&coreId);\
	event12345->coreId = u8(coreId);\
	event12345->type = eventtype;\
	event12345->file = filename;	\
	event12345->GUID = UniqueGUID; \
	event12345->blockName = blockname; \
	event12345->line = linenumber;	\
	event12345->threadId = u2(GetFastThreadId());
#define RecordDebugEventNoBracket(counter, eventtype, filename, blockname, linenumber) RecordDebugEventNoBracket_(counter, eventtype, filename, blockname, linenumber)

#if INTERNAL_BUILD
#define TIMED_FUNCTION__(line) TimedBlock block##line(__FILE__, __FUNCTION__, __LINE__, __COUNTER__)
#define TIMED_FUNCTION_(line) TIMED_FUNCTION__(line)
#define TIMED_FUNCTION TIMED_FUNCTION_(__LINE__)

#define TIMED_BLOCK_BEGIN__(counter, fileName, name, lineNumber) { \
	RecordDebugEventNoBracket_(counter, Event_Time_BlockBegin, fileName, name, lineNumber); }
#define TIMED_BLOCK_BEGIN_(counter, fileName, name, lineNumber) \
	TIMED_BLOCK_BEGIN__(counter, fileName, name, lineNumber)
#define TIMED_BLOCK_BEGIN(blockName)		\
	u16 counter##blockName = __COUNTER__;	\
	TIMED_BLOCK_BEGIN_(counter##blockName, __FILE__, #blockName, __LINE__)

#define TIMED_BLOCK_END_(counter) { \
	RecordDebugEventNoBracket(counter, Event_Time_BlockEnd, __FILE__, "TimedBlockEnd", __LINE__); }
#define TIMED_BLOCK_END(blockName) \
	TIMED_BLOCK_END_(counter##blockName)

#define MARKUP_FRAME_BEGIN \
	debugGlobalState->frameStartCycles[debugGlobalState->currentFrameIndex] = __rdtsc();
#define MARKUP_FRAME_END { \
	u32 oldFrameIndex = debugGlobalState->currentFrameIndex; \
	debugGlobalState->currentFrameIndex = !debugGlobalState->currentFrameIndex;	\
	u64 oldFrameAndEventIndex = AtomicExchangeU64(&debugGlobalState->frameAndEventIndex, u64(debugGlobalState->currentFrameIndex) << 32); \
	debugGlobalState->eventsCount[oldFrameIndex] = oldFrameAndEventIndex & U32_MAX;\
	MARKUP_FRAME_BEGIN }

inline DebugId DEBUG_POINTER_ID(void* ptr, u32 objId);
inline void DEBUG_HIT(DebugId did, Rect2 boundingBox);
inline bool DEBUG_HIGHLIGHTED(DebugId did, V4* color);
inline bool DEBUG_DATA_BLOCK_REQUESTED(DebugId did);
internal DebugEvent* InitializePermanentDebugVariable(DebugEvent* subevent, DebugEventType type, const char* name, const char* file, u16 line, const char* GUID);

#define DEBUG_UI_ENABLED 1
#define DEBUG_BEGIN_DATA_BLOCK(Name, debugid) { \
	u16 counter##Name = __COUNTER__; \
	RecordDebugEventNoBracket(counter##Name, Event_Data_BlockBegin, __FILE__, #Name, __LINE__) \
	event12345->data_DebugId = debugid; }
#define DEBUG_END_DATA_BLOCK { \
	RecordDebugEventNoBracket(0, Event_Data_BlockEnd, __FILE__, "DataEndBlock", __LINE__) }
#define DEBUG_DATA(type, data) { \
	RecordDebugEventNoBracket(0, Event_Data_##type, __FILE__, #data, __LINE__); \
	event12345->data_##type = data; }
#define DEFINE_DEBUG_VARIABLE_WITH_INIT(type, variable, init) \
	local_persist DebugEvent variable = *InitializePermanentDebugVariable((variable.data_##type = init, &variable), Event_Data_##type, #variable, __FILE__, __LINE__, UniqueGUID)
#define DEFINE_DEBUG_VARIABLE_(type, variable, init) DEFINE_DEBUG_VARIABLE_WITH_INIT(type, variable, init)
#define DEFINE_DEBUG_VARIABLE(type, variable) DEFINE_DEBUG_VARIABLE_(type, variable, CONSTANT_##variable)
#define DEBUG_IF(variable) \
	DEFINE_DEBUG_VARIABLE(bool, variable); \
	if (variable.data_bool)

#define RecordMemoryDebugEvent_(type, arenaArg) \
	RecordDebugEventNoBracket(0, type, __FILE__, #arenaArg, __LINE__) \
	event12345->GUID = ptrcast(const char, &(arenaArg)); \
	event12345->data_MemoryArenaSnapshot.arena = arenaArg; \
	event12345->data_MemoryArenaSnapshot.parent = 0;
#define RecordMemoryDebugEvent(type, arenaArg) { RecordMemoryDebugEvent_(type, arenaArg) }
#define RecordSubArenaDebugEvent(subarenaArg, arenaArg) { \
	RecordMemoryDebugEvent_(Event_MemoryArenaInitialize, subarenaArg) \
	event12345->data_MemoryArenaSnapshot.parent = &(arenaArg); }
#define RecordAssetMemoryBlockEvent(block) { \
	RecordDebugEventNoBracket(0, Event_AssetMemoryBlock, __FILE__, "AssetMemoryBlock", __LINE__) \
	event12345->data_AssetMemoryBlock = *(block); }

#else
#define RecordMemoryDebugEvent(...)
#define RecordSubArenaDebugEvent(...)
#define RecordAssetMemoryBlockEvent(...)

#define TIMED_FUNCTION
#define TIMED_BLOCK_BEGIN__(...)
#define TIMED_BLOCK_BEGIN(...)
#define TIMED_BLOCK_END_(...)
#define TIMED_BLOCK_END(...)

#define MARKUP_FRAME_BEGIN
#define MARKUP_FRAME_END

inline DebugId DEBUG_POINTER_ID(void* ptr, u32 objId) { return {}; }
inline void DEBUG_HIT(DebugId did, Rect2 boundingBox) {}
inline bool DEBUG_HIGHLIGHTED(DebugId did, V4* color) { return false;}
inline bool DEBUG_DATA_BLOCK_REQUESTED(DebugId did) { return false; }
#define DEBUG_UI_ENABLED 0
#define DEBUG_BEGIN_DATA_BLOCK(...)
#define DEBUG_END_DATA_BLOCK
#define DEBUG_DATA(...)
#define DEFINE_DEBUG_VARIABLE(type, variable) DebugEvent variable = {}
#define DEBUG_IF(...) if (0)
#endif

struct ManualTimedBlock {
	u16 counter;
};

struct TimedBlock {
	ManualTimedBlock block;

	TimedBlock(const char* file, const char* blockName, u16 line, u16 counter, u32 hitCount = 1) {
		block.counter = counter;
		TIMED_BLOCK_BEGIN__(counter, file, blockName, line)
	}

	~TimedBlock() {
		TIMED_BLOCK_END_(block.counter);
	}
};

struct DebugVariableLink;
struct DebugVariableGroup {
	const char* name;
	u32 nameLength;
	bool expanded;

	u32 introspectionObjectIndex;
	u32 dataReceivingFrameIndex;
	DebugId introspectionId;
	DebugVariableGroup* nextInHash;

	DebugVariableGroup* parentGroup;

	DebugVariableLink* containingLink;
	DebugVariableLink* firstLink;
};

struct DebugVariableLink {
	DebugVariableGroup* group;
	DebugVariable* variable;

	DebugVariableLink* next;
	DebugVariableGroup* parentGroup;
};

struct DebugProfilerSpan {
	f32 minT;
	f32 maxT;
	u8 thread;
	u32 spanId; //NOTE: Id unique in scope of a collation frame
	u32 parentSpanId;
	const char* name;
	const char* parentName;

	DebugProfilerSpan* next;
};


struct DebugCollationFrame {
	u64 startCycles;
	u32 eventsCount;
	u32 frameIndex;

	u8 threadCount;
	u32 spanCount;
	DebugProfilerSpan* firstCpuSpan; // NOTE: These are connected with nextSpan
	DebugProfilerSpan* lastCpuSpan;

	DebugCollationFrame* next;
	DebugCollationFrame* prev;
};

struct DebugTree {
	V2 pos;
	DebugVariableGroup rootGroup;

	DebugTree* next;
	DebugTree* prev;
};

struct DebugScroll {
	f32 value; //NOTE: <0,1>
	f32 min;
	f32 range;
	f32 containerWidth;

	f32 distancePerTick; //NOTE: in pixels
};

enum class DebugInteractionObject {
	None,
	LinkInTree,
	Introspectable,
	MovedRect2,
	ResizedRect2,
	ProfilerSpan,
	ArenaView,
	Float
};

enum class DebugInteractionType {
	None,

	Toggle,
	ResizeRect2,
	DragIncrease,
	Tear,
	MoveV2,
	MoveRect2,
	Select,
	SelectProfilerSpan,
	SelectArenaView,
	ScrollProfiler
};

struct DebugModifiedV2 {
	V2 initial;
	V2* actual;
};

struct DebugModifiedRect2 {
	Rect2 initial;
	Rect2* actual;
};

struct DebugVirtualView {
	f32 zoom;
	V2 offset;
	Rect2 rect;
	Projection projection;
};

enum DebugAxis {
	Axis_X,
	Axis_Y
};

struct DebugDraggedFloat {
	f32 initial;
	f32* actual;
	f32 amountPerPixel;
	DebugAxis axis;
};
enum DebugSpanSelectionType {
	SpanSelection_None,
	SpanSelection_ById,
	SpanSelection_ByName
};
struct DebugSelectedSpan {
	DebugSpanSelectionType type;
	const char* name;
	u32 spanId;
	u32 captureFrameIndex;
};

struct DebugVariableLinkInTree {
	DebugVariableLink* link;
	DebugTree* tree;
};

struct DebugVariableGroupInTree {
	DebugVariableGroup* group;
	DebugTree* tree;
};

struct DebugProfiler {
	u32 selectedSpanCount;
	DebugSelectedSpan selectedSpans[MAX_DEPTH_SPANS];
	DebugVirtualView view;
};

struct DebugArenaView {
	const char* name;
	const char* GUID;
	DebugStoredEvent* event;

	DebugArenaView* next;
	DebugArenaView* firstChild;
};

struct DebugProfiler;
struct DebugInteraction {
	DebugInteractionType type;
	DebugInteractionObject obj;
	union {
		void* generic;
		DebugId id;
		DebugVariableLinkInTree linkInTree;
		DebugModifiedV2 mod_V2;
		DebugModifiedRect2 mod_Rect2;
		DebugDraggedFloat dragged_f32;
		DebugSelectedSpan selectedSpan;
		DebugArenaView* arenaView;
		DebugProfiler* profiler;
	};
	V2 startMousePos;
	Rect2 startBoundingBox;
};

struct LoadedFont;
struct FontDrawContext {
	f32 scale;
	V2 leftTopStart;
	V2 leftTopCurrent;
	f32 lineAdvance;
	LoadedFont* font;
};
