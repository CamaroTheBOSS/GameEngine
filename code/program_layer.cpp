#include "main_header.h"

internal
void AddSineWaveToBuffer(SoundData& dst, float amplitude, float toneHz) {
	/*UINT32 padding = 0;
	globalSoundData.audio->GetCurrentPadding(&padding);
	UINT32 framesAvailable = dst.bufferSizeInSamples - padding;
	if (framesAvailable == 0) {
		return;
	}*/

	//void* bufferData = nullptr;
	//HRESULT hr = globalSoundData.renderer->GetBuffer(framesAvailable, reinterpret_cast<BYTE**>(&bufferData));
	//if (!SUCCEEDED(hr)) {
	//	// TODO: log error
	//	return;
	//}

	static float tSine = 0.;
	static u64 runninngSampleIndex = 0;
	float sampleInterval = static_cast<float>(dst.nSamplesPerSec) / (2 * PI * toneHz);
	float* fData = reinterpret_cast<float*>(dst.data);
	for (int frame = 0; frame < dst.nSamples; frame++) {
		tSine += 1.0f / sampleInterval;
		float value = amplitude * sinf(tSine);
		for (size_t channel = 0; channel < dst.nChannels; channel++) {
			*fData++ = value;
		}
		runninngSampleIndex++;
	}
	//hr = globalSoundData.renderer->ReleaseBuffer(framesAvailable, 0);
	//if (!SUCCEEDED(hr)) {
	//	// TODO log error with _com_error err.ErrorMessage()
	//	return;
	//}
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

void GameMainLoopFrame(BitmapData& bitmap, SoundData& soundData, InputData& inputData) {
	static float offsetVelX = 0.;
	static float offsetVelY = 0.;
	static float offsetX = 0;
	static float offsetY = 0;
	static float toneHz = 255.;

	if (inputData.isADown) offsetVelX-= 0.1;
	if (inputData.isWDown) offsetVelY-= 0.1;
	if (inputData.isSDown) offsetVelY+= 0.1;
	if (inputData.isDDown) offsetVelX+= 0.1;
	if (inputData.isUpDown) toneHz++;
	if (inputData.isDownDown) toneHz--;
	offsetX += offsetVelX;
	offsetY += offsetVelY;

	AddSineWaveToBuffer(soundData, 0.05, toneHz);
	RenderWeirdGradient(bitmap, offsetX, offsetY);
}