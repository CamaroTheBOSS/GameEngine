#include "engine_common.h"

// ------------------- DEBUG VARIABLES --------------------
enum class DebugVarType : u8 {
	CompileTimeBool,
	CompileTimeFloat,
	CompileTimeCount,

	Bool,
	Float,
	Group,
	CompilationSwitch,
	ProfilerUI,
};

struct DebugProfilerSettings {
	Rect2 rect;
};

struct DebugVariableRef;
struct DebugVariableGroup {
	bool expanded;
	DebugVariableRef* firstChild;
};

struct DebugVariable {
	DebugVarType type;
	char* name;
	union {
		bool boolean;
		f32 fl32;
		DebugProfilerSettings profiler;
		DebugVariableGroup group;
	};
};

struct DebugVariableRef {
	DebugVariable* var;
	DebugVariableRef* next;
	DebugVariableRef* parent;
};

struct DebugVariableContext {
	DebugVariableRef* parent;
};

enum DebugInteraction {
	DebugInteract_None,

	DebugInteract_PressedClick,
	DebugInteract_ReleasedClick,
	DebugInteract_Drag,
	DebugInteract_Hover
};

// ------------------- EVENT PROFILER --------------------
#if !defined(TRANSLATION_UNIT)
#define TRANSLATION_UNIT 0
#endif
#define MAX_DEBUG_EVENTS 962153
#define MAX_DEBUG_FRAMES 40
#define MAX_DEBUG_RECORDS 65535
#define MAX_TRANSLATION_UNIT 3
#define MAX_DEBUG_THREADS 64
#define MAX_STACK_REGIONS 4096
#define DEBUG_CPU_FREQ (2.9f * 1000 * 1000)
#define DEBUG_TARGET_REFRESH_MS 16.6666666f
static_assert(TRANSLATION_UNIT < MAX_TRANSLATION_UNIT);
static_assert(MAX_DEBUG_RECORDS <= U16_MAX);

struct DebugRecord {
	const char* file;
	const char* blockName;
	u32 line;
	u32 reserved;
};

enum DebugEventType : u8 {
	Event_BlockBegin,
	Event_BlockEnd,
};

struct DebugEvent {
	DebugEventType type;
	u32 debugRecordIndex;
	u32 threadId;
	u32 hitCount;
	u32 coreId;
	u32 translationUnit;
	u64 cycles;
};

struct DebugProfilerRegion {
	u32 laneId;
	u32 recordIndex;
	u32 translationUnit;

	f32 minT;
	f32 maxT;
	u64 startCycles;
	u64 endCycles;

	u32 parentRegionIndex;
	DebugRecord* parentRecord;
};

struct DebugProfilerRegionSelection {
	bool selecting;
	u32 laneId;
	u32 recordIndex;
	u32 translationUnit;
};

struct DebugFrameInfo {
	u32 regionsCount;
	DebugProfilerRegion regions[MAX_STACK_REGIONS];
};

struct DebugVariable;
struct DebugGlobalState {
	DebugRecord debugRecords[MAX_TRANSLATION_UNIT][MAX_DEBUG_RECORDS];
	u32 debugRecordsCount[MAX_TRANSLATION_UNIT];

	DebugEvent debugEvents[MAX_DEBUG_FRAMES][MAX_DEBUG_EVENTS];
	u32 debugEventsCount[MAX_DEBUG_FRAMES];

	u64 frameStartCycles[MAX_DEBUG_FRAMES];
	volatile u64 frameAndEventIndex;

	u32 debugVariableCount;
	DebugVariable* debugVariables;
};

extern u32 debugRecordsCount_Main;
extern u32 debugRecordsCount_Optimized;
extern DebugGlobalState* debugGlobalState;

struct OpenDebugEvent {
	DebugEvent* event;
	u32 childRegionCount;
	u16 childRegionIndexes[32];
	OpenDebugEvent* next;
};

