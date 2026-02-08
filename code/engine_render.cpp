#include "engine.h"
#include "engine_render.h"
#include "engine_intrinsics.h"
#include "engine_rand.h"

struct EntityProjectedParams {
	V2 center;
	V2 size;
	bool valid;
};

internal
EntityProjectedParams CalculatePerspectiveProjection(ProjectionProps& projection,
	V3 entityCenter, V2 entitySize)
{
	EntityProjectedParams result = {};
	if (projection.orthographic) {
		result.center = projection.screenCenter + entityCenter.XY * projection.metersToPixels;
		result.size = entitySize * projection.metersToPixels;
		result.valid = true;
	}
	else {
		V3 rawCoords = ToV3(entityCenter.XY, 1.f);
		f32 denominator = projection.camera.distanceToTarget - entityCenter.Z;
		if (denominator > projection.camera.nearClip) {
			V3 projectedCoords = projection.camera.focalLength * rawCoords / denominator;
			result.center = projection.screenCenter + projectedCoords.XY * projection.metersToPixels;
			result.size = projectedCoords.Z * entitySize * projection.metersToPixels;
			result.valid = true;
		}
	}
	
	return result;
}

inline
V4 SRGB255ToLinear1(V4 input) {
	V4 result = {};
	f32 inv255 = 1.f / 255.f;
	result.R = Squared(inv255 * input.R);
	result.G = Squared(inv255 * input.G);
	result.B = Squared(inv255 * input.B);
	result.A = inv255 * input.A;
	return result;
}

inline
V4 Linear1ToSRGB255(V4 input) {
	V4 result = {};
	result.R = 255.f * SquareRoot(input.R);
	result.G = 255.f * SquareRoot(input.G);
	result.B = 255.f * SquareRoot(input.B);
	result.A = 255.f * input.A;
	return result;
}

inline
V4 Unpack4x8(u32* pixel) {
	V4 result = { f4((*pixel >> 16) & 0xFF),
				  f4((*pixel >> 8) & 0xFF),
				  f4((*pixel >> 0) & 0xFF),
				  f4((*pixel >> 24) & 0xFF)};
	return result;
}

inline
V4 Sample4x8FromTexture(LoadedBitmap& texture, u32 X, u32 Y) {
	i32 texelIndex = Y * texture.pitch + X * BITMAP_BYTES_PER_PIXEL;
	u32* texelPtr = ptrcast(u32, ptrcast(u8, texture.data) + texelIndex);
	V4 result = Unpack4x8(texelPtr);
	return result;
}

struct BilinearSample {
	V4 t00, t01;
	V4 t10, t11;
};

inline
BilinearSample SampleBilinearFromTexture(LoadedBitmap& texture, u32 X, u32 Y) {
	Assert(X >= 0 && X < u4(texture.width - 1));
	Assert(Y >= 0 && Y < u4(texture.height - 1));
	BilinearSample result = {};
	result.t00 = Sample4x8FromTexture(texture, X, Y);
	result.t01 = Sample4x8FromTexture(texture, X + 1, Y);
	result.t10 = Sample4x8FromTexture(texture, X, Y + 1);
	result.t11 = Sample4x8FromTexture(texture, X + 1, Y + 1);
	return result;
}

inline
V4 BilinearLerp(LoadedBitmap& texture, BilinearSample sample, f32 fX, f32 fY) {
	sample.t00 = SRGB255ToLinear1(sample.t00);
	sample.t01 = SRGB255ToLinear1(sample.t01);
	sample.t10 = SRGB255ToLinear1(sample.t10);
	sample.t11 = SRGB255ToLinear1(sample.t11);
	V4 result = Lerp(
		Lerp(sample.t00, fX, sample.t01),
		fY,
		Lerp(sample.t10, fX, sample.t11)
	);
	return result;
}

inline
V4 SampleEnvMap(EnvironmentMap envMap, V2 screenSpaceUV, V3 rayDirection) {
	//TODO: Finish this function
	f32 distanceToMapInZMeters = 3.0f;
	f32 metersToUVs = 0.01f;
	LoadedBitmap* LOD = envMap.LOD;

	V2 startP = screenSpaceUV; // TODO: Should it be changed not to sample one screen to one env map?
#if 0
	V2 offsetP = metersToUVs * distanceToMapInZMeters * rayDirection.XY * Length(rayDirection.XY) * rayDirection.Z;

#else
	// TODO: How rayCastZ affects actual results of lighting?
	f32 rayCastZ = 0.f;
	f32 len = (distanceToMapInZMeters - rayCastZ) / rayDirection.Z;
	V2 offsetP = metersToUVs * len * rayDirection.XY;
#endif
	V2 sum = startP + offsetP;
	V2 sampleUV = Clip01(sum);
	if (sampleUV.Y < 0.4) {
		int breakHere = 0;
	}
	f32 Xf = sampleUV.X * (envMap.LOD[0].width - 2);
	f32 Yf = sampleUV.Y * (envMap.LOD[0].height - 2);
	u32 X = u4(Xf);
	u32 Y = u4(Yf);
	f32 fX = Xf - X;
	f32 fY = Yf - Y;
#if 0 // Draw sampled values on envMap;
	u32 colorU32 = (255 << 24) |
		(X << 16) |
		(Y << 8) |
		(X << 0);
	u8* row = ptrcast(u8, LOD->data) + Y * LOD->pitch + X * BITMAP_BYTES_PER_PIXEL;
	u32* pixel = ptrcast(u32, row);
	*pixel = colorU32;
#endif
	BilinearSample sample = SampleBilinearFromTexture(*LOD, X, Y);
	V4 lightProbe = BilinearLerp(*LOD, sample, fX, fY);
	return lightProbe;
}

internal
void RenderRectangleTransparent(LoadedBitmap& bitmap, V2 start, V2 end, V4 color, bool even, Rect2i clipRect) {
	Rect2i fillRect;
	fillRect.minY = FloorF32ToI32(start.Y);
	fillRect.maxY = CeilF32ToI32(end.Y) + 1;
	fillRect.minX = FloorF32ToI32(start.X);
	fillRect.maxX = CeilF32ToI32(end.X) + 1;
	clipRect = Intersection(clipRect, { 0, 0, bitmap.width, bitmap.height });
	fillRect = Intersection(fillRect, clipRect);
	i32 minY = fillRect.minY;
	i32 maxY = fillRect.maxY;
	i32 minX = fillRect.minX;
	i32 maxX = fillRect.maxX;
	if (even == (minY & 1)) {
		minY++;
	}
	u8* dstRow = ptrcast(u8, bitmap.data) + minY * bitmap.pitch + minX * BITMAP_BYTES_PER_PIXEL;
	u32 rowAdvance = 2 * bitmap.pitch;
	color.RGB *= color.A;
	for (i32 Y = minY; Y < maxY; Y += 2) {
		u32* dstPixel = ptrcast(u32, dstRow);
		for (i32 X = minX; X < maxX; X++) {
			V4 dest = {
					f4((*dstPixel >> 16) & 0xFF),
					f4((*dstPixel >> 8) & 0xFF),
					f4((*dstPixel >> 0) & 0xFF),
					f4((*dstPixel >> 24) & 0xFF)
			};
			f32 inv255 = 1.f / 255.f;
			dest.R = Squared(inv255 * dest.R);
			dest.G = Squared(inv255 * dest.G);
			dest.B = Squared(inv255 * dest.B);
			dest.A = inv255 * dest.A;
			V4 output = {
					color.R + (1 - color.A) * dest.R,
					color.G + (1 - color.A) * dest.G,
					color.B + (1 - color.A) * dest.B,
					color.A + dest.A - color.A * dest.A
			};

			output = Linear1ToSRGB255(output);
			*dstPixel = (u4(output.A + 0.5f) << 24) |
				(u4(output.R + 0.5f) << 16) |
				(u4(output.G + 0.5f) << 8) |
				(u4(output.B + 0.5f) << 0);

			dstPixel++;
		}
		dstRow += rowAdvance;
	}
}

internal
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


