#include "engine_common.h"

// ------------------- EVENT PROFILER --------------------
#define MAX_DEBUG_EVENTS 200000
#define MAX_DEBUG_FRAMES 40
#define MAX_DEBUG_THREADS 64
#define MAX_STACK_REGIONS 4096
#define DEBUG_CPU_FREQ (2.9f * 1000 * 1000)
#define DEBUG_TARGET_REFRESH_MS 16.6666666f

struct DebugId {
	void* val[2];
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
	
	Event_ProfilerUI,
	Event_PermanentVariableDeclaration,
	Event_Count,
};

struct DebugEventCountMetrics {
	u32 count[Event_Count + 1];
};


struct DebugEvent {
	DebugEventType type;
	u8 coreId;
	u16 reserved;
	u16 threadId;
	u64 cycles;
	const char* file;
	const char* blockName;
	u32 line;
	union {
		DebugId data_DebugId;
		DebugEvent* data_DebugEvent;

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

struct DebugProfilerRegion {
	u8 laneId;
	u32 parentRegionIndex;

	u32 durationCycles;
	u32 reserved2;
	f32 minT;
	f32 maxT;

	const char* regionName;
	const char* parentEventId;
};

struct DebugProfilerRegionSelection {
	bool selecting;
	u8 laneId;
	u8 translationUnit;
	u16 recordIndex;	
};

struct DebugFrameInfo {
	u32 regionsCount;
	DebugProfilerRegion regions[MAX_STACK_REGIONS];
	DebugEventCountMetrics eventCount;
};

struct DebugGlobalState {
	DebugEvent debugEvents[MAX_DEBUG_FRAMES][MAX_DEBUG_EVENTS];
	u32 debugEventsCount[MAX_DEBUG_FRAMES];

	u64 frameStartCycles[MAX_DEBUG_FRAMES];
	volatile u64 frameAndEventIndex;
};

#if INTERNAL_BUILD
extern DebugGlobalState* debugGlobalState;
#endif

struct OpenDebugEvent {
	DebugEvent* event;
	u32 childRegionCount;
	u16 childRegionIndexes[32];
	OpenDebugEvent* next;
};

struct DebugThreadStack {
	u16 threadId;
	u8 laneId; //NOTE: de facto threadId starting from 0,1,2,3,4...N
	OpenDebugEvent* timeEvents;
	OpenDebugEvent* dataEvents;
};

#define RecordDebugEventNoBracket_(counter, eventtype, filename, blockname, linenumber) \
	u64 frameAndEventIndex = AtomicAddU64(&debugGlobalState->frameAndEventIndex, 1);\
	u32 frameIndex = frameAndEventIndex >> 32;\
	u32 eventIndex = frameAndEventIndex & U32_MAX;\
	Assert(eventIndex < MAX_DEBUG_EVENTS);\
	DebugEvent* event = debugGlobalState->debugEvents[frameIndex] + eventIndex;\
	u32 coreId;\
	event->cycles = __rdtscp(&coreId);\
	event->coreId = u8(coreId);\
	event->type = eventtype;\
	event->file = filename;	\
	event->blockName = blockname; \
	event->line = linenumber;	\
	event->threadId = u2(GetFastThreadId());
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
	debugGlobalState->frameStartCycles[debugGlobalState->frameAndEventIndex >> 32] = __rdtsc();
#define MARKUP_FRAME_END { \
	u32 newFrameIndex = ((debugGlobalState->frameAndEventIndex >> 32) + 1) % MAX_DEBUG_FRAMES;						\
	u64 oldFrameAndEventIndex = AtomicExchangeU64(&debugGlobalState->frameAndEventIndex, u64(newFrameIndex) << 32); \
	u32 oldFrameIndex = oldFrameAndEventIndex >> 32;\
	debugGlobalState->debugEventsCount[oldFrameIndex] = oldFrameAndEventIndex & U32_MAX;\
	MARKUP_FRAME_BEGIN }

inline DebugId DEBUG_POINTER_ID(void* ptr);
inline void DEBUG_HIT(DebugId did, Rect2 boundingBox);
inline bool DEBUG_HIGHLIGHTED(DebugId did, V4* color);
inline bool DEBUG_DATA_BLOCK_REQUESTED(DebugId did);
internal DebugEvent* InitializePermanentDebugVariable(DebugEvent* subevent, DebugEventType type, const char* name, const char* file, u16 line);

#define DEBUG_UI_ENABLED 1
#define DEBUG_BEGIN_DATA_BLOCK(Name, debugid) { \
	u16 counter##Name = __COUNTER__; \
	RecordDebugEventNoBracket(counter##Name, Event_Data_BlockBegin, __FILE__, #Name, __LINE__) \
	event->data_DebugId = debugid; }
#define DEBUG_END_DATA_BLOCK { \
	RecordDebugEventNoBracket(0, Event_Data_BlockEnd, __FILE__, "DataEndBlock", __LINE__) }
