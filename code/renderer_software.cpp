#include "renderer.h"
#include "engine_arena.h"

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
				  f4((*pixel >> 24) & 0xFF) };
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
void RenderRectangleTransparent(LoadedBitmap& bitmap, V2 start, V2 end, V4 color, Rect2i clipRect) {
	TIMED_FUNCTION;
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

	u8* dstRow = ptrcast(u8, bitmap.data) + minY * bitmap.pitch + minX * BITMAP_BYTES_PER_PIXEL;
	u32 rowAdvance = bitmap.pitch;
	color.RGB *= color.A;
	for (i32 Y = minY; Y < maxY; Y++) {
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

// TODO: Do I really need Opaque version for faster rendering when I know alpha is 255.f?
internal
void RenderRectangleOpaque(LoadedBitmap& bitmap, V2 start, V2 end, V3 color) {
	TIMED_FUNCTION;
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
void RenderBitmap(LoadedBitmap& screenBitmap, LoadedBitmap& loadedBitmap, V2 position) {
	TIMED_FUNCTION;
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
void RenderRectangleSlowly(LoadedBitmap& bitmap, V2 origin, V2 xAxis, V2 yAxis, V4 color,
	LoadedBitmap& texture, LoadedBitmap* normalMap, EnvironmentMap* topMap,
	EnvironmentMap* middleMap, EnvironmentMap* bottomMap)
{
	TIMED_FUNCTION;
	u32 colorU32 = (scast(u32, 255 * color.A) << 24) +
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
					texel.RGB = V3{ 1, 1, 1 } *normal.G;
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
}

void RenderFilledRectangleOptimized(LoadedBitmap& bitmap, V2 origin, V2 xAxis, V2 yAxis, V4 color,
	Rect2i clipRect)
{
	TIMED_FUNCTION;
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
	u32 rowAdvance = bitmap.pitch;
	u8* row = ptrcast(u8, bitmap.data) + minY * bitmap.pitch + minX * BITMAP_BYTES_PER_PIXEL;
	LLVM_MCA_BEGIN(opt_render_filled_rect);
	for (i32 Y = minY; Y < maxY; Y++) {
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
}


void RenderRectangleOptimized(LoadedBitmap& bitmap, V2 origin, V2 xAxis, V2 yAxis, V4 color,
	LoadedBitmap& texture, Rect2i clipRect)
{
	TIMED_FUNCTION;
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
	u32 rowAdvance = bitmap.pitch;
	u8* row = ptrcast(u8, bitmap.data) + minY * bitmap.pitch + minX * BITMAP_BYTES_PER_PIXEL;
	LLVM_MCA_BEGIN(opt_render_rect);
	for (i32 Y = minY; Y < maxY; Y++) {
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
}

inline
void ResetRenderCommands(RenderCommandBuffer* commands) {
	commands->pushBufferCount = 0;
	commands->pushBufferSize = 0;
	commands->sortBufferAt = commands->maxPushBufferSize;
}

internal
void RenderCommandsToBuffer(RenderCommandBuffer* commands, LoadedBitmap& dstBuffer, Rect2i clipRect) {
	TIMED_FUNCTION;
	SortElement* sortElement = ptrcast(SortElement, commands->pushBuffer + commands->sortBufferAt);
	for (u32 sortIndex = 0; sortIndex < commands->pushBufferCount; sortIndex++, sortElement++) {
		u8* address = commands->pushBuffer + sortElement->offset;
		RenderCallHeader* header = ptrcast(RenderCallHeader, address);
		address += sizeof(RenderCallHeader);
		switch (header->type) {
		case RenderCallType_RenderCallClear: {
			RenderCallClear* call = ptrcast(RenderCallClear, address);
			RenderRectangleTransparent(dstBuffer, V2{ 0, 0 }, V2i(dstBuffer.width, dstBuffer.height), call->color, clipRect);
		} break;
		case RenderCallType_RenderCallRectangle: {
			RenderCallRectangle* call = ptrcast(RenderCallRectangle, address);
#if 0
			V2 min = call->center - call->size / 2.f;
			V2 max = min + call->size;
			RenderRectangleTransparent(dstBuffer, min, max, call->color, clipRect);
#else
			V2 xAxis = V2{ call->size.X, 0 };
			V2 yAxis = V2{ 0, call->size.Y };
			V2 origin = call->center - call->size / 2.f;
			RenderFilledRectangleOptimized(dstBuffer, origin, xAxis, yAxis, call->color, clipRect);
#endif
		} break;
		case RenderCallType_RenderCallBitmap: {
			// TODO: RenderCallBitmap and RenderCallRectangle have different approaches to calculate center
			// It should be unified (check groundLevel) which is different from RenderCallRectangle,
			// also, size is properly changed in RenderCallRectangle and not in RenderCallBitmap
			RenderCallBitmap* call = ptrcast(RenderCallBitmap, address);
			V2 xAxis = V2{ call->size.X, 0 };
			V2 yAxis = V2{ 0, call->size.Y };
			V2 origin = call->center - Hadamard(call->bitmap->align, call->size);
			RenderRectangleOptimized(dstBuffer, origin, xAxis, yAxis, call->color, *call->bitmap, clipRect);
		} break;
		case RenderCallType_RenderCallCoordinateSystem: {
			RenderCallCoordinateSystem* call = ptrcast(RenderCallCoordinateSystem, address);
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
		} break;
													  //InvalidDefaultCase;
		}
	}
}

inline
void Swap(SortElement* A, SortElement* B) {
	SortElement swap = *A;
	*A = *B;
	*B = swap;
}

internal
void BubbleSort(SortElement* array, u32 count) {
	TIMED_FUNCTION;
	for (;;) {
		bool swapped = false;
		for (u32 base = 0; base < count - 1; base++) {
			SortElement* A = array + base;
			SortElement* B = array + base + 1;

			if (A->key > B->key) {
				Swap(A, B);
				swapped = true;
			}
		}
		if (!swapped) {
			break;
		}
	}
}

internal
u32 MergeSortRecursive(SortElement* array, u32 count, SortElement* tempBuffer, u32 depth) {
	if (count == 1) {
		return depth;
	}
	if (count == 2) {
		SortElement* A = array;
		SortElement* B = array + 1;
		if (A->key > B->key) {
			Swap(A, B);
		}
		return depth;
	}
	u32 leftCount = count / 2;
	u32 rightCount = count - leftCount;

	SortElement* leftArray = array;
	SortElement* rightArray = array + leftCount;
	SortElement* leftTemp = tempBuffer;
	SortElement* rightTemp = tempBuffer + leftCount;
	MergeSortRecursive(leftTemp, leftCount, leftArray, depth + 1);
	u32 maxDepth = MergeSortRecursive(rightTemp, rightCount, rightArray, depth + 1);

	SortElement* out = array;
	u32 leftRemaining = leftCount;
	u32 rightRemaining = rightCount;
	for (u32 elementIndex = 0; elementIndex < count; elementIndex++) {
		SortElement* A = leftTemp;
		SortElement* B = rightTemp;

		if (leftRemaining && rightRemaining) {
			if (A->key > B->key) {
				*out++ = *B;
				rightTemp++;
				rightRemaining--;
			}
			else {
				*out++ = *A;
				leftTemp++;
				leftRemaining--;
			}
		}
		else if (leftRemaining) {
			*out++ = *A;
			leftTemp++;
			leftRemaining--;
		}
		else if (rightRemaining) {
			*out++ = *B;
			rightTemp++;
			rightRemaining--;
		}

	}
	return maxDepth;
}

inline
void MergeSort(SortElement* array, u32 count, SortElement* tempBuffer) {
	TIMED_FUNCTION;
	CopySize(array, tempBuffer, count * sizeof(SortElement));
	u32 maxDepth = MergeSortRecursive(array, count, tempBuffer, 0);
}

internal
void MergeSortWithCopying(SortElement* array, u32 count, SortElement* tempBuffer) {
	TIMED_FUNCTION;
	if (count == 1) {
		return;
	}
	if (count == 2) {
		SortElement* A = array;
		SortElement* B = array + 1;
		if (A->key > B->key) {
			Swap(A, B);
		}
		return;
	}
	u32 leftCount = count / 2;
	u32 rightCount = count - leftCount;

	SortElement* leftArray = array;
	SortElement* rightArray = array + leftCount;
	SortElement* leftTemp = tempBuffer;
	SortElement* rightTemp = tempBuffer + leftCount;
	MergeSortWithCopying(leftArray, leftCount, leftTemp);
	MergeSortWithCopying(rightArray, rightCount, rightTemp);

	SortElement* out = tempBuffer;
	u32 leftRemaining = leftCount;
	u32 rightRemaining = rightCount;
	for (u32 elementIndex = 0; elementIndex < count; elementIndex++) {
		SortElement* A = leftArray;
		SortElement* B = rightArray;

		if (leftRemaining && rightRemaining) {
			if (A->key > B->key) {
				*out++ = *B;
				rightArray++;
				rightRemaining--;
			}
			else {
				*out++ = *A;
				leftArray++;
				leftRemaining--;
			}
		}
		else if (leftRemaining) {
			*out++ = *A;
			leftArray++;
			leftRemaining--;
		}
		else if (rightRemaining) {
			*out++ = *B;
			rightArray++;
			rightRemaining--;
		}
	}
	SortElement* dest = array;
	SortElement* src = tempBuffer;
	for (u32 elementIndex = 0; elementIndex < count; elementIndex++) {
		*dest++ = *src++;
	}
	return;
}

inline
u32 FloatToRadixValue(f32 value) {
	u32 result = *ptrcast(u32, &value);

	/* NOTE:
	   FLOAT = |Sign bit (1)|Exponent (7)|Mantissa (24)|
	   Exponent and mantissa are monotonically increasing when float is increasing
	   Only sign bit is problematic (is 1 when float is negative)
	   So we set the sign bit for the positive numbers to make them higher than negatives
	   and we negate negative numbers to unset the sign bit and keep the monotonically
	   increasing order
	*/
	if (value < 0) {
		result = ~result;
	}
	else {
		result |= 0x80000000;
	}
	return result;
}

internal
void RadixSort(SortElement* array, u32 count, SortElement* tempBuffer) {
	TIMED_FUNCTION;
	for (u32 bitOffset = 0; bitOffset < 32; bitOffset += 8) {
		u32 bucketOffsets[256] = {};
		for (u32 elementIndex = 0; elementIndex < count; elementIndex++) {
			SortElement* element = array + elementIndex;
			u32 radixValue = FloatToRadixValue(element->key);
			u8 radixPiece = (radixValue >> bitOffset) & 0xFF;
			bucketOffsets[radixPiece]++;
		}

		u32 total = 0;
		for (u32 bucketIndex = 0; bucketIndex < ArrayCount(bucketOffsets); bucketIndex++) {
			u32 bucketCount = bucketOffsets[bucketIndex];
			bucketOffsets[bucketIndex] = total;
			total += bucketCount;
		}

		for (u32 elementIndex = 0; elementIndex < count; elementIndex++) {
			SortElement* src = array + elementIndex;

			u32 radixValue = FloatToRadixValue(src->key);
			u8 radixPiece = (radixValue >> bitOffset) & 0xFF;
			u32 position = bucketOffsets[radixPiece]++;

			SortElement* dest = tempBuffer + position;
			*dest = *src;
		}
		SortElement* swapper = array;
		array = tempBuffer;
		tempBuffer = swapper;
	}
}

internal
void SortRenderCommands(RenderCommandBuffer* commands) {
	TIMED_FUNCTION;
	SortElement* array = ptrcast(SortElement, commands->pushBuffer + commands->sortBufferAt);
	u32 count = commands->pushBufferCount;
	SortElement* tempBuffer = commands->sortTempBuffer;
#if 0
	count = 8;
	array = PushArray(tempArena, count, SortElement);
	SortElement* tempBuffer = PushArray(tempArena, count, SortElement);
	array[0] = SortElement{ 40.f,  70 /* i */ };
	array[1] = SortElement{ -20.f, 10 /* b */ };
	array[2] = SortElement{ 30.f,  60 /* g */ };
	array[3] = SortElement{ -30.f, 0  /* a */ };
	array[4] = SortElement{ 0.0f,  30 /* d */ };
	array[5] = SortElement{ 20.f,  50 /* f */ };
	array[6] = SortElement{ 10.f,  40 /* e */ };
	array[7] = SortElement{ -10.f, 20 /* c */ };

	//array[8] = SortElement{ 45.f,  75 /* i */ };
	//array[9] = SortElement{ -25.f, 15 /* b */ };
	//array[10] = SortElement{ 35.f,  65 /* g */ };
	//array[11] = SortElement{ -35.f, 5  /* a */ };
	//array[12] = SortElement{ 0.5f,  35 /* d */ };
	//array[13] = SortElement{ 25.f,  55 /* f */ };
	//array[14] = SortElement{ 15.f,  45 /* e */ };
	//array[15] = SortElement{ -15.f, 25 /* c */ };
#endif
	//BubbleSort(array, count);
	//MergeSort(array, count, tempBuffer);
	//MergeSortWithCopying(array, count, tempBuffer);
	RadixSort(array, count, tempBuffer);
#if SLOW_VALIDATION
	for (u32 sortIndex = 0; sortIndex < count - 1; sortIndex++) {
		SortElement* A = array + sortIndex;
		SortElement* B = array + sortIndex + 1;
		if (A->key > B->key) {
			Assert(!"Sorting does not work!");
		}
	}
#endif
}

internal
void RenderCommandsToBuffer(RenderCommandBuffer* commands, LoadedBitmap& dstBuffer) {
	Rect2i clipRect = { 0, 0, dstBuffer.width, dstBuffer.height };
	RenderCommandsToBuffer(commands, dstBuffer, clipRect);
}

struct RenderTiledArgs {
	Rect2i clipRect;
	RenderCommandBuffer* commands;
	LoadedBitmap* dstBuffer;
};

internal
void RenderTiled(void* data) {
	TIMED_FUNCTION;
	RenderTiledArgs* args = ptrcast(RenderTiledArgs, data);
	RenderCommandsToBuffer(args->commands, *args->dstBuffer, args->clipRect);
}

internal
void TiledRenderGroupToBuffer(RenderCommandBuffer* commands, LoadedBitmap& dstBuffer, PlatformQueue* queue) {
	TIMED_FUNCTION;
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
	Assert((reinterpret_cast<uptr>(dstBuffer.data) & 31) == 0);
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
			args->commands = commands;
#if 1 // Switch on = multithreaded rendering
			Platform->QueuePushTask(queue, RenderTiled, args);
#else
			RenderTiled(args);
#endif
		}
	}
	Platform->QueueWaitForCompletion(queue);
}