// TODO: Do I really need Opaque version for faster rendering when I know alpha is 255.f?
internal
void RenderRectangleOpaque(LoadedBitmap& bitmap, V2 start, V2 end, V3 color) {
	i32 minX = RoundF32ToI32(start.X);
	i32 maxX = RoundF32ToI32(end.X);
	i32 minY = RoundF32ToI32(start.Y);
	i32 maxY = RoundF32ToI32(end.Y);
	if (minX < 0) {
		minX = 0;
	}
	if (minY < 0) {
		minY = 0;
	}
	if (maxX > scast(i32, bitmap.width)) {
		maxX = scast(i32, bitmap.width);
	}
	if (maxY > scast(i32, bitmap.height)) {
		maxY = scast(i32, bitmap.height);
	}
	u8* dstRow = ptrcast(u8, bitmap.data) + minY * bitmap.pitch + minX * BITMAP_BYTES_PER_PIXEL;
	u32 colorU32 = (255 << 24) |
				   (u4(255.f * color.R + 0.5f) << 16) |
				   (u4(255.f * color.G + 0.5f) << 8) |
				   (u4(255.f * color.B + 0.5f) << 0);
	for (i32 Y = minY; Y < maxY; Y++) {
		u32* dstPixel = ptrcast(u32, dstRow);
		for (i32 X = minX; X < maxX; X++) {
			*dstPixel++ = colorU32;
		}
		dstRow += bitmap.pitch;
	}
}


internal
void RenderRectangleSlowly(LoadedBitmap& bitmap, V2 origin, V2 xAxis, V2 yAxis, V4 color,
	LoadedBitmap& texture, LoadedBitmap* normalMap, EnvironmentMap* topMap, 
	EnvironmentMap* middleMap, EnvironmentMap* bottomMap)
{
	BEGIN_TIMED_SECTION(RenderRectangleSlowly);
	u32 colorU32 =  (scast(u32, 255 * color.A) << 24) +
					(scast(u32, 255 * color.R) << 16) +
					(scast(u32, 255 * color.G) << 8) +
					(scast(u32, 255 * color.B) << 0);
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
	i32 minY = RoundF32ToI32(fminY);
	i32 maxY = RoundF32ToI32(fmaxY);
	i32 minX = RoundF32ToI32(fminX);
	i32 maxX = RoundF32ToI32(fmaxX);
	if (minY < 0) minY = 0;
	if (maxY > bitmap.height) maxY = bitmap.height;
	if (minX < 0) minX = 0;
	if (maxX > bitmap.width) maxX = bitmap.width;
	
	f32 xAxisLength = Length(xAxis);
	f32 yAxisLength = Length(yAxis);
	// TODO: think about it, is it screen space responsible that this equation is
	// actually p'=px*X*|X|/|Y| + py*Y*|Y|/|X| instead of 
	//			p'=px*X*|Y|/|X| + py*Y*|X|/|Y| ??????
	V2 xScaleRotNormalizer = Normalize(xAxis) * (xAxisLength / yAxisLength);
	V2 yScaleRotNormalizer = Normalize(yAxis) * (yAxisLength / xAxisLength);
	f32 testCoefficient = (xAxisLength / yAxisLength);
	
	u8* row = ptrcast(u8, bitmap.data) + minY * bitmap.pitch + minX * BITMAP_BYTES_PER_PIXEL;
	BEGIN_TIMED_SECTION(FillPixel);
	for (i32 Y = minY; Y < maxY; Y++) {
		u32* dstPixel = ptrcast(u32, row);
		for (i32 X = minX; X < maxX; X++) {
			V2 point = V2i(X, Y) + V2{ 0.5f, 0.5f };
			V2 d = point - origin;
			f32 edge0 = Inner(d, Perp(yAxis));
			f32 edge1 = Inner(d, -Perp(xAxis));
			f32 edge2 = Inner(d - xAxis, -Perp(yAxis));
			f32 edge3 = Inner(d - xAxis - yAxis, Perp(xAxis));
			if (edge0 < 0 &&
				edge1 < 0 &&
				edge2 < 0 &&
				edge3 < 0) {
				V2 screenSpaceUV = V2{ f4(X) / f4(bitmap.width - 1),
									   f4(Y) / f4(bitmap.height - 1) };
				f32 u = Inner(d, xAxis) / (LengthSq(xAxis));
				f32 v = Inner(d, yAxis) / (LengthSq(yAxis));
				// TODO: U and V values should be clipped 
				Assert(u >= -0.0012 && u <= 1.0012f);
				Assert(v >= -0.0012 && v <= 1.0012f);
				// TODO: What with last row and last column?
				V2 texelVec = V2{
					 u * (texture.width - 2),
					 v * (texture.height - 2)
				};
				i32 texelX = u4(texelVec.X);
				i32 texelY = u4(texelVec.Y);
				V2 frac = V2{
					texelVec.X - texelX,
					texelVec.Y - texelY
				};

				BilinearSample texelBSample = SampleBilinearFromTexture(texture, texelX, texelY);
				V4 texel = BilinearLerp(texture, texelBSample, frac.X, frac.Y);
				if (normalMap) {
					if (u > 0.49f && u < 0.51f &&
						v > 0.49f && v < 0.51f) {
						int breakHere = 5;
					}
					f32 uSq = Squared(u);
					f32 vSq = Squared(v);
					f32 sumUVSq = uSq + vSq;
					if (sumUVSq > 0.99f && sumUVSq < 1.01f) {
						int breakHere = 5;
					}
					BilinearSample normalBSample = SampleBilinearFromTexture(*normalMap, texelX, texelY);
					normalBSample.t00.RGB -= 127.f * V3{ 1, 1, 1 };
					normalBSample.t00.RGB = normalBSample.t00.RGB / 127.f;
					normalBSample.t01.RGB -= 127.f * V3{ 1, 1, 1 };
					normalBSample.t01.RGB = normalBSample.t01.RGB / 127.f;
					normalBSample.t10.RGB -= 127.f * V3{ 1, 1, 1 };
					normalBSample.t10.RGB = normalBSample.t10.RGB / 127.f;
					normalBSample.t11.RGB -= 127.f * V3{ 1, 1, 1 };
					normalBSample.t11.RGB = normalBSample.t11.RGB / 127.f;
					V4 normal = Lerp(
						Lerp(normalBSample.t00, frac.X, normalBSample.t01),
						frac.Y,
						Lerp(normalBSample.t10, frac.X, normalBSample.t11)
					);
					normal.XYZ = Normalize(normal.XYZ);
					normal.XY = normal.X * xScaleRotNormalizer + normal.Y * yScaleRotNormalizer;
					normal.XYZ = Normalize(normal.XYZ);
					// TODO: Test this Note;
					// NOTE: Insetad of swapping normal.Y with normal.Z it is better to
					// swap 'e' vector (POV) Y and Z. This way we can work with env maps
					// as they were actually aligned with the Z axis and all the objects 
					// looking in front of them
					// |-------- ENVMAP --------|
					//           ^^^ <-normal vectors
					//          _|||_     e vector
					// |________|obj|<-------------
					V3 e = V3{ 0, 1, 0 }; // Assume top-down view // TODO: optimize this equation
					V3 rayDirection = -e + 2 * Inner(e, normal.RGB) * normal.RGB;
					f32 intensity = 0.f;
					EnvironmentMap* map = 0;
					// TODO: If I want to change these conditions to other vales I need
					// to have different intensity function to map intensity properly
					// from 0 to 1
					// TODO: Add middle map and overlap between maps
					// TODO: based on the ANGLE of coord system, I think we should
					// have different conditions here or sth
					// TODO: Shapes with different size across axis X and Y are somehow
					// busted, we need to take a look
					if (normal.Y < -0.5f) {
						intensity = -2.f * normal.Y - 1.f;
						map = topMap;
					}
					else if (normal.Y > 0.5f) {
						intensity = 2.f * normal.Y - 1.f;
						map = bottomMap;
					}
					else {

					}
					
					V4 lightProbe = {};
					if (map && texel.A > 0) {
						lightProbe = SampleEnvMap(*map, screenSpaceUV, rayDirection);
						lightProbe.RGB *= intensity;
					}
#if 1
					texel.RGB += texel.A * lightProbe.RGB;
#else
					texel.RGB = texel.A * Abs(normal.RGB);
#endif
#if 0
					texel.RGB = V3{ 0, 0, 0 };
					texel.R = texel.A * rayDirection.Z;
					//texel.RGB = texel.A * Abs(rayDirection);
					
#endif
#if 0
					texel.RGB = V3{1, 1, 1} * normal.G;
#endif
				}

				V4 dest = Unpack4x8(dstPixel);
				dest = SRGB255ToLinear1(dest);

				V4 output = {
					texel.R + (1 - texel.A) * dest.R,
					texel.G + (1 - texel.A) * dest.G,
					texel.B + (1 - texel.A) * dest.B,
					texel.A + dest.A - texel.A * dest.A
				};
				output = Linear1ToSRGB255(output);
				*dstPixel = (u4(output.A + 0.5f) << 24) | 
							(u4(output.R + 0.5f) << 16) | 
							(u4(output.G + 0.5f) << 8) | 
							(u4(output.B + 0.5f) << 0);
			}
			dstPixel++;
		}
		row += bitmap.pitch;
	}
	END_TIMED_SECTION_COUNTED(FillPixel, (maxY - minY) * (maxX - minX));
	END_TIMED_SECTION(RenderRectangleSlowly);
}

