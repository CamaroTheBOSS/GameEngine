#include "engine.h"

DebugMemory* debugGlobalMemory;

void RenderFilledRectangleOptimized(LoadedBitmap& bitmap, V2 origin, V2 xAxis, V2 yAxis, V4 color,
	bool even, Rect2i clipRect)
{
	BEGIN_TIMED_SECTION(RenderFilledRectangleOptimized);
	V2 points[4] = {
		origin,
		origin + xAxis,
		origin + yAxis,
		origin + xAxis + yAxis
	};
	f32 fminY = F32_MAX;
	f32 fmaxY = -F32_MAX;
	f32 fminX = F32_MAX;
	f32 fmaxX = -F32_MAX;
	for (u32 pIndex = 0; pIndex < ArrayCount(points); pIndex++) {
		V2 testP = points[pIndex];
		if (testP.Y < fminY) fminY = testP.Y;
		if (testP.Y > fmaxY) fmaxY = testP.Y;
		if (testP.X < fminX) fminX = testP.X;
		if (testP.X > fmaxX) fmaxX = testP.X;
	}
	// TODO: Allocate aligned!!!
	clipRect = Intersection(clipRect, { 0, 0, bitmap.width, bitmap.height });
	Rect2i fillRect = {};
	fillRect.minY = FloorF32ToI32(fminY);
	fillRect.maxY = CeilF32ToI32(fmaxY) + 1;
	fillRect.minX = FloorF32ToI32(fminX);
	fillRect.maxX = CeilF32ToI32(fmaxX) + 1;
	fillRect = Intersection(fillRect, clipRect);
	if (!HasArea(fillRect)) {
		return;
	}
	i32 alignedMinX = AlignDown8(fillRect.minX);
	__m256i startupClipMask = _mm256_set1_epi8(-1);
	if (fillRect.minX != alignedMinX) {
		i32 alignment = fillRect.minX - alignedMinX;
		fillRect.minX = alignedMinX;
		__m128i startupClipMaskLow = _mm_set1_epi8(-1);
		__m128i startupClipMaskHigh = _mm_set1_epi8(-1);
		switch (alignment) {
		case 1: { startupClipMaskLow = _mm_slli_si128(startupClipMaskLow, 1 * 4); } break;
		case 2: { startupClipMaskLow = _mm_slli_si128(startupClipMaskLow, 2 * 4); } break;
		case 3: { startupClipMaskLow = _mm_slli_si128(startupClipMaskLow, 3 * 4); } break;
		case 4: { startupClipMaskLow = _mm_slli_si128(startupClipMaskLow, 4 * 4); } break;
		default: { startupClipMaskLow = _mm_set1_epi8(0); } break;
		}
		switch (alignment) {
		case 5: { startupClipMaskHigh = _mm_slli_si128(startupClipMaskHigh, 1 * 4); } break;
		case 6: { startupClipMaskHigh = _mm_slli_si128(startupClipMaskHigh, 2 * 4); } break;
		case 7: { startupClipMaskHigh = _mm_slli_si128(startupClipMaskHigh, 3 * 4); } break;
		}
		startupClipMask = _mm256_setr_m128i(startupClipMaskLow, startupClipMaskHigh);
	}
	i32 alignedMaxX = AlignUp8(fillRect.maxX);
	__m256i endClipMask = _mm256_set1_epi8(-1);
	if (fillRect.maxX != alignedMaxX) {
		i32 alignment = alignedMaxX - fillRect.maxX;
		fillRect.maxX = alignedMaxX;
		__m128i endClipMaskLow = _mm_set1_epi8(-1);
		__m128i endClipMaskHigh = _mm_set1_epi8(-1);
		switch (alignment) {
		case 1: { endClipMaskHigh = _mm_srli_si128(endClipMaskHigh, 1 * 4); } break;
		case 2: { endClipMaskHigh = _mm_srli_si128(endClipMaskHigh, 2 * 4); } break;
		case 3: { endClipMaskHigh = _mm_srli_si128(endClipMaskHigh, 3 * 4); } break;
		case 4: { endClipMaskHigh = _mm_srli_si128(endClipMaskHigh, 4 * 4); } break;
		default: { endClipMaskHigh = _mm_set1_epi8(0); } break;
		}
		switch (alignment) {
		case 5: { endClipMaskLow = _mm_srli_si128(endClipMaskLow, 1 * 4); } break;
		case 6: { endClipMaskLow = _mm_srli_si128(endClipMaskLow, 2 * 4); } break;
		case 7: { endClipMaskLow = _mm_srli_si128(endClipMaskLow, 3 * 4); } break;
		}
		endClipMask = _mm256_setr_m128i(endClipMaskLow, endClipMaskHigh);
	}
	i32 minY = fillRect.minY;
	i32 maxY = fillRect.maxY;
	i32 minX = fillRect.minX;
	i32 maxX = fillRect.maxX;
	if (even == (minY & 1)) {
		minY++;
	}
	// NOTE: Assume aligned X boundaries and proper clipping
	Assert(((maxX - minX) & 7) == 0);
	Assert(maxX <= clipRect.maxX);
	Assert(minX >= clipRect.minX);
	Assert((minX * BITMAP_BYTES_PER_PIXEL & 31) == 0)
		i32 packsNum = (maxX - minX) >> 3; // Divide by 8

	static_assert(BITMAP_BYTES_PER_PIXEL == 4);
	color.RGB *= color.A;
	f32 uCf = 1.f / (Squared(xAxis.X) + Squared(xAxis.Y));
	f32 vCf = 1.f / (Squared(yAxis.X) + Squared(yAxis.Y));
	__m256i zeroTo7 = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
	__m256 zero = _mm256_set1_ps(0.f);
	__m256 one = _mm256_set1_ps(1.0f);
	__m256 u16max = _mm256_set1_ps(Squared(255.f));
	__m256i onei = _mm256_set1_epi32(1);
	__m256 o255 = _mm256_set1_ps(255.f);
	__m256 inv255wide = _mm256_set1_ps(1.f / 255.f);
	__m256 uCfx8 = _mm256_set1_ps(uCf);
	__m256 vCfx8 = _mm256_set1_ps(vCf);
	__m256 originX = _mm256_set1_ps(origin.X);
	__m256 originY = _mm256_set1_ps(origin.Y);
	__m256 xAX = _mm256_set1_ps(xAxis.X);
	__m256 xAY = _mm256_set1_ps(xAxis.Y);
	__m256 yAX = _mm256_set1_ps(yAxis.X);
	__m256 yAY = _mm256_set1_ps(yAxis.Y);
	__m256i maskFF = _mm256_set1_epi32(0xFF);
	__m256 colorA = _mm256_mul_ps(o255, _mm256_set1_ps(color.A));
	__m256 colorR = _mm256_mul_ps(u16max, _mm256_set1_ps(color.R));
	__m256 colorG = _mm256_mul_ps(u16max, _mm256_set1_ps(color.G));
	__m256 colorB = _mm256_mul_ps(u16max, _mm256_set1_ps(color.B));
	__m256 eight = _mm256_set1_ps(8.f);
	__m256 dxBase = _mm256_set1_ps(f4(minX) - origin.X);
#define E(mm, i) ptrcast(f32, &mm)[i]
#define Ei(mm, i) ptrcast(u32, &mm)[i]
	u32 rowAdvance = 2 * bitmap.pitch;
	u8* row = ptrcast(u8, bitmap.data) + minY * bitmap.pitch + minX * BITMAP_BYTES_PER_PIXEL;
	BEGIN_TIMED_SECTION(FillPixelRectangleRoutine);
	LLVM_MCA_BEGIN(opt_render_filled_rect);
	for (i32 Y = minY; Y < maxY; Y += 2) {
		u32* dstPixel = ptrcast(u32, row);
		__m256 dy = _mm256_set1_ps(f4(Y) + 0.5f - origin.Y);
		__m256 dx = _mm256_add_ps(dxBase, _mm256_setr_ps(0.5f, 1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f));
		__m256i clipMask = startupClipMask;
		for (i32 iter = 0; iter < packsNum; iter++) {
			__m256 u = _mm256_mul_ps(_mm256_add_ps(_mm256_mul_ps(dx, xAX), _mm256_mul_ps(dy, xAY)), uCfx8);
			__m256 v = _mm256_mul_ps(_mm256_add_ps(_mm256_mul_ps(dx, yAX), _mm256_mul_ps(dy, yAY)), vCfx8);
			__m256i writeMask = _mm256_castps_si256(
				_mm256_and_ps(
					_mm256_and_ps(
						_mm256_cmp_ps(u, zero, _CMP_GE_OQ),
						_mm256_cmp_ps(u, one, _CMP_LE_OQ)
					),
					_mm256_and_ps(
						_mm256_cmp_ps(v, zero, _CMP_GE_OQ),
						_mm256_cmp_ps(v, one, _CMP_LE_OQ)
					)
				)
			);
			writeMask = _mm256_and_si256(writeMask, clipMask);

			// Gather ARGB data
			__m256i dest_ARGBi = _mm256_i32gather_epi32(ptrcast(int, dstPixel), zeroTo7, 4);

			// Convert ARGB to individual channels 
			__m256 destA = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(dest_ARGBi, 24), maskFF));
			__m256 destR = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(dest_ARGBi, 16), maskFF));
			__m256 destG = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(dest_ARGBi, 8), maskFF));
			__m256 destB = _mm256_cvtepi32_ps(_mm256_and_si256(dest_ARGBi, maskFF));

			// Dest from SRGB255 to linear255
			destR = _mm256_mul_ps(destR, destR);
			destG = _mm256_mul_ps(destG, destG);
			destB = _mm256_mul_ps(destB, destB);

			// Blend output
			__m256 invAlpha = _mm256_sub_ps(one, _mm256_mul_ps(inv255wide, colorA));
			__m256 outputR = _mm256_add_ps(colorR, _mm256_mul_ps(invAlpha, destR));
			__m256 outputG = _mm256_add_ps(colorG, _mm256_mul_ps(invAlpha, destG));
			__m256 outputB = _mm256_add_ps(colorB, _mm256_mul_ps(invAlpha, destB));
			__m256 outputA = _mm256_add_ps(colorA, _mm256_mul_ps(invAlpha, destA));

			// Back to SRGB255
			outputR = _mm256_sqrt_ps(outputR);
			outputG = _mm256_sqrt_ps(outputG);
			outputB = _mm256_sqrt_ps(outputB);

			__m256i outputRInt = _mm256_cvtps_epi32(outputR);
			__m256i outputGInt = _mm256_cvtps_epi32(outputG);
			__m256i outputAInt = _mm256_cvtps_epi32(outputA);
			__m256i outputARGB = _mm256_cvtps_epi32(outputB);

			// TODO: Use blend instead of masks and check result
			outputARGB = _mm256_add_epi32(outputARGB, _mm256_slli_epi32(outputGInt, 8));
			outputARGB = _mm256_add_epi32(outputARGB, _mm256_slli_epi32(outputRInt, 16));
			outputARGB = _mm256_add_epi32(outputARGB, _mm256_slli_epi32(outputAInt, 24));

			outputARGB = _mm256_add_epi32(
				_mm256_and_si256(writeMask, outputARGB),
				_mm256_andnot_si256(writeMask, dest_ARGBi)
			);
			_mm256_store_si256(ptrcast(__m256i, dstPixel), outputARGB);
			dstPixel += 8;
			dx = _mm256_add_ps(dx, eight);
			if (iter == packsNum - 2) {
				clipMask = endClipMask;
			}
			else {
				clipMask = _mm256_set1_epi8(-1);
			}
		}
		row += rowAdvance;
	}
	LLVM_MCA_END(opt_render_filled_rect);
	u32 pixelCount = 0;
	if (maxY > minY && maxX > minX) {
		pixelCount = ((maxY - minY) * (maxX - minX)) / 2;
	}
	// TODO: This counter is not thread safe!
	END_TIMED_SECTION_COUNTED(FillPixelRectangleRoutine, pixelCount);
	END_TIMED_SECTION(RenderFilledRectangleOptimized);
}


