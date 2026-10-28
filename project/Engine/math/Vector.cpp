#include "Vector.h"
#include <math.h>

/// <summary>
/// 2 つの値を加算した結果を返します。
/// </summary>
Vector3 Add(const Vector3& v1, const Vector3& v2) {
	Vector3 add = {v1.x + v2.x, v1.y + v2.y, v1.z + v2.z};
	return add;
}

Vector3 operator+(const Vector3& v1, const Vector3& v2) { return Add(v1, v2); }




/// <summary>
/// 2 つの値を減算した結果を返します。
/// </summary>
Vector3 Subtract(const Vector3& v1, const Vector3& v2) {
	Vector3 subtract = {v1.x + -v2.x, v1.y - v2.y, v1.z - v2.z};
	return subtract;
}

Vector3 operator-(const Vector3& v1, const Vector3& v2) { return Subtract(v1, v2); }

/// <summary>
/// 2 つの値を乗算した結果を返します。
/// </summary>
Vector3 Multiply(float scalar, const Vector3& v) {
	Vector3 multply = {scalar * v.x, scalar * v.y, scalar * v.z};
	return multply;
}

Vector3 operator*(float scalar, const Vector3& v) { return Multiply(scalar, v); }

float Dot(const Vector3& v1, const Vector3& v2) { return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z); }

float Length(const Vector3& v) { return sqrtf((v.x * v.x) + (v.y * v.y) + (v.z * v.z)); }

/// <summary>
/// 値を正規化して扱いやすい状態にします。
/// </summary>
Vector3 Normalize(const Vector3& v) {
	float length = sqrtf((v.x * v.x) + (v.y * v.y) + (v.z * v.z));

	Vector3 normalize = v;
	if (length != 0) {
		normalize.x = v.x / length;
		normalize.y = v.y / length;
		normalize.z = v.z / length;
	}
	return normalize;
}

Vector3 Leap(const Vector3& v1, const Vector3& v2, const float t)
{
	Vector3 result;

	result.x = v1.x + (v2.x - v1.x) * t;
	result.y = v1.y + (v2.y - v1.y) * t;
	result.z = v1.z + (v2.z - v1.z) * t;

	return result;
}
Vector3 Cross(const Vector3& v1, const Vector3& v2) {
	Vector3 cross = {v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x};
	return cross;
}


