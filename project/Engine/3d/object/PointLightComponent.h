#pragma once

#include "core/Component.h"
#include "Vector.h"

/// <summary>
/// GameObjectをシーン光源として扱うための設定です。
/// 表面の発光表現はObject3dのMaterialに残し、周囲を照らす責務だけを保持します。
/// </summary>
class PointLightComponent final : public Component {
public:
	void SetColor(const Vector4& color) { color_ = color; }
	const Vector4& GetColor() const { return color_; }

	void SetIntensity(float intensity) { intensity_ = intensity < 0.0f ? 0.0f : intensity; }
	float GetIntensity() const { return intensity_; }

	void SetRadius(float radius) { radius_ = radius < 0.01f ? 0.01f : radius; }
	float GetRadius() const { return radius_; }

	void SetDecay(float decay) { decay_ = decay < 0.0f ? 0.0f : decay; }
	float GetDecay() const { return decay_; }

	void SetPositionOffset(const Vector3& offset) { positionOffset_ = offset; }
	const Vector3& GetPositionOffset() const { return positionOffset_; }

	/// <summary>有効時は同じGameObjectのMaterial発光色をライト色として使用します。</summary>
	void SetUseMaterialEmissionColor(bool useMaterialEmissionColor) {
		useMaterialEmissionColor_ = useMaterialEmissionColor;
	}
	bool GetUseMaterialEmissionColor() const { return useMaterialEmissionColor_; }

private:
	Vector4 color_ = {1.0f, 1.0f, 1.0f, 1.0f};
	Vector3 positionOffset_ = {0.0f, 0.0f, 0.0f};
	float intensity_ = 1.0f;
	float radius_ = 10.0f;
	float decay_ = 1.0f;
	bool useMaterialEmissionColor_ = false;
};