void RenderRectangleOptimized(LoadedBitmap& bitmap, V2 origin, V2 xAxis, V2 yAxis, V4 color,
	LoadedBitmap& texture, bool even, Rect2i clipRect)
{
	BEGIN_TIMED_SECTION(RenderRectangleOptimized);
	V2 points[4] = {
		origin,
		origin + xAxis,
		origin + yAxis,
		origin + xAxis + yAxis
	};
	f32 fminY = F32_MAX;
	f32 fmaxY = -F32_MAX;
	f32 fminX = F32_MAX;
	f32 fmaxX = -F32_MAX;
	for (u32 pIndex = 0; pIndex < ArrayCount(points); pIndex++) {
		V2 testP = points[pIndex];
		if (testP.Y < fminY) fminY = testP.Y;
		if (testP.Y > fmaxY) fmaxY = testP.Y;
		if (testP.X < fminX) fminX = testP.X;
		if (testP.X > fmaxX) fmaxX = testP.X;
	}
	clipRect = Intersection(clipRect, { 0, 0, bitmap.width, bitmap.height });
	Rect2i fillRect = {};
	fillRect.minY = FloorF32ToI32(fminY);
	fillRect.maxY = CeilF32ToI32(fmaxY) + 1;
	fillRect.minX = FloorF32ToI32(fminX);
	fillRect.maxX = CeilF32ToI32(fmaxX) + 1;
	fillRect = Intersection(fillRect, clipRect);
	if (!HasArea(fillRect)) {
		return;
	}
	i32 alignedMinX = AlignDown8(fillRect.minX);
	__m256i startupClipMask = _mm256_set1_epi8(-1);
	if (fillRect.minX != alignedMinX) {
		i32 alignment = fillRect.minX - alignedMinX;
		fillRect.minX = alignedMinX;
		__m128i startupClipMaskLow = _mm_set1_epi8(-1);
		__m128i startupClipMaskHigh = _mm_set1_epi8(-1);
		switch (alignment) {
		case 1: { startupClipMaskLow = _mm_slli_si128(startupClipMaskLow, 1 * 4); } break;
		case 2: { startupClipMaskLow = _mm_slli_si128(startupClipMaskLow, 2 * 4); } break;
		case 3: { startupClipMaskLow = _mm_slli_si128(startupClipMaskLow, 3 * 4); } break;
		case 4: { startupClipMaskLow = _mm_slli_si128(startupClipMaskLow, 4 * 4); } break;
		default: { startupClipMaskLow = _mm_set1_epi8(0); } break;
		}
		switch (alignment) {
		case 5: { startupClipMaskHigh = _mm_slli_si128(startupClipMaskHigh, 1 * 4); } break;
		case 6: { startupClipMaskHigh = _mm_slli_si128(startupClipMaskHigh, 2 * 4); } break;
		case 7: { startupClipMaskHigh = _mm_slli_si128(startupClipMaskHigh, 3 * 4); } break;
		}
		startupClipMask = _mm256_setr_m128i(startupClipMaskLow, startupClipMaskHigh);
	}
	i32 alignedMaxX = AlignUp8(fillRect.maxX);
	__m256i endClipMask = _mm256_set1_epi8(-1);
	if (fillRect.maxX != alignedMaxX) {
		i32 alignment = alignedMaxX - fillRect.maxX;
		fillRect.maxX = alignedMaxX;
		__m128i endClipMaskLow = _mm_set1_epi8(-1);
		__m128i endClipMaskHigh = _mm_set1_epi8(-1);
		switch (alignment) {
		case 1: { endClipMaskHigh = _mm_srli_si128(endClipMaskHigh, 1 * 4); } break;
		case 2: { endClipMaskHigh = _mm_srli_si128(endClipMaskHigh, 2 * 4); } break;
		case 3: { endClipMaskHigh = _mm_srli_si128(endClipMaskHigh, 3 * 4); } break;
		case 4: { endClipMaskHigh = _mm_srli_si128(endClipMaskHigh, 4 * 4); } break;
		default: { endClipMaskHigh = _mm_set1_epi8(0); } break;
		}
		switch (alignment) {
		case 5: { endClipMaskLow = _mm_srli_si128(endClipMaskLow, 1 * 4); } break;
		case 6: { endClipMaskLow = _mm_srli_si128(endClipMaskLow, 2 * 4); } break;
		case 7: { endClipMaskLow = _mm_srli_si128(endClipMaskLow, 3 * 4); } break;
		}
		endClipMask = _mm256_setr_m128i(endClipMaskLow, endClipMaskHigh);
	}
	i32 minY = fillRect.minY;
	i32 maxY = fillRect.maxY;
	i32 minX = fillRect.minX;
	i32 maxX = fillRect.maxX;
	if (even == (minY & 1)) {
		minY++;
	}
	// NOTE: Assume aligned X boundaries and proper clipping
	Assert(((maxX - minX) & 7) == 0);
	Assert(maxX <= clipRect.maxX);
	Assert(minX >= clipRect.minX);
	Assert((minX * BITMAP_BYTES_PER_PIXEL & 31) == 0);
	i32 packsNum = (maxX - minX) >> 3; // Divide by 8

	static_assert(BITMAP_BYTES_PER_PIXEL == 4);
	color.RGB *= color.A;
	f32 uCf = 1.f / (Squared(xAxis.X) + Squared(xAxis.Y));
	f32 vCf = 1.f / (Squared(yAxis.X) + Squared(yAxis.Y));
	u32 pitch = texture.pitch;
	int* textureGatherBase = ptrcast(int, texture.data);

	__m256i zeroTo7 = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
	__m256 zero = _mm256_set1_ps(0.f);
	__m256 half = _mm256_set1_ps(0.5f);
	__m256 one = _mm256_set1_ps(1.0f);
	__m256 u16max = _mm256_set1_ps(Squared(255.f));
	__m256i onei = _mm256_set1_epi32(1);
	__m256 o255 = _mm256_set1_ps(255.f);
	__m256 inv255wide = _mm256_set1_ps(1.f / 255.f);
	__m256 uCfx8 = _mm256_set1_ps(uCf);
	__m256 vCfx8 = _mm256_set1_ps(vCf);
	__m256 originX = _mm256_set1_ps(origin.X);
	__m256 originY = _mm256_set1_ps(origin.Y);
	__m256 xAX = _mm256_set1_ps(xAxis.X);
	__m256 xAY = _mm256_set1_ps(xAxis.Y);
	__m256 yAX = _mm256_set1_ps(yAxis.X);
	__m256 yAY = _mm256_set1_ps(yAxis.Y);
	__m256i maskFF = _mm256_set1_epi32(0xFF);
	// TODO: What with last row and last column?
	__m256 uWcf = _mm256_set1_ps(f4(texture.width - 2));
	__m256 vHcf = _mm256_set1_ps(f4(texture.height - 2));
	__m256 colorA = _mm256_set1_ps(color.A);
	__m256 colorR = _mm256_set1_ps(color.R);
	__m256 colorG = _mm256_set1_ps(color.G);
	__m256 colorB = _mm256_set1_ps(color.B);
	__m256i pitchWide = _mm256_set1_epi32(pitch);
	__m256 eight = _mm256_set1_ps(8.f);
	__m256 dxBase = _mm256_set1_ps(f4(minX) - origin.X);
#define E(mm, i) ptrcast(f32, &mm)[i]
#define Ei(mm, i) ptrcast(u32, &mm)[i]
	u32 rowAdvance = 2 * bitmap.pitch;
	u8* row = ptrcast(u8, bitmap.data) + minY * bitmap.pitch + minX * BITMAP_BYTES_PER_PIXEL;
	BEGIN_TIMED_SECTION(FillPixel);
	LLVM_MCA_BEGIN(opt_render_rect);
	for (i32 Y = minY; Y < maxY; Y += 2) {
		u32* dstPixel = ptrcast(u32, row);
		__m256 dy = _mm256_set1_ps(f4(Y) - origin.Y);
		__m256 dx = _mm256_add_ps(dxBase, _mm256_setr_ps(0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f));
		__m256i clipMask = startupClipMask;
		for (i32 iter = 0; iter < packsNum; iter++) {
			__m256 u = _mm256_mul_ps(_mm256_add_ps(_mm256_mul_ps(dx, xAX), _mm256_mul_ps(dy, xAY)), uCfx8);
			__m256 v = _mm256_mul_ps(_mm256_add_ps(_mm256_mul_ps(dx, yAX), _mm256_mul_ps(dy, yAY)), vCfx8);
			__m256i writeMask = _mm256_castps_si256(
				_mm256_and_ps(
					_mm256_and_ps(
						_mm256_cmp_ps(u, zero, _CMP_GE_OQ),
						_mm256_cmp_ps(u, one, _CMP_LE_OQ)
					),
					_mm256_and_ps(
						_mm256_cmp_ps(v, zero, _CMP_GE_OQ),
						_mm256_cmp_ps(v, one, _CMP_LE_OQ)
					)
				)
			);
			u = _mm256_min_ps(one, _mm256_max_ps(u, zero));
			v = _mm256_min_ps(one, _mm256_max_ps(v, zero));

			writeMask = _mm256_and_si256(writeMask, clipMask);
			__m256 texelX = _mm256_add_ps(_mm256_mul_ps(u, uWcf), half);
			__m256 texelY = _mm256_add_ps(_mm256_mul_ps(v, vHcf), half);
			__m256i texelXint = _mm256_cvttps_epi32(texelX);
			__m256i texelYint = _mm256_cvttps_epi32(texelY);
			__m256 fX = _mm256_sub_ps(texelX, _mm256_cvtepi32_ps(texelXint));
			__m256 fY = _mm256_sub_ps(texelY, _mm256_cvtepi32_ps(texelYint));

			// Calculate memory indicies for gathering bilinear sample
			__m256i mulY_Pitch = _mm256_mullo_epi32(texelYint, pitchWide);
			__m256i sliX_2 = _mm256_slli_epi32(texelXint, 2);
			__m256i mulY_Plus1_Pitch = _mm256_mullo_epi32(_mm256_add_epi32(texelYint, onei), pitchWide);
			__m256i sliX_Plus1_2 = _mm256_slli_epi32(_mm256_add_epi32(texelXint, onei), 2);
			__m256i texelAIndexes = _mm256_add_epi32(mulY_Pitch, sliX_2);
			__m256i texelBIndexes = _mm256_add_epi32(mulY_Pitch, sliX_Plus1_2);
			__m256i texelCIndexes = _mm256_add_epi32(mulY_Plus1_Pitch, sliX_2);
			__m256i texelDIndexes = _mm256_add_epi32(mulY_Plus1_Pitch, sliX_Plus1_2);

			// Gather ARGB data
			__m256i texelA_ARGBi = _mm256_i32gather_epi32(textureGatherBase, texelAIndexes, 1);
			__m256i texelB_ARGBi = _mm256_i32gather_epi32(textureGatherBase, texelBIndexes, 1);
			__m256i texelC_ARGBi = _mm256_i32gather_epi32(textureGatherBase, texelCIndexes, 1);
			__m256i texelD_ARGBi = _mm256_i32gather_epi32(textureGatherBase, texelDIndexes, 1);
			__m256i dest_ARGBi = _mm256_i32gather_epi32(ptrcast(int, dstPixel), zeroTo7, 4);

			// Convert ARGB to individual channels 
			// TODO: should I use unpacks?
			__m256 texelAA = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(texelA_ARGBi, 24), maskFF));
			__m256 texelAR = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(texelA_ARGBi, 16), maskFF));
			__m256 texelAG = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(texelA_ARGBi, 8), maskFF));
			__m256 texelAB = _mm256_cvtepi32_ps(_mm256_and_si256(texelA_ARGBi, maskFF));

			__m256 texelBA = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(texelB_ARGBi, 24), maskFF));
			__m256 texelBR = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(texelB_ARGBi, 16), maskFF));
			__m256 texelBG = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(texelB_ARGBi, 8), maskFF));
			__m256 texelBB = _mm256_cvtepi32_ps(_mm256_and_si256(texelB_ARGBi, maskFF));

			__m256 texelCA = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(texelC_ARGBi, 24), maskFF));
			__m256 texelCR = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(texelC_ARGBi, 16), maskFF));
			__m256 texelCG = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(texelC_ARGBi, 8), maskFF));
			__m256 texelCB = _mm256_cvtepi32_ps(_mm256_and_si256(texelC_ARGBi, maskFF));

			__m256 texelDA = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(texelD_ARGBi, 24), maskFF));
			__m256 texelDR = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(texelD_ARGBi, 16), maskFF));
			__m256 texelDG = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(texelD_ARGBi, 8), maskFF));
			__m256 texelDB = _mm256_cvtepi32_ps(_mm256_and_si256(texelD_ARGBi, maskFF));

			__m256 destA = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(dest_ARGBi, 24), maskFF));
			__m256 destR = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(dest_ARGBi, 16), maskFF));
			__m256 destG = _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(dest_ARGBi, 8), maskFF));
			__m256 destB = _mm256_cvtepi32_ps(_mm256_and_si256(dest_ARGBi, maskFF));

			// srgb255 to linear255
			texelAR = _mm256_mul_ps(texelAR, texelAR);
			texelAG = _mm256_mul_ps(texelAG, texelAG);
			texelAB = _mm256_mul_ps(texelAB, texelAB);

			texelBR = _mm256_mul_ps(texelBR, texelBR);
			texelBG = _mm256_mul_ps(texelBG, texelBG);
			texelBB = _mm256_mul_ps(texelBB, texelBB);

			texelCR = _mm256_mul_ps(texelCR, texelCR);
			texelCG = _mm256_mul_ps(texelCG, texelCG);
			texelCB = _mm256_mul_ps(texelCB, texelCB);

			texelDR = _mm256_mul_ps(texelDR, texelDR);
			texelDG = _mm256_mul_ps(texelDG, texelDG);
			texelDB = _mm256_mul_ps(texelDB, texelDB);

			// BILINEAR LERP
			__m256 texelABA = _mm256_add_ps(texelAA, _mm256_mul_ps(fX, _mm256_sub_ps(texelBA, texelAA)));
			__m256 texelABR = _mm256_add_ps(texelAR, _mm256_mul_ps(fX, _mm256_sub_ps(texelBR, texelAR)));
			__m256 texelABG = _mm256_add_ps(texelAG, _mm256_mul_ps(fX, _mm256_sub_ps(texelBG, texelAG)));
			__m256 texelABB = _mm256_add_ps(texelAB, _mm256_mul_ps(fX, _mm256_sub_ps(texelBB, texelAB)));

			__m256 texelCDA = _mm256_add_ps(texelCA, _mm256_mul_ps(fX, _mm256_sub_ps(texelDA, texelCA)));
			__m256 texelCDR = _mm256_add_ps(texelCR, _mm256_mul_ps(fX, _mm256_sub_ps(texelDR, texelCR)));
			__m256 texelCDG = _mm256_add_ps(texelCG, _mm256_mul_ps(fX, _mm256_sub_ps(texelDG, texelCG)));
			__m256 texelCDB = _mm256_add_ps(texelCB, _mm256_mul_ps(fX, _mm256_sub_ps(texelDB, texelCB)));

			__m256 texelA = _mm256_add_ps(texelABA, _mm256_mul_ps(fY, _mm256_sub_ps(texelCDA, texelABA)));
			__m256 texelR = _mm256_add_ps(texelABR, _mm256_mul_ps(fY, _mm256_sub_ps(texelCDR, texelABR)));
			__m256 texelG = _mm256_add_ps(texelABG, _mm256_mul_ps(fY, _mm256_sub_ps(texelCDG, texelABG)));
			__m256 texelB = _mm256_add_ps(texelABB, _mm256_mul_ps(fY, _mm256_sub_ps(texelCDB, texelABB)));

			// Color modulation
			texelA = _mm256_mul_ps(texelA, colorA);
			texelR = _mm256_mul_ps(texelR, colorR);
			texelG = _mm256_mul_ps(texelG, colorG);
			texelB = _mm256_mul_ps(texelB, colorB);

			// Clamp
			texelA = _mm256_max_ps(texelA, zero);
			texelR = _mm256_max_ps(texelR, zero);
			texelG = _mm256_max_ps(texelG, zero);
			texelB = _mm256_max_ps(texelB, zero);
			texelA = _mm256_min_ps(texelA, o255);
			texelR = _mm256_min_ps(texelR, u16max);
			texelG = _mm256_min_ps(texelG, u16max);
			texelB = _mm256_min_ps(texelB, u16max);

			// Dest from SRGB255 to linear255
			destR = _mm256_mul_ps(destR, destR);
			destG = _mm256_mul_ps(destG, destG);
			destB = _mm256_mul_ps(destB, destB);

			// Blend output
			__m256 invAlpha = _mm256_sub_ps(one, _mm256_mul_ps(inv255wide, texelA));
			__m256 outputR = _mm256_add_ps(texelR, _mm256_mul_ps(invAlpha, destR));
			__m256 outputG = _mm256_add_ps(texelG, _mm256_mul_ps(invAlpha, destG));
			__m256 outputB = _mm256_add_ps(texelB, _mm256_mul_ps(invAlpha, destB));
			__m256 outputA = _mm256_add_ps(texelA, _mm256_mul_ps(invAlpha, destA));

			// Back to SRGB255
			outputR = _mm256_sqrt_ps(outputR);
			outputG = _mm256_sqrt_ps(outputG);
			outputB = _mm256_sqrt_ps(outputB);

			__m256i outputRInt = _mm256_cvtps_epi32(outputR);
			__m256i outputGInt = _mm256_cvtps_epi32(outputG);
			__m256i outputAInt = _mm256_cvtps_epi32(outputA);
			__m256i outputARGB = _mm256_cvtps_epi32(outputB);

			// TODO: Use blend instead of masks and check result
			outputARGB = _mm256_add_epi32(outputARGB, _mm256_slli_epi32(outputGInt, 8));
			outputARGB = _mm256_add_epi32(outputARGB, _mm256_slli_epi32(outputRInt, 16));
			outputARGB = _mm256_add_epi32(outputARGB, _mm256_slli_epi32(outputAInt, 24));
			outputARGB = _mm256_add_epi32(
				_mm256_and_si256(writeMask, outputARGB),
				_mm256_andnot_si256(writeMask, dest_ARGBi)
			);
			_mm256_store_si256(ptrcast(__m256i, dstPixel), outputARGB);
			dstPixel += 8;
			dx = _mm256_add_ps(dx, eight);
			if (iter == packsNum - 2) {
				clipMask = endClipMask;
			}
			else {
				clipMask = _mm256_set1_epi8(-1);
			}
		}

		row += rowAdvance;
	}
	LLVM_MCA_END(opt_render_rect);
	u32 pixelCount = 0;
	if (maxY > minY && maxX > minX) {
		pixelCount = ((maxY - minY) * (maxX - minX)) / 2;
	}
	// TODO: This counter is not thread safe!
	END_TIMED_SECTION_COUNTED(FillPixel, pixelCount);
	END_TIMED_SECTION(RenderRectangleOptimized);
}