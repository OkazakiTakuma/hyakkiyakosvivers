#include "InstancingModel.h"
#include "../object/Object3dCommon.h"
#include "../sky/AtmosphereSystem.h"
#include "TextureManager.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace {
struct FrustumPlane {
	Vector3 normal{};
	float distance = 0.0f;
};

using FrustumPlanes = std::array<FrustumPlane, 6>;

FrustumPlane MakeNormalizedPlane(float x, float y, float z, float distance) {
	const float length = std::sqrt(x * x + y * y + z * z);
	if (length <= 0.000001f) {
		return {};
	}
	const float inverseLength = 1.0f / length;
	return {{x * inverseLength, y * inverseLength, z * inverseLength}, distance * inverseLength};
}

FrustumPlanes ExtractFrustumPlanes(const Matrix4x4& viewProjection) {
	// このプロジェクトは行ベクトルで座標変換するため、クリップ空間の各条件を列から取り出す。
	return {
		MakeNormalizedPlane(
			viewProjection.m[0][3] + viewProjection.m[0][0],
			viewProjection.m[1][3] + viewProjection.m[1][0],
			viewProjection.m[2][3] + viewProjection.m[2][0],
			viewProjection.m[3][3] + viewProjection.m[3][0]),
		MakeNormalizedPlane(
			viewProjection.m[0][3] - viewProjection.m[0][0],
			viewProjection.m[1][3] - viewProjection.m[1][0],
			viewProjection.m[2][3] - viewProjection.m[2][0],
			viewProjection.m[3][3] - viewProjection.m[3][0]),
		MakeNormalizedPlane(
			viewProjection.m[0][3] + viewProjection.m[0][1],
			viewProjection.m[1][3] + viewProjection.m[1][1],
			viewProjection.m[2][3] + viewProjection.m[2][1],
			viewProjection.m[3][3] + viewProjection.m[3][1]),
		MakeNormalizedPlane(
			viewProjection.m[0][3] - viewProjection.m[0][1],
			viewProjection.m[1][3] - viewProjection.m[1][1],
			viewProjection.m[2][3] - viewProjection.m[2][1],
			viewProjection.m[3][3] - viewProjection.m[3][1]),
		MakeNormalizedPlane(
			viewProjection.m[0][2], viewProjection.m[1][2],
			viewProjection.m[2][2], viewProjection.m[3][2]),
		MakeNormalizedPlane(
			viewProjection.m[0][3] - viewProjection.m[0][2],
			viewProjection.m[1][3] - viewProjection.m[1][2],
			viewProjection.m[2][3] - viewProjection.m[2][2],
			viewProjection.m[3][3] - viewProjection.m[3][2])};
}

bool IsBoundsInsideFrustum(
	const Vector3& minBounds, const Vector3& maxBounds,
	const Vector3& offset, const FrustumPlanes& planes) {
	const Vector3 center = {
		(minBounds.x + maxBounds.x) * 0.5f + offset.x,
		(minBounds.y + maxBounds.y) * 0.5f + offset.y,
		(minBounds.z + maxBounds.z) * 0.5f + offset.z};
	const Vector3 extents = {
		(maxBounds.x - minBounds.x) * 0.5f,
		(maxBounds.y - minBounds.y) * 0.5f,
		(maxBounds.z - minBounds.z) * 0.5f};

	for (const FrustumPlane& plane : planes) {
		const float signedDistance =
			plane.normal.x * center.x + plane.normal.y * center.y +
			plane.normal.z * center.z + plane.distance;
		const float projectedRadius =
			std::abs(plane.normal.x) * extents.x +
			std::abs(plane.normal.y) * extents.y +
			std::abs(plane.normal.z) * extents.z;
		if (signedDistance + projectedRadius < 0.0f) {
			return false;
		}
	}
	return true;
}
}

