#pragma once
#define internal static
#define noapi
#define PI 3.14159f
#define kB(bytes) ((bytes) * 1024)
#define MB(bytes) (kB(bytes) * 1024)
#define GB(bytes) (MB(bytes) * 1024)
#define TB(bytes) (GB(bytes) * 1024)
#define Assert(expression) if (!(expression)) { *(char*)0 = 0; }
#define scast(type, expression) static_cast<type>(expression)
#define ptrcast(type, expression) reinterpret_cast<type*>(expression)
#define ArrayCount(arr, type) (sizeof(arr) / sizeof(type)); 

#include <cstdint>
#include <utility>
#include <math.h>
#include <span>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef float f32;
typedef double f64;

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

struct InputData {
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

struct TilePosition {
	// 28 bytes = chunk pos, 4 bytes = tile pos inside chunk
	u32 absX;
	u32 absY;

	// Pos inside tile in meters
	f32 X;
	f32 Y;
};

struct TileChunkPosition {
	u32 chunkX;
	u32 chunkY;
	u32 chunkZ;

	// tile index relative to chunk
	u32 relTileX;
	u32 relTileY;
};

struct MemoryArena {
	u8* data;
	u64 capacity;
	u64 used;
};

inline internal
void* PushSize_(MemoryArena& arena, u64 size) {
	Assert(arena.used + size <= arena.capacity);
	void* ptr = arena.data + arena.used;
	arena.used += size;
	return ptr;
}
#define PushStructSize(arena, type) PushSize_(arena, sizeof(type))
#define PushArray(arena, length, type) PushSize_(arena, (length) * sizeof(type))

struct TileChunk {
	u32* tiles;
};

struct TileMap {
	u32 chunkCountX;
	u32 chunkCountY;
	u32 chunkCountZ;
	u32 chunkDim;
	u32 chunkShift;
	u32 chunkMask;

	u32 tileCountX;
	u32 tileCountY;
	f32 widthMeters;
	f32 heightMeters;

	TileChunk* tileChunks;
};

struct World {
	TileMap tilemap;
};

struct ProgramState {
	// Global state of the program
	TilePosition playerPos;
	MemoryArena worldArena;
	World world;
	bool isInitialized;
};

#include <stdlib.h>
/* Functionalities served by the program layer for platform layer */
#define GAME_MAIN_LOOP_FRAME(name) void name(ProgramMemory& memory, BitmapData& bitmap, SoundData& soundData, InputData& inputData)
typedef GAME_MAIN_LOOP_FRAME(game_main_loop_frame);
extern "C" GAME_MAIN_LOOP_FRAME(GameMainLoopFrameStub) {
	float* data = reinterpret_cast<float*>(soundData.data);
	for (int frame = 0; frame < soundData.nSamples; frame++) {
		for (int channel = 0; channel < soundData.nChannels; channel++) {
			*data++ = 0;
		}
	}
}
