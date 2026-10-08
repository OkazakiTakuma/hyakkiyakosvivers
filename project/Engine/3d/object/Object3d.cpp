#include "Object3d.h"
#include "../utilities/PrimitiveMeshGenerator.h"
#include "../utilities/SkeletonAnimationUtility.h"
#include "../../2d/TextureManager.h"
#include "../../2d/LineDrawer.h"
#include "../../base/SrvManager.h"
#include "../../base/GameTime.h"
#include "../../base/Logger.h"
#include "../../math/MathConstants.h"
#include "../model/Model.h"
#include "../model/ModelManager.h"
#include "../sky/AtmosphereSystem.h"
#include "Object3dCommon.h"
#include <algorithm>
#include <cmath>

namespace {
float MoveTowards(float current, float target, float maxDelta)
{
	if (std::fabs(target - current) <= maxDelta) {
		return target;
	}
	return current + (target > current ? maxDelta : -maxDelta);
}

Vector3 GetTranslateFromMatrix(const Matrix4x4& matrix)
{
	return {matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]};
}

Matrix4x4 MakePlanarShadowMatrix(const Vector3& lightDirection, float planeY)
{
	Vector3 direction = NormalizeReturnVector(lightDirection);
	if (std::fabs(direction.y) < 0.001f) {
		direction.y = direction.y < 0.0f ? -0.001f : 0.001f;
	}

	const float xFactor = direction.x / direction.y;
	const float zFactor = direction.z / direction.y;
	Matrix4x4 shadowMatrix = MakeIdentity4x4();
	shadowMatrix.m[1][0] = -xFactor;
	shadowMatrix.m[1][1] = 0.0f;
	shadowMatrix.m[1][2] = -zFactor;
	shadowMatrix.m[3][0] = xFactor * planeY;
	shadowMatrix.m[3][1] = planeY;
	shadowMatrix.m[3][2] = zFactor * planeY;
	return shadowMatrix;
}

void DrawDebugWireSphere(const Vector3& center, float radius, const Vector4& color, bool ignoreDepth = false)
{
	constexpr uint32_t kSegmentCount = 8;

	for (uint32_t index = 0; index < kSegmentCount; ++index) {
		const float currentAngle = MathConstants::kTwoPi * static_cast<float>(index) / static_cast<float>(kSegmentCount);
		const float nextAngle = MathConstants::kTwoPi * static_cast<float>(index + 1) / static_cast<float>(kSegmentCount);

		const float currentCos = std::cos(currentAngle) * radius;
		const float currentSin = std::sin(currentAngle) * radius;
		const float nextCos = std::cos(nextAngle) * radius;
		const float nextSin = std::sin(nextAngle) * radius;

		LineDrawer::GetInstance()->DrawLine(
		    {center.x + currentCos, center.y + currentSin, center.z},
		    {center.x + nextCos, center.y + nextSin, center.z},
		    color,
		    ignoreDepth
		);
		LineDrawer::GetInstance()->DrawLine(
		    {center.x, center.y + currentCos, center.z + currentSin},
		    {center.x, center.y + nextCos, center.z + nextSin},
		    color,
		    ignoreDepth
		);
		LineDrawer::GetInstance()->DrawLine(
		    {center.x + currentCos, center.y, center.z + currentSin},
		    {center.x + nextCos, center.y, center.z + nextSin},
		    color,
		    ignoreDepth
		);
	}
}
}

