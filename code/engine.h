#include "engine_common.h"
#include "engine_world.h"

struct BitmapData {
	// Platform independent buffer to render graphics
	void* data;
	u32 width;
	u32 height;
	u32 pitch;
	u32 bytesPerPixel;
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

	// Mouse data
	int mouseX;
	int mouseY;
	bool isMouseLDown = false;
	bool isMouseMDown = false;
	bool isMouseRDown = false;
	bool isMouse1BDown = false;
	bool isMouse2BDown = false;
	
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
	u32* data;
	u32 height;
	u32 width;
	u32 bytesPerPixel;
	u32 alignX;
	u32 alignY;
};

struct HighEntity {
	V2 pos;
	V2 vel;
	u32 lowEntityIndex;
};

struct LowEntity {
	TilePosition pos;
	V2 size;
	u32 faceDir;
	u32 highEntityIndex;
};

struct Entity {
	TilePosition pos;
	V2 size;
	V2 vel;
	u32 faceDir;
};

struct World {
	TileMap tilemap;
};

struct ProgramState {
	// Global state of the program
	TilePosition cameraPos;
	MemoryArena worldArena;
	World world;

	u32 entityCount;
	Entity entities[256];
	u32 highEntityCount;
	HighEntity highEntities[256];
	u32 lowEntityCount;
	LowEntity lowEntities[4096];

	u32 playerEntityIndexes[MAX_CONTROLLERS];
	u32 cameraEntityIndex;
	LoadedBitmap playerMoveAnim[4];
	
	bool isInitialized;
};

#include <stdlib.h>
/* Functionalities served by the program layer for platform layer */
#define GAME_MAIN_LOOP_FRAME(name) void name(ProgramMemory& memory, BitmapData& bitmap, SoundData& soundData, Controller* controllers)
typedef GAME_MAIN_LOOP_FRAME(game_main_loop_frame);
extern "C" GAME_MAIN_LOOP_FRAME(GameMainLoopFrameStub) {
	float* data = reinterpret_cast<float*>(soundData.data);
	for (int frame = 0; frame < soundData.nSamples; frame++) {
		for (int channel = 0; channel < soundData.nChannels; channel++) {
			*data++ = 0;
		}
	}
}
