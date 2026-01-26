#pragma once
#include "engine_common.h"

struct RandomSeries {
	u32 index;
};

inline RandomSeries RandomSeed(u32 seed);
inline u32 NextRandom(RandomSeries& series);
inline f32 RandomUnilateral(RandomSeries& series);
inline f32 RandomBilateral(RandomSeries& series);
inline f32 RandomInRange(RandomSeries& series, f32 min, f32 max);
inline u32 RandomChoice(RandomSeries& series, u32 count);
inline u32 RandomChoiceBetween(RandomSeries& series, u32 min, u32 max);