/// <summary>
/// 必要なリソースを準備し、オブジェクトを初期化します。
/// </summary>
void Object3d::Initialize() {
	Object3dCommon* common = Object3dCommon::GetInstance();
	dxCommon_ = common ? common->GetDxCommon() : nullptr;
	assert(dxCommon_);
	if (!dxCommon_) {
		Logger::Log("Object3d::Initialize failed: DirectXCommon is not initialized.\n");
		return;
	}

	environmentMultiplier = 0.0f;

	CreateWVPResource();
	CreateDirectionalLightResource();
	CreateCameraResource();
	CreatePointLightResource();
	shadowWvpResource = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
	shadowWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&shadowTransformationMatrix));
	shadowTransformationMatrix->WVP = MakeIdentity4x4();
	shadowTransformationMatrix->world = MakeIdentity4x4();
	shadowTransformationMatrix->WorldInverseTranspose = MakeIdentity4x4();
	shadowTransformationMatrix->lightWVP = MakeIdentity4x4();
	shadowMapWvpResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
	shadowMapWvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&shadowMapTransformationMatrix_));
	*shadowMapTransformationMatrix_ = *shadowTransformationMatrix;
	shadowMaterialResource = dxCommon_->CreateBufferResource(sizeof(MaterialData));
	shadowMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&shadowMaterialData));
	shadowMaterialData->color = {0.0f, 0.0f, 0.0f, shadowAlpha_};
	shadowMaterialData->enableLighting = -1;
	shadowMaterialData->uvTransform = MakeIdentity4x4();
	shadowMaterialData->shininess = 1.0f;
	shadowMaterialData->emissiveColor = {0.0f, 0.0f, 0.0f};
	shadowMaterialData->emissiveIntensity = 0.0f;
	materialOverrideResource_ = dxCommon_->CreateBufferResource(sizeof(MaterialData));
	materialOverrideResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialOverrideData_));
	materialOverrideData_->color = {1.0f, 1.0f, 1.0f, 1.0f};
	materialOverrideData_->enableLighting = 1;
	materialOverrideData_->uvTransform = MakeIdentity4x4();
	materialOverrideData_->shininess = 20.0f;
	materialOverrideData_->emissiveColor = {0.0f, 0.0f, 0.0f};
	materialOverrideData_->emissiveIntensity = 0.0f;

	transform = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	this->camera = common->GetDefaultCamera();
	TextureManager::GetInstance()->LoadTexture(envMapTexturePath);
}
/// <param name="textureFilePath">使用するテクスチャまたはモデルのファイルパスを指定します。</param>
void Object3d::SetEnvironmentMap(const std::string& textureFilePath) {
	envMapTexturePath = textureFilePath;

	// =======================================================
	// =======================================================
	environmentMultiplier = 1.0f;
}
/// <summary>
/// WVPResource を作成し、利用できる状態にします。
/// </summary>
void Object3d::CreateWVPResource() {
	wvpResorceModel = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
	wvpResorceModel->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrix));
	transformationMatrix->WVP = MakeIdentity4x4();
	transformationMatrix->world = MakeIdentity4x4();
	transformationMatrix->WorldInverseTranspose = MakeIdentity4x4();
	transformationMatrix->lightWVP = MakeIdentity4x4();
}

/// <summary>
/// CameraResource を作成し、利用できる状態にします。
/// </summary>
void Object3d::CreateCameraResource() {
	cameraResource = dxCommon_->CreateBufferResource(sizeof(CameraForGPU));
	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
	cameraData->worldPosition = {0.0f, 0.0f, 0.0f};
	cameraData->environmentMultiplier = environmentMultiplier;
	cameraData->horizonColor = {0.0f, 0.0f, 0.0f};
	cameraData->distanceFogDensity = 0.0f;
	cameraData->heightFogDensity = 0.0f;
	cameraData->aerialPerspectiveStrength = 0.0f;
	cameraData->enableAerialPerspective = 0;
	cameraData->paddingAtmosphere = 0.0f;
}

/// <summary>
/// SkinningPaletteResource を作成し、利用できる状態にします。
/// </summary>
void Object3d::CreateSkinningPaletteResource(uint32_t paletteCount) {
	paletteCount = (std::max)(paletteCount, 1u);
	auto newResource = dxCommon_->CreateBufferResource(sizeof(Matrix4x4) * paletteCount);
	if (!newResource) {
		return;
	}

	Matrix4x4* newPaletteData = nullptr;
	const HRESULT hr = newResource->Map(0, nullptr, reinterpret_cast<void**>(&newPaletteData));
	if (FAILED(hr) || !newPaletteData) {
		return;
	}
	for (uint32_t index = 0; index < paletteCount; ++index) {
		newPaletteData[index] = MakeIdentity4x4();
	}

	if (skinningPaletteResource && skinningPaletteData) {
		skinningPaletteResource->Unmap(0, nullptr);
	}
	skinningPaletteResource = std::move(newResource);
	skinningPaletteData = newPaletteData;
	skinningPaletteCapacity_ = paletteCount;
}

