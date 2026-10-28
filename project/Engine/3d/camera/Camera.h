#pragma once
#include "Matrix.h"
#include "Vector.h"

class Camera {
public:
	Camera();
	/// <summary>
	/// 毎フレームの状態更新を行います。
	/// </summary>
	void Update();
	const Vector3& GetTranslate() { return transform.translate; }
	void SetTranslate(const Vector3& newTransform) { transform.translate = newTransform; }
	const Vector3& GetRotate() { return transform.rotate; }
	void SetRotate(const Vector3& newTransformRotate) { transform.rotate = newTransformRotate; }
	void SetfovY(float fovy) { fovY = fovy; }
	void SetAspectRatio(float aspect) { aspectRatio = aspect; }
	void SetNearClip(float nearC) { nearClip = nearC; }
	void SetFarClip(float farC) { farClip = farC; }
	float GetFovY() const { return fovY; }
	float GetAspectRatio() const { return aspectRatio; }
	float GetNearClip() const { return nearClip; }
	float GetFarClip() const { return farClip; }

	const Matrix4x4& GetWorldMatrix() const { return worldMatrix; }
	const Matrix4x4& GetViewMatrix() const { return viewMatrix; }
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix; }
	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix; }



private:
	EulerTransform transform = {
	    {1.0f, 1.0f, 1.0f  },
	    {0.0f, 0.0f, 0.0f  },
	    {0.0f, 4.0f, -10.0f}
	};
	Matrix4x4 worldMatrix;
	Matrix4x4 viewMatrix;
	Matrix4x4 projectionMatrix;
	Matrix4x4 viewProjectionMatrix;
	float fovY;
	float aspectRatio;
	float nearClip;
	float farClip;
};
