#pragma once
#include"Vector.h"
#include"Quaternion.h"

struct Matrix4x4 {
	float m[4][4];
	
};

struct EulerTransform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct QuaternionTransform {
	Vector3 scale;
	Quaternion rotate;
	Vector3 translate;
};

/// <summary>
/// 2 つの値を加算した結果を返します。
/// </summary>
/// <param name="m1">計算に使用する行列を指定します。</param>
/// <param name="m2">計算に使用する行列を指定します。</param>
Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2);
/// <param name="m1">計算に使用する行列を指定します。</param>
/// <param name="m2">計算に使用する行列を指定します。</param>
Matrix4x4 operator+(const Matrix4x4& m1, const Matrix4x4& m2);
/// <summary>
/// 2 つの値を減算した結果を返します。
/// </summary>
/// <param name="m1">計算に使用する行列を指定します。</param>
/// <param name="m2">計算に使用する行列を指定します。</param>
Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2);
/// <param name="m1">計算に使用する行列を指定します。</param>
/// <param name="m2">計算に使用する行列を指定します。</param>
Matrix4x4 operator-(const Matrix4x4& m1, const Matrix4x4& m2);
/// <summary>
/// 2 つの値を乗算した結果を返します。
/// </summary>
/// <param name="m1">計算に使用する行列を指定します。</param>
/// <param name="m2">計算に使用する行列を指定します。</param>
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);
/// <param name="m1">計算に使用する行列を指定します。</param>
/// <param name="m2">計算に使用する行列を指定します。</param>
Matrix4x4 operator*(const Matrix4x4& m1, const Matrix4x4& m2);
/// <param name="m">計算に使用する行列を指定します。</param>
Vector3 MultiplyVector3(const Matrix4x4& m, const Vector3& v);
/// <param name="m">計算に使用する行列を指定します。</param>
Vector3 operator*(const Matrix4x4& m, const Vector3& v);

/// <summary>
/// 逆行列または逆元を計算して返します。
/// </summary>
/// <param name="m">計算に使用する行列を指定します。</param>
Matrix4x4 Inverse(const Matrix4x4& m);
/// <summary>
/// 転置行列を計算して返します。
/// </summary>
/// <param name="m">計算に使用する行列を指定します。</param>
Matrix4x4 Transpose(const Matrix4x4& m);
/// <summary>
/// Identity4x4 を生成して返します。
/// </summary>
Matrix4x4 MakeIdentity4x4();
/// <summary>
/// TranslateMatrix を生成して返します。
/// </summary>
/// <param name="translate">位置を指定します。</param>
Matrix4x4 MakeTranslateMatrix(const Vector3& translate);
/// <summary>
/// ScaleMatrix を生成して返します。
/// </summary>
/// <param name="scale">拡大率を指定します。</param>
Matrix4x4 MakeScaleMatrix(const Vector3& scale);
/// <summary>
/// RotateXMatrix を生成して返します。
/// </summary>
/// <param name="radians">回転角をラジアンで指定します。</param>
Matrix4x4 MakeRotateXMatrix(float radians);
/// <summary>
/// RotateYMatrix を生成して返します。
/// </summary>
/// <param name="radians">回転角をラジアンで指定します。</param>
Matrix4x4 MakeRotateYMatrix(float radians);
/// <summary>
/// RotateZMatrix を生成して返します。
/// </summary>
/// <param name="radians">回転角をラジアンで指定します。</param>
Matrix4x4 MakeRotateZMatrix(float radians);
/// <summary>
/// RotateXYZMatrix を生成して返します。
/// </summary>
/// <param name="radians">各軸の回転角をラジアンで指定します。</param>
Matrix4x4 MakeRotateXYZMatrix(const Vector3& radians);
/// <summary>
/// AffineMatrix を生成して返します。
/// </summary>
/// <param name="scale">拡大率を指定します。</param>
/// <param name="rotate">回転量を指定します。</param>
/// <param name="translate">位置を指定します。</param>
Matrix4x4 MakeAffineMatrix(Vector3 scale, Vector3 rotate, Vector3 translate);
/// <param name="matrix">計算に使用する行列を指定します。</param>
Vector3 Transformation(const Vector3& vector, const Matrix4x4& matrix);
/// <param name="near">近平面までの距離を指定します。</param>
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom,float near,float far);

/// <summary>
/// 透視投影行列を生成して返します。
/// </summary>
/// <param name="nearClip">近平面までの距離を指定します。</param>
/// <param name="farClip">遠平面までの距離を指定します。</param>
/// <param name="fovY">垂直方向の視野角をラジアンで指定します。</param>
/// <param name="aspectRatio">アスペクト比を指定します。</param>
/// <returns></returns>
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

/// <summary>
/// ビューポート変換行列を生成して返します。
/// </summary>
/// <param name="left">ビューポートの左端を指定します。</param>
/// <param name="top">ビューポートの上端を指定します。</param>
/// <param name="width">ビューポートの幅を指定します。</param>
/// <param name="height">ビューポートの高さを指定します。</param>
/// <param name="minDepth">深度バッファの最小値を指定します。</param>
/// <param name="maxDepth">深度バッファの最大値を指定します。</param>
/// <returns></returns>
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);
