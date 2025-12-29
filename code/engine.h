#include "engine_common.h"
#include "engine_world.h"
#include "engine_simulation.h"

#define BITMAP_BYTES_PER_PIXEL 4
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

	debug_read_entire_file* debugReadEntireFile;
	debug_write_file* debugWriteFile;
	debug_free_file* debugFreeFile;
};

struct LoadedBitmap {
	void* bufferStart;
	u32* data;
	i32 height;
	i32 width;
	i32 pitch;
	u32 alignX;
	u32 alignY;
};

struct DrawCall {
	LoadedBitmap* bitmap;
	V3 center;
	V3 rectSize;
	f32 R, G, B, A;
	V2 offset;
};

struct DrawCallGroup {
	u32 count;
	DrawCall drawCalls[8];
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
	
	u32 playerEntityIndexes[MAX_CONTROLLERS];
	PlayerControls playerControls[MAX_CONTROLLERS];
	u32 cameraEntityIndex;
	LoadedBitmap playerMoveAnim[4];
	LoadedBitmap groundBmps[2];
	LoadedBitmap grassBmps[2];
	LoadedBitmap cachedGround;

	CollisionVolumeGroup* wallCollision;
	CollisionVolumeGroup* playerCollision;
	CollisionVolumeGroup* monsterCollision;
	CollisionVolumeGroup* familiarCollision;
	CollisionVolumeGroup* swordCollision;
	CollisionVolumeGroup* stairsCollision;
	bool isInitialized;
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
