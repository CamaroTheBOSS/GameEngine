#include "main_header.h"

internal
void AddSineWaveToBuffer(SoundData& dst, float amplitude, float toneHz) {
	static u64 runninngSampleIndex = 0;
	float sampleInterval = static_cast<float>(dst.nSamplesPerSec) / (2 * PI * toneHz);
	float* fData = reinterpret_cast<float*>(dst.data);
	for (int frame = 0; frame < dst.nSamples; frame++) {
		dst.tSine += 1.0f / sampleInterval;
		float value = amplitude * sinf(dst.tSine);
		for (size_t channel = 0; channel < dst.nChannels; channel++) {
			*fData++ = value;
		}
		runninngSampleIndex++;
	}
	if (dst.tSine > 2 * PI * toneHz) {
		// NOTE: sinf() seems to be inaccurate for high tSine values and quantization goes crazy 
		dst.tSine -= 2 * PI * toneHz;
	}
}

internal
void RenderWeirdGradient(BitmapData& bitmap, int xOffset, int yOffset) {
	u8* row = reinterpret_cast<u8*>(bitmap.data);
	for (int y = 0; y < bitmap.height; y++) {
		u8* pixel = reinterpret_cast<u8*>(row);
		for (int x = 0; x < bitmap.width; x++) {
			pixel[0] = static_cast<u8>(x + xOffset);
			pixel[1] = static_cast<u8>(y + yOffset);
			pixel[2] = 0;
			pixel[3] = 0;
			pixel += 4;
		}
		row += bitmap.pitch;
	}
}

extern "C" GAME_MAIN_LOOP_FRAME(GameMainLoopFrame) {
	ProgramState* state = reinterpret_cast<ProgramState*>(memory.permanentMemory);
	if (inputData.isADown) state->offsetVelX-= 0.1f;
	if (inputData.isWDown) state->offsetVelY-= 0.1f;
	if (inputData.isSDown) state->offsetVelY+= 0.1f;
	if (inputData.isDDown) state->offsetVelX+= 0.1f;
	if (inputData.isUpDown) state->toneHz++;
	if (inputData.isDownDown) state->toneHz--;
	state->offsetX += state->offsetVelX;
	state->offsetY += state->offsetVelY;

	static bool fileRead = false;
	if (!fileRead) {
		FileData file = memory.debugReadEntireFile(__FILE__);
		if (file.content) {
			memory.debugWriteFile("some_other_file.cpp", file.content, file.size);
			memory.debugFreeFile(file);
		}
		fileRead = true;
	}

	AddSineWaveToBuffer(soundData, 0.05f, state->toneHz);
	RenderWeirdGradient(bitmap, static_cast<int>(state->offsetX), static_cast<int>(state->offsetY));
}