#include "main_header.h"

internal
void AddSineWaveToBuffer(SoundData& dst, float amplitude, float toneHz) {
	static float tSine = 0.;
	static u64 runninngSampleIndex = 0;
	float sampleInterval = static_cast<float>(dst.nSamplesPerSec) / (2 * PI * toneHz);
	float* fData = reinterpret_cast<float*>(dst.data);
	if (tSine > 40000.) {
		// NOTE: sinf() seems to be inaccurate for high tSine values and quantization goes crazy 
		tSine = 0;
	}
	for (int frame = 0; frame < dst.nSamples; frame++) {
		tSine += 1.0f / sampleInterval;
		float value = amplitude * sinf(tSine);
		for (size_t channel = 0; channel < dst.nChannels; channel++) {
			*fData++ = value;
		}
		runninngSampleIndex++;
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

void GameMainLoopFrame(ProgramMemory& memory, BitmapData& bitmap, SoundData& soundData, InputData& inputData) {
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
		FileData file = DebugReadEntireFile("sds");
		if (file.content) {
			DebugWriteToFile("some_other_file.cpp", file.content, file.size);
		}
		fileRead = true;
	}

	AddSineWaveToBuffer(soundData, 0.05f, state->toneHz);
	RenderWeirdGradient(bitmap, static_cast<int>(state->offsetX), static_cast<int>(state->offsetY));
}