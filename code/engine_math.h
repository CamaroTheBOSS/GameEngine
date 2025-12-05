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
		f32 Z;
	};
	struct {
		f32 X;
		V2 YZ;
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

inline f32 LengthSq(V3 A) {
	return Inner(A, A);
}

inline f32 Length(V3 A) {
	return SquareRoot(LengthSq(A));
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

struct Rect3 {
	V3 min;
	V3 max;
};

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
#pragma warning(pop)