internal
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

internal
void RenderBitmap(LoadedBitmap& screenBitmap, LoadedBitmap& loadedBitmap, V2 position) {
	i32 minX = RoundF32ToI32(position.X);
	i32 maxX = minX + loadedBitmap.width;
	i32 minY = RoundF32ToI32(position.Y);
	i32 maxY = minY + loadedBitmap.height;
	i32 offsetX = 0;
	i32 offsetY = 0;
	if (minX < 0) {
		offsetX = -minX;
		minX = 0;
	}
	if (minY < 0) {
		offsetY = -minY;
		minY = 0;
	}
	if (maxX > scast(i32, screenBitmap.width)) {
		maxX = scast(i32, screenBitmap.width);
	}
	if (maxY > scast(i32, screenBitmap.height)) {
		maxY = scast(i32, screenBitmap.height);
	}
	u8* dstRow = ptrcast(u8, screenBitmap.data) + minY * screenBitmap.pitch + minX * BITMAP_BYTES_PER_PIXEL;
	u8* srcRow = ptrcast(u8, loadedBitmap.data) + offsetY * loadedBitmap.pitch + offsetX * BITMAP_BYTES_PER_PIXEL;
	for (i32 Y = minY; Y < maxY; Y++) {
		u32* dstPixel = ptrcast(u32, dstRow);
		u32* srcPixel = ptrcast(u32, srcRow);
		for (i32 X = minX; X < maxX; X++) {
			V4 dest = {
					f4((*dstPixel >> 16) & 0xFF),
					f4((*dstPixel >> 8) & 0xFF),
					f4((*dstPixel >> 0) & 0xFF),
					f4((*dstPixel >> 24) & 0xFF)
			};
			dest = SRGB255ToLinear1(dest);

			V4 texel = {
					f4((*srcPixel >> 16) & 0xFF),
					f4((*srcPixel >> 8) & 0xFF),
					f4((*srcPixel >> 0) & 0xFF),
					f4((*srcPixel >> 24) & 0xFF)
			};
			texel = SRGB255ToLinear1(texel);

			V4 output = {
					texel.R + (1 - texel.A) * dest.R,
					texel.G + (1 - texel.A) * dest.G,
					texel.B + (1 - texel.A) * dest.B,
					texel.A + dest.A - texel.A * dest.A
			};
			output = Linear1ToSRGB255(output);
			*dstPixel = (u4(output.A + 0.5f) << 24) |
						(u4(output.R + 0.5f) << 16) |
						(u4(output.G + 0.5f) << 8) |
						(u4(output.B + 0.5f) << 0);

			dstPixel++;
			srcPixel++;
		}
		dstRow += screenBitmap.pitch;
		srcRow += loadedBitmap.pitch;
	}
}

internal
void RenderGroupToBuffer(RenderGroup& group, LoadedBitmap& dstBuffer, Rect2i clipRect, bool even) {
	u32 relativeRenderAddress = 0;
	while (relativeRenderAddress < group.pushBufferSize) {
		RenderCallHeader* header = ptrcast(RenderCallHeader, group.pushBuffer + relativeRenderAddress);
		relativeRenderAddress += sizeof(RenderCallHeader);
		u32 relativeAddressBeforeSwitchCase = relativeRenderAddress;
		switch (header->type) {
		case RenderCallType_RenderCallClear: {
			RenderCallClear* call = ptrcast(RenderCallClear, group.pushBuffer + relativeRenderAddress);
			RenderRectangleTransparent(dstBuffer, V2{ 0, 0 }, V2i(dstBuffer.width, dstBuffer.height), call->color, even, clipRect);
			relativeRenderAddress += sizeof(RenderCallClear);
		} break;
		case RenderCallType_RenderCallRectangle: {
			RenderCallRectangle* call = ptrcast(RenderCallRectangle, group.pushBuffer + relativeRenderAddress);
#if 0
			V2 min = call->center - call->size / 2.f;
			V2 max = min + call->size;
			RenderRectangleTransparent(dstBuffer, min, max, call->color, even, clipRect);
#else
			V2 xAxis = V2{ call->size.X, 0 };
			V2 yAxis = V2{ 0, call->size.Y };
			V2 origin = call->center - call->size / 2.f;
			RenderFilledRectangleOptimized(dstBuffer, origin, xAxis, yAxis, call->color, even, clipRect);
#endif
			relativeRenderAddress += sizeof(RenderCallRectangle);
		} break;
		case RenderCallType_RenderCallBitmap: {
			// TODO: RenderCallBitmap and RenderCallRectangle have different approaches to calculate center
			// It should be unified (check groundLevel) which is different from RenderCallRectangle,
			// also, size is properly changed in RenderCallRectangle and not in RenderCallBitmap
			RenderCallBitmap* call = ptrcast(RenderCallBitmap, group.pushBuffer + relativeRenderAddress);
			V2 xAxis = V2{ call->size.X, 0 };
			V2 yAxis = V2{ 0, call->size.Y };
			V2 origin = call->center - Hadamard(call->bitmap->align, call->size);
			RenderRectangleOptimized(dstBuffer, origin, xAxis, yAxis, call->color, *call->bitmap, even, clipRect);
			relativeRenderAddress += sizeof(RenderCallBitmap);
		} break;
		case RenderCallType_RenderCallCoordinateSystem: {
			RenderCallCoordinateSystem* call = ptrcast(RenderCallCoordinateSystem, group.pushBuffer + relativeRenderAddress);
			RenderRectangleSlowly(dstBuffer, call->origin, call->xAxis, call->yAxis,
				call->color, *call->bitmap, call->normalMap, call->topEnvMap,
				call->middleEnvMap, call->bottomEnvMap
			);
			V4 color = V4{ 1.f, 0.f, 0.f, 0.f };
			V2 size = V2{ 5.f, 5.f };
			V2 points[4]{
				call->origin,
				call->origin + call->xAxis,
				call->origin + call->yAxis,
				call->origin + call->xAxis + call->yAxis
			};
			RenderRectangleOpaque(dstBuffer, points[0], points[0] + size, color.RGB);
			RenderRectangleOpaque(dstBuffer, points[1], points[1] + size, color.RGB);
			RenderRectangleOpaque(dstBuffer, points[2], points[2] + size, color.RGB);
			RenderRectangleOpaque(dstBuffer, points[3], points[3] + size, color.RGB);
			relativeRenderAddress += sizeof(RenderCallCoordinateSystem);
		} break;
		InvalidDefaultCase;
		}
		Assert(relativeAddressBeforeSwitchCase < relativeRenderAddress);
	}
}

struct RenderTiledArgs {
	Rect2i clipRect;
	RenderGroup* group;
	LoadedBitmap* dstBuffer;
};

