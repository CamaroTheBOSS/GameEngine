#pragma once
#define internal static
#define noapi
#define PI 3.14159f
#define kB(bytes) ((bytes) * 1024)
#define MB(bytes) (kB(bytes) * 1024)
#define GB(bytes) (MB(bytes) * 1024)
#define TB(bytes) (GB(bytes) * 1024)
#define Assert(expression) if (!(expression)) { *(char*)0 = 0; }

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
	int width;
	int height;
	int pitch;
	int bytesPerPixel;
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
	u64 permanentMemorySize;
	void* permanentMemory;
	u64 transientMemorySize;
	void* transientMemory;

	debug_read_entire_file* debugReadEntireFile;
	debug_write_file* debugWriteFile;
	debug_free_file* debugFreeFile;
};

struct ProgramState {
	// Global state of the program
	float offsetVelX;
	float offsetVelY;
	float offsetX;
	float offsetY;
	float toneHz;
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
