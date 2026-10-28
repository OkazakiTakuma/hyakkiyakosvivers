#pragma once
struct Vector3 {
	float x, y, z;
};
/// <summary>
/// 2 つの値を加算した結果を返します。
/// </summary>
Vector3 Add(const Vector3& v1, const Vector3& v2);
Vector3 operator+(const Vector3& v1, const Vector3& v2);

/// <summary>
/// 2 つの値を減算した結果を返します。
/// </summary>
Vector3 Subtract(const Vector3& v1, const Vector3& v2);
Vector3 operator-(const Vector3& v1, const Vector3& v2) ;
/// <summary>
/// 2 つの値を乗算した結果を返します。
/// </summary>
Vector3 Multiply(float scalar, const Vector3& v);
Vector3 operator*(float scalar, const Vector3& v);
float Dot(const Vector3& v1, const Vector3& v2);
float Length(const Vector3& v);
/// <summary>
/// 値を正規化して扱いやすい状態にします。
/// </summary>
Vector3 Normalize(const Vector3& v);
Vector3 Leap(const Vector3& v1, const Vector3& v2, const float t);

Vector3 Cross(const Vector3& v1, const Vector3& v2);
struct Vector4 {
	float x, y, z, w;
	Vector4(float x = 0.0f, float y = 0.0f, float z = 0.0f, float w = 1.0f) : x(x), y(y), z(z), w(w) {}
	Vector4 operator+(const Vector4& other) const { return Vector4(x + other.x, y + other.y, z + other.z, w + other.w); }
	Vector4 operator-(const Vector4& other) const { return Vector4(x - other.x, y - other.y, z - other.z, w - other.w); }
};

struct Vector2 {
	float x;
	float y;
};