void RenderTiled(void* data) {
	RenderTiledArgs* args = ptrcast(RenderTiledArgs, data);
	RenderGroupToBuffer(*args->group, *args->dstBuffer, args->clipRect, false);
	RenderGroupToBuffer(*args->group, *args->dstBuffer, args->clipRect, true);
}

internal
void TiledRenderGroupToBuffer(RenderGroup& group, LoadedBitmap& dstBuffer, PlatformQueue* queue) {
	constexpr u32 tileCountX = 4;
	constexpr u32 tileCountY = 4;
	RenderTiledArgs workArgs[tileCountX * tileCountY] = {};
	u32 tileWidth = AlignUp8(RoundF32ToU32(f4(dstBuffer.width) / tileCountX));
	u32 tileHeight = dstBuffer.height / tileCountY;

	// NOTE: Until buffer is overallocated and I can easly write outside the boundaries of the
	// buffer, there is a need for some assumptions to make multithreaded tile rendering work:
	// 1. Tile width MUST be divisible by 8 (for AVX2 wide instructions)
	// 2. Buffer width MUST be divisible by tile width! (if not, last in row, the smallest buffer width
	// MUST be divisible by 8)
	// That means no bizzare resolutions and number of tiles in X. If these constrains are not met,
	// there is no chance to safely render the tiles, because overdrawing beyond the tile boundaries
	// might cause data race where 2 neighbour threads reads the same pixels, but in one of them they
	// are masked and in the second they are actually overriden. Even when masking is provided, there is
	// no guarrantee that between reading these pixels and writing them back, neighbour thread didn't
	// change these pixels to something else. If so, masked override will actually change the pixels
	// which shouldn't happened!
	Assert((reinterpret_cast<uptr>(dstBuffer.bufferStart) & 31) == 0);
	Assert((tileWidth & 7) == 0);
	Assert(((dstBuffer.width % tileWidth) & 7) == 0);
	// NOTE: These are sanity checks, too many tiles on too small resolutions will cause stupid things like
	// tasks with zero pixels to draw :)
	Assert(tileWidth * tileCountX < dstBuffer.width + tileWidth);
	Assert(tileHeight * tileCountY < dstBuffer.height + tileHeight);

	for (u32 tileY = 0; tileY < tileCountY; tileY++) {
		for (u32 tileX = 0; tileX < tileCountX; tileX++) {
			RenderTiledArgs* args = workArgs + tileY * tileCountY + tileX;
			args->clipRect.minY = tileY * tileHeight;
			args->clipRect.maxY = args->clipRect.minY + tileHeight;
			args->clipRect.minX = tileX * tileWidth;
			args->clipRect.maxX = args->clipRect.minX + tileWidth;
			args->dstBuffer = &dstBuffer;
			args->group = &group;
#if 1 // Switch on = multithreaded rendering
			Platform->QueuePushTask(queue, RenderTiled, args);
#else
			RenderTiled(args);
#endif
		}
	}
	Platform->QueueWaitForCompletion(queue);
}

internal
void RenderGroupToBuffer(RenderGroup& group, LoadedBitmap& dstBuffer) {
	Rect2i clipRect = { 0, 0, dstBuffer.width, dstBuffer.height };
	RenderGroupToBuffer(group, dstBuffer, clipRect, true);
	RenderGroupToBuffer(group, dstBuffer, clipRect, false);
}

inline
V2 UnprojectCoords(ProjectionProps& projection, V2 projectedCoords, f32 atDistanceFromCamera) {
	V2 worldCoords = atDistanceFromCamera * projectedCoords / projection.camera.focalLength;
	return worldCoords;
}

inline
Rect2 GetRenderRectangleAtDistance(ProjectionProps& projection, u32 width, u32 height, f32 distance) {
	V2 screenDim = 0.5f * V2i(width, height) / projection.metersToPixels;
	V2 projectedDims = UnprojectCoords(projection, screenDim, distance);
	Rect2 result = GetRectFromCenterHalfDim(V2{ 0, 0 }, projectedDims);
	return result;
}

inline 
Rect2 GetRenderRectangleAtTarget(ProjectionProps& projection, u32 width, u32 height) {
	Rect2 result = GetRenderRectangleAtDistance(projection, width, height, projection.camera.distanceToTarget);
	return result;
}

inline
CameraProps GetStandardCamera() {
	CameraProps result = {};
	result.distanceToTarget = 10.f;
	result.focalLength = 0.7f;
	result.nearClip = 0.2f;
	return result;
}

inline
ProjectionProps GetOrtographicProjection(u32 widthPix, u32 heightPix, f32 metersToPixels) {
	ProjectionProps result = {};
	result.monitorWidth = 0.52f;
	result.metersToPixels = metersToPixels;
	result.screenCenter = { widthPix / 2.f,
							heightPix / 2.f };
	result.orthographic = true;
	result.camera = GetStandardCamera();
	return result;
}

inline
ProjectionProps GetStandardProjection(u32 widthPix, u32 heightPix) {
	ProjectionProps result = {};
	result.monitorWidth = 0.52f;
	result.metersToPixels = widthPix * result.monitorWidth;
	result.screenCenter = { widthPix / 2.f,
							heightPix / 2.f };
	result.orthographic = false;
	result.camera = GetStandardCamera();
	return result;
}

inline
RenderGroup AllocateRenderGroup(MemoryArena& arena, u32 size) {
	RenderGroup result = {};
	result.pushBuffer = PushArray(arena, size, u8);
	result.maxPushBufferSize = size;
	result.pushBufferSize = 0;
	return result;
}

inline
TaskWithMemory* TryBeginBackgroundTask(TransientState* tranState) {
	TaskWithMemory* result = 0;
	for (u32 taskIndex = 0; taskIndex < ArrayCount(tranState->tasks); taskIndex++) {
		TaskWithMemory* task = tranState->tasks + taskIndex;
		if (AtomicCompareExchange(&task->done, 0, 1)) {
			CheckArena(task->arena);
			task->memory = BeginTempMemory(task->arena);
			result = task;
			break;
		}
	}
	return result;
}

inline
void EndBackgroundTask(TaskWithMemory* task) {
	EndTempMemory(task->memory);
	WriteCompilatorFence;
	task->done = true;
}

internal
LoadedBitmap LoadBmpFile(const char* filename, V2 bottomUpAlignRatio = V2{ 0.5f, 0.5f })
{
#pragma pack(push, 1)
	struct BmpHeader {
		u16 signature; // must be 0x42 = BMP
		u32 fileSize;
		u32 reservedZeros;
		u32 bitmapOffset; // where pixels starts
		u32 headerSize;
		u32 width;
		u32 height;
		u16 planes;
		u16 bitsPerPixel;
		u32 compression;
		u32 imageSize;
		u32 resolutionPixPerMeterX;
		u32 resolutionPixPerMeterY;
		u32 colorsUsed;
		u32 ColorsImportant;
		u32 redMask;
		u32 greenMask;
		u32 blueMask;
		u32 alphaMask;
	};
#pragma pack(pop)

	FileData bmpData = debugGlobalMemory->readEntireFile(filename);
	if (!bmpData.content) {
		return {};
	}
	BmpHeader* header = ptrcast(BmpHeader, bmpData.content);
	Assert(header->compression == 3);
	Assert(header->bitsPerPixel == 32);
	u32 redShift = LeastSignificantHighBit(header->redMask).index;
	u32 greenShift = LeastSignificantHighBit(header->greenMask).index;
	u32 blueShift = LeastSignificantHighBit(header->blueMask).index;
	u32 alphaShift = LeastSignificantHighBit(header->alphaMask).index;

	LoadedBitmap result = {};
	result.bufferStart = ptrcast(void, ptrcast(u8, bmpData.content) + header->bitmapOffset);
	result.height = header->height;
	result.width = header->width;
	result.widthOverHeight = f4(result.width) / f4(result.height);
	Assert((header->bitsPerPixel / 8) == BITMAP_BYTES_PER_PIXEL);
	result.pitch = result.width * BITMAP_BYTES_PER_PIXEL;
	result.data = ptrcast(u32, result.bufferStart);
	result.align = bottomUpAlignRatio;

	u32* pixels = ptrcast(u32, result.bufferStart);
	for (u32 Y = 0; Y < header->height; Y++) {
		for (u32 X = 0; X < header->width; X++) {
			V4 texel = {
				f4((*pixels >> redShift) & 0xFF),
				f4((*pixels >> greenShift) & 0xFF),
				f4((*pixels >> blueShift) & 0xFF),
				f4((*pixels >> alphaShift) & 0xFF),
			};
			texel = SRGB255ToLinear1(texel);
			texel.RGB *= texel.A;
			texel = Linear1ToSRGB255(texel);
			*pixels++ = (u4(texel.A + 0.5f) << 24) +
				(u4(texel.R + 0.5f) << 16) +
				(u4(texel.G + 0.5f) << 8) +
				(u4(texel.B + 0.5f) << 0);
		}
	}

	return result;
}

