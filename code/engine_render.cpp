#include "engine_common.h"
#include "engine_render.h"

#define PushRenderEntry(group, type) ptrcast(type, PushRenderEntry_(group, sizeof(type), RenderCallType::##type))
inline 
RenderCallHeader* PushRenderEntry_(RenderGroup& group, u32 size, RenderCallType type) {
	Assert(group.pushBufferSize + size <= group.maxPushBufferSize);
	if (group.pushBufferSize + size > group.maxPushBufferSize) {
		return 0;
	}
	RenderCallHeader* result = ptrcast(RenderCallHeader, group.pushBuffer + group.pushBufferSize);
	result->type = type;
	group.pushBufferSize += size;
	return result;
}

inline
RenderCallClear* PushClearCall(RenderGroup& group, f32 R, f32 G, f32 B, f32 A) {
	RenderCallClear* call = PushRenderEntry(group, RenderCallClear);
	call->R = R;
	call->G = G;
	call->B = B;
	call->A = A;
	return call;
}

inline
RenderCallBitmap* PushBitmap(RenderGroup& group, LoadedBitmap* bitmap, V3 center, f32 A, V2 offset) {
	RenderCallBitmap* call = PushRenderEntry(group, RenderCallBitmap);
	call->bitmap = bitmap;
	call->center = center;
	call->offset = offset;
	call->alpha = A;
	return call;
}

inline
RenderCallRectangle* PushRect(RenderGroup& group, V3 center, V3 rectSize, f32 R, f32 G, f32 B, f32 A, V2 offset) {
	RenderCallRectangle* call = PushRenderEntry(group, RenderCallRectangle);
	call->center = center;
	call->rectSize = rectSize;
	call->R = R;
	call->G = G;
	call->B = B;
	call->A = A;
	call->offset = offset;
	return call;
}

inline
RenderCallCoordinateSystem* PushCoordinateSystem(RenderGroup& group, V2 origin, V2 xAxis, V2 yAxis, V4 color, LoadedBitmap* bitmap) {
	RenderCallCoordinateSystem* call = PushRenderEntry(group, RenderCallCoordinateSystem);
	call->origin = origin;
	call->xAxis = xAxis;
	call->yAxis = yAxis;
	call->color = color;
	call->bitmap = bitmap;
	return call;
}

internal
void RenderRectangle(LoadedBitmap& bitmap, V2 start, V2 end, f32 R, f32 G, f32 B) {
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
	u32 color = (scast(u32, 255 * R) << 16) +
		(scast(u32, 255 * G) << 8) +
		(scast(u32, 255 * B) << 0);
	u8* row = ptrcast(u8, bitmap.data) + minY * bitmap.pitch + minX * BITMAP_BYTES_PER_PIXEL;
	for (i32 Y = minY; Y < maxY; Y++) {
		u32* pixel = ptrcast(u32, row);
		for (i32 X = minX; X < maxX; X++) {
			*pixel++ = color;
		}
		row += bitmap.pitch;
	}
}