/// <summary>
/// 必要なリソースを準備し、オブジェクトを初期化します。
/// </summary>
void InstancingModel::Initialize(Model* model, uint32_t maxInstanceCount) {
    model_ = model;
    maxInstanceCount_ = maxInstanceCount;
	modelBoundingRadius_ = 0.0f;
	if (model_) {
		for (const VertexData& vertex : model_->GetModelData().vertices) {
			const float radiusSquared =
				vertex.position.x * vertex.position.x +
				vertex.position.y * vertex.position.y +
				vertex.position.z * vertex.position.z;
			modelBoundingRadius_ = (std::max)(modelBoundingRadius_, std::sqrt(radiusSquared));
		}
	}

    chunks_.reserve(128);
    chunkLookup_.reserve(128);

    CreateConstantBuffers();
}

/// <summary>
/// 破棄時に必要な解放処理を行います。
/// </summary>
InstancingModel::~InstancingModel() {
    if (lightResource_) lightResource_->Unmap(0, nullptr);
    if (cameraResource_) cameraResource_->Unmap(0, nullptr);
    if (pointLightResource_) pointLightResource_->Unmap(0, nullptr);
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
    cameraData_->viewProjection = MakeIdentity4x4();
    cameraData_->worldPosition = { 0.0f, 0.0f, -10.0f };
    cameraData_->environmentMultiplier = 0.0f;
	cameraData_->horizonColor = {0.0f, 0.0f, 0.0f};
	cameraData_->distanceFogDensity = 0.0f;
	cameraData_->heightFogDensity = 0.0f;
	cameraData_->aerialPerspectiveStrength = 0.0f;
	cameraData_->enableAerialPerspective = 0;
	cameraData_->paddingAtmosphere = 0.0f;

    pointLightResource_ = dxCommon->CreateBufferResource(sizeof(PointLight));
    pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));
    pointLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    pointLightData_->position = { 0.0f, 0.0f, 0.0f };
    pointLightData_->intensity = 0.0f;
    pointLightData_->radius = 10.0f;
    pointLightData_->decay = 1.0f;

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

	InstanceChunk& chunk = chunks_[chunkIt->second];
	const float maximumScale = (std::max)({
		std::abs(transform.scale.x),
		std::abs(transform.scale.y),
		std::abs(transform.scale.z)});
	const float instanceRadius = modelBoundingRadius_ * maximumScale;
	const Vector3 instanceMin = {
		transform.translate.x - instanceRadius,
		transform.translate.y - instanceRadius,
		transform.translate.z - instanceRadius};
	const Vector3 instanceMax = {
		transform.translate.x + instanceRadius,
		transform.translate.y + instanceRadius,
		transform.translate.z + instanceRadius};
	if (chunk.transforms.empty()) {
		chunk.minBounds = instanceMin;
		chunk.maxBounds = instanceMax;
	} else {
		chunk.minBounds = {
			(std::min)(chunk.minBounds.x, instanceMin.x),
			(std::min)(chunk.minBounds.y, instanceMin.y),
			(std::min)(chunk.minBounds.z, instanceMin.z)};
		chunk.maxBounds = {
			(std::max)(chunk.maxBounds.x, instanceMax.x),
			(std::max)(chunk.maxBounds.y, instanceMax.y),
			(std::max)(chunk.maxBounds.z, instanceMax.z)};
	}
	chunk.transforms.push_back(transform);
    ++instanceCount_;
	visibilityCacheValid_ = false;
}

