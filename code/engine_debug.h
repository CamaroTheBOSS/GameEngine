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

struct DebugParsedGUID {
	String8 GUID;
	u16 fileStart;
	u16 fileLength;

	u16 lineStart;
	u16 lineLength;

	u16 counterStart;
	u16 counterLength;

	u16 nameStart;
	u16 nameLength;
};

struct DebugEvent {
	DebugEventType type;
	u8 coreId;
	u16 reserved;
	u16 threadId;
	u64 cycles;
	const char* GUID;
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
	DebugParsedGUID parsedGuid;
	bool permanent;

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
	DebugEvent swapEvent;
	u32 eventsCount[2];
	u64 frameStartCycles[2];
	u64 frameEndCycles[2];
	u64 frameStartCyclesDebugFinishFrame[2];
	u64 frameEndCyclesDebugFinishFrame[2];
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

struct DebugVariableLink;
struct OpenDebugEvent {
	DebugEvent event;
	union {
		DebugProfilerSpanChildren childSpans;
		DebugVariableLink* group;
	};
	OpenDebugEvent* next;
};

struct DebugThreadStack {
	u16 threadId;
	u8 laneId; //NOTE: de facto threadId starting from 0,1,2,3,4...N
	OpenDebugEvent* timeEvents;
	OpenDebugEvent* dataEvents;
};

#define UniqueGUID__(name, file, line, counter) file "|" #line "|" #counter "|" name
#define UniqueGUID_(name, file, line, counter) UniqueGUID__(name, file, line, counter)
#define UniqueGUID(name) UniqueGUID_(name, __FILE__, __LINE__, __COUNTER__)
#define DEBUG_NAME(name) UniqueGUID(name)
#define RecordDebugEvent(eventtype, InputGUID) \
	u64 frameAndEventIndex_ = AtomicAddU64(&debugGlobalState->frameAndEventIndex, 1);\
	u32 frameIndex_ = frameAndEventIndex_ >> 32;\
	u32 eventIndex_ = frameAndEventIndex_ & U32_MAX;\
	Assert(eventIndex_ < MAX_DEBUG_EVENTS);\
	DebugEvent* event_ = debugGlobalState->events[frameIndex_] + eventIndex_;\
	u32 coreId;\
	event_->cycles = __rdtscp(&coreId);\
	event_->coreId = u8(coreId);\
	event_->type = eventtype;\
	event_->GUID = InputGUID; \
	event_->threadId = u2(GetFastThreadId());

#if INTERNAL_BUILD
#define TIMED_FUNCTION__(line, GUID) TimedBlock block##line(GUID)
#define TIMED_FUNCTION_(line, GUID) TIMED_FUNCTION__(line, GUID)
#define TIMED_FUNCTION TIMED_FUNCTION_(__LINE__, DEBUG_NAME(__FUNCTION__))

#define TIMED_BLOCK_BEGIN__(GUID) { RecordDebugEvent(Event_Time_BlockBegin, GUID); }
#define TIMED_BLOCK_BEGIN_(GUID) TIMED_BLOCK_BEGIN__(GUID)
#define TIMED_BLOCK_BEGIN(name) TIMED_BLOCK_BEGIN_(DEBUG_NAME(#name))
#define TIMED_BLOCK_END { RecordDebugEvent(Event_Time_BlockEnd, DEBUG_NAME("EndTimedBlock")); }

#define MARKUP_FRAME_BEGIN \
	debugGlobalState->frameStartCycles[debugGlobalState->currentFrameIndex] = __rdtsc();
#define MARKUP_FRAME_END { \
	u32 oldFrameIndex = debugGlobalState->currentFrameIndex; \
	debugGlobalState->frameEndCycles[oldFrameIndex] = __rdtsc();\
	debugGlobalState->currentFrameIndex = !oldFrameIndex;\
	u64 oldFrameAndEventIndex = AtomicExchangeU64(&debugGlobalState->frameAndEventIndex, u64(debugGlobalState->currentFrameIndex) << 32); \
	debugGlobalState->eventsCount[oldFrameIndex] = oldFrameAndEventIndex & U32_MAX;\
	MARKUP_FRAME_BEGIN }

inline DebugId DEBUG_POINTER_ID(void* ptr, u32 objId);
inline void DEBUG_HIT(DebugId did, Rect2 boundingBox);
inline bool DEBUG_HIGHLIGHTED(DebugId did, V4* color);
inline bool DEBUG_DATA_BLOCK_REQUESTED(DebugId did);