struct LoadBitmapTaskArgs {
	const char* filename;
	Asset* asset;
	V2 alignment;
	TaskWithMemory* task;
};

internal
void LoadBitmapBackgroundTask(void* data) {
	LoadBitmapTaskArgs* args = ptrcast(LoadBitmapTaskArgs, data);
	args->asset->bitmap = LoadBmpFile(args->filename, args->alignment);
	WriteCompilatorFence;
	args->asset->state = AssetState::Ready;
	EndBackgroundTask(args->task);
}

inline
bool IsValid(SoundId sid) {
	return sid.id > 0;
}

inline
bool IsValid(BitmapId bid) {
	return bid.id > 0;
}

inline
Asset* GetAsset(Assets& assets, u32 id) {
	Asset* asset = &assets.assets[id];
	return asset;
}

inline
AssetMetadata* GetAssetMetadata(Assets& assets, Asset& asset) {
	AssetMetadata* metadata = &assets.metadatas[asset.metadataId];
	return metadata;
}

inline
AssetFeatures* GetAssetFeatures(Assets& assets, u32 id) {
	AssetFeatures* asset = &assets.features[id];
	return asset;
}

inline
bool IsReady(Asset* asset) {
	return asset && asset->state == AssetState::Ready;
}

inline
AssetGroup* GetAssetGroup(Assets& assets, AssetTypeID typeId) {
	AssetGroup* group = &assets.groups[typeId];
	Assert(group);
	// TESTING NOTE: These are handy in development, comment it out when want to test
	// whether asset system is resistent on lack of assets loaded
#if 1
	Assert(group->firstAssetIndex != 0);
	Assert(group->firstAssetIndex < group->onePastLastAssetIndex);
#endif
	return group;
}

inline
u32 _GetFirstAssetIdWithType(Assets& assets, AssetTypeID typeId) {
	AssetGroup* group = GetAssetGroup(assets, typeId);
	return group->firstAssetIndex;
}

inline
u32 _GetRandomAssetId(Assets& assets, AssetTypeID typeId, RandomSeries& series) {
	AssetGroup* group = GetAssetGroup(assets, typeId);
	u32 id = RandomChoiceBetween(series, group->firstAssetIndex, group->onePastLastAssetIndex);
	return id;
}

internal
u32 _GetBestFitAssetId(Assets& assets, AssetTypeID typeId, AssetFeatures match, AssetFeatures weight, f32 halfPeriod) {
	AssetGroup* group = GetAssetGroup(assets, typeId);
	u32 best = {};
	f32 bestScore = F32_MAX;
	for (u32 assetIndex = group->firstAssetIndex;
		assetIndex < group->onePastLastAssetIndex;
		assetIndex++
		) {
		Asset* asset = GetAsset(assets, assetIndex);
		if (!asset) {
			continue;
		}
		AssetFeatures* features = GetAssetFeatures(assets, assetIndex);
		f32 score = 0;
		for (u32 featureIndex = 0; featureIndex < ArrayCount(*features); featureIndex++) {
			//f32 sign = SignF32(halfPeriod - match[featureIndex]);
			f32 a1 = (*features)[featureIndex];
			f32 a2 = (*features)[featureIndex] - 2 * halfPeriod;
			f32 d1 = Abs(match[featureIndex] - a1);
			f32 d2 = Abs(match[featureIndex] - a2);
			f32 distance = Minimum(d1, d2);
			score += weight[featureIndex] * Abs(distance);
		}
		if (score < bestScore) {
			bestScore = score;
			best = assetIndex;
		}
	}
	return best;
}

inline
SoundId GetFirstSoundIdWithType(Assets& assets, AssetTypeID typeId) {
	SoundId id = { _GetFirstAssetIdWithType(assets, typeId) };
	return id;
}

inline
SoundId GetRandomSoundId(Assets& assets, AssetTypeID typeId, RandomSeries& series) {
	SoundId id = { _GetRandomAssetId(assets, typeId, series) };
	return id;
}

inline
SoundId GetBestFitSoundId(Assets& assets, AssetTypeID typeId, AssetFeatures match, AssetFeatures weight, f32 halfPeriod) {
	SoundId id = { _GetBestFitAssetId(assets, typeId, match, weight, halfPeriod) };
	return id;
}

inline
BitmapId GetFirstBitmapIdWithType(Assets& assets, AssetTypeID typeId) {
	BitmapId id = { _GetFirstAssetIdWithType(assets, typeId) };
	return id;
}

inline
BitmapId GetRandomBitmapId(Assets& assets, AssetTypeID typeId, RandomSeries& series) {
	BitmapId id = { _GetRandomAssetId(assets, typeId, series) };
	return id;
}

inline
BitmapId GetBestFitBitmapId(Assets& assets, AssetTypeID typeId, AssetFeatures match, AssetFeatures weight, f32 halfPeriod) {
	BitmapId id = { _GetBestFitAssetId(assets, typeId, match, weight, halfPeriod) };
	return id;
}

inline
bool NeedsFetching(Asset& asset) {
	return asset.state == AssetState::NotReady;
}

struct LoadAssetTaskArgs {
	const char* filename;
	void* buffer;
	u32 offset;
	u32 size;
	TaskWithMemory* task;
	AssetState* state;
};

internal
void LoadAssetBackgroundTask(void* data) {
	// TODO: Can I do that without opening/closing file all the time?
	LoadAssetTaskArgs* args = ptrcast(LoadAssetTaskArgs, data);
	PlatformFileHandle* handle = Platform->FileOpen(args->filename);
	Platform->FileRead(handle, args->offset, args->size, args->buffer);
	if (!Platform->FileErrors(handle)) {
		WriteCompilatorFence;
		*args->state = AssetState::Ready;
		EndBackgroundTask(args->task);
	}
	Platform->FileClose(handle);
}

internal
bool PrefetchBitmap(Assets& assets, BitmapId bid) {
	Asset& asset = assets.assets[bid.id];
	if (!IsValid(bid) || !NeedsFetching(asset)) {
		return false;
	}
	TaskWithMemory* task = TryBeginBackgroundTask(assets.tranState);
	if (!task) {
		return false;
	}
	AssetFileBitmapInfo* metadata = &GetAssetMetadata(assets, asset)->_bitmapInfo;
	asset.bitmap.align = metadata->alignment;
	asset.bitmap.height = metadata->height;
	asset.bitmap.width = metadata->width;
	asset.bitmap.pitch = metadata->pitch;
	asset.bitmap.widthOverHeight = f4(metadata->width) / f4(metadata->height);
	asset.bitmap.data = ptrcast(u32, PushArray(assets.arena, metadata->pitch * metadata->height, u8));
	asset.state = AssetState::Pending;

	LoadAssetTaskArgs* args = PushStructSize(task->arena, LoadAssetTaskArgs);
	args->filename = assets.filenames[asset.filenameHandle];
	args->offset = metadata->dataOffset;
	args->size = metadata->pitch * metadata->height;
	args->buffer = asset.bitmap.data;
	args->task = task;
	args->state = &asset.state;

	WriteCompilatorFence;
	Platform->QueuePushTask(assets.tranState->lowPriorityQueue, LoadAssetBackgroundTask, args);
	return true;
#if 0
	BitmapInfo* info = &GetAssetMetadata(assets, asset)->bitmapInfo;
	Assert(info->filename);
	LoadBitmapTaskArgs* args = PushStructSize(task->arena, LoadBitmapTaskArgs);
	args->asset = &asset;
	args->task = task;
	args->alignment = info->alignment;
	args->filename = info->filename;
	asset.state = AssetState::Pending;
	WriteCompilatorFence;
	Platform->QueuePushTask(assets.tranState->lowPriorityQueue, LoadBitmapBackgroundTask, args);
	return true;
#endif
}

