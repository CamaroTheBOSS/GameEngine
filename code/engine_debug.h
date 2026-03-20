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
	Event_FrameMarker,
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
	volatile u32 nextEventIndex;
	volatile u32 frameIndex;
};

extern u32 debugRecordsCount_Main;
extern u32 debugRecordsCount_Optimized;
extern DebugGlobalState* debugGlobalState;

#include <windows.h>
inline
void RecordDebugEvent(u32 counter, DebugEventType type, u32 hitCount = 1) {
	u32 eventIndex = AtomicAddU32(&debugGlobalState->nextEventIndex, 1);
	Assert(eventIndex < MAX_DEBUG_EVENTS);
	DebugEvent* event = debugGlobalState->debugEvents[debugGlobalState->frameIndex] + eventIndex;
	event->debugRecordIndex = counter;
	event->cycles = __rdtsc();
	event->hitCount = hitCount;
	event->type = type;
	__rdtscp(&event->coreId);
	u64 threadLocalGsPtr = __readgsqword(0x30);
	event->threadId = *ptrcast(u32, threadLocalGsPtr + 0x48);
	
}

#define TIMED_BLOCK__(line) TimedBlock block##line(__FILE__, __FUNCTION__, __LINE__, __COUNTER__)
#define TIMED_BLOCK_(line) TIMED_BLOCK__(line)
#define TIMED_BLOCK TIMED_BLOCK_(__LINE__)

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