internal
void RenderRectangleSlowly(LoadedBitmap& bitmap, V2 origin, V2 xAxis, V2 yAxis, V4 color, LoadedBitmap& texture) {
	u32 colorU32 =  (scast(u32, 255 * color.A) << 24) +
					(scast(u32, 255 * color.R) << 16) +
					(scast(u32, 255 * color.G) << 8) +
					(scast(u32, 255 * color.B) << 0);
	i32 minY = i4(origin.Y);
	i32 maxY = i4((origin + xAxis + yAxis).Y);
	i32 minX = i4(origin.X);
	i32 maxX = i4((origin + xAxis + yAxis).X);
	V2 points[4] = {
		origin,
		origin + xAxis,
		origin + yAxis,
		origin + xAxis + yAxis
	};
	for (u32 pIndex = 0; pIndex < ArrayCount(points); pIndex++) {
		V2 testP = points[pIndex];
		if (testP.Y < minY) minY = i4(testP.Y);
		if (testP.Y > maxY) maxY = i4(testP.Y);
		if (testP.X < minX) minX = i4(testP.X);
		if (testP.X > maxX) maxX = i4(testP.X);
	}
	if (minY < 0) minY = 0;
	if (maxY > bitmap.height) maxY = bitmap.height;
	if (minX < 0) minX = 0;
	if (maxX > bitmap.width) maxX = bitmap.width;
	
	
	u8* row = ptrcast(u8, bitmap.data) + minY * bitmap.pitch + minX * BITMAP_BYTES_PER_PIXEL;
	for (i32 Y = minY; Y < maxY; Y++) {
		u32* dstPixel = ptrcast(u32, row);
		for (i32 X = minX; X < maxX; X++) {
#if 1
			if (Y == (minY + maxY) / 2 &&
				X == (minX + maxY) / 2) {
				int breakHere = 5;
			}
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

				f32 u = Inner(d, xAxis) / (LengthSq(xAxis));
				f32 v = Inner(d, yAxis) / (LengthSq(yAxis));
				// TODO: U and V values should be clipped 
				Assert(u >= 0 && u <= 1.0012f);
				Assert(v >= 0 && v <= 1.0012f);
				// TODO: What with last row and last column?
				V2 texelVec = V2{
					 1.f + u * (texture.width - 3),
					 1.f + v * (texture.height - 3)
				};
				i32 texelX = u4(texelVec.X);
				i32 texelY = u4(texelVec.Y);
				V2 texelFracHalf = V2{
					texelVec.X - texelX,
					texelVec.Y - texelY
				};
				i32 neighbourX = CeilF32ToU32(texelFracHalf.X * 2.f - 1.f);

				i32 texelByteIndex00 = texelY * texture.pitch + texelX * BITMAP_BYTES_PER_PIXEL;
				i32 texelByteIndex01 = texelY * texture.pitch + (texelX + 1) * BITMAP_BYTES_PER_PIXEL;
				i32 texelByteIndex10 = (texelY + 1) * texture.pitch + texelX * BITMAP_BYTES_PER_PIXEL;
				i32 texelByteIndex11 = (texelY + 1) * texture.pitch + (texelX + 1) * BITMAP_BYTES_PER_PIXEL;

				u32* texel00 = ptrcast(u32, ptrcast(u8, texture.data) + texelByteIndex00);
				u32* texel01 = ptrcast(u32, ptrcast(u8, texture.data) + texelByteIndex01);
				u32* texel10 = ptrcast(u32, ptrcast(u8, texture.data) + texelByteIndex10);
				u32* texel11 = ptrcast(u32, ptrcast(u8, texture.data) + texelByteIndex11);

				// RGBA
				V4 texelColor00 = {
					f4((*texel00 >> 16) & 0xFF),
					f4((*texel00 >> 8) & 0xFF),
					f4((*texel00 >> 0) & 0xFF),
					f4((*texel00 >> 24) & 0xFF)
				};
				V4 texelColor01 = {
					f4((*texel01 >> 16) & 0xFF),
					f4((*texel01 >> 8) & 0xFF),
					f4((*texel01 >> 0) & 0xFF),
					f4((*texel01 >> 24) & 0xFF)
				};
				V4 texelColor10 = {
					f4((*texel10 >> 16) & 0xFF),
					f4((*texel10 >> 8) & 0xFF),
					f4((*texel10 >> 0) & 0xFF),
					f4((*texel10 >> 24) & 0xFF)
				};
				V4 texelColor11 = {
					f4((*texel11 >> 16) & 0xFF),
					f4((*texel11 >> 8) & 0xFF),
					f4((*texel11 >> 0) & 0xFF),
					f4((*texel11 >> 24) & 0xFF)
				};
#if 1
				V4 finalTexel = Lerp(
					Lerp(texelColor00, texelFracHalf.X, texelColor01),
					texelFracHalf.Y,
					Lerp(texelColor10, texelFracHalf.X, texelColor11)
				);
#else
				V4 finalTexel = texelColor00;
#endif

				f32 sA = finalTexel.A / 255.f;
				f32 sR = finalTexel.R;
				f32 sG = finalTexel.G;
				f32 sB = finalTexel.B;

				f32 dA = scast(f32, (*dstPixel >> 24) & 0xFF) / 255.f;
				f32 dR = scast(f32, (*dstPixel >> 16) & 0xFF);
				f32 dG = scast(f32, (*dstPixel >> 8) & 0xFF);
				f32 dB = scast(f32, (*dstPixel >> 0) & 0xFF);

				u8 a = scast(u8, 255.f * (sA + dA - sA * dA));
				u8 r = scast(u8, sR + (1 - sA) * dR);
				u8 g = scast(u8, sG + (1 - sA) * dG);
				u8 b = scast(u8, sB + (1 - sA) * dB);

				*dstPixel = (a << 24) | (r << 16) | (g << 8) | (b << 0);
			}
#else
			*dstPixel = colorU32;
#endif
			dstPixel++;
		}
		row += bitmap.pitch;
	}
}

internal
void RenderRectBorders(LoadedBitmap& bitmap, V2 center, V2 size, V3 color, f32 thickness) {
	V2 leftUpWallStart = {
		center.X - 0.5f * (size.X + thickness),
		center.Y - 0.5f * (size.X + thickness),
	};
	V2 leftWallEnd = {
		center.X - 0.5f * (size.X - thickness),
		center.Y + 0.5f * (size.X + thickness),
	};
	V2 upWallEnd = {
		center.X + 0.5f * (size.X + thickness),
		center.Y - 0.5f * (size.X - thickness),
	};
	V2 rightWallStart = {
		center.X + 0.5f * (size.X - thickness),
		center.Y - 0.5f * (size.X - thickness),
	};
	V2 bottomWallStart = {
		center.X - 0.5f * (size.X + thickness),
		center.Y + 0.5f * (size.X - thickness),
	};
	V2 rightBottomWallEnd = {
		center.X + 0.5f * (size.X + thickness),
		center.Y + 0.5f * (size.X + thickness),
	};
	RenderRectangle(bitmap, leftUpWallStart, leftWallEnd, color.R, color.G, color.B);
	RenderRectangle(bitmap, leftUpWallStart, upWallEnd, color.R, color.G, color.B);
	RenderRectangle(bitmap, rightWallStart, rightBottomWallEnd, color.R, color.G, color.B);
	RenderRectangle(bitmap, bottomWallStart, rightBottomWallEnd, color.R, color.G, color.B);
}

// TODO: compress this function to only RenderRectBorders()
internal
void PushRectBorders(RenderGroup& group, V3 center, V3 size, f32 thickness) {
	PushRect(group, center - V3{ 0.5f * size.X, 0, 0 },
		V3{ thickness, size.Y, size.Z }, 0.f, 0.f, 1.f, 1.f, {});
	PushRect(group, center + V3{ 0.5f * size.X, 0, 0 },
		V3{ thickness, size.Y, size.Z }, 0.f, 0.f, 1.f, 1.f, {});
	PushRect(group, center - V3{ 0, 0.5f * size.Y, 0 },
		V3{ size.X, thickness, size.Z }, 0.f, 0.f, 1.f, 1.f, {});
	PushRect(group, center + V3{ 0, 0.5f * size.Y, 0 },
		V3{ size.X, thickness, size.Z }, 0.f, 0.f, 1.f, 1.f, {});
}

internal
void RenderBitmap(LoadedBitmap& screenBitmap, LoadedBitmap& loadedBitmap, V2 position) {
	i32 minX = RoundF32ToI32(position.X) - loadedBitmap.alignX;
	i32 maxX = minX + loadedBitmap.width;
	i32 minY = RoundF32ToI32(position.Y) - loadedBitmap.alignY;
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
			f32 dA = scast(f32, (*dstPixel >> 24) & 0xFF) / 255.f;
			f32 dR = scast(f32, (*dstPixel >> 16) & 0xFF);
			f32 dG = scast(f32, (*dstPixel >> 8) & 0xFF);
			f32 dB = scast(f32, (*dstPixel >> 0) & 0xFF);

			f32 sA = scast(f32, (*srcPixel >> 24) & 0xFF) / 255.f;
			f32 sR = scast(f32, (*srcPixel >> 16) & 0xFF);
			f32 sG = scast(f32, (*srcPixel >> 8) & 0xFF);
			f32 sB = scast(f32, (*srcPixel >> 0) & 0xFF);

			u8 a = scast(u8, 255.f * (sA + dA - sA * dA));
			u8 r = scast(u8, sR + (1 - sA) * dR);
			u8 g = scast(u8, sG + (1 - sA) * dG);
			u8 b = scast(u8, sB + (1 - sA) * dB);

			*dstPixel++ = (a << 24) | (r << 16) | (g << 8) | (b << 0);
			srcPixel++;
		}
		dstRow += screenBitmap.pitch;
		srcRow += loadedBitmap.pitch;
	}
}

internal
V2 CalculateRenderingObjectCenter() {
	/*V3 groundLevel = call->center - 0.5f * V3{ 0, 0, call->rectSize.Z };
	f32 zFudge = 0.1f * groundLevel.Z;
	V2 center = { (1.f + zFudge) * groundLevel.X * pixelsPerMeter + dstBuffer.width / 2.0f,
				  scast(f32, dstBuffer.height) - (1.f + zFudge) * groundLevel.Y * pixelsPerMeter - dstBuffer.height / 2.0f - groundLevel.Z * pixelsPerMeter };*/
	return V2{ 0, 0 };
}

internal
void RenderGroupToBuffer(RenderGroup& group, LoadedBitmap& dstBuffer) {
	u32 relativeRenderAddress = 0;
	while (relativeRenderAddress < group.pushBufferSize) {
		u32 relativeAddressBeforeSwitchCase = relativeRenderAddress;
		RenderCallHeader* header = ptrcast(RenderCallHeader, group.pushBuffer + relativeRenderAddress);
		switch (header->type) {
		case RenderCallType::RenderCallClear: {
			RenderCallClear* call = ptrcast(RenderCallClear, header);
			RenderRectangle(
				dstBuffer, 
				V2{ 0, 0 }, 
				V2i(dstBuffer.width, dstBuffer.height), 
				call->R, call->G, call->B
			);
			relativeRenderAddress += sizeof(RenderCallClear);
		} break;
		case RenderCallType::RenderCallRectangle: {
			RenderCallRectangle* call = ptrcast(RenderCallRectangle, header);
			V3 groundLevel = call->center -0.5f * V3{ 0, 0, call->rectSize.Z };
			f32 zFudge = 0.1f * groundLevel.Z;
			V2 center = { (1.f + zFudge) * groundLevel.X * pixelsPerMeter + dstBuffer.width / 2.0f,
						  scast(f32, dstBuffer.height) - (1.f + zFudge) * groundLevel.Y * pixelsPerMeter - dstBuffer.height / 2.0f - groundLevel.Z * pixelsPerMeter };
			V2 size = { (1.f + zFudge) * call->rectSize.X,
						(1.f + zFudge) * call->rectSize.Y };

			V2 min = center - size / 2.f * pixelsPerMeter;
			V2 max = min + size * pixelsPerMeter;
			RenderRectangle(dstBuffer, min, max, call->R, call->G, call->B);
			relativeRenderAddress += sizeof(RenderCallRectangle);
		} break;
		case RenderCallType::RenderCallBitmap: {
			// TODO: RenderCallBitmap and RenderCallRectangle have different approaches to calculate center
			// It should be unified (check groundLevel) which is different from RenderCallRectangle,
			// also, size is properly changed in RenderCallRectangle and not in RenderCallBitmap
			RenderCallBitmap* call = ptrcast(RenderCallBitmap, header);
			V3 groundLevel = call->center; // - 0.5f* V3{ 0, 0, call->rectSize.Z };
			f32 zFudge = 0.1f * groundLevel.Z;
			V2 center = { (1.f + zFudge) * groundLevel.X * pixelsPerMeter + dstBuffer.width / 2.0f,
						  scast(f32, dstBuffer.height) - (1.f + zFudge) * groundLevel.Y * pixelsPerMeter - dstBuffer.height / 2.0f - groundLevel.Z * pixelsPerMeter };
			/*V2 size = { (1.f + zFudge) * call->rectSize.X,
						(1.f + zFudge) * call->rectSize.Y };*/

			V2 offset = {
					center.X - call->offset.X * pixelsPerMeter,
					center.Y + call->offset.Y * pixelsPerMeter
			};
			RenderBitmap(dstBuffer, *call->bitmap, offset);
			relativeRenderAddress += sizeof(RenderCallBitmap);
		} break;
		case RenderCallType::RenderCallCoordinateSystem: {
			RenderCallCoordinateSystem* call = ptrcast(RenderCallCoordinateSystem, header);
			RenderRectangleSlowly(dstBuffer, call->origin, call->xAxis, call->yAxis, call->color, *call->bitmap);
			
			RenderRectangle(dstBuffer, call->origin, call->origin + V2{5.f, 5.f}, 1.f, 0.f, 0.f);
			RenderRectangle(dstBuffer, call->origin + call->xAxis, call->origin + call->xAxis + V2{ 5.f, 5.f }, 1.f, 0.f, 0.f);
			RenderRectangle(dstBuffer, call->origin + call->yAxis, call->origin + call->yAxis + V2{ 5.f, 5.f }, 1.f, 0.f, 0.f);
			
			relativeRenderAddress += sizeof(RenderCallCoordinateSystem);
		} break;
		InvalidDefaultCase;
		}
		Assert(relativeAddressBeforeSwitchCase < relativeRenderAddress);
	}
}

inline
RenderGroup AllocateRenderGroup(MemoryArena& arena, u32 size) {
	RenderGroup result = {};
	result.pushBuffer = PushArray(arena, size, u8);
	result.maxPushBufferSize = size;
	result.pushBufferSize = 0;
	return result;
}