internal
bool PrefetchSound(Assets& assets, SoundId sid) {
	Asset& asset = assets.assets[sid.id];
	if (!IsValid(sid) || !NeedsFetching(asset)) {
		return false;
	}
	TaskWithMemory* task = TryBeginBackgroundTask(assets.tranState);
	if (!task) {
		return false;
	}

	AssetFileSoundInfo* metadata = &GetAssetMetadata(assets, asset)->_soundInfo;
	u32 floatsToAllocate = (metadata->sampleCount + SOUND_CHUNK_SAMPLE_OVERLAP) * metadata->nChannels;
	asset.sound.nChannels = metadata->nChannels;
	asset.sound.sampleCount = metadata->sampleCount;
	asset.sound.samples[0] = PushArray(assets.arena, floatsToAllocate, f32);
	asset.sound.samples[1] = asset.sound.samples[0] + metadata->sampleCount + SOUND_CHUNK_SAMPLE_OVERLAP;
	asset.state = AssetState::Pending;

	LoadAssetTaskArgs* args = PushStructSize(task->arena, LoadAssetTaskArgs);
	args->filename = assets.filenames[asset.filenameHandle];
	args->offset = metadata->samplesOffset[0];
	args->size = floatsToAllocate * sizeof(f32);
	args->buffer = asset.bitmap.data;
	args->task = task;
	args->state = &asset.state;
	WriteCompilatorFence;
	Platform->QueuePushTask(assets.tranState->lowPriorityQueue, LoadAssetBackgroundTask, args);
	return true;
#if 0
	SoundInfo* info = &GetAssetMetadata(assets, asset)->soundInfo;
	Assert(info->filename);
	LoadSoundTaskArgs* args = PushStructSize(task->arena, LoadSoundTaskArgs);
	args->asset = &asset;
	args->task = task;
	args->filename = info->filename;
	args->firstSampleIndex = info->firstSampleIndex;
	args->chunkSampleCount = info->chunkSampleCount;
	asset.state = AssetState::Pending;
	WriteCompilatorFence;
	Platform->QueuePushTask(assets.tranState->lowPriorityQueue, LoadSoundBackgroundTask, args);
	return true;
#endif
}

internal
LoadedSound LoadWAV(const char* filename, u32 firstSampleIndex, u32 chunkSampleCount) {
#define CHUNK_ID(a, b, c, d) ((d << 24) + (c << 16) + (b << 8) + a)
#pragma pack(push, 1)
	struct RiffHeader {
		u32 riffId;
		u32 size;
		u32 waveId;
	};
	struct ChunkHeader {
		u32 chunkId;
		u32 chunkSize;
	};
	struct FmtHeader {
		u16 formatCode;
		u16 nChannels;
		u32 nSamplesPerSec;
		u32 nAvgBytesPerSec;
		u16 nBlockAlign;
		u16 bitsPerSample;
		u16 cbSize;
		u16 validBitsPerSample;
		u32 channelMask;
		char subformat[16];
	};
#pragma pack(pop)
	LoadedSound sound = {};
	FileData data = debugGlobalMemory->readEntireFile(filename);
	if (!data.content) {
		return sound;
	}
	
	RiffHeader* riffHeader = ptrcast(RiffHeader, data.content);
	Assert(riffHeader->riffId == CHUNK_ID('R', 'I', 'F', 'F'));
	Assert(riffHeader->waveId == CHUNK_ID('W', 'A', 'V', 'E'));
	u32 chunkSize = riffHeader->size - 4;
	u8* ptr = ptrcast(u8, data.content) + sizeof(RiffHeader);
	u32 sizeRead = 0;
	i16* fileSamples = 0;
	u32 samplesSizeInBytes = 0;
	u32 nChannels = 0;
	while (sizeRead < chunkSize) {
		ChunkHeader* header = ptrcast(ChunkHeader, ptr);
		sizeRead += sizeof(header);
		ptr += sizeof(header);
		switch (header->chunkId) {
		case CHUNK_ID('f', 'm', 't', ' '): {
			FmtHeader* fmt = ptrcast(FmtHeader, ptr);
			Assert(fmt->nSamplesPerSec == 48000);
			Assert(fmt->bitsPerSample == 16);
			Assert(fmt->nChannels <= 2);
			nChannels = fmt->nChannels;
		} break;
		case CHUNK_ID('d', 'a', 't', 'a'): {
			fileSamples = ptrcast(i16, ptr);
			samplesSizeInBytes = header->chunkSize;
		} break;
		}
		u32 paddedSize = (header->chunkSize + 1) & ~1;
		sizeRead += paddedSize;
		ptr += paddedSize;
	}
	if (chunkSampleCount == 0) {
		chunkSampleCount = U32_MAX;
	}
	u32 chunkOverlap = 8;
	u32 wavSampleCount = samplesSizeInBytes / (sizeof(i16) * nChannels);
	Assert(samplesSizeInBytes > 0);
	Assert(wavSampleCount > firstSampleIndex);
	Assert(nChannels == 2);
	sound.nChannels = nChannels;
	sound.sampleCount = Minimum(wavSampleCount - firstSampleIndex, chunkSampleCount);
	Assert(sound.sampleCount != 0);
	Assert((sound.sampleCount & 1) == 0);
	u64 bytesToAllocate = (sound.sampleCount + chunkOverlap) * sizeof(f32) * sound.nChannels;
	sound.samples[0] = ptrcast(f32, debugGlobalMemory->allocate(bytesToAllocate));
	sound.samples[1] = sound.samples[0] + sound.sampleCount + chunkOverlap;
	f32* dest[2] = { sound.samples[0], sound.samples[1] };
	i16* src = fileSamples + sound.nChannels * firstSampleIndex;
	f32 dividor = f4(I16_MAX);
	// NOTE: copy a little bit more samples if this is not last chank to make sound seamless
	bool lastChunk = (wavSampleCount - firstSampleIndex) <= chunkSampleCount;
	u32 samplesToCopy = sound.sampleCount;
	if (!lastChunk) {
		samplesToCopy += chunkOverlap;
	}
	for (u32 sampleIndex = 0; sampleIndex < samplesToCopy; sampleIndex ++) {
		for (u32 channelIndex = 0; channelIndex < sound.nChannels; channelIndex++) {
			*dest[channelIndex]++ = f4(*src++) / dividor;
		}
	}
	return sound;
}

struct LoadSoundTaskArgs {
	const char* filename;
	Asset* asset;
	u32 firstSampleIndex;
	u32 chunkSampleCount;
	TaskWithMemory* task;
};

internal
void LoadSoundBackgroundTask(void* data) {
	LoadSoundTaskArgs* args = ptrcast(LoadSoundTaskArgs, data);
	args->asset->sound = LoadWAV(args->filename, args->firstSampleIndex, args->chunkSampleCount);
	WriteCompilatorFence;
	args->asset->state = AssetState::Ready;
	EndBackgroundTask(args->task);
}

inline
bool LoadIfNotAllAssetsAreReady(Assets& assets, AssetTypeID typeId) {
	AssetGroup* group = GetAssetGroup(assets, typeId);
	bool ready = true;
	for (u32 assetIndex = group->firstAssetIndex; 
		assetIndex < group->onePastLastAssetIndex;
		assetIndex++) 
	{
		Asset* asset = GetAsset(assets, assetIndex);
		if (!IsReady(asset)) {
			ready = false;
			if (group->type == AssetGroup_Bitmap) {
				PrefetchBitmap(assets, { assetIndex });
			}
			else {
				PrefetchSound(assets, { assetIndex });
			}
		}
	}
	return ready;
}