#define DEBUG_DATA(type, data) { \
	RecordDebugEventNoBracket(0, Event_Data_##type, __FILE__, #data, __LINE__); \
	event->data_##type = data; }
#define DEFINE_DEBUG_VARIABLE(type, variable) \
	local_persist DebugEvent variable = *InitializePermanentDebugVariable((variable.data_##type = CONSTANT_##variable, &variable), Event_Data_##type, #variable, __FILE__, __LINE__)
#define DEBUG_IF(variable) \
	DEFINE_DEBUG_VARIABLE(bool, variable); \
	if (variable.data_bool)

#else
#define TIMED_FUNCTION
#define TIMED_BLOCK_BEGIN__(...)
#define TIMED_BLOCK_BEGIN(...)
#define TIMED_BLOCK_END_(...)
#define TIMED_BLOCK_END(...)

#define MARKUP_FRAME_BEGIN
#define MARKUP_FRAME_END

inline DebugId DEBUG_POINTER_ID(void* ptr) { return 0; }
inline void DEBUG_HIT(DebugId did) {}
inline bool DEBUG_HIGHLIGHTED(DebugId did) { return false;}
inline bool DEBUG_DATA_BLOCK_REQUESTED(DebugId did) { return false; }
#define DEBUG_UI_ENABLED 0
#define DEBUG_BEGIN_DATA_BLOCK(...)
#define DEBUG_END_DATA_BLOCK
#define DEBUG_DATA(...)
#define DEFINE_DEBUG_VARIABLE(...)
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
		TIMED_BLOCK_END_(block.counter)
	}
};

// ------------------- DEBUG VARIABLES --------------------

struct DebugEvent;
struct DebugVariableGroup;
struct DebugVariableLink {
	DebugEvent* event;

	DebugVariableLink* next;
	DebugVariableLink* prev;
	DebugVariableLink* parent;
	DebugVariableLink* children;
};

struct DebugTree {
	V2 pos;
	DebugVariableLink root;

	DebugTree* next;
	DebugTree* prev;
};

enum DebugInteractionType {
	DebugInteract_None,

	DebugInteract_Toggle,
	DebugInteract_Resize,
	DebugInteract_Move,
	DebugInteract_DragIncrease,
	DebugInteract_Compile,
	DebugInteract_Tear,
	DebugInteract_Select,
};

struct DebugModifiedPosition {
	V2 initial;
	V2* actual;
};

struct DebugInteraction {
	DebugId id;
	DebugInteractionType type;

	V2 startMousePos;
	Rect2 startBoundingBox;
	DebugTree* relevantTree;
	DebugVariableLink* link;
	union {
		void* generic;
		bool* boolean;
		DebugModifiedPosition pos;
	};
};

struct LoadedFont;
struct FontDrawContext {
	f32 scale;
	V2 leftTopStart;
	V2 leftTopCurrent;
	f32 lineAdvance;
	LoadedFont* font;
};

#if 0
DebugVariable* QueryDebugVariable(DebugVarQueryName query);
#endif