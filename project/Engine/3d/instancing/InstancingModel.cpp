#include "InstancingModel.h"
#include "../object/Object3dCommon.h"
#include "TextureManager.h"
#include "GameTime.h"
#include <cmath>

/// <summary>
/// 必要なリソースを準備し、オブジェクトを初期化します。
/// </summary>
void InstancingModel::Initialize(Model* model, uint32_t maxInstanceCount) {
    model_ = model;
    maxInstanceCount_ = maxInstanceCount;

    chunks_.reserve(128);
    chunkLookup_.reserve(128);

    CreateInstanceBuffer();
    CreateConstantBuffers();
}

/// <summary>
/// 破棄時に必要な解放処理を行います。
/// </summary>
InstancingModel::~InstancingModel() {
    if (instanceBuffer_) instanceBuffer_->Unmap(0, nullptr);
    if (lightResource_) lightResource_->Unmap(0, nullptr);
    if (cameraResource_) cameraResource_->Unmap(0, nullptr);
    if (pointLightResource_) pointLightResource_->Unmap(0, nullptr);
    if (grassResource_) grassResource_->Unmap(0, nullptr);
}

/// <summary>
/// InstanceBuffer を作成し、利用できる状態にします。
/// </summary>
void InstancingModel::CreateInstanceBuffer() {
    auto dxCommon = Object3dCommon::GetInstance()->GetDxCommon();

    uint32_t bufferSize = sizeof(InstancingMatrixData) * maxInstanceCount_;
    instanceBuffer_ = dxCommon->CreateBufferResource(bufferSize);

    instanceBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData_));
}

/// <summary>
/// ConstantBuffers を作成し、利用できる状態にします。
/// </summary>
void InstancingModel::CreateConstantBuffers() {
    auto dxCommon = Object3dCommon::GetInstance()->GetDxCommon();

    lightResource_ = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
    lightResource_->Map(0, nullptr, reinterpret_cast<void**>(&lightData_));
    lightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    lightData_->direction = { 0.0f, -1.0f, 0.0f };
    lightData_->intensity = 1.0f;

    cameraResource_ = dxCommon->CreateBufferResource(sizeof(CameraForGPU));
    cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
    cameraData_->worldPosition = { 0.0f, 0.0f, -10.0f };
    cameraData_->environmentMultiplier = 0.0f;

    pointLightResource_ = dxCommon->CreateBufferResource(sizeof(PointLight));
    pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));
    pointLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    pointLightData_->position = { 0.0f, 0.0f, 0.0f };
    pointLightData_->intensity = 0.0f;
    pointLightData_->radius = 10.0f;
    pointLightData_->decay = 1.0f;

    grassResource_ = dxCommon->CreateBufferResource(sizeof(GrassParameter));
    grassResource_->Map(0, nullptr, reinterpret_cast<void**>(&grassData_));
    grassData_->time = 0.0f;
    grassData_->windStrength = 0.08f;
    grassData_->windFrequency = 2.0f;
    grassData_->padding = 0.0f;
}

void InstancingModel::AddInstance(const EulerTransform& transform) {
    if (instanceCount_ >= maxInstanceCount_) {
        return;
    }

    const int32_t chunkX = static_cast<int32_t>(std::floor(transform.translate.x / kChunkSize));
    const int32_t chunkZ = static_cast<int32_t>(std::floor(transform.translate.z / kChunkSize));
    const uint64_t key =
        (static_cast<uint64_t>(static_cast<uint32_t>(chunkX)) << 32) |
        static_cast<uint32_t>(chunkZ);

    auto chunkIt = chunkLookup_.find(key);
    if (chunkIt == chunkLookup_.end()) {
        const size_t chunkIndex = chunks_.size();
        InstanceChunk chunk;
        chunk.x = chunkX;
        chunk.z = chunkZ;
        chunk.center = {
            (static_cast<float>(chunkX) + 0.5f) * kChunkSize,
            0.0f,
            (static_cast<float>(chunkZ) + 0.5f) * kChunkSize
        };
        chunk.transforms.reserve(256);
        chunks_.push_back(std::move(chunk));
        chunkLookup_.emplace(key, chunkIndex);
        chunkIt = chunkLookup_.find(key);
    }

    chunks_[chunkIt->second].transforms.push_back(transform);
    ++instanceCount_;
}

