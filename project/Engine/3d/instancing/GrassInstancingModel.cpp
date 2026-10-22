#include "GrassInstancingModel.h"

#include "InstancingModelCommon.h"
#include "GameTime.h"
#include "../object/Object3dCommon.h"

void GrassInstancingModel::Initialize(Model* model, uint32_t maxInstanceCount) {
	InstancingModel::Initialize(model, maxInstanceCount);

	auto* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	instanceBuffer_ = dxCommon->CreateBufferResource(sizeof(GrassInstanceData) * maxInstanceCount);
	instanceBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_));

	grassResource_ = dxCommon->CreateBufferResource(sizeof(GrassParameter));
	grassResource_->Map(0, nullptr, reinterpret_cast<void**>(&grassData_));
	grassData_->time = 0.0f;
	grassData_->windStrength = 0.08f;
	grassData_->windFrequency = 2.0f;
	grassData_->padding = 0.0f;
}

GrassInstancingModel::~GrassInstancingModel() {
	if (instanceBuffer_) {
		instanceBuffer_->Unmap(0, nullptr);
	}
	if (grassResource_) {
		grassResource_->Unmap(0, nullptr);
	}
	instanceData_ = nullptr;
	grassData_ = nullptr;
}

void GrassInstancingModel::PrepareDraw(ID3D12GraphicsCommandList* commandList) {
	InstancingModelCommon::GetInstance()->SetDraw();
	grassData_->time += GameTime::GetDeltaTime();
	commandList->SetGraphicsRootShaderResourceView(2, instanceBuffer_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(7, grassResource_->GetGPUVirtualAddress());
}

void GrassInstancingModel::WriteVisibleInstance(
	uint32_t index, const EulerTransform& transform, const Vector3& drawPosition) {
	instanceData_[index].position = drawPosition;
	instanceData_[index].scale = transform.scale.x;
	instanceData_[index].rotationY = transform.rotate.y;
	instanceData_[index].padding[0] = 0.0f;
	instanceData_[index].padding[1] = 0.0f;
	instanceData_[index].padding[2] = 0.0f;
}