/// <summary>
/// 現在の状態をもとに描画処理を行います。
/// </summary>
/// <param name="camera">描画や座標変換に使用するカメラを指定します。</param>
void InstancingModel::Draw(Camera* camera) {
    if (instanceCount_ == 0 || !model_ || !camera) return;

    const Vector3 cameraPosition = camera->GetTranslate();
	const Matrix4x4 viewProjection = camera->GetViewProjectionMatrix();
	// 距離判定に加えて視錐台も使うため、位置またはカメラ行列が変わった場合だけ可視一覧を更新する。
	const bool visibilityUnchanged = visibilityCacheValid_ &&
		cameraPosition.x == cachedCameraPosition_.x &&
		cameraPosition.y == cachedCameraPosition_.y &&
		cameraPosition.z == cachedCameraPosition_.z &&
		std::memcmp(&viewProjection, &cachedViewProjection_, sizeof(Matrix4x4)) == 0;

	uint32_t visibleInstanceCount = cachedVisibleInstanceCount_;
	if (!visibilityUnchanged) {
		const float maxDistanceSquared = maxDrawDistance_ * maxDrawDistance_;
		const FrustumPlanes frustumPlanes = ExtractFrustumPlanes(viewProjection);
		visibleInstanceCount = 0;
		for (const InstanceChunk& chunk : chunks_) {
			// 草原の元データは保持したまま、各軸でカメラに最も近い周期分のコピーを描画する。
			// 折り返し境界は最大描画距離より外にあるため、画面内で草が飛ぶことはない。
			Vector3 loopOffset{};
			if (loopPeriodX_ > 0.0f) {
				loopOffset.x = std::round((cameraPosition.x - chunk.center.x) / loopPeriodX_) * loopPeriodX_;
			}
			if (loopPeriodZ_ > 0.0f) {
				loopOffset.z = std::round((cameraPosition.z - chunk.center.z) / loopPeriodZ_) * loopPeriodZ_;
			}
			const float chunkRadius = kChunkSize * 0.7072f;
			const float dx = chunk.center.x + loopOffset.x - cameraPosition.x;
			const float dz = chunk.center.z + loopOffset.z - cameraPosition.z;
			const float chunkDistance = maxDrawDistance_ + chunkRadius;
			if (dx * dx + dz * dz > chunkDistance * chunkDistance) {
				continue;
			}
			if (!IsBoundsInsideFrustum(
					chunk.minBounds, chunk.maxBounds, loopOffset, frustumPlanes)) {
				continue;
			}

			for (const EulerTransform& transform : chunk.transforms) {
				const Vector3 drawPosition = transform.translate + loopOffset;
				const float instanceDx = drawPosition.x - cameraPosition.x;
				const float instanceDz = drawPosition.z - cameraPosition.z;
				if (instanceDx * instanceDx + instanceDz * instanceDz > maxDistanceSquared) {
					continue;
				}

				WriteVisibleInstance(visibleInstanceCount, transform, drawPosition);
				++visibleInstanceCount;
			}
		}
		cachedCameraPosition_ = cameraPosition;
		cachedViewProjection_ = viewProjection;
		cachedVisibleInstanceCount_ = visibleInstanceCount;
		visibilityCacheValid_ = true;
	}

    if (visibleInstanceCount == 0) return;

    cameraData_->viewProjection = viewProjection;
    cameraData_->worldPosition = cameraPosition;
	const AtmosphereSettings& atmosphere = AtmosphereSystem::GetInstance()->GetSettings();
	cameraData_->horizonColor = atmosphere.horizonColor;
	cameraData_->distanceFogDensity = atmosphere.distanceFogDensity;
	cameraData_->heightFogDensity = atmosphere.heightFogDensity;
	cameraData_->aerialPerspectiveStrength = atmosphere.aerialPerspectiveStrength;
	cameraData_->enableAerialPerspective = atmosphere.enabled ? 1 : 0;

    auto commandList = Object3dCommon::GetInstance()->GetDxCommon()->GetCommandList();
	PrepareDraw(commandList.Get());

    SrvManager::GetInstance()->PreDraw();

    commandList->IASetVertexBuffers(0, 1, &model_->vertexBufferView);
    commandList->IASetIndexBuffer(&model_->indexBufferView);
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

    std::string envPath = envMapTexturePath_.empty()
        ? "Resources/rostock_laage_airport_4k.dds"
        : envMapTexturePath_;
    commandList->SetGraphicsRootDescriptorTable(6, TextureManager::GetInstance()->GetSRVHandleGPU(envPath));

    const uint32_t indexCount = static_cast<uint32_t>(model_->GetModelData().indices.size());
    commandList->DrawIndexedInstanced(indexCount, visibleInstanceCount, 0, 0, 0);

    // 配置を保持する。固定配置の草原などは、毎フレーム AddInstance し直さなくてよい。
}