struct DebugEventStack {
	u32 threadId;
	u32 laneId;
	OpenDebugEvent* events;
};

#define RecordDebugEvent(counter, eventtype, hitcount) {\
	u64 frameAndEventIndex = AtomicAddU64(&debugGlobalState->frameAndEventIndex, 1);\
	u32 frameIndex = frameAndEventIndex >> 32;\
	u32 eventIndex = frameAndEventIndex & U32_MAX;\
	Assert(eventIndex < MAX_DEBUG_EVENTS);\
	DebugEvent* event = debugGlobalState->debugEvents[frameIndex] + eventIndex;\
	event->debugRecordIndex = counter;\
	event->cycles = __rdtscp(&event->coreId);\
	event->hitCount = hitcount;\
	event->type = eventtype;\
	event->translationUnit = TRANSLATION_UNIT;\
	event->threadId = GetFastThreadId();\
}
#if defined(PROFILER)
#define TIMED_FUNCTION__(line) TimedBlock block##line(__FILE__, __FUNCTION__, __LINE__, __COUNTER__)
#define TIMED_FUNCTION_(line) TIMED_FUNCTION__(line)
#define TIMED_FUNCTION TIMED_FUNCTION_(__LINE__)

#define TIMED_BLOCK_BEGIN__(counter, fileName, name, lineNumber)									\
	DebugRecord* record##lineNumber = debugGlobalState->debugRecords[TRANSLATION_UNIT] + counter;	\
	record##lineNumber->file = fileName;															\
	record##lineNumber->blockName = name;															\
	record##lineNumber->line = lineNumber;															\
	RecordDebugEvent(counter, Event_BlockBegin, 1);
#define TIMED_BLOCK_BEGIN_(counter, fileName, name, lineNumber) \
	TIMED_BLOCK_BEGIN__(counter, fileName, name, lineNumber)
#define TIMED_BLOCK_BEGIN(blockName)		\
	u16 counter##blockName = __COUNTER__;	\
	TIMED_BLOCK_BEGIN_(counter##blockName, __FILE__, #blockName, __LINE__)

#define TIMED_BLOCK_END_(counter) \
	RecordDebugEvent(counter, Event_BlockEnd, 1);
#define TIMED_BLOCK_END(blockName) \
	TIMED_BLOCK_END_(counter##blockName)

#define MARKUP_FRAME_BEGIN \
	debugGlobalState->frameStartCycles[debugGlobalState->frameAndEventIndex >> 32] = __rdtsc();
#define MARKUP_FRAME_END { \
	u32 newFrameIndex = ((debugGlobalState->frameAndEventIndex >> 32) + 1) % MAX_DEBUG_FRAMES;						\
	u64 oldFrameAndEventIndex = AtomicExchangeU64(&debugGlobalState->frameAndEventIndex, u64(newFrameIndex) << 32); \
	u32 oldFrameIndex = oldFrameAndEventIndex >> 32;\
	debugGlobalState->debugEventsCount[oldFrameIndex] = oldFrameAndEventIndex & U32_MAX;}
	
#else
#define TIMED_FUNCTION
#define TIMED_BLOCK_BEGIN__(counter, fileName, name, lineNumber)
#define TIMED_BLOCK_BEGIN(blockName)
#define TIMED_BLOCK_END_(counter)
#define TIMED_BLOCK_END(blockName)
#define MARKUP_FRAME_BEGIN
#define MARKUP_FRAME_END
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

struct LoadedFont;
struct FontDrawContext {
	f32 scale;
	V2 leftTopStart;
	V2 leftTopCurrent;
	LoadedFont* font;
};

struct ProgramMemory;
struct LoadedBitmap;
struct InputData;
void DebugRenderOverlay(ProgramMemory* memory, LoadedBitmap& dstBitmap, InputData& input);
inline void DebugBegin(LoadedBitmap& screenBitmap);