#define DEBUG_DATA_BLOCK_DISPATCH_DEF(type) \
	void DEBUG_DATA_BLOCK_DISPATCH(type& data, const char* GUID) { \
		RecordDebugEvent(Event_Data_##type, GUID); \
		if(debugGlobalState->swapEvent.GUID == GUID){ \
			data = debugGlobalState->swapEvent.data_##type; \
			debugGlobalState->swapEvent.GUID = 0;\
		}\
		event_->data_##type = data;	\
	}
DEBUG_DATA_BLOCK_DISPATCH_DEF(bool);
DEBUG_DATA_BLOCK_DISPATCH_DEF(f32);
DEBUG_DATA_BLOCK_DISPATCH_DEF(u32);
DEBUG_DATA_BLOCK_DISPATCH_DEF(i32);
DEBUG_DATA_BLOCK_DISPATCH_DEF(V2);
DEBUG_DATA_BLOCK_DISPATCH_DEF(V3);
DEBUG_DATA_BLOCK_DISPATCH_DEF(V4);
DEBUG_DATA_BLOCK_DISPATCH_DEF(Rect2);
DEBUG_DATA_BLOCK_DISPATCH_DEF(Rect3);

#define DEBUG_DATA_BLOCK__(line, GUID) DataBlock block##line(GUID)
#define DEBUG_DATA_BLOCK_(line, GUID) DEBUG_DATA_BLOCK__(line, GUID)
#define DEBUG_DATA_BLOCK(name) DEBUG_DATA_BLOCK_(__LINE__, DEBUG_NAME(name))
#define DEBUG_BEGIN_DATA_BLOCK(GUID) { RecordDebugEvent(Event_Data_BlockBegin, GUID) }
#define DEBUG_END_DATA_BLOCK { RecordDebugEvent(Event_Data_BlockEnd, DEBUG_NAME("EndDataBlock")) }
#define DEBUG_DATA__(data, GUID) { DEBUG_DATA_BLOCK_DISPATCH(data, GUID); }
#define DEBUG_DATA_(data, GUID) DEBUG_DATA__(data, GUID)
#define DEBUG_DATA(data) DEBUG_DATA_(data, DEBUG_NAME(#data))

#define RecordMemoryDebugEvent(type, arenaArg) \
	RecordDebugEvent(type, DEBUG_NAME(#arenaArg)) \
	event_->GUID = ptrcast(const char, &(arenaArg)); \
	event_->data_MemoryArenaSnapshot.arena = arenaArg; \
	event_->data_MemoryArenaSnapshot.parent = 0;
#define RecordSubArenaDebugEvent(subarenaArg, arenaArg) { \
	RecordMemoryDebugEvent(Event_MemoryArenaInitialize, subarenaArg) \
	event_->data_MemoryArenaSnapshot.parent = &(arenaArg); }
#define RecordAssetMemoryBlockEvent(block) { \
	RecordDebugEvent(Event_AssetMemoryBlock, DEBUG_NAME("AssetMemoryBlock")) \
	event_->data_AssetMemoryBlock = *(block); }

#else
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
#define DEBUG_BEGIN_DATA_BLOCK(...)
#define DEBUG_END_DATA_BLOCK
#define DEBUG_DATA(...)
#define DEFINE_DEBUG_VARIABLE(type, variable) DebugEvent variable = {}
#define DEBUG_IF(...) if (0)
#endif

struct TimedBlock {
	TimedBlock(const char* GUID, u32 hitCount = 1) {
		TIMED_BLOCK_BEGIN__(GUID)
	}

	~TimedBlock() {
		TIMED_BLOCK_END;
	}
};

struct DataBlock {
	DataBlock(const char* GUID) {
		DEBUG_BEGIN_DATA_BLOCK(GUID);
	}

	~DataBlock() {
		DEBUG_END_DATA_BLOCK;
	}
};

struct DebugVariableLink {
	DebugVariable* variable;

	DebugVariableLink* next;
	DebugVariableLink* nextInHash;
	DebugVariableLink* firstChild;
	DebugVariableLink* parent;

	bool isGroup;
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
	u64 endCycles;
	u64 startCyclesDebugFinishFrame;
	u64 endCyclesDebugFinishFrame;
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
	DebugVariableLink rootGroup;

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
	Tree,
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

struct DebugInteractionTree {
	DebugTree* tree;
	DebugVariableLink* link;
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
		DebugInteractionTree tree;
		DebugModifiedV2 mod_V2;
		DebugModifiedRect2 mod_Rect2;
		DebugDraggedFloat dragged_f32;
		DebugSelectedSpan selectedSpan;
		DebugArenaView* arenaView;
		DebugProfiler* profiler;
	};
	V2 startMousePos;
	Rect2 startBoundingBox;
	DebugVariable* var;
};

struct LoadedFont;
struct FontDrawContext {
	f32 scale;
	V2 leftTopStart;
	V2 leftTopCurrent;
	f32 lineAdvance;
	LoadedFont* font;
};

debug_variable bool DEBUG_Debug_ShowInteractions = 1;
debug_variable bool DEBUG_Debug_ShowEventsCount = 1;
debug_variable bool DEBUG_Profiler_Memory;
debug_variable bool DEBUG_Profiler_Cpu;
debug_variable bool DEBUG_Profiler_CpuSpansList;
debug_variable bool DEBUG_Profiler_Pause;
debug_variable bool DEBUG_Camera_Zoomout;
debug_variable f32 DEBUG_Camera_ZoomoutValue = 10.f;
debug_variable bool DEBUG_Renderer_WithSoftware;
debug_variable bool DEBUG_Renderer_DifferentResolution;
debug_variable f32 DEBUG_Renderer_ResolutionWidth = 960.f;
debug_variable f32 DEBUG_Renderer_ResolutionHeight = 540.f;
