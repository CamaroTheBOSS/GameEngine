#include "engine_common.h"

#if !defined(TRANSLATION_UNIT)
#define TRANSLATION_UNIT 0
#endif
#define MAX_TRANSLATION_UNIT 3
#define TIMED_BLOCK__(line) TimedBlock block##line(__FILE__, __FUNCTION__, __LINE__, __COUNTER__)
#define TIMED_BLOCK_(line) TIMED_BLOCK__(line)
#define TIMED_BLOCK TIMED_BLOCK_(__LINE__)
static_assert(TRANSLATION_UNIT < MAX_TRANSLATION_UNIT);

struct DebugRecord {
	const char* file;
	const char* function;
	u16 line;
	u64 cycles_hitcount;
};

struct DebugGlobalState {
	DebugRecord debugRecords[MAX_TRANSLATION_UNIT][65536];
	u32 debugRecordsCount[MAX_TRANSLATION_UNIT];
};

extern u32 debugRecordsCount_Main;
extern u32 debugRecordsCount_Optimized;
extern DebugGlobalState* debugGlobalState;

struct TimedBlock {
	u16 id;
	u64 cycles;
	u32 hitcount;

	TimedBlock(const char* file, const char* function, u16 line, u16 id, u32 hitCount = 1) {
		this->id = id;

		DebugRecord* record = debugGlobalState->debugRecords[TRANSLATION_UNIT] + id;
		record->file = file;
		record->function = function;
		record->line = line;

		this->cycles = __rdtsc();
		this->hitcount = hitCount;
	}

	~TimedBlock() {
		DebugRecord* record = debugGlobalState->debugRecords[TRANSLATION_UNIT] + id;
		u64 delta = ((__rdtsc() - this->cycles) << 32) | this->hitcount;
		AtomicAddU64(&record->cycles_hitcount, delta);
	}
};

struct TransientState;
struct LoadedBitmap;
void DebugRenderOverlay(TransientState* state, LoadedBitmap& dstBitmap);