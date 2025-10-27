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

struct ProgramMemory {
	// Platform independent memory arenas
	u64 permanentMemorySize;
	void* permanentMemory;
	u64 transientMemorySize;
	void* transientMemory;
};

struct FileData {
	// Platform independent file content
	void* content;
	u64 size;
};

/* Functionalities served by the platform layer for program layer */
internal FileData DebugReadEntireFile(const char* filename);
internal bool DebugWriteToFile(const char* filename, void* buffer, u64 size);

struct ProgramState {
	// Global state of the program
	float offsetVelX;
	float offsetVelY;
	float offsetX;
	float offsetY;
	float toneHz;
};