void Object3d::UpdateSkinningPaletteResource() {
	if (!skinningPaletteData) {
		CreateSkinningPaletteResource(1);
	}
	if (!skinningPaletteData || skinningPaletteCapacity_ == 0) {
		return;
	}

	if (!model || !model->HasSkinCluster()) {
		skinningPaletteData[0] = MakeIdentity4x4();
		return;
	}

	const uint32_t requiredPaletteCount = model->GetSkinningPaletteSize();
	if (requiredPaletteCount > skinningPaletteCapacity_) {
		CreateSkinningPaletteResource(requiredPaletteCount);
	}
	if (!skinningPaletteData || requiredPaletteCount > skinningPaletteCapacity_) {
		return;
	}

	model->BuildSkinningPalette(skeleton, skinningPalette_);
	const size_t copyCount = (std::min)(skinningPalette_.size(), static_cast<size_t>(skinningPaletteCapacity_));
	if (copyCount == 0) {
		return;
	}
	std::copy_n(skinningPalette_.begin(), copyCount, skinningPaletteData);
}

/// <summary>
/// DirectionalLightResource を作成し、利用できる状態にします。
/// </summary>
void Object3d::CreateDirectionalLightResource() {
	lightResource = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
	lightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionallightData));
	directionallightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionallightData->direction = NormalizeReturnVector(Vector3(0.0f, -1.0f, 0.0f));
	directionallightData->intensity = 1.0f;
}

/// <summary>
/// PointLightResource を作成し、利用できる状態にします。
/// </summary>
void Object3d::CreatePointLightResource() {
	pointLightResource = dxCommon_->CreateBufferResource(sizeof(PointLight));
	pointLightResource->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData));
	pointLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLightData->position = { 0.0f, 0.0f, 0.0f };
	pointLightData->intensity = 1.0f;
	pointLightData->radius = 10.0f;
	pointLightData->decay = 1.0f;
}

/// <summary>
/// Cylinder を作成し、利用できる状態にします。
/// </summary>
/// <param name="radius">半径を指定します。</param>
/// <param name="height">高さを指定します。</param>
void Object3d::CreateCylinder(float radius, float height, uint32_t subdivision, bool createTopCap, bool createBottomCap) {
	PrimitiveMeshGenerator::MeshData mesh = PrimitiveMeshGenerator::GenerateCylinder(
	    radius, height, subdivision, createTopCap, createBottomCap);
	if (mesh.vertices.empty() || mesh.indices.empty()) {
		cylinderIndexCount = 0;
		return;
	}
	const std::vector<VertexData>& vertices = mesh.vertices;
	const std::vector<uint32_t>& indices = mesh.indices;

	cylinderIndexCount = (uint32_t)indices.size();

	// -------------------------------------------------------
	// -------------------------------------------------------
	DirectXCommon* dxCommon = dxCommon_;
	if (!dxCommon) {
		return;
	}

	size_t vertexBufferSize = sizeof(VertexData) * vertices.size();
	vertexResourceCylinder = dxCommon->CreateBufferResource(vertexBufferSize);

	VertexData* vertexData = nullptr;
	vertexResourceCylinder->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, vertices.data(), vertexBufferSize);
	vertexResourceCylinder->Unmap(0, nullptr);

	vertexBufferViewCylinder.BufferLocation = vertexResourceCylinder->GetGPUVirtualAddress();
	vertexBufferViewCylinder.SizeInBytes = static_cast<UINT>(vertexBufferSize);
	vertexBufferViewCylinder.StrideInBytes = sizeof(VertexData);

	size_t indexBufferSize = sizeof(uint32_t) * indices.size();
	indexResourceCylinder = dxCommon->CreateBufferResource(indexBufferSize);

	uint32_t* indexData = nullptr;
	indexResourceCylinder->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	std::memcpy(indexData, indices.data(), indexBufferSize);
	indexResourceCylinder->Unmap(0, nullptr);

	indexBufferViewCylinder.BufferLocation = indexResourceCylinder->GetGPUVirtualAddress();
	indexBufferViewCylinder.SizeInBytes = static_cast<UINT>(indexBufferSize);
	indexBufferViewCylinder.Format = DXGI_FORMAT_R32_UINT;

	if (!materialResourceCylinder) {
		materialResourceCylinder = dxCommon->CreateBufferResource(sizeof(MaterialData));
		materialResourceCylinder->Map(0, nullptr, reinterpret_cast<void**>(&materialDataCylinder));

		materialDataCylinder->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		materialDataCylinder->enableLighting = 1;
		materialDataCylinder->uvTransform = MakeIdentity4x4();
		materialDataCylinder->shininess = 50.0f;
		materialDataCylinder->emissiveColor = {0.0f, 0.0f, 0.0f};
		materialDataCylinder->emissiveIntensity = 0.0f;
	}
}

/// <param name="textureFilePath">使用するテクスチャまたはモデルのファイルパスを指定します。</param>
void Object3d::SetTexture(const std::string& textureFilePath) {
	TextureManager::GetInstance()->LoadTexture(textureFilePath);
	textureHandleCylinder = TextureManager::GetInstance()->GetSRVHandleGPU(textureFilePath);
	isTextureSetCylinder = true;
}

/// <summary>
/// 設定済みモデルの描画テクスチャを変更します。
/// </summary>
/// <param name="textureFilePath">使用するテクスチャのファイルパスを指定します。</param>
void Object3d::SetModelTexture(const std::string& textureFilePath) {
	if (model) {
		model->SetTextureFilePath(textureFilePath);
	}
}

/// <summary>
/// 共有Modelの設定は変更せず、このObject3dだけに適用する描画テクスチャを設定します。
/// </summary>
/// <param name="textureFilePath">描画時に優先して使用するテクスチャのパスを指定します。</param>
void Object3d::SetModelTextureOverride(const std::string& textureFilePath) {
	modelTextureOverridePath_ = textureFilePath;
	if (!modelTextureOverridePath_.empty()) {
		TextureManager::GetInstance()->LoadTexture(modelTextureOverridePath_);
	}
}

/// <summary>
/// 設定済みモデルの描画テクスチャパスを取得します。
/// </summary>
/// <returns>モデルのテクスチャパスを返します。</returns>
std::string Object3d::GetModelTextureFilePath() const {
	return model ? model->GetTextureFilePath() : std::string();
}

/// <summary>
/// 毎フレームの状態更新を行います。
/// </summary>
void Object3d::Update() {
	if (!transformationMatrix || !cameraData) {
		return;
	}

	// 装備品などは外部で合成済みの行列を優先し、通常オブジェクトは自身のTransformから生成する。
	Matrix4x4 worldMatrix = hasWorldMatrixOverride_
	                           ? worldMatrixOverride_
	                           : MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

	if (model) {
		if (HasAnimation()) {
			const float targetBlendWeight = isAnimationPlaying_ ? 1.0f : 0.0f;
			animationBlendWeight_ = MoveTowards(animationBlendWeight_, targetBlendWeight, animationBlendSpeed_ * GameTime::GetFrameScale60());
			if (isAnimationPlaying_) {
				animationTime += GameTime::GetDeltaTime();
				animationTime = std::fmod(animationTime, animation.duration);
			}
			SkeletonAnimationUtility::ApplyAnimation(skeleton, animation, animationTime, animationBlendWeight_);
		}
		// アニメーションの有無に関係なく、シーン演出用のボーン回転を最後に合成する。
		for (const auto& [jointName, rotationOffset] : jointRotationOffsets_) {
			const auto jointIterator = skeleton.jointMap.find(jointName);
			if (jointIterator == skeleton.jointMap.end()) {
				continue;
			}
			Joint& joint = skeleton.joints[jointIterator->second];
			joint.transform.rotate = Normalize(Multiply(rotationOffset, joint.transform.rotate));
		}

		SkeletonAnimationUtility::UpdateMatrices(skeleton);
		UpdateSkinningPaletteResource();
		const Matrix4x4 modelLocalMatrix = model->HasSkinCluster()
		    ? MakeIdentity4x4()
		    : hasSkeleton && skeleton.root >= 0 && skeleton.root < static_cast<int32_t>(skeleton.joints.size())
		          ? skeleton.joints[skeleton.root].skeletonSpaceMatrix
		          : model->GetRootNode().localMatrix;
		worldMatrix = Multiply(modelLocalMatrix, worldMatrix);
	} else {
		UpdateSkinningPaletteResource();
	}

	if (camera) {
		cameraData->worldPosition = camera->GetTranslate();
		Matrix4x4 wvpMatrix = Multiply(worldMatrix, camera->GetViewProjectionMatrix());
		transformationMatrix->WVP = wvpMatrix;
		transformationMatrix->world = worldMatrix;
		transformationMatrix->WorldInverseTranspose = Transpose(Inverse(worldMatrix));
		if (shadowTransformationMatrix && directionallightData) {
			const Matrix4x4 shadowWorldMatrix = Multiply(worldMatrix, MakePlanarShadowMatrix(directionallightData->direction, shadowPlaneY_));
			shadowTransformationMatrix->WVP = Multiply(shadowWorldMatrix, camera->GetViewProjectionMatrix());
			shadowTransformationMatrix->world = shadowWorldMatrix;
			shadowTransformationMatrix->WorldInverseTranspose = MakeIdentity4x4();
		}
	}
	else {
		transformationMatrix->WVP = worldMatrix;
		transformationMatrix->world = worldMatrix;
		transformationMatrix->WorldInverseTranspose = Transpose(Inverse(worldMatrix));
		if (shadowTransformationMatrix && directionallightData) {
			const Matrix4x4 shadowWorldMatrix = Multiply(worldMatrix, MakePlanarShadowMatrix(directionallightData->direction, shadowPlaneY_));
			shadowTransformationMatrix->WVP = shadowWorldMatrix;
			shadowTransformationMatrix->world = shadowWorldMatrix;
			shadowTransformationMatrix->WorldInverseTranspose = MakeIdentity4x4();
		}
	}


	cameraData->environmentMultiplier = environmentMultiplier;
	// 空と3Dモデルが同じ地平線色を参照することで、遠景が背景へ自然に溶けます。
	const AtmosphereSettings& atmosphere = AtmosphereSystem::GetInstance()->GetSettings();
	cameraData->horizonColor = atmosphere.horizonColor;
	cameraData->distanceFogDensity = atmosphere.distanceFogDensity;
	cameraData->heightFogDensity = atmosphere.heightFogDensity;
	cameraData->aerialPerspectiveStrength = atmosphere.aerialPerspectiveStrength;
	cameraData->enableAerialPerspective = atmosphere.enabled ? 1 : 0;
}

bool Object3d::GetJointSkeletonSpaceMatrix(const std::string& jointName, Matrix4x4& jointMatrix) const {
	// jointMapを使って名前から現在のアニメーション姿勢のボーンを検索する。
	const auto jointIterator = skeleton.jointMap.find(jointName);
	if (jointIterator == skeleton.jointMap.end()) {
		return false;
	}

	const int32_t jointIndex = jointIterator->second;
	if (jointIndex < 0 || jointIndex >= static_cast<int32_t>(skeleton.joints.size())) {
		return false;
	}

	// ワールド変換を含まないため、呼び出し側で任意の親行列と安全に合成できる。
	jointMatrix = skeleton.joints[jointIndex].skeletonSpaceMatrix;
	return true;
}

bool Object3d::GetJointWorldMatrix(const std::string& jointName, Matrix4x4& jointWorldMatrix) const {
	Matrix4x4 jointMatrix;
	if (!GetJointSkeletonSpaceMatrix(jointName, jointMatrix)) {
		return false;
	}

	// Object3d自身のTransformを最後に一度だけ合成してボーンのワールド行列を作る。
	const Matrix4x4 objectWorldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	jointWorldMatrix = Multiply(jointMatrix, objectWorldMatrix);
	return true;
}

std::vector<std::string> Object3d::GetJointNames() const {
	std::vector<std::string> names;
	names.reserve(skeleton.joints.size());
	for (const Joint& joint : skeleton.joints) {
		names.push_back(joint.name);
	}
	return names;
}

bool Object3d::HasAnimation() const {
	return model && model->GetIsAnimation() && animation.duration > 0.0f;
}

bool Object3d::SetAnimation(const std::string& animationName, bool restart) {
	// 同じModelを共有するObject3d同士でも、選択中クリップと再生時刻は個別に保持する。
	if (!model) {
		return false;
	}
	const Animation* selectedAnimation = model->FindAnimation(animationName);
	if (!selectedAnimation) {
		return false;
	}
	if (activeAnimationName_ == animationName && !restart) {
		return true;
	}
	animation = *selectedAnimation;
	activeAnimationName_ = animationName;
	if (restart) {
		// インスペクターなど明示的な切り替えでは、新クリップを初期姿勢から再生する。
		animationTime = 0.0f;
		useInitialSkinningPose_ = false;
		isAnimationPlaying_ = true;
		animationBlendWeight_ = 1.0f;
	}
	return true;
}

const std::vector<std::string>& Object3d::GetAnimationNames() const {
	static const std::vector<std::string> emptyNames;
	return model ? model->GetAnimationNames() : emptyNames;
}

void Object3d::SetAnimationPlaying(bool isPlaying) {
	if (isAnimationPlaying_ == isPlaying) {
		return;
	}

	isAnimationPlaying_ = isPlaying;
	useInitialSkinningPose_ = !isAnimationPlaying_;
	if (isAnimationPlaying_ && animationBlendWeight_ <= 0.0f) {
		animationTime = 0.0f;
	}
}

void Object3d::RestartAnimation() {
	animationTime = 0.0f;
	useInitialSkinningPose_ = false;
	isAnimationPlaying_ = true;
	animationBlendWeight_ = 1.0f;
	SkeletonAnimationUtility::ApplyAnimation(skeleton, animation, animationTime, animationBlendWeight_);
	SkeletonAnimationUtility::UpdateMatrices(skeleton);
	UpdateSkinningPaletteResource();
}

void Object3d::ResetAnimationPoseToInitial() {
	animationTime = 0.0f;
	useInitialSkinningPose_ = true;
	isAnimationPlaying_ = false;
	animationBlendWeight_ = 0.0f;
	if (model) {
		skeleton = SkeletonAnimationUtility::CreateSkeleton(model->GetRootNode());
		hasSkeleton = !skeleton.joints.empty();
	}
	SkeletonAnimationUtility::UpdateMatrices(skeleton);
	UpdateSkinningPaletteResource();
}

/// <summary>
/// 現在の状態をもとに描画処理を行います。
/// </summary>
void Object3d::Draw() {
	if (!dxCommon_ || !wvpResorceModel || !lightResource || !cameraResource || !pointLightResource) return;

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = dxCommon_->GetCommandList();
	SrvManager::GetInstance()->PreDraw();
	Object3dCommon* common = Object3dCommon::GetInstance();
	transformationMatrix->lightWVP = Multiply(
	    transformationMatrix->world, common->GetLightViewProjectionMatrix());
	// isPointLight をシェーダーへ渡し、無効なオブジェクトでは局所光の計算を省略する。
	pointLightData->enabled = isPointLightSet ? 1.0f : 0.0f;

	commandList->SetGraphicsRootConstantBufferView(1, wvpResorceModel->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(2, lightResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(4, cameraResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(5, pointLightResource->GetGPUVirtualAddress());
	if (!skinningPaletteResource) {
		CreateSkinningPaletteResource(1);
	}
	commandList->SetGraphicsRootShaderResourceView(7, skinningPaletteResource->GetGPUVirtualAddress());
	if (envMapTexturePath.empty()) {
		envMapTexturePath = "Resources/rostock_laage_airport_4k.dds";
	}
	TextureManager::GetInstance()->LoadTexture(envMapTexturePath);
	D3D12_GPU_DESCRIPTOR_HANDLE envMapHandle = TextureManager::GetInstance()->GetSRVHandleGPU(envMapTexturePath);
	commandList->SetGraphicsRootDescriptorTable(6, envMapHandle);

	if (model) {
		model->Draw(materialOverrideResource_.Get(), modelTextureOverridePath_);
		DrawDebugSkeleton();
	}
	else if (cylinderIndexCount > 0) {
		if (materialResourceCylinder) {
			commandList->SetGraphicsRootConstantBufferView(0, materialResourceCylinder->GetGPUVirtualAddress());
		}
		if (isTextureSetCylinder) {
			// =======================================================
			// =======================================================
			commandList->SetGraphicsRootDescriptorTable(3, textureHandleCylinder);
		}
		commandList->IASetVertexBuffers(0, 1, &vertexBufferViewCylinder);
		commandList->IASetIndexBuffer(&indexBufferViewCylinder);
		commandList->DrawIndexedInstanced(cylinderIndexCount, 1, 0, 0, 0);
	}

	// 従来の平面影の代わりに、通常描画済みの形状へ影だけを合成します。
	common->SetShadowReceiverDraw();
	common->BindShadowMap();
	if (model) {
		model->Draw(materialOverrideResource_.Get(), modelTextureOverridePath_);
	} else if (cylinderIndexCount > 0 && isTextureSetCylinder) {
		commandList->SetGraphicsRootConstantBufferView(0, materialResourceCylinder->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(3, textureHandleCylinder);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferViewCylinder);
		commandList->IASetIndexBuffer(&indexBufferViewCylinder);
		commandList->DrawIndexedInstanced(cylinderIndexCount, 1, 0, 0, 0);
	}
	common->SetDraw();
}

void Object3d::DrawShadowMap() {
	if (!dxCommon_ || !isShadowEnabled_ || !shadowMapWvpResource_ || !transformationMatrix) return;
	auto commandList = dxCommon_->GetCommandList();
	SrvManager::GetInstance()->PreDraw();
	Object3dCommon* common = Object3dCommon::GetInstance();
	common->SetShadowMapDraw();
	// 通常描画で確定したworldを使い、床高ではなくライト空間へ変換します。
	shadowMapTransformationMatrix_->WVP = Multiply(
	    transformationMatrix->world, common->GetLightViewProjectionMatrix());
	shadowMapTransformationMatrix_->world = transformationMatrix->world;
	shadowMapTransformationMatrix_->WorldInverseTranspose = transformationMatrix->WorldInverseTranspose;
	shadowMapTransformationMatrix_->lightWVP = shadowMapTransformationMatrix_->WVP;
	commandList->SetGraphicsRootConstantBufferView(1, shadowMapWvpResource_->GetGPUVirtualAddress());
	if (!skinningPaletteResource) CreateSkinningPaletteResource(1);
	commandList->SetGraphicsRootShaderResourceView(7, skinningPaletteResource->GetGPUVirtualAddress());
	if (model) {
		model->Draw(materialOverrideResource_.Get(), modelTextureOverridePath_);
	} else if (cylinderIndexCount > 0 && isTextureSetCylinder) {
		commandList->SetGraphicsRootConstantBufferView(0, materialResourceCylinder->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(3, textureHandleCylinder);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferViewCylinder);
		commandList->IASetIndexBuffer(&indexBufferViewCylinder);
		commandList->DrawIndexedInstanced(cylinderIndexCount, 1, 0, 0, 0);
	}
}

void Object3d::DrawDebugSkeleton() {
#ifndef USE_IMGUI
	return;
#else
	if (!isDrawSkeleton_ || !hasSkeleton || skeleton.joints.empty()) {
		return;
	}

	const Matrix4x4 objectWorldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	const Vector4 boneColor = {1.0f, 0.85f, 0.1f, 1.0f};
	const Vector4 jointColor = {0.2f, 0.8f, 1.0f, 1.0f};
	const float jointSphereRadius = 0.05f;

	for (const Joint& joint : skeleton.joints) {
		const Matrix4x4 jointWorldMatrix = Multiply(joint.skeletonSpaceMatrix, objectWorldMatrix);
		const Vector3 jointPosition = GetTranslateFromMatrix(jointWorldMatrix);

		DrawDebugWireSphere(jointPosition, jointSphereRadius, jointColor, true);

		if (!joint.parent) {
			continue;
		}

		const Joint& parent = skeleton.joints[*joint.parent];
		const Matrix4x4 parentWorldMatrix = Multiply(parent.skeletonSpaceMatrix, objectWorldMatrix);
		const Vector3 parentPosition = GetTranslateFromMatrix(parentWorldMatrix);
		LineDrawer::GetInstance()->DrawLine(parentPosition, jointPosition, boneColor, true);
	}
#endif
}

void Object3d::SetModel(Model* newModel) {
	model = newModel;
	hasSkeleton = false;
	skeleton = {};
	animationTime = 0.0f;
	activeAnimationName_.clear();
	jointRotationOffsets_.clear();
	jointRotationOffsetEuler_.clear();
	useInitialSkinningPose_ = false;
	animation = {};
	if (model && model->GetIsAnimation() && !model->GetAnimationNames().empty()) {
		// 保存された指定がないオブジェクトでも、従来どおり先頭クリップを自動再生する。
		SetAnimation(model->GetAnimationNames().front(), true);
	}
	if (model) {
		skeleton = SkeletonAnimationUtility::CreateSkeleton(model->GetRootNode());
		hasSkeleton = !skeleton.joints.empty();
		SkeletonAnimationUtility::UpdateMatrices(skeleton);
		CreateSkinningPaletteResource(model->GetSkinningPaletteSize());
		UpdateSkinningPaletteResource();
	} else {
		CreateSkinningPaletteResource(1);
	}
}

void Object3d::SetModel(const std::string& filePath) {
	SetModel(ModelManager::GetInstance()->FindModel(filePath));
}



/// <summary>
/// 破棄時に必要な解放処理を行います。
/// </summary>
Object3d::~Object3d() {
	if (wvpResorceModel) wvpResorceModel->Unmap(0, nullptr);
	if (shadowWvpResource) shadowWvpResource->Unmap(0, nullptr);
	if (shadowMapWvpResource_) shadowMapWvpResource_->Unmap(0, nullptr);
	if (shadowMaterialResource) shadowMaterialResource->Unmap(0, nullptr);
	if (materialOverrideResource_) materialOverrideResource_->Unmap(0, nullptr);
	if (lightResource) lightResource->Unmap(0, nullptr);
	if (materialResourceCylinder) materialResourceCylinder->Unmap(0, nullptr);
	if (skinningPaletteResource) skinningPaletteResource->Unmap(0, nullptr);

	wvpResorceModel.Reset();
	shadowWvpResource.Reset();
	shadowMapWvpResource_.Reset();
	shadowMaterialResource.Reset();
	materialOverrideResource_.Reset();
	lightResource.Reset();
	cameraResource.Reset();
	pointLightResource.Reset();
	vertexResourceCylinder.Reset();
	indexResourceCylinder.Reset();
	materialResourceCylinder.Reset();
	skinningPaletteResource.Reset();

	transformationMatrix = nullptr;
	shadowTransformationMatrix = nullptr;
	shadowMapTransformationMatrix_ = nullptr;
	shadowMaterialData = nullptr;
	materialOverrideData_ = nullptr;
	cameraData = nullptr;
	directionallightData = nullptr;
	pointLightData = nullptr;
	materialDataCylinder = nullptr;
	skinningPaletteData = nullptr;
	camera = nullptr;
	model = nullptr;
}

/// <param name="color">色を指定します。</param>
/// <param name="intensity">強度を指定します。</param>
void Object3d::SetDirectionalLight(const Vector4& color, const Vector3& direction, float intensity) {
	if (directionallightData) {
		directionallightData->color = color;
		directionallightData->direction = NormalizeReturnVector(direction);
		directionallightData->intensity = intensity;
		// シーン内で共有する影パスも、通常描画と同じ太陽方向へ揃えます。
		Object3dCommon::GetInstance()->SetShadowLightDirection(directionallightData->direction);
	}
}

/// <param name="color">色を指定します。</param>
/// <param name="position">位置を指定します。</param>
/// <param name="intensity">強度を指定します。</param>
/// <param name="radius">半径を指定します。</param>
void Object3d::SetPointLight(const Vector4& color, const Vector3& position, float intensity, float radius, float decay) {
	if (pointLightData) {
		pointLightData->color = color;
		pointLightData->position = position;
		pointLightData->intensity = intensity;
		pointLightData->radius = radius;
		pointLightData->decay = decay;
	}
}

void Object3d::SetEnvironmentMultiplier(float multiplier) {
	environmentMultiplier = multiplier;
}