#if 0
inline 
void AddBmpAsset(Assets& assets, AssetTypeID id, const char* filename, V2 alignment = V2{0.5f, 0.5f}) {
	Assert(assets.assetCount < assets.assetMaxCount);
	Asset* asset = &assets.assets[assets.assetCount];
	BitmapInfo* info = &assets.metadatas[assets.assetCount].bitmapInfo;
	info->filename = filename;
	info->alignment = alignment;
	info->typeId = id;
	asset->metadataId = assets.assetCount;
	AssetGroup* group = &assets.groups[id];
	if (group->firstAssetIndex == 0) {
		group->firstAssetIndex = assets.assetCount;
		group->onePastLastAssetIndex = group->firstAssetIndex + 1;
		group->type = AssetGroup_Bitmap;
	}
	else {
		Assert(group->type == AssetGroup_Bitmap);
		group->onePastLastAssetIndex++;
		if (assets.assetCount > 0) {
			BitmapInfo* prevInfo = &assets.metadatas[assets.assetCount - 1].bitmapInfo;
			Assert(prevInfo->typeId == info->typeId);
		}
	}
	assets.assetCount++;
}

inline
SoundId AddSoundAsset(Assets& assets, AssetTypeID id, const char* filename, u32 firstSampleIndex = 0, u32 chunkSampleCount = 0) {
	Assert(assets.assetCount < assets.assetMaxCount);
	SoundId result = { assets.assetCount };
	Asset* asset = &assets.assets[assets.assetCount];
	SoundInfo* info = &assets.metadatas[assets.assetCount].soundInfo;
	info->filename = filename;
	info->typeId = id;
	info->chain = { SoundChain::None, 0 };
	info->firstSampleIndex = firstSampleIndex;
	info->chunkSampleCount = chunkSampleCount;
	asset->metadataId = assets.assetCount;
	AssetGroup* group = &assets.groups[id];
	if (group->firstAssetIndex == 0) {
		group->firstAssetIndex = assets.assetCount;
		group->onePastLastAssetIndex = group->firstAssetIndex + 1;
		group->type = AssetGroup_Sound;
	}
	else {
		Assert(group->type == AssetGroup_Sound);
		group->onePastLastAssetIndex++;
		if (assets.assetCount > 0) {
			SoundInfo* prevInfo = &assets.metadatas[assets.assetCount - 1].soundInfo;
			Assert(prevInfo->typeId == info->typeId);
		}
	}
	assets.assetCount++;
	return result;
}

internal
void AddFeature(Assets& assets, AssetFeatureID fId, f32 value) {
	Assert(assets.assetCount > 0);
	AssetFeatures* features = GetAssetFeatures(assets, assets.assetCount - 1);
	*features[fId] = value;
}
#endif

internal
void AllocateAssets(TransientState* tranState) {
#if 1
	u32 assetsCount = 0;
	PlatformFileGroupNames fileGroupNames = Platform->FileGetAllWithExtension("assf");
	PlatformFileGroupHandles fileGroup = Platform->FileOpenAllInGroup(fileGroupNames);
	// NOTE: Read how many assets do we have at all first!
	for (u32 fileIndex = 0; fileIndex < fileGroup.count; fileIndex++) {
		PlatformFileHandle* file = *(fileGroup.files + fileIndex);
		AssetFileHeader header;
		Platform->FileRead(file, 0, sizeof(header), &header);
		if (Platform->FileErrors(file) || 
			header.magicString != EAF_MAGIC_STRING('a', 's', 's', 'f') ||
			header.version != 0) {
			// TODO: Inform user about IO error
			continue;
		}
		// NOTE: First asset is null asset, it shouldn't be taken into account
		assetsCount += header.assetsCount - 1;
	}
	Assets& assets = tranState->assets;
	SubArena(assets.arena, tranState->arena, MB(512));
	assets.tranState = tranState;
	assets.assetCount = assetsCount + 1;
	assets.assets = PushArray(assets.arena, assets.assetCount, Asset);
	assets.features = PushArray(assets.arena, assets.assetCount, AssetFeatures);
	assets.metadatas = PushArray(assets.arena, assets.assetCount, AssetMetadata);
	assets.filenames = fileGroupNames.names;

	TemporaryMemory scratchMemory = BeginTempMemory(assets.arena);
	AssetFileHeader* headers = PushArray(assets.arena, fileGroup.count, AssetFileHeader);
	// NOTE: Read the headers first!
	for (u32 fileIndex = 0; fileIndex < fileGroup.count; fileIndex++) {
		AssetFileHeader* header = headers + fileIndex;
		PlatformFileHandle* file = *(fileGroup.files + fileIndex);
		Platform->FileRead(file, 0, sizeof(AssetFileHeader), header);
		if (Platform->FileErrors(file) ||
			header->magicString != EAF_MAGIC_STRING('a', 's', 's', 'f') ||
			header->version != 0) {
			// TODO: Inform user about IO error
			continue;
		}
	}

	// NOTE: The first asset is NULL asset, so readAssetsCount must start from first NON NULL asset
	// NOTE: Also, first group in the file is NULL group, so start from NON NULL group
	u32 readAssetsCount = 1;
	for (u32 groupIndex = 1; groupIndex < Asset_Count; groupIndex++) {
		AssetGroup* combinedAssetGroup = assets.groups + groupIndex;
		AssetGroup* fileAssetGroup = PushStructSize(assets.arena, AssetGroup);
		
		for (u32 fileIndex = 0; fileIndex < fileGroup.count; fileIndex++) {
			PlatformFileHandle* file = *(fileGroup.files + fileIndex);
			AssetFileHeader* header = headers + fileIndex;

			Platform->FileRead(
				file,
				u4(header->assetGroupsOffset) + sizeof(AssetGroup) * groupIndex,
				sizeof(AssetGroup),
				fileAssetGroup
			);
			Assert(fileAssetGroup->firstAssetIndex != 0);
			Assert(fileAssetGroup->onePastLastAssetIndex != 0);
			Assert(fileAssetGroup->firstAssetIndex < header->assetsCount);
			u32 fileAssetCountInGroup = fileAssetGroup->onePastLastAssetIndex - fileAssetGroup->firstAssetIndex;

			Platform->FileRead(
				file,
				u4(header->featuresOffset) + sizeof(AssetFeatures) * fileAssetGroup->firstAssetIndex,
				sizeof(AssetFeatures) * fileAssetCountInGroup,
				assets.features + readAssetsCount
			);
			Platform->FileRead(
				file,
				u4(header->assetMetadatasOffset) + sizeof(AssetMetadata) * fileAssetGroup->firstAssetIndex,
				sizeof(AssetMetadata) * fileAssetCountInGroup,
				assets.metadatas + readAssetsCount
			);
			if (Platform->FileErrors(file)) {
				// TODO: Inform user about IO error
				continue;
			}
			for (u32 baseIndex = 0; baseIndex < fileAssetCountInGroup; baseIndex++) {
				Asset* dstAsset = assets.assets + readAssetsCount + baseIndex;
				dstAsset->filenameHandle = fileIndex;
				dstAsset->metadataId = readAssetsCount + baseIndex;
#if 0
				AssetMetadata* dstMetadata = assets.metadatas + readAssetsCount + baseIndex;
				switch (fileAssetGroup->type) {
				case AssetGroup_Bitmap: {
					// TODO: Shouldn't it be flat loading?
					dstMetadata->_bitmapInfo = srcAssetInfo->bmp;
					dstAsset->fileHandle = file;
					// TODO: TypeId is only needed for assertion, so maybe it can be structured
					// in other way to just delete it?
					// Also filename is not relevant
					// asset->bitmapInfo.typeId = assetFileInfo->bmp.
					// asset->bitmapInfo.filename = 0;
				} break;
				case AssetGroup_Sound: {
					// TODO: Shouldn't it be flat loading?
					dstMetadata->_soundInfo = srcAssetInfo->sound;
					dstAsset->fileHandle = file;
					/*
					TODO: Again not relevant stuff here
					asset->soundInfo.chunkSampleCount = assetFileInfo->sound.sampleCount;
					asset->soundInfo.firstSampleIndex = 0;
					asset->soundInfo.filename = 0;
					*/
				} break;
				InvalidDefaultCase;
				}
#endif
			}
			combinedAssetGroup->firstAssetIndex = readAssetsCount;
			combinedAssetGroup->onePastLastAssetIndex = readAssetsCount + fileAssetCountInGroup;
			combinedAssetGroup->type = fileAssetGroup->type;
			readAssetsCount += fileAssetCountInGroup;
		}
	}
	Assert(assets.assetCount == readAssetsCount);

	// NOTE: CLOSE THE FKING HANDLES!
	Platform->FileCloseAllInGroup(fileGroup);
	EndTempMemory(scratchMemory);
	CheckArena(assets.arena);
	
#endif

#if 0
	Assets& assets = tranState->assets;
	SubArena(assets.arena, tranState->arena, MB(12));
	assets.tranState = tranState;
	assets.assetMaxCount = 256 * Asset_Count;
	assets.assets = PushArray(assets.arena, assets.assetMaxCount, Asset);
	assets.features = PushArray(assets.arena, assets.assetMaxCount, AssetFeatures);
	assets.metadatas = PushArray(assets.arena, assets.assetMaxCount, AssetMetadata);
	AddBmpAsset(assets, Asset_Null, 0);
	AddBmpAsset(assets, Asset_Tree, "test/tree.bmp", V2{ 0.5f, 0.25f });
	AddFeature(assets, Feature_Height, 1.f);
	AddBmpAsset(assets, Asset_Tree, "test/tree2.bmp", V2{ 0.5f, 0.25f });
	AddFeature(assets, Feature_Height, 3.f);
	AddBmpAsset(assets, Asset_Tree, "test/tree3.bmp", V2{ 0.5f, 0.25f });
	AddFeature(assets, Feature_Height, 2.f);
	AddBmpAsset(assets, Asset_Ground, "test/ground0.bmp");
	AddBmpAsset(assets, Asset_Ground, "test/ground1.bmp");
	AddBmpAsset(assets, Asset_Grass, "test/grass0.bmp");
	AddBmpAsset(assets, Asset_Grass, "test/grass1.bmp");

	V2 playerBitmapsAlignment = V2{ 0.5f, 0.2f };
	AddBmpAsset(assets, Asset_Player, "test/hero-right.bmp", playerBitmapsAlignment);
	AddFeature(assets, Feature_FacingDirection, 0.f * TAU);
	AddBmpAsset(assets, Asset_Player, "test/hero-up.bmp", playerBitmapsAlignment);
	AddFeature(assets, Feature_FacingDirection, 0.25f * TAU);
	AddBmpAsset(assets, Asset_Player, "test/hero-left.bmp", playerBitmapsAlignment);
	AddFeature(assets, Feature_FacingDirection, 0.5f * TAU);
	AddBmpAsset(assets, Asset_Player, "test/hero-down.bmp", playerBitmapsAlignment);
	AddFeature(assets, Feature_FacingDirection, 0.75f * TAU);

	u32 silksongSampleCount = 7762944;
	u32 chunkSampleCount = 4 * 48000; // 2seconds;
	u32 firstSampleIndex = 0;
	Asset* prevAsset = 0;
	// TODO: What with feature based asset retrieval? It is possible for asset system to return
	// not first music chunk?
	while (firstSampleIndex < silksongSampleCount) {
		SoundId nextAssetId = AddSoundAsset(assets, Asset_Music, "sound/silksong.wav", firstSampleIndex, chunkSampleCount);
		Asset* nextAsset = GetAsset(assets, nextAssetId.id);
		if (prevAsset) {
			AssetMetadata* metadata = GetAssetMetadata(assets, *prevAsset);
			metadata->soundInfo.chain = { SoundChain::Advance, 1 };
		}
		prevAsset = nextAsset;
		firstSampleIndex += chunkSampleCount;
	}
	AddSoundAsset(assets, Asset_Bloop, "sound/bloop2.wav");
#endif
}

