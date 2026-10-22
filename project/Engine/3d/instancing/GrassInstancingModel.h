#pragma once

#include "InstancingModel.h"

/// <summary>風で揺れる草を描画する、草専用のインスタンシングレンダラーです。</summary>
class GrassInstancingModel final : public InstancingModel {
public:
	void Initialize(Model* model, uint32_t maxInstanceCount);
	~GrassInstancingModel() override;

	void SetWind(float strength, float frequency) {
		if (!grassData_) return;
		grassData_->windStrength = strength;
		grassData_->windFrequency = frequency;
	}

protected:
	void PrepareDraw(ID3D12GraphicsCommandList* commandList) override;
	void WriteVisibleInstance(
		uint32_t index, const EulerTransform& transform, const Vector3& drawPosition) override;

private:
	struct GrassInstanceData {
		Vector3 position;
		float scale;
		float rotationY;
		float padding[3];
	};
	static_assert(sizeof(GrassInstanceData) == 32, "GrassInstanceData must match the HLSL structured-buffer layout.");

	struct GrassParameter {
		float time;
		float windStrength;
		float windFrequency;
		float padding;
	};

	Microsoft::WRL::ComPtr<ID3D12Resource> grassResource_;
	GrassParameter* grassData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_;
	GrassInstanceData* instanceData_ = nullptr;
};
