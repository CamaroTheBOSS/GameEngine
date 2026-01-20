#pragma once
#include "engine_common.h"
#include "engine_world.h"
#include "engine_simulation.h"
#include "engine_render.h"

// BitmapData has to have 4 bytes per pixel which is defined by BITMAP_BYTES_PER_PIXEL = 4 in engine_render.h
struct BitmapData {
	// Platform independent buffer to render graphics
	void* data;
	u32 width;
	u32 height;
	u32 pitch;
};

struct SoundData {
	// Platform independent buffer to play sounds
	void* data;
	int nSamples;
	int nSamplesPerSec;
	int nChannels;
	float tSine = 0.;
};

struct TimeData {
	// Platform independent struct for everything connected with time like delta between frames.
};

#define MAX_CONTROLLERS 5
#define KB_CONTROLLER_IDX (MAX_CONTROLLERS - 4)
struct Controller {
	// Platform independent user input
	bool isWDown = false;
	bool isSDown = false;
	bool isDDown = false;
	bool isADown = false;
	bool isUpDown = false;
	bool isDownDown = false;
	bool isLeftDown = false;
	bool isRightDown = false;
	bool isSpaceDown = false;
	bool isEscDown = false;

	// Mouse data (<0-1> value relative to window size where 0 is left or top, and 1 is right or bottom)
	f32 mouseX;
	f32 mouseY;
	bool isMouseLDown = false;
	bool isMouseMDown = false;
	bool isMouseRDown = false;
	bool isMouse1BDown = false;
	bool isMouse2BDown = false;
};

struct InputData {
	Controller controllers[MAX_CONTROLLERS];
	f32 dtFrame;
	bool executableReloaded;
};

struct FileData {
	// Platform independent file content
	void* content;
	u64 size;
};

/* Functionalities served by the platform layer for program layer */
#define DEBUG_READ_ENTIRE_FILE(name) FileData name(const char* filename)
#define DEBUG_WRITE_FILE(name) bool name(const char* filename, void* buffer, u64 size)
#define DEBUG_FREE_FILE(name) void name(FileData& file)
typedef DEBUG_READ_ENTIRE_FILE(debug_read_entire_file);
typedef DEBUG_WRITE_FILE(debug_write_file);
typedef DEBUG_FREE_FILE(debug_free_file);
enum DebugPerformanceCountersType {
	DPCT_GameMainLoop,
	DPCT_RenderRectangleSlowly,
	DPCT_RenderRectangleOptimized,
	DPCT_FillPixel
};
struct DebugPerformanceCounters {
	u64 cycles;
	u64 counts;
};
struct DebugMemory {
	debug_read_entire_file* readEntireFile;
	debug_write_file* writeFile;
	debug_free_file* freeFile;
	DebugPerformanceCounters performanceCounters[256];
};
#define BEGIN_TIMED_SECTION(id) u64 startCycleCount_##id = __rdtsc();
#define END_TIMED_SECTION_COUNTED(id, count) debugGlobalMemory->performanceCounters[DPCT_##id].cycles += __rdtsc() - startCycleCount_##id; \
	debugGlobalMemory->performanceCounters[DPCT_##id].counts += count
#define END_TIMED_SECTION_MULTITHREADED(id, count) InterlockedAdd(ptrcast(volatile long, &debugGlobalMemory->performanceCounters[DPCT_##id].cycles), __rdtsc() - startCycleCount_##id); \
	InterlockedAdd(ptrcast(volatile long, &debugGlobalMemory->performanceCounters[DPCT_##id].counts), count)
#define END_TIMED_SECTION(id) END_TIMED_SECTION_COUNTED(id, 1)
DebugMemory* debugGlobalMemory;

// PlatformQueue
struct ThreadContext {
	u32 threadId;
};
struct PlatformQueue;
typedef void (*PlatformQueueCallback)(void* data, ThreadContext& context);
typedef void(*_PlatformWaitForQueueCompletion)(PlatformQueue* queue);
typedef bool(*_PlatformPushTaskToQueue)(PlatformQueue* queue, PlatformQueueCallback callback, void* args);
_PlatformWaitForQueueCompletion PlatformWaitForQueueCompletion;
_PlatformPushTaskToQueue PlatformPushTaskToQueue;


/*----------------------------------------------------------------*/
struct ProgramMemory {
	// Platform independent memory arenas
	// NOTE: memoryBlock is whole memory block whereas the rest of the blocks are blocks inside memoryBlock
	void* memoryBlock;
	u64 memoryBlockSize;
	u64 permanentMemorySize;
	void* permanentMemory;
	u64 transientMemorySize;
	void* transientMemory;

	PlatformQueue* highPriorityQueue;
	PlatformQueue* renderQueue;

	DebugMemory debug;
	_PlatformWaitForQueueCompletion PlatformWaitForQueueCompletion;
	_PlatformPushTaskToQueue PlatformPushTaskToQueue;
};

struct PlayerControls {
	V3 acceleration;
};

struct ProgramState {
	// Global state of the program
	f32 highFreqBoundDim;
	f32 highFreqBoundHeight;
	WorldPosition cameraPos;
	World world;
	PlatformQueue* highPriorityQueue;
	
	u32 playerEntityIndexes[MAX_CONTROLLERS];
	PlayerControls playerControls[MAX_CONTROLLERS];
	u32 cameraEntityIndex;
	LoadedBitmap playerMoveAnim[4];
	LoadedBitmap groundBmps[2];
	LoadedBitmap grassBmps[2];
	LoadedBitmap treeBmp;

	CollisionVolumeGroup* wallCollision;
	CollisionVolumeGroup* playerCollision;
	CollisionVolumeGroup* monsterCollision;
	CollisionVolumeGroup* familiarCollision;
	CollisionVolumeGroup* swordCollision;
	CollisionVolumeGroup* stairsCollision;
	bool isInitialized;

	LoadedBitmap testNormalMap;
	LoadedBitmap testDiffusionTexture;
};

struct GroundBuffer {
	LoadedBitmap buffer;
	WorldPosition pos;
};

struct TransientState {
	MemoryArena arena;
	GroundBuffer groundBuffers[64];
	bool isInitialized;
	PlatformQueue* renderQueue;

	EnvironmentMap topEnvMap;
	EnvironmentMap middleEnvMap;
	EnvironmentMap bottomEnvMap;
};

/* Functionalities served by the program layer for platform layer */
#define GAME_MAIN_LOOP_FRAME(name) void name(ProgramMemory& memory, BitmapData& bitmap, SoundData& soundData, InputData& input)
typedef GAME_MAIN_LOOP_FRAME(game_main_loop_frame);
extern "C" GAME_MAIN_LOOP_FRAME(GameMainLoopFrameStub) {
	float* data = reinterpret_cast<float*>(soundData.data);
	for (int frame = 0; frame < soundData.nSamples; frame++) {
		for (int channel = 0; channel < soundData.nChannels; channel++) {
			*data++ = 0;
		}
	}
}
