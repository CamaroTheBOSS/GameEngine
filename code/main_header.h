#pragma once
#define internal static
#define noapi
#define PI 3.14159

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

struct BitmapData {
	void* data;
	int width;
	int height;
	int pitch;
	int bytesPerPixel;
};

struct SoundData {
	void* data;
	int nSamples;
	int nSamplesPerSec;
	int nChannels;
};

struct InputData {
	bool isWDown = false;
	bool isSDown = false;
	bool isDDown = false;
	bool isADown = false;
	bool isUpDown = false;
	bool isDownDown = false;
};