/// <summary>
/// 現在の状態をもとに描画処理を行います。
/// </summary>
/// <param name="camera">描画や座標変換に使用するカメラを指定します。</param>
void InstancingModel::Draw(Camera* camera) {
    if (instanceCount_ == 0 || !model_ || !camera) return;

    const Vector3 cameraPosition = camera->GetTranslate();
    const float maxDistanceSquared = maxDrawDistance_ * maxDrawDistance_;
    uint32_t visibleInstanceCount = 0;

    for (const InstanceChunk& chunk : chunks_) {
        const float chunkRadius = kChunkSize * 0.7072f;
        const float dx = chunk.center.x - cameraPosition.x;
        const float dz = chunk.center.z - cameraPosition.z;
        const float chunkDistance = maxDrawDistance_ + chunkRadius;
        if (dx * dx + dz * dz > chunkDistance * chunkDistance) {
            continue;
        }

        for (const EulerTransform& transform : chunk.transforms) {
            const float instanceDx = transform.translate.x - cameraPosition.x;
            const float instanceDz = transform.translate.z - cameraPosition.z;
            if (instanceDx * instanceDx + instanceDz * instanceDz > maxDistanceSquared) {
                continue;
            }

            Matrix4x4 worldMatrix = MakeAffineMatrix(
                transform.scale,
                transform.rotate,
                transform.translate);

            mappedData_[visibleInstanceCount].world = worldMatrix;
            mappedData_[visibleInstanceCount].WVP =
                Multiply(worldMatrix, camera->GetViewProjectionMatrix());
            mappedData_[visibleInstanceCount].WorldInverseTranspose = MakeIdentity4x4();
            ++visibleInstanceCount;
        }
    }

    if (visibleInstanceCount == 0) return;

    cameraData_->worldPosition = camera->GetTranslate();
    grassData_->time += GameTime::GetDeltaTime();

    auto commandList = Object3dCommon::GetInstance()->GetDxCommon()->GetCommandList();

    SrvManager::GetInstance()->PreDraw();

    // [2] t2: インスタンシングデータ（SRV）
    commandList->SetGraphicsRootShaderResourceView(2, instanceBuffer_->GetGPUVirtualAddress());

    commandList->IASetVertexBuffers(0, 1, &model_->vertexBufferView);
    // [0] b0: マテリアル
    commandList->SetGraphicsRootConstantBufferView(0, model_->materialResource->GetGPUVirtualAddress());
    // [1] t0: テクスチャ
    commandList->SetGraphicsRootDescriptorTable(1, TextureManager::GetInstance()->GetSRVHandleGPU(model_->modelData.material.textureFilePath));

    // [3] b2: 平行光源
    commandList->SetGraphicsRootConstantBufferView(3, lightResource_->GetGPUVirtualAddress());
    // [4] b3: カメラ情報
    commandList->SetGraphicsRootConstantBufferView(4, cameraResource_->GetGPUVirtualAddress());
    // [5] b4: 点光源
    commandList->SetGraphicsRootConstantBufferView(5, pointLightResource_->GetGPUVirtualAddress());
	// [7] b5: 草の風パラメータ
	commandList->SetGraphicsRootConstantBufferView(7, grassResource_->GetGPUVirtualAddress());

    std::string envPath = envMapTexturePath_.empty()
        ? "Resources/rostock_laage_airport_4k.dds"
        : envMapTexturePath_;
    commandList->SetGraphicsRootDescriptorTable(6, TextureManager::GetInstance()->GetSRVHandleGPU(envPath));

    uint32_t vertexCount = model_->GetVertexCount();
    commandList->DrawInstanced(vertexCount, visibleInstanceCount, 0, 0);

    // 配置を保持する。固定配置の草原などは、毎フレーム AddInstance し直さなくてよい。
}
