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
	u32 nSamples;
	u32 nSamplesPerSec;
	u32 nChannels;
};

struct TimeData {
	// Platform independent struct for everything connected with time like delta between frames.
};

#define MAX_CONTROLLERS 5
#define KB_CONTROLLER_IDX (MAX_CONTROLLERS - 4)

struct Button {
	bool isDown;
	bool wasDown;
};

#pragma warning(push)
#pragma warning(disable : 4201)
struct Controller {
	// Platform independent user input
	union {
		struct Buttons {
			Button kW;
			Button kS;
			Button kD;
			Button kA;
			Button kArrowUp;
			Button kArrowDown;
			Button kArrowLeft;
			Button kArrowRight;
			Button kSpace;
			Button kEsc;
			Button mouseLeft;
			Button mouseRight;
			Button mouseMiddle;
			Button mouse1B;
			Button mouse2B;
		};
		Buttons B;
		Button E[15];
		static_assert(sizeof(B) == sizeof(E));
	};
	// Mouse data (<0-1> value relative to window size where 0 is left or top, and 1 is right or bottom)
	V2 mouse;
};
#pragma warning(pop)

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
// DEBUG API
#define DEBUG_READ_ENTIRE_FILE(name) FileData name(const char* filename)
#define DEBUG_WRITE_FILE(name) bool name(const char* filename, void* buffer, u64 size)
#define DEBUG_FREE_FILE(name) void name(FileData& file)
#define DEBUG_ALLOCATE(name) void* name(u64 size)
typedef DEBUG_READ_ENTIRE_FILE(debug_read_entire_file);
typedef DEBUG_WRITE_FILE(debug_write_file);
typedef DEBUG_FREE_FILE(debug_free_file);
typedef DEBUG_ALLOCATE(debug_allocate);

// File API
struct PlatformFileHandle {};
struct PlatformFileGroup {
	PlatformFileHandle** files;
	u32 count;
};
struct PlatformFileGroupNames {
	char** names;
	u32 count;
};
typedef PlatformFileHandle*		(*_PlatformFileOpen)(const char* filename);
typedef void					(*_PlatformFileClose)(PlatformFileHandle* file);
typedef PlatformFileGroup*		(*_PlatformFileOpenAllWithExtension)(const char* extension);
typedef void					(*_PlatformFileCloseAllInGroup)(PlatformFileGroup* group);
typedef bool					(*_PlatformFileErrors)(PlatformFileHandle* file);
typedef void					(*_PlatformFileRead)(PlatformFileHandle* file, u32 offset, u32 size, void* dst);

// Queue API
struct PlatformQueue;
typedef void	(*PlatformQueueCallback)(void* data);
typedef void	(*_PlatformWaitForQueueCompletion)(PlatformQueue* queue);
typedef bool	(*_PlatformPushTaskToQueue)(PlatformQueue* queue, PlatformQueueCallback callback, void* args);

enum DebugPerformanceCountersType {
	DPCT_GameMainLoop,
	DPCT_RenderRectangleSlowly,
	DPCT_RenderRectangleOptimized,
	DPCT_RenderFilledRectangleOptimized,
	DPCT_FillPixel,
	DPCT_FillPixelRectangleRoutine,
};
struct DebugPerformanceCounters {
	u64 cycles;
	u64 counts;
};
struct DebugMemory {
	debug_read_entire_file* readEntireFile;
	debug_write_file* writeFile;
	debug_free_file* freeFile;
	debug_allocate* allocate;
	DebugPerformanceCounters performanceCounters[256];
};
#define BEGIN_TIMED_SECTION(id) u64 startCycleCount_##id = __rdtsc();
#define END_TIMED_SECTION_COUNTED(id, count) debugGlobalMemory->performanceCounters[DPCT_##id].cycles += __rdtsc() - startCycleCount_##id; \
	debugGlobalMemory->performanceCounters[DPCT_##id].counts += count
#define END_TIMED_SECTION_MULTITHREADED(id, count) InterlockedAdd(ptrcast(volatile long, &debugGlobalMemory->performanceCounters[DPCT_##id].cycles), __rdtsc() - startCycleCount_##id); \
	InterlockedAdd(ptrcast(volatile long, &debugGlobalMemory->performanceCounters[DPCT_##id].counts), count)
#define END_TIMED_SECTION(id) END_TIMED_SECTION_COUNTED(id, 1)
DebugMemory* debugGlobalMemory;

/*----------------------------------------------------------------*/
struct PlatformAPI {
	// Thread Queue API
	_PlatformWaitForQueueCompletion QueueWaitForCompletion;
	_PlatformPushTaskToQueue QueuePushTask;

	// File API
	_PlatformFileOpen FileOpen;
	_PlatformFileClose FileClose;
	_PlatformFileOpenAllWithExtension FileOpenAllWithExtension;
	_PlatformFileCloseAllInGroup FileCloseAllInGroup;
	_PlatformFileErrors FileErrors;
	_PlatformFileRead FileRead;
};
PlatformAPI* Platform;

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
	PlatformQueue* lowPriorityQueue;

	DebugMemory debug;
	PlatformAPI platformAPI;
};

struct PlayerControls {
	V3 acceleration;
};

#define SOUND_SAMPLES_PER_SECOND 48000
struct PlayingSound {
	SoundId soundId;
	f32 currentSample;

	V2 currentVolume;
	V2 requestedVolume;
	V2 volumeChangeSpeed;
	f32 pitch;

	PlayingSound* next;
};

struct AudioState {
	MemoryArena arena;
	PlayingSound* playingSounds;
	PlayingSound* freeListSounds;
};

struct ProgramState {
	// Global state of the program
	f32 highFreqBoundDim;
	f32 highFreqBoundHeight;
	WorldPosition cameraPos;
	MemoryArena mainArena;
	World world;
	
	u32 playerEntityIndexes[MAX_CONTROLLERS];
	PlayerControls playerControls[MAX_CONTROLLERS];
	u32 cameraEntityIndex;

	AudioState audio;

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

enum class GroundBufferState {
	NotReady,
	Pending,
	Ready,
};

struct GroundBuffer {
	LoadedBitmap buffer;
	WorldPosition pos;
	GroundBufferState state;
};

struct TaskWithMemory {
	MemoryArena arena;
	TemporaryMemory memory;
	volatile u32 done;
};

struct TransientState {
	MemoryArena arena;
	GroundBuffer groundBuffers[256];
	bool isInitialized;
	PlatformQueue* lowPriorityQueue;
	PlatformQueue* highPriorityQueue;
	TaskWithMemory tasks[4];
	Assets assets;

	EnvironmentMap topEnvMap;
	EnvironmentMap middleEnvMap;
	EnvironmentMap bottomEnvMap;
};

/* Functionalities served by the program layer for platform layer */
#define GAME_MAIN_LOOP_FRAME(name) void name(ProgramMemory& memory, BitmapData& bitmap, SoundData& soundData, InputData& input)
typedef GAME_MAIN_LOOP_FRAME(game_main_loop_frame);
extern "C" GAME_MAIN_LOOP_FRAME(GameMainLoopFrameStub) {
	f32* data = reinterpret_cast<f32*>(soundData.data);
	for (u32 frame = 0; frame < soundData.nSamples; frame++) {
		for (u32 channel = 0; channel < soundData.nChannels; channel++) {
			*data++ = 0;
		}
	}
}
