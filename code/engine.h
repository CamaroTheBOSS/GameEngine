#pragma once
#include "engine_platform.h"
#include "engine_world.h"
#include "engine_simulation.h"
#include "engine_render.h"
#include "engine_rand.h"
#include "engine_assets.h"

struct PlayerControls {
	V3 acceleration;
};

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

	RandomSeries generalEntropy;
	RandomSeries effectsEntropy;

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

struct DebugState {
	MemoryArena mainArena;
	MemoryArena collationArena;
	TemporaryMemory collationScratchBuffer;
	RenderGroup renderGroup;
	PlatformQueue* highPriorityQueue;

	// Event Collation
	u32 eventStacksCount;
	DebugEventStack* eventStacks;
	OpenDebugEvent* openEventFreeList;

	DebugVariableRef* UITree;

	// Profiler
	u32 frameReadIndex;
	u32 frameWriteIndex;
	DebugFrameInfo* frames;
	DebugRecord* hotRecord;
	u32 hotRegionIndex;
	u32 hotFrameIndex;
	DebugRecord* selectedRecord;
	u32 selectedRegionIndex;
	u32 selectedFrameIndex;
	DebugVariableRef* profilerPause;

	FontDrawContext fontContext;
	PlatformCommandHandle compilationHandle;
	Rect2 overlayBoundaries;
	LoadedFont* font;
	
	u32 variableCount;
	DebugVariable variables[128];
	DebugInteraction interaction;
	DebugInteraction nextHotInteraction;
	bool interacting;

	bool isInitialized;
};

inline
TaskWithMemory* TryBeginBackgroundTask(TransientState* tranState) {
	AssertMainThread;
	TaskWithMemory* result = 0;
	for (u32 taskIndex = 0; taskIndex < ArrayCount(tranState->tasks); taskIndex++) {
		TaskWithMemory* task = tranState->tasks + taskIndex;
		if (AtomicCompareExchange(&task->done, 0, 1)) {
			CheckArena(task->arena);
			task->memory = BeginTempMemory(task->arena);
			result = task;
			break;
		}
	}
	return result;
}

inline
void EndBackgroundTask(TaskWithMemory* task) {
	EndTempMemory(task->memory);
	WriteCompilatorFence;
	task->done = true;
}
