#include "engine_common.h"

#if !defined(TRANSLATION_UNIT)
#define TRANSLATION_UNIT 0
#endif
#define MAX_DEBUG_EVENTS 462143
#define MAX_DEBUG_FRAMES 40
#define MAX_DEBUG_RECORDS 65535
#define MAX_TRANSLATION_UNIT 3
#define MAX_DEBUG_THREADS 64
#define MAX_STACK_REGIONS 128
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
	f32 minT;
	f32 maxT;
	u32 laneId;
	u32 recordIndex;
	u32 translationUnit;
};

struct DebugFrameInfo {
	u64 startCycles;
	u64 endCycles;

	u32 regionsCount;
	DebugProfilerRegion regions[MAX_STACK_REGIONS];
};

struct DebugGlobalState {
	DebugRecord debugRecords[MAX_TRANSLATION_UNIT][MAX_DEBUG_RECORDS];
	u32 debugRecordsCount[MAX_TRANSLATION_UNIT];

	DebugEvent debugEvents[MAX_DEBUG_FRAMES][MAX_DEBUG_EVENTS];
	u32 debugEventsCount[MAX_DEBUG_FRAMES];

	volatile u64 frameAndEventIndex;
};

extern u32 debugRecordsCount_Main;
extern u32 debugRecordsCount_Optimized;
extern DebugGlobalState* debugGlobalState;

struct OpenDebugEvent {
	DebugEvent* event;
	DebugEvent* parent;
	OpenDebugEvent* next;
};

struct DebugEventStack {
	u32 threadId;
	u32 laneId;
	OpenDebugEvent* events;
};

struct DebugState {
	MemoryArena arena;
	TemporaryMemory scratchBuffer;

	u32 eventStacksCount;
	DebugEventStack* eventStacks;
	OpenDebugEvent* openEventFreeList;

	u32 frameReadIndex;
	u32 frameWriteIndex;
	DebugFrameInfo* frames;

	bool paused;
	bool restartRequested;
	bool isInitialized;
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

#define MARKUP_FRAME(frameInfo, cyclesStart, cyclesEnd) { \
	u32 newFrameIndex = ((debugGlobalState->frameAndEventIndex >> 32) + 1) % MAX_DEBUG_FRAMES;						\
	u64 oldFrameAndEventIndex = AtomicExchangeU64(&debugGlobalState->frameAndEventIndex, u64(newFrameIndex) << 32); \
	u32 oldFrameIndex = oldFrameAndEventIndex >> 32;																\
	if (frameInfo) {frameInfo->startCycles = cyclesStart; frameInfo->endCycles = cyclesEnd;}									\
	debugGlobalState->debugEventsCount[oldFrameAndEventIndex >> 32] = oldFrameAndEventIndex & U32_MAX; }

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

struct FontDrawContext {
	f32 scale;
	V2 leftTopStart;
	V2 leftTopCurrent;
	V4 color;
};

struct ProgramMemory;
struct LoadedBitmap;
struct InputData;
void DebugRenderOverlay(ProgramMemory* memory, LoadedBitmap& dstBitmap, InputData& input);