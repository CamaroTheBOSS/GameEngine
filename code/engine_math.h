#pragma once
#include "engine_common.h"
#include "engine_intrinsics.h"
#pragma warning(push)
#pragma warning(disable : 4201)

union V2 {
	struct {
		f32 X, Y;
	};
	f32 E[2];
};

V2 V2i(i32 X, i32 Y) {
	V2 result = V2{ f4(X), f4(Y) };
	return result;
}

inline
V2 operator+(V2 A, V2 B) {
	return V2{
		A.X + B.X,
		A.Y + B.Y
	};
}

inline
V2 operator-(V2 A, V2 B) {
	return V2{
		A.X - B.X,
		A.Y - B.Y
	};
}

inline
V2 operator-(V2 A) {
	return V2{
		-A.X,
		-A.Y
	};
}

inline
V2 operator*(f32 scalar, V2 A) {
	return V2{
		scalar * A.X,
		scalar * A.Y
	};
}

inline
V2 operator*(V2 A, f32 scalar) {
	return scalar * A;
}

inline
V2 operator/(V2 A, f32 scalar) {
	return V2{
		A.X / scalar,
		A.Y / scalar
	};
}

inline
V2& operator*=(V2& A, f32 scalar) {
	A.X *= scalar;
	A.Y *= scalar;
	return A;
}

inline
V2& operator+=(V2& A, V2 B) {
	A.X += B.X;
	A.Y += B.Y;
	return A;
}

inline
V2& operator-=(V2& A, V2 B) {
	A.X -= B.X;
	A.Y -= B.Y;
	return A;
}

inline
bool operator==(V2 A, V2 B) {
	return A.X == B.X && 
		   A.Y == B.Y;
}

inline
bool operator!=(V2 A, V2 B) {
	return !(A == B);
}

inline // Inner / Scalar / Dot
f32 Inner(V2 A, V2 B) {
	return A.X * B.X + A.Y * B.Y;
}

inline
V2 Hadamard(V2 A, V2 B) {
	return V2{ A.X * B.X, A.Y * B.Y };
}

inline f32 LengthSq(V2 A) {
	return Inner(A, A);
}

inline f32 Length(V2 A) {
	return SquareRoot(LengthSq(A));
}

union V3 {
	struct {
		f32 X, Y, Z;
	};
	struct {
		V2 XY;
		f32 _Z;
	};
	struct {
		f32 _X;
		V2 YZ;
	};
	struct {
		f32 R, G, B;
	};
	f32 E[3];
};

inline
V3 operator+(V3 A, V3 B) {
	return V3{
		A.X + B.X,
		A.Y + B.Y,
		A.Z + B.Z
	};
}

inline
V3 operator-(V3 A, V3 B) {
	return V3{
		A.X - B.X,
		A.Y - B.Y,
		A.Z - B.Z
	};
}

inline
V3 operator-(V3 A) {
	return V3{
		-A.X,
		-A.Y,
		-A.Z
	};
}

inline
V3 operator*(f32 scalar, V3 A) {
	return V3{
		scalar * A.X,
		scalar * A.Y,
		scalar * A.Z
	};
}

inline
V3 operator*(V3 A, f32 scalar) {
	return scalar * A;
}

inline
V3 operator/(V3 A, f32 scalar) {
	return V3{
		A.X / scalar,
		A.Y / scalar,
		A.Z / scalar
	};
}

inline
V3& operator*=(V3& A, f32 scalar) {
	A.X *= scalar;
	A.Y *= scalar;
	A.Z *= scalar;
	return A;
}

inline
V3& operator+=(V3& A, V3 B) {
	A.X += B.X;
	A.Y += B.Y;
	A.Z += B.Z;
	return A;
}

inline
V3& operator-=(V3& A, V3 B) {
	A.X -= B.X;
	A.Y -= B.Y;
	A.Z -= B.Z;
	return A;
}

inline
bool operator==(V3 A, V3 B) {
	return A.X == B.X &&
		A.Y == B.Y &&
		A.Z == B.Z;
}

inline
bool operator!=(V3 A, V3 B) {
	return !(A == B);
}

inline // Inner / Scalar / Dot
f32 Inner(V3 A, V3 B) {
	return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
}

inline
V3 Hadamard(V3 A, V3 B) {
	return V3{ A.X * B.X, A.Y * B.Y, A.Z * B.Z };
}

inline f32 LengthSq(V3 A) {
	return Inner(A, A);
}

