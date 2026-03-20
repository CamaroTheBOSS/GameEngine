#include "engine_common.h"

#if !defined(TRANSLATION_UNIT)
#define TRANSLATION_UNIT 0
#endif
#define MAX_DEBUG_EVENTS 262143
#define MAX_DEBUG_FRAMES 120
#define MAX_DEBUG_RECORDS 65535
#define MAX_TRANSLATION_UNIT 3
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
	u32 coreId;
	u32 hitCount;
	u64 cycles;
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

inline
void RecordDebugEvent(u32 counter, DebugEventType type, u32 hitCount = 1) {
	u64 frameAndEventIndex = AtomicAddU64(&debugGlobalState->frameAndEventIndex, 1);
	u32 frameIndex = frameAndEventIndex >> 32;
	u32 eventIndex = frameAndEventIndex && U32_MAX;
	Assert(eventIndex < MAX_DEBUG_EVENTS);
	DebugEvent* event = debugGlobalState->debugEvents[frameIndex] + eventIndex;
	event->debugRecordIndex = counter;
	event->cycles = __rdtscp(&event->coreId);
	event->hitCount = hitCount;
	event->type = type;
	u64 threadLocalGsPtr = __readgsqword(0x30);
	event->threadId = *ptrcast(u32, threadLocalGsPtr + 0x48);
}

#define TIMED_FUNCTION__(line) TimedBlock block##line(__FILE__, __FUNCTION__, __LINE__, __COUNTER__)
#define TIMED_FUNCTION_(line) TIMED_FUNCTION__(line)
#define TIMED_FUNCTION TIMED_FUNCTION_(__LINE__)

#define TIMED_BLOCK_BEGIN__(counter, fileName, name, lineNumber) \
	DebugRecord* record##lineNumber = debugGlobalState->debugRecords[TRANSLATION_UNIT] + counter;	\
	record##lineNumber->file = fileName;																\
	record##lineNumber->blockName = name;														\
	record##lineNumber->line = lineNumber;
#define TIMED_BLOCK_BEGIN_(counter, fileName, name, lineNumber) \
	TIMED_BLOCK_BEGIN__(counter, fileName, name, lineNumber)
#define TIMED_BLOCK_BEGIN(blockName)		\
	u16 counter##blockName = __COUNTER__;	\
	TIMED_BLOCK_BEGIN_(counter##blockName, __FILE__, #blockName, __LINE__)

#define TIMED_BLOCK_END_(counter) \
	RecordDebugEvent(counter, Event_BlockEnd);
#define TIMED_BLOCK_END(blockName) \
	TIMED_BLOCK_END_(counter##blockName)

#define MARKUP_FRAME { \
	u32 newFrameIndex = ((debugGlobalState->frameAndEventIndex >> 32) + 1) % MAX_DEBUG_FRAMES; \
	u64 oldFrameAndEventIndex = AtomicExchangeU64(&debugGlobalState->frameAndEventIndex, u64(newFrameIndex) << 32); \
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

struct TransientState;
struct LoadedBitmap;
void DebugRenderOverlay(TransientState* state, LoadedBitmap& dstBitmap);