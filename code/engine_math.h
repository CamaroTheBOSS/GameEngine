#pragma once
#include "engine_common.h"
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
f32 Squared(f32 A) {
	return A * A;
}

inline // Inner / Scalar / Dot
f32 Inner(V2 A, V2 B) {
	return A.X * B.X + A.Y * B.Y;
}
#pragma warning(pop)