inline f32 Length(V3 A) {
	return SquareRoot(LengthSq(A));
}

union V4 {
	struct {
		f32 X, Y, Z, W;
	};
	struct {
		V2 XY;
		f32 _pad0[2];
	};
	struct {
		V3 XYZ;
		f32 _pad1;
	};
	struct {
		f32 R, G, B, A;
	};
	struct {
		V3 RGB;
		f32 _pad2;
	};
	f32 E[4];
};

V4 V4i(i32 X, i32 Y, i32 Z, i32 W) {
	V4 result = V4{ f4(X), f4(Y), f4(Z), f4(W) };
	return result;
}

inline
V4 operator+(V4 A, V4 B) {
	return V4{
		A.X + B.X,
		A.Y + B.Y,
		A.Z + B.Z,
		A.W + B.W
	};
}

inline
V4 operator-(V4 A, V4 B) {
	return V4{
		A.X - B.X,
		A.Y - B.Y,
		A.Z - B.Z,
		A.W - B.W
	};
}

inline
V4 operator-(V4 A) {
	return V4{
		-A.X,
		-A.Y,
		-A.Z,
		-A.W
	};
}

inline
V4 operator*(f32 scalar, V4 A) {
	return V4{
		scalar * A.X,
		scalar * A.Y,
		scalar * A.Z,
		scalar * A.W
	};
}

inline
V4 operator*(V4 A, f32 scalar) {
	return scalar * A;
}

inline
V4 operator/(V4 A, f32 scalar) {
	return V4{
		A.X / scalar,
		A.Y / scalar,
		A.Z / scalar,
		A.W / scalar
	};
}

inline
V4& operator*=(V4& A, f32 scalar) {
	A.X *= scalar;
	A.Y *= scalar;
	A.Z *= scalar;
	A.W *= scalar;
	return A;
}

inline
V4& operator+=(V4& A, V4 B) {
	A.X += B.X;
	A.Y += B.Y;
	A.Z += B.Z;
	A.W += B.W;
	return A;
}

inline
V4& operator-=(V4& A, V4 B) {
	A.X -= B.X;
	A.Y -= B.Y;
	A.Z -= B.Z;
	A.W -= B.W;
	return A;
}

inline
bool operator==(V4 A, V4 B) {
	return A.X == B.X &&
		A.Y == B.Y &&
		A.Z == B.Z &&
		A.W == B.W;
}

inline
bool operator!=(V4 A, V4 B) {
	return !(A == B);
}

inline // Inner / Scalar / Dot
f32 Inner(V4 A, V4 B) {
	return A.X * B.X + 
		A.Y * B.Y + 
		A.Z * B.Z + 
		A.W * B.W;
}

inline
V4 Hadamard(V4 A, V4 B) {
	return V4{ 
		A.X * B.X, 
		A.Y * B.Y, 
		A.Z * B.Z, 
		A.W * B.W 
	};
}

inline f32 LengthSq(V4 A) {
	return Inner(A, A);
}

inline f32 Length(V4 A) {
	return SquareRoot(LengthSq(A));
}

inline
V2 ToV2(f32 X, f32 Y) {
	return V2{ X, Y };
}

inline
V3 ToV3(V2 XY, f32 Z) {
	V3 result;
	result.XY = XY;
	result.Z = Z;
	return result;
}

inline
V4 ToV4(V3 XYZ, f32 W) {
	V4 result;
	result.XYZ = XYZ;
	result.W = W;
	return result;
}

inline
V2 Normalize(V2 A) {
	return A / Length(A);
}

inline
V3 Normalize(V3 A) {
	return A / Length(A);
}

inline
V4 Normalize(V4 A) {
	return A / Length(A);
}

inline
f32 Clip01(f32 A) {
	if (A < 0.f) return 0.f;
	else if (A > 1.f) return 1.f;
	return A;
}

inline
V2 Clip01(V2 A) {
	V2 result = A;
	result.X = Clip01(result.X);
	result.Y = Clip01(result.Y);
	return result;
}

inline
V3 Clip01(V3 A) {
	V3 result = A;
	result.X = Clip01(result.X);
	result.Y = Clip01(result.Y);
	result.Z = Clip01(result.Z);
	return result;
}

inline
V4 Clip01(V4 A) {
	V4 result = A;
	result.X = Clip01(result.X);
	result.Y = Clip01(result.Y);
	result.Z = Clip01(result.Z);
	result.W = Clip01(result.W);
	return result;
}

inline
f32 Squared(f32 A) {
	return A * A;
}

struct Rect2 {
	V2 min;
	V2 max;
};

inline
V2 GetDim(Rect2 rect) {
	V2 result = rect.max - rect.min;
	return result;
}

inline
V2 GetMinCorner(Rect2 rect) {
	return rect.min;
}

inline
V2 GetMaxCorner(Rect2 rect) {
	return rect.max;
}

inline
V2 GetCenter(Rect2 rect) {
	return 0.5f * (rect.max - rect.min);
}

inline
Rect2 GetRectFromMinMax(V2 min, V2 max) {
	return Rect2{ min, max };
}

inline
Rect2 GetRectFromCenterHalfDim(V2 center, f32 halfDim) {
	Rect2 rect = {};
	rect.min = V2{
		center.X - halfDim,
		center.Y - halfDim
	};
	rect.max = V2{
		center.X + halfDim,
		center.Y + halfDim
	};
	return rect;
}

inline
Rect2 GetRectFromCenterHalfDim(V2 center, V2 halfDims) {
	Rect2 rect{
		.min = center - halfDims,
		.max = center + halfDims,
	};
	return rect;
}

inline
Rect2 GetRectFromCenterDim(V2 center, f32 dim) {
	return GetRectFromCenterHalfDim(center, dim / 2.f);
}

inline
Rect2 GetRectFromCenterDim(V2 center, V2 dims) {
	Rect2 rect{
		.min = center - dims / 2.f,
		.max = center + dims / 2.f,
	};
	return rect;
}

inline
bool IsInRectangle(Rect2 rect, V2 point) {
	return point.X < rect.max.X &&
		   point.X >= rect.min.X &&
		   point.Y < rect.max.Y &&
		   point.Y >= rect.min.Y;
}

inline
V2 Clip(V2 val, f32 min, f32 max) {
	// TODO: Speed up with SIMD
	V2 result = val;
	if		(result.X < min) result.X = min;
	else if (result.X > max) result.X = max;
	if		(result.Y < min) result.Y = min;
	else if (result.Y > max) result.Y = max;
	return result;
}

inline 
f32 Clip(f32 val, f32 min, f32 max) {
	f32 result = val;
	if		(result < min) result = min;
	else if (result > max) result = max;
	return result;
}

inline
V2 PointRelativeToRect(Rect2 rect, V2 point) {
	V2 result = V2{
		(point.X - rect.min.X) / (rect.max.X - rect.min.X),
		(point.Y - rect.min.Y) / (rect.max.Y - rect.min.Y),
	};
	result = Clip(result, 0.f, 1.f);
	return result;
}

inline 
bool EntityOverlapsWithRegion(V2 XY, V2 dim, Rect2 rect) {
	Rect2 grownRect = GetRectFromMinMax(rect.min - dim / 2, rect.max + dim / 2);
	return IsInRectangle(grownRect, XY);
}

inline
bool RectanglesOverlapsWithEachOther(Rect2 A, Rect2 B) {
	bool notOverlap = A.min.X > B.max.X ||
					  B.min.X > A.max.X ||
					  A.min.Y > B.max.Y ||
					  B.min.Y > A.max.Y;
	return !notOverlap;
}

struct Rect3 {
	V3 min;
	V3 max;
};

Rect3 ToRect3(Rect2 rect2, V2 minMaxZ) {
	Rect3 result{
		.min = ToV3(rect2.min, minMaxZ.E[0]),
		.max = ToV3(rect2.max, minMaxZ.E[1])
	};
	return result;
}

inline
V3 GetDim(Rect3 rect) {
	V3 result = rect.max - rect.min;
	return result;
}


inline
V3 GetMinCorner(Rect3 rect) {
	return rect.min;
}

inline
V3 GetMaxCorner(Rect3 rect) {
	return rect.max;
}

inline
V3 GetCenter(Rect3 rect) {
	return 0.5f * (rect.max - rect.min);
}

inline
Rect3 GetRectFromMinMax(V3 min, V3 max) {
	return Rect3{ min, max };
}

inline
Rect3 GetRectFromCenterHalfDim(V3 center, f32 halfDim) {
	Rect3 rect = {};
	rect.min = V3{
		center.X - halfDim,
		center.Y - halfDim,
		center.Z - halfDim
	};
	rect.max = V3{
		center.X + halfDim,
		center.Y + halfDim,
		center.Z + halfDim
	};
	return rect;
}

inline
Rect3 GetRectFromCenterDim(V3 center, f32 dim) {
	return GetRectFromCenterHalfDim(center, dim / 2.f);
}

inline
Rect3 GetRectFromCenterDim(V3 center, V3 dims) {
	Rect3 rect{
		.min = center - dims / 2.f,
		.max = center + dims / 2.f,
	};
	return rect;
}

inline
bool IsInRectangle(Rect3 rect, V3 point) {
	return point.X < rect.max.X &&
		point.X >= rect.min.X &&
		point.Y < rect.max.Y &&
		point.Y >= rect.min.Y &&
		point.Z < rect.max.Z &&
		point.Z >= rect.min.Z;
}

inline
bool IsInsideRectangle(Rect3 rect, V3 point) {
	return point.X < rect.max.X &&
		point.X > rect.min.X &&
		point.Y < rect.max.Y &&
		point.Y > rect.min.Y &&
		point.Z < rect.max.Z &&
		point.Z > rect.min.Z;
}

inline
bool IsInOrAtRectangle(Rect3 rect, V3 point) {
	return point.X < rect.max.X &&
		point.X >= rect.min.X &&
		point.Y <= rect.max.Y &&
		point.Y >= rect.min.Y &&
		point.Z <= rect.max.Z &&
		point.Z >= rect.min.Z;
}

inline
V3 Clip(V3 val, f32 min, f32 max) {
	// TODO: Speed up with SIMD
	V3 result = val;
	if		(result.X < min) result.X = min;
	else if (result.X > max) result.X = max;
	if		(result.Y < min) result.Y = min;
	else if (result.Y > max) result.Y = max;
	if		(result.Z < min) result.Z = min;
	else if (result.Z > max) result.Z = max;
	return result;
}

inline
V3 PointRelativeToRect(Rect3 rect, V3 point) {
	V3 result = V3{
		(point.X - rect.min.X) / (rect.max.X - rect.min.X),
		(point.Y - rect.min.Y) / (rect.max.Y - rect.min.Y),
		(point.Z - rect.min.Z) / (rect.max.Z - rect.min.Z),
	};
	result = Clip(result, 0.f, 1.f);
	return result;
}

inline
bool EntityOverlapsWithRegion(V3 XYZ, V3 dim, Rect3 rect) {
	Rect3 grownRect = GetRectFromMinMax(rect.min - dim / 2, rect.max + dim / 2);
	return IsInRectangle(grownRect, XYZ);
}

inline
bool RectanglesOverlapsWithEachOther(Rect3 A, Rect3 B) {
	bool notOverlap = A.min.X > B.max.X ||
					  B.min.X > A.max.X ||
					  A.min.Y > B.max.Y ||
					  B.min.Y > A.max.Y ||
					  A.min.Z > B.max.Z ||
					  B.min.Z > A.max.Z;
	return !notOverlap;
}

inline
f32 Lerp(f32 A, f32 unilateral, f32 B) {
	f32 result = A + unilateral * (B - A);
	return result;
}

inline
V2 Lerp(V2 A, f32 unilateral, V2 B) {
	V2 result = {
		Lerp(A.X, unilateral, B.X),
		Lerp(A.Y, unilateral, B.Y),
	};
	return result;
}

inline
V3 Lerp(V3 A, f32 unilateral, V3 B) {
	V3 result = {
		Lerp(A.X, unilateral, B.X),
		Lerp(A.Y, unilateral, B.Y),
		Lerp(A.Z, unilateral, B.Z),
	};
	return result;
}

inline
V4 Lerp(V4 A, f32 unilateral, V4 B) {
	V4 result = {
		Lerp(A.X, unilateral, B.X),
		Lerp(A.Y, unilateral, B.Y),
		Lerp(A.Z, unilateral, B.Z),
		Lerp(A.W, unilateral, B.W),
	};
	return result;
}

inline
V2 Abs(V2 A) {
	A.X = Abs(A.X);
	A.Y = Abs(A.Y);
	return A;
}

inline
V3 Abs(V3 A) {
	A.X = Abs(A.X);
	A.Y = Abs(A.Y);
	A.Z = Abs(A.Z);
	return A;
}

inline
V4 Abs(V4 A) {
	A.X = Abs(A.X);
	A.Y = Abs(A.Y);
	A.Z = Abs(A.Z);
	A.W = Abs(A.W);
	return A;
}

inline 
V2 Perp(V2 A) {
	return V2{ -A.Y, A.X };
}
#pragma warning(pop)