#define Text(text) text
#define PushRenderEntry(group, type) ptrcast(type, PushRenderEntry_(group, sizeof(type), RenderCallType_##type))
inline
void* PushRenderEntry_(RenderGroup& group, u32 size, RenderCallType type) {
	size += sizeof(RenderCallHeader);
	Assert(group.pushBufferSize + size <= group.maxPushBufferSize);
	if (group.pushBufferSize + size > group.maxPushBufferSize) {
		return 0;
	}
	RenderCallHeader* header = ptrcast(RenderCallHeader, group.pushBuffer + group.pushBufferSize);
	header->type = type;
	void* result = (header + 1);
	group.pushBufferSize += size;
	return result;
}

inline
bool PushClearCall(RenderGroup& group, V4 color) {
	RenderCallClear* call = PushRenderEntry(group, RenderCallClear);
	call->color = color;
	return true;
}

inline
bool PushBitmap(RenderGroup& group, LoadedBitmap* bitmap, V3 center, f32 height, V2 offset, V4 color) {
	V2 sizeUnprojected = height * V2{ bitmap->widthOverHeight, 1 };
	EntityProjectedParams params = CalculatePerspectiveProjection(group.projection, center, sizeUnprojected);
	if (!params.valid) {
		return false;
	}
	RenderCallBitmap* call = PushRenderEntry(group, RenderCallBitmap);
	call->bitmap = bitmap;
	call->center = params.center;
	call->offset = offset;
	call->color = color;
	call->size = params.size;
	return true;
}

inline
bool PushBitmap(RenderGroup& group, Assets& assets, BitmapId bid, V3 center, f32 height, V2 offset, V4 color) {
	Asset* asset = GetAsset(assets, bid.id);
	if (IsReady(asset)) {
		PushBitmap(group, &asset->bitmap, center, height, offset, color);
	}
	else {
		PrefetchBitmap(assets, bid);
	}
	return true;
}

inline
bool PushRect(RenderGroup& group, V3 center, V2 size, V2 offset, V4 color) {
	EntityProjectedParams params = CalculatePerspectiveProjection(group.projection, center, size);
	if (!params.valid) {
		return false;
	}
	RenderCallRectangle* call = PushRenderEntry(group, RenderCallRectangle);
	call->center = params.center;
	call->size = params.size;
	call->offset = offset;
	call->color = color;
	return true;
}

internal
bool PushRectBorders(RenderGroup& group, V3 center, V2 size, V4 color, f32 thickness) {
	V3 basePos = center;
	basePos.X = center.X - 0.5f * size.X;
	PushRect(group, basePos, V2{ thickness, size.Y }, V2{ 0, 0 }, color);
	basePos.X = center.X + 0.5f * size.X;
	PushRect(group, basePos, V2{ thickness, size.Y }, V2{ 0, 0 }, color);
	basePos = center;
	basePos.Y = center.Y - 0.5f * size.Y;
	PushRect(group, basePos, V2{ size.X, thickness }, V2{ 0, 0 }, color);
	basePos.Y = center.Y + 0.5f * size.Y;
	PushRect(group, basePos, V2{ size.X, thickness }, V2{ 0, 0 }, color);
	return true;
}

inline
bool PushCoordinateSystem(RenderGroup& group, V2 origin, V2 xAxis, V2 yAxis, V4 color,
	LoadedBitmap* bitmap, LoadedBitmap* normalMap, EnvironmentMap* topEnvMap,
	EnvironmentMap* middleEnvMap, EnvironmentMap* bottomEnvMap)
{
	RenderCallCoordinateSystem* call = PushRenderEntry(group, RenderCallCoordinateSystem);
	call->origin = origin;
	call->xAxis = xAxis;
	call->yAxis = yAxis;
	call->color = color;
	call->bitmap = bitmap;
	call->normalMap = normalMap;
	call->topEnvMap = topEnvMap;
	call->middleEnvMap = middleEnvMap;
	call->bottomEnvMap = bottomEnvMap;
	return true;
}