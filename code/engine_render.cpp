#include "engine_common.h"
#include "engine_render.h"

inline
void PushDrawCall(RenderGroup& group, LoadedBitmap* bitmap, V3 center, V3 rectSize, f32 R, f32 G, f32 B, f32 A, V2 offset) {
	Assert(group.count < ArrayCount(group.drawCalls));
	DrawCall* call = &group.drawCalls[group.count++];
	call->bitmap = bitmap;
	call->center = center;
	call->rectSize = rectSize;
	call->R = R;
	call->G = G;
	call->B = B;
	call->A = A;
	call->offset = offset;
}

inline
void PushBitmap(RenderGroup& group, LoadedBitmap* bitmap, V3 center, f32 A, V2 offset) {
	PushDrawCall(group, bitmap, center, {}, 0, 0, 0, A, offset);
}

inline
void PushRect(RenderGroup& group, V3 center, V3 rectSize, f32 R, f32 G, f32 B, f32 A, V2 offset) {
	PushDrawCall(group, 0, center, rectSize, R, G, B, A, offset);
}