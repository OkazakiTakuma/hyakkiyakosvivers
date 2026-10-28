#include "base_scene/BaseScene.h"
#include "StringUtility.h"
#include "helpers/BaseSceneEditorGeometry.h"
#include "helpers/BaseSceneResourceCatalog.h"
#include "repositories/EnemyStatusRepository.h"
#include "repositories/ParticlePresetRepository.h"
#include "repositories/PlayerStatusRepository.h"

#include <array>

#ifdef USE_IMGUI
#include "../../../../../imgui/ImGuizmo.h"
#include <cctype>
#include <limits>
#endif
#include "base_scene/details/BaseSceneEditorDetails.h"

void BaseScene::DrawParticleEmitterInspector(GameObject* selectedObject) {
#ifdef USE_IMGUI
	if (!selectedObject) {
		return;
	}

	ParticleEmitterComponent* emitter = selectedObject->GetComponent<ParticleEmitterComponent>();
	if (!emitter) {
		return;
	}

	ImGui::Separator();
	ImGui::Text("ParticleEmitterComponent");

	const std::vector<std::string> particlePresets = ParticlePresetRepository::LoadNames();
	if (!particlePresets.empty()) {
		if (selectedParticlePresetIndex_ >= static_cast<int>(particlePresets.size())) {
			selectedParticlePresetIndex_ = 0;
		}

		std::vector<const char*> presetLabels;
		presetLabels.reserve(particlePresets.size());
		for (const std::string& presetName : particlePresets) {
			presetLabels.push_back(presetName.c_str());
		}

		ImGui::Combo("Preset", &selectedParticlePresetIndex_, presetLabels.data(), static_cast<int>(presetLabels.size()));
		if (ImGui::Button("Apply Preset")) {
			ParticlePresetRepository::Apply(particlePresets[selectedParticlePresetIndex_], emitter);
		}
	} else {
		ImGui::Text("No particle presets");
	}

	if (particlePresetNameBuffer_[0] == '\0') {
		const std::string currentGroupName = emitter->GetGroupName();
		const size_t copyLength = currentGroupName.size() < particlePresetNameBuffer_.size() - 1 ? currentGroupName.size() : particlePresetNameBuffer_.size() - 1;
		std::memcpy(particlePresetNameBuffer_.data(), currentGroupName.data(), copyLength);
		particlePresetNameBuffer_[copyLength] = '\0';
	}
	ImGui::InputText("Preset Name", particlePresetNameBuffer_.data(), particlePresetNameBuffer_.size());
	if (ImGui::Button("Save Preset")) {
		ParticlePresetRepository::Save(particlePresetNameBuffer_.data(), emitter);
	}

	const std::vector<std::string> textures = CollectResourceTexturePaths();
	if (!textures.empty()) {
		int textureIndex = 0;
		for (int index = 0; index < static_cast<int>(textures.size()); index++) {
			if (textures[index] == emitter->GetTextureFilePath()) {
				textureIndex = index;
				break;
			}
		}
		std::vector<const char*> textureLabels = MakeLabelPointers(textures);
		if (ImGui::Combo("Particle Texture", &textureIndex, textureLabels.data(), static_cast<int>(textureLabels.size()))) {
			emitter->SetTexture(textures[textureIndex]);
		}
	}

	bool isActive = emitter->GetIsActive();
	if (ImGui::Checkbox("Emitter Active", &isActive)) {
		emitter->SetIsActive(isActive);
	}

	float frequency = emitter->GetFrequency();
	if (ImGui::DragFloat("Emit Frequency", &frequency, 0.01f, 0.0f, 10.0f)) {
		emitter->SetFrequency(frequency);
	}

	ParticleEmitParam param = emitter->GetParam();
	int count = static_cast<int>(param.count);
	if (ImGui::DragInt("Emit Count", &count, 1.0f, 0, static_cast<int>(ParticleManager::kMaxParticle))) {
		param.count = static_cast<uint32_t>(count);
		emitter->SetParam(param);
	}
	if (ImGui::DragFloat("Life Time", &param.lifeTime, 0.01f, 0.01f, 30.0f)) {
		emitter->SetParam(param);
	}
	if (ImGui::DragFloat3("Particle Scale", &param.scale.x, 0.05f, 0.0f, 100.0f)) {
		emitter->SetParam(param);
	}
	if (ImGui::DragFloat3("End Scale", &param.endScale.x, 0.05f, 0.0f, 100.0f)) {
		emitter->SetParam(param);
	}
	if (ImGui::DragFloat3("Random Position", &param.randomPositionRange.x, 0.05f, 0.0f, 100.0f)) {
		emitter->SetParam(param);
	}
	if (ImGui::DragFloat3("Base Velocity", &param.baseVelocity.x, 0.05f, -100.0f, 100.0f)) {
		emitter->SetParam(param);
	}
	if (ImGui::DragFloat3("Random Velocity", &param.randomVelocityRange.x, 0.05f, 0.0f, 100.0f)) {
		emitter->SetParam(param);
	}
	if (ImGui::DragFloat3("Acceleration", &param.acceleration.x, 0.05f, -100.0f, 100.0f)) {
		emitter->SetParam(param);
	}
	if (ImGui::DragFloat3("Base Rotate", &param.baseRotate.x, 0.05f, -10.0f, 10.0f)) {
		emitter->SetParam(param);
	}
	if (ImGui::Checkbox("Random Rotate", &param.isRandomRotate)) {
		emitter->SetParam(param);
	}
	if (ImGui::DragFloat3("Random Rotate Range", &param.randomRotateRange.x, 0.05f, 0.0f, 10.0f)) {
		emitter->SetParam(param);
	}
	if (ImGui::DragFloat3("Random Scale", &param.randomScaleRange.x, 0.05f, 0.0f, 100.0f)) {
		emitter->SetParam(param);
	}
	if (ImGui::Checkbox("Billboard", &param.isBillboard)) {
		emitter->SetParam(param);
	}
	if (ImGui::Checkbox("Vortex Motion", &param.isVortex)) {
		emitter->SetParam(param);
	}
	// 渦運動を使わないエミッターでは専用項目を隠し、通常パーティクルの編集を簡潔に保つ。
	if (param.isVortex) {
		if (ImGui::DragFloat("Vortex Angular Speed", &param.vortexAngularSpeed, 0.1f, -30.0f, 30.0f)) {
			emitter->SetParam(param);
		}
		if (ImGui::DragFloat("Vortex Base Radius", &param.vortexBaseRadius, 0.05f, 0.0f, 100.0f)) {
			emitter->SetParam(param);
		}
		if (ImGui::DragFloat("Vortex Top Radius", &param.vortexTopRadius, 0.05f, 0.0f, 100.0f)) {
			emitter->SetParam(param);
		}
		if (ImGui::DragFloat("Vortex Height", &param.vortexHeight, 0.05f, 0.01f, 100.0f)) {
			emitter->SetParam(param);
		}
	}
	if (ImGui::ColorEdit4("Start Color", &param.color.x)) {
		emitter->SetParam(param);
	}
	if (ImGui::ColorEdit4("End Color", &param.endColor.x)) {
		emitter->SetParam(param);
	}
	emitter->SetParam(param);

	int meshType = static_cast<int>(emitter->GetMeshType());
	const char* meshTypeLabels[] = {"Quad", "Ring", "Cylinder"};
	if (ImGui::Combo("Mesh Type", &meshType, meshTypeLabels, _countof(meshTypeLabels))) {
		emitter->SetMeshType(static_cast<ParticleMeshType>(meshType));
	}

	int blendMode = static_cast<int>(emitter->GetBlendMode());
	const char* blendModeLabels[] = {"None", "Normal", "Add", "Subtract", "Multiply", "Screen"};
	if (ImGui::Combo("Blend Mode", &blendMode, blendModeLabels, _countof(blendModeLabels))) {
		emitter->SetBlendMode(static_cast<BlendMode>(blendMode));
	}

	if (ImGui::Button("Test Emit")) {
		emitter->Emit();
	}
#else
	(void)selectedObject;
#endif
}

/// <summary>
/// PointLightComponentの編集UIを描画します。
/// </summary>
void BaseScene::DrawPointLightInspector(GameObject* selectedObject) {
#ifdef USE_IMGUI
	if (!selectedObject) {
		return;
	}

	PointLightComponent* pointLight = selectedObject->GetComponent<PointLightComponent>();
	ImGui::Separator();
	if (!pointLight) {
		if (ImGui::Button("Create Point Light Component")) {
			selectedObject->AddComponent<PointLightComponent>();
		}
		return;
	}

	ImGui::Text("PointLightComponent");
	if (ImGui::Button("Destroy Point Light Component")) {
		selectedObject->RemoveComponent<PointLightComponent>();
		return;
	}

	bool useMaterialColor = pointLight->GetUseMaterialEmissionColor();
	if (ImGui::Checkbox("Use Material Emission Color", &useMaterialColor)) {
		pointLight->SetUseMaterialEmissionColor(useMaterialColor);
	}
	Vector4 color = pointLight->GetColor();
	if (!useMaterialColor && ImGui::ColorEdit4("Light Color", &color.x)) {
		pointLight->SetColor(color);
	}
	float intensity = pointLight->GetIntensity();
	if (ImGui::DragFloat("Light Intensity", &intensity, 0.05f, 0.0f, 100.0f)) {
		pointLight->SetIntensity(intensity);
	}
	float radius = pointLight->GetRadius();
	if (ImGui::DragFloat("Light Radius", &radius, 0.1f, 0.01f, 1000.0f)) {
		pointLight->SetRadius(radius);
	}
	float decay = pointLight->GetDecay();
	if (ImGui::DragFloat("Light Decay", &decay, 0.01f, 0.0f, 10.0f)) {
		pointLight->SetDecay(decay);
	}
	Vector3 positionOffset = pointLight->GetPositionOffset();
	if (ImGui::DragFloat3("Light Position Offset", &positionOffset.x, 0.05f)) {
		pointLight->SetPositionOffset(positionOffset);
	}
#else
	(void)selectedObject;
#endif
}

/// <summary>
/// 夕日や魔法球など、発光物の周囲へ重ねる2層Haloを編集します。
/// </summary>
void BaseScene::DrawGlowBillboardInspector(GameObject* selectedObject) {
#ifdef USE_IMGUI
	if (!selectedObject) {
		return;
	}

	GlowBillboardComponent* glow = selectedObject->GetComponent<GlowBillboardComponent>();
	ImGui::Separator();
	if (!glow) {
		if (ImGui::Button("Create Glow Billboard Component")) {
			selectedObject->AddComponent<GlowBillboardComponent>();
		}
		return;
	}

	ImGui::Text("GlowBillboardComponent");
	if (ImGui::Button("Destroy Glow Billboard Component")) {
		selectedObject->RemoveComponent<GlowBillboardComponent>();
		return;
	}

	// 内側は輪郭の明るいにじみ、外側は大きく薄い光輪として個別調整する。
	Vector4 innerColor = glow->GetInnerColor();
	if (ImGui::ColorEdit4("Halo Inner Color", &innerColor.x)) {
		glow->SetInnerColor(innerColor);
	}
	Vector4 outerColor = glow->GetOuterColor();
	if (ImGui::ColorEdit4("Halo Outer Color", &outerColor.x)) {
		glow->SetOuterColor(outerColor);
	}

	float innerSize = glow->GetInnerSize();
	if (ImGui::DragFloat("Halo Inner Size", &innerSize, 0.05f, 0.01f, 1000.0f)) {
		glow->SetInnerSize(innerSize);
	}
	float outerSize = glow->GetOuterSize();
	if (ImGui::DragFloat("Halo Outer Size", &outerSize, 0.05f, 0.01f, 1000.0f)) {
		glow->SetOuterSize(outerSize);
	}

	float innerIntensity = glow->GetInnerIntensity();
	if (ImGui::DragFloat("Halo Inner Intensity", &innerIntensity, 0.005f, 0.0f, 4.0f)) {
		glow->SetInnerIntensity(innerIntensity);
	}
	float outerIntensity = glow->GetOuterIntensity();
	if (ImGui::DragFloat("Halo Outer Intensity", &outerIntensity, 0.005f, 0.0f, 4.0f)) {
		glow->SetOuterIntensity(outerIntensity);
	}

	// 脈動量を小さく保つと、魔法球ではなく静かな夕日の揺らぎとして見える。
	float pulseAmount = glow->GetPulseAmount();
	if (ImGui::DragFloat("Halo Pulse Amount", &pulseAmount, 0.001f, 0.0f, 0.5f)) {
		glow->SetPulseAmount(pulseAmount);
	}
	float pulseSpeed = glow->GetPulseSpeed();
	if (ImGui::DragFloat("Halo Pulse Speed", &pulseSpeed, 0.01f, 0.0f, 20.0f)) {
		glow->SetPulseSpeed(pulseSpeed);
	}

	Vector3 positionOffset = glow->GetPositionOffset();
	if (ImGui::DragFloat3("Halo Position Offset", &positionOffset.x, 0.01f)) {
		glow->SetPositionOffset(positionOffset);
	}
#else
	(void)selectedObject;
#endif
}

/// <summary>
/// コライダーコンポーネントの編集UIを描画します。
/// </summary>
void BaseScene::DrawOBBColliderInspector(GameObject* selectedObject) {
#ifdef USE_IMGUI
	if (!selectedObject) {
		return;
	}

	OBBColliderComponent* collider = selectedObject->GetComponent<OBBColliderComponent>();
	SphereColliderComponent* sphereCollider = selectedObject->GetComponent<SphereColliderComponent>();
	ImGui::Separator();
	if (!collider) {
		if (ImGui::Button("Create OBB Collider")) {
			selectedObject->AddComponent<OBBColliderComponent>();
		}
	} else {
		ImGui::Text("OBBColliderComponent");
		if (ImGui::Button("Destroy OBB Collider")) {
			selectedObject->RemoveComponent<OBBColliderComponent>();
			collider = nullptr;
		}
	}

	if (!sphereCollider) {
		if (ImGui::Button("Create Sphere Collider")) {
			selectedObject->AddComponent<SphereColliderComponent>();
		}
	} else {
		ImGui::Text("SphereColliderComponent");
		if (ImGui::Button("Destroy Sphere Collider")) {
			selectedObject->RemoveComponent<SphereColliderComponent>();
			sphereCollider = nullptr;
		}
	}

	if (!collider && !sphereCollider) {
		return;
	}

	if (collider) {
		ImGui::Separator();
		ImGui::Text("OBB Settings");
		Vector3 centerOffset = collider->GetCenterOffset();
		if (ImGui::DragFloat3("OBB Center", &centerOffset.x, 0.05f)) {
			collider->SetCenterOffset(centerOffset);
		}

		Vector3 halfSize = collider->GetHalfSize();
		if (ImGui::DragFloat3("OBB Half Size", &halfSize.x, 0.05f, 0.0f, 1000.0f)) {
			collider->SetHalfSize(halfSize);
		}
		Vector3 fullSize = {halfSize.x * 2.0f, halfSize.y * 2.0f, halfSize.z * 2.0f};
		if (ImGui::DragFloat3("OBB Size", &fullSize.x, 0.1f, 0.0f, 2000.0f)) {
			collider->SetHalfSize({fullSize.x * 0.5f, fullSize.y * 0.5f, fullSize.z * 0.5f});
		}

		bool isDrawDebug = collider->GetDrawDebug();
		if (ImGui::Checkbox("Draw OBB Collider", &isDrawDebug)) {
			collider->SetDrawDebug(isDrawDebug);
		}
		// 有効にすると、衝突時にこの OBB のオーナーを重なり分だけ押し戻します。
		bool isPushBackEnabled = collider->GetPushBackEnabled();
		if (ImGui::Checkbox("OBB Push Back", &isPushBackEnabled)) {
			collider->SetPushBackEnabled(isPushBackEnabled);
		}
		ImGui::Text("OBB Collision: %s", collider->IsColliding() ? "Hit" : "None");
	}

	if (sphereCollider) {
		ImGui::Separator();
		ImGui::Text("Sphere Settings");
		Vector3 centerOffset = sphereCollider->GetCenterOffset();
		if (ImGui::DragFloat3("Sphere Center", &centerOffset.x, 0.05f)) {
			sphereCollider->SetCenterOffset(centerOffset);
		}

		float radius = sphereCollider->GetRadius();
		if (ImGui::DragFloat("Sphere Radius", &radius, 0.05f, 0.0f, 1000.0f)) {
			sphereCollider->SetRadius(radius);
		}

		bool isDrawDebug = sphereCollider->GetDrawDebug();
		if (ImGui::Checkbox("Draw Sphere Collider", &isDrawDebug)) {
			sphereCollider->SetDrawDebug(isDrawDebug);
		}
		// 有効にすると、衝突時にこの Sphere のオーナーを重なり分だけ押し戻します。
		bool isPushBackEnabled = sphereCollider->GetPushBackEnabled();
		if (ImGui::Checkbox("Sphere Push Back", &isPushBackEnabled)) {
			sphereCollider->SetPushBackEnabled(isPushBackEnabled);
		}
		ImGui::Text("Sphere Collision: %s", sphereCollider->IsColliding() ? "Hit" : "None");
	}
#else
	(void)selectedObject;
#endif
}

/// <summary>
/// カメラコンポーネントの編集UIを描画します。
/// </summary>
void BaseScene::DrawCameraInspector(GameObject* selectedObject) {
#ifdef USE_IMGUI
	CameraComponent* cameraComponent = selectedObject->GetComponent<CameraComponent>();
	if (!cameraComponent) {
		return;
	}

	ImGui::Separator();
	ImGui::Text("CameraComponent");

	const bool isActive = selectedObject->GetName() == activeCameraObjectName_;
	if (isActive) {
		ImGui::Text("Active Camera");
	} else if (ImGui::Button("Set Active Camera")) {
		SetActiveCameraObject(selectedObject);
	}

	float fovY = cameraComponent->GetFovY();
	if (ImGui::SliderAngle("FOV", &fovY, 1.0f, 120.0f)) {
		cameraComponent->SetFovY(fovY);
	}
	float nearClip = cameraComponent->GetNearClip();
	if (ImGui::DragFloat("Near Clip", &nearClip, 0.01f, 0.001f, 100.0f)) {
		cameraComponent->SetNearClip(nearClip);
	}
	float farClip = cameraComponent->GetFarClip();
	if (ImGui::DragFloat("Far Clip", &farClip, 1.0f, 1.0f, 10000.0f)) {
		cameraComponent->SetFarClip(farClip);
	}
	Vector3 localOffset = cameraComponent->GetLocalOffset();
	if (ImGui::DragFloat3("Camera Local Offset", &localOffset.x, 0.1f)) {
		cameraComponent->SetLocalOffset(localOffset);
	}
	Vector3 followOffset = cameraComponent->GetFollowOffset();
	if (ImGui::DragFloat3("Follow Offset", &followOffset.x, 0.1f)) {
		cameraComponent->SetFollowOffset(followOffset);
	}
	bool overrideRotationEnabled = cameraComponent->GetOverrideRotationEnabled();
	if (ImGui::Checkbox("Use Fixed Camera Rotation", &overrideRotationEnabled)) {
		cameraComponent->SetOverrideRotationEnabled(overrideRotationEnabled);
	}
	Vector3 overrideRotation = cameraComponent->GetOverrideRotation();
	bool overrideRotationChanged = false;
	overrideRotationChanged |= ImGui::SliderAngle("Fixed Rotate X", &overrideRotation.x, -180.0f, 180.0f);
	overrideRotationChanged |= ImGui::SliderAngle("Fixed Rotate Y", &overrideRotation.y, -180.0f, 180.0f);
	overrideRotationChanged |= ImGui::SliderAngle("Fixed Rotate Z", &overrideRotation.z, -180.0f, 180.0f);
	if (overrideRotationChanged) {
		cameraComponent->SetOverrideRotation(overrideRotation);
	}
	if (ImGui::Button("Set Top Down View")) {
		cameraComponent->SetLocalOffset({0.0f, 15.0f, 0.0f});
		cameraComponent->SetFollowOffset({0.0f, 15.0f, 0.0f});
		cameraComponent->SetOverrideRotationEnabled(true);
		cameraComponent->SetOverrideRotation({MathConstants::kPi * 0.5f, 0.0f, 0.0f});
		cameraComponent->SetFovY(0.75f);
		cameraComponent->SetFarClip(1000.0f);
	}

	std::vector<std::string> linkLabels;
	linkLabels.push_back("None");
	int currentLinkIndex = 0;
	for (const auto& object : sceneObjects_) {
		if (object.get() == selectedObject) {
			continue;
		}
		linkLabels.push_back(object->GetName());
		if (cameraComponent->GetFollowTarget() == object.get() || cameraComponent->GetFollowTargetName() == object->GetName()) {
			currentLinkIndex = static_cast<int>(linkLabels.size()) - 1;
		}
	}

	std::vector<const char*> linkLabelPtrs;
	linkLabelPtrs.reserve(linkLabels.size());
	for (const std::string& label : linkLabels) {
		linkLabelPtrs.push_back(label.c_str());
	}

	if (ImGui::Combo("Follow Object", &currentLinkIndex, linkLabelPtrs.data(), static_cast<int>(linkLabelPtrs.size()))) {
		if (currentLinkIndex == 0) {
			cameraComponent->SetFollowTarget(nullptr);
		} else {
			cameraComponent->SetFollowTarget(FindObjectByName(linkLabels[currentLinkIndex]));
		}
	}
#endif
}

/// <summary>
/// 選択中オブジェクトを操作するギズモを描画します。
/// </summary>
void BaseScene::DrawEditorGizmo() {
#ifdef USE_IMGUI
	if (!isGizmoEnabled_ || selectedObjectIndex_ < 0 || selectedObjectIndex_ >= static_cast<int>(sceneObjects_.size())) {
		return;
	}

	Camera* camera = Object3dCommon::GetInstance()->GetDefaultCamera();
	if (!camera) {
		return;
	}

	GameObject* selectedObject = sceneObjects_[selectedObjectIndex_].get();
	EulerTransform& transform = selectedObject->GetTransform();

	// ギズモの描画・入力範囲を、ドッキングされた GameView の表示領域に合わせます。
	// GameView がまだ初期化されていない場合は、メインビューポート全体を代替領域として使用します。
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImVec2 viewportPos = ImGuiManager::GetInstance()->GetGameViewContentPosition();
	ImVec2 viewportSize = ImGuiManager::GetInstance()->GetGameViewContentSize();
	if (viewportSize.x <= 1.0f || viewportSize.y <= 1.0f) {
		viewportPos = viewport ? viewport->Pos : ImVec2(0.0f, 0.0f);
		viewportSize = viewport ? viewport->Size : ImGui::GetIO().DisplaySize;
	}

	// ImGuizmo は行列を受け取るため、オブジェクトの各変換要素を操作用行列へ合成します。
	// 回転角はエンジン側ではラジアン、ImGuizmo 側では度数法として扱います。
	Matrix4x4 objectMatrix{};
	float translation[3] = {transform.translate.x, transform.translate.y, transform.translate.z};
	float rotation[3] = {
	    BaseSceneEditorGeometry::ToDegrees(transform.rotate.x),
	    BaseSceneEditorGeometry::ToDegrees(transform.rotate.y),
	    BaseSceneEditorGeometry::ToDegrees(transform.rotate.z)
	};
	float scale[3] = {transform.scale.x, transform.scale.y, transform.scale.z};
	ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, &objectMatrix.m[0][0]);

	ImGuizmo::BeginFrame();
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList(viewport));
	ImGuizmo::SetAlternativeWindow(ImGuiManager::GetInstance()->GetGameViewWindow());
	ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

	// インスペクターで選択された操作を ImGuizmo の操作種別へ対応付けます。
	// スケールはワールド軸で操作すると行列が歪むため、ローカル座標系に固定します。
	const Matrix4x4& viewMatrix = camera->GetViewMatrix();
	const Matrix4x4& projectionMatrix = camera->GetProjectionMatrix();
	const ImGuizmo::OPERATION gizmoOperations[] = {
	    ImGuizmo::TRANSLATE,
	    ImGuizmo::ROTATE,
	    ImGuizmo::SCALE
	};
	const int operationIndex = gizmoOperationIndex_ >= 0 && gizmoOperationIndex_ < _countof(gizmoOperations) ? gizmoOperationIndex_ : 0;
	const ImGuizmo::MODE gizmoMode = gizmoOperations[operationIndex] == ImGuizmo::SCALE ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

	// 操作中に更新された行列を再分解し、選択中オブジェクトの Transform へ反映します。
	if (ImGuizmo::Manipulate(&viewMatrix.m[0][0], &projectionMatrix.m[0][0], gizmoOperations[operationIndex], gizmoMode, &objectMatrix.m[0][0])) {
		ImGuizmo::DecomposeMatrixToComponents(&objectMatrix.m[0][0], translation, rotation, scale);
		transform.translate = {translation[0], translation[1], translation[2]};
		transform.rotate = {
		    BaseSceneEditorGeometry::ToRadians(rotation[0]),
		    BaseSceneEditorGeometry::ToRadians(rotation[1]),
		    BaseSceneEditorGeometry::ToRadians(rotation[2])
		};
		transform.scale = {scale[0], scale[1], scale[2]};
	}
#endif
}

/// <summary>
/// エディタ用スカイボックスを生成または再読み込みします。
/// </summary>
void BaseScene::CreateOrReloadEditorSkyBox(const std::string& textureFilePath) {
	if (textureFilePath.empty()) {
		editorSkyBox_.reset();
		skyBoxTextureFilePath_.clear();
		return;
	}

	TextureManager::GetInstance()->LoadTexture(textureFilePath);
	auto skyBox = std::make_unique<SkyBox>();
	skyBox->Initialize(textureFilePath);
	editorSkyBox_ = std::move(skyBox);
	skyBoxTextureFilePath_ = textureFilePath;
}

void BaseScene::DrawEditorSkyBoxControls() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("SkyBox", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	AtmosphereSettings& atmosphere = AtmosphereSystem::GetInstance()->GetSettings();
	ImGui::TextUnformatted("Procedural Atmosphere");
	ImGui::Checkbox("Enable Atmosphere", &atmosphere.enabled);
	if (!atmosphere.enabled) {
		ImGui::BeginDisabled();
	}
	// 方向は常に単位ベクトルへ戻し、円盤位置と平行光源の強度計算を安定させます。
	if (ImGui::DragFloat3("Sun Direction", &atmosphere.sunDirection.x, 0.005f, -1.0f, 1.0f)) {
		atmosphere.sunDirection = Normalize(atmosphere.sunDirection);
	}
	ImGui::SliderFloat("Sun Angular Radius", &atmosphere.sunAngularRadius, 0.002f, 0.06f, "%.3f");
	ImGui::SliderFloat("Sun HDR Intensity", &atmosphere.sunIntensity, 0.0f, 30.0f);
	ImGui::ColorEdit3("Rayleigh Color", &atmosphere.rayleighColor.x);
	ImGui::SliderFloat("Rayleigh Strength", &atmosphere.rayleighStrength, 0.0f, 4.0f);
	ImGui::ColorEdit3("Mie Color", &atmosphere.mieColor.x);
	ImGui::SliderFloat("Mie Strength", &atmosphere.mieStrength, 0.0f, 0.5f);
	ImGui::SliderFloat("Mie Directionality", &atmosphere.mieG, 0.0f, 0.98f);
	ImGui::SliderFloat("Atmosphere Density", &atmosphere.atmosphereDensity, 0.0f, 4.0f);
	ImGui::ColorEdit3("Horizon Fog Color", &atmosphere.horizonColor.x);
	ImGui::SliderFloat("Distance Fog Density", &atmosphere.distanceFogDensity, 0.0f, 0.08f, "%.3f");
	ImGui::SliderFloat("Height Fog Density", &atmosphere.heightFogDensity, 0.0f, 1.0f);
	ImGui::SliderFloat("Aerial Perspective", &atmosphere.aerialPerspectiveStrength, 0.0f, 2.0f);
	if (!atmosphere.enabled) {
		ImGui::EndDisabled();
	}
	ImGui::Separator();

	ImGui::Checkbox("Enable SkyBox", &isEditorSkyBoxEnabled_);

	const std::vector<std::string> ddsTextures = CollectResourceDdsTexturePaths();
	if (ddsTextures.empty()) {
		ImGui::Text("No DDS textures in Resources");
		if (ImGui::Button("Clear SkyBox")) {
			CreateOrReloadEditorSkyBox("");
			isEditorSkyBoxEnabled_ = false;
		}
		return;
	}

	if (!skyBoxTextureFilePath_.empty()) {
		for (int index = 0; index < static_cast<int>(ddsTextures.size()); ++index) {
			if (ddsTextures[index] == skyBoxTextureFilePath_) {
				selectedSkyBoxTextureIndex_ = index;
				break;
			}
		}
	}
	if (selectedSkyBoxTextureIndex_ >= static_cast<int>(ddsTextures.size())) {
		selectedSkyBoxTextureIndex_ = 0;
	}

	std::vector<const char*> textureLabels = MakeLabelPointers(ddsTextures);
	ImGui::Combo("DDS Texture", &selectedSkyBoxTextureIndex_, textureLabels.data(), static_cast<int>(textureLabels.size()));

	if (ImGui::Button("Create SkyBox")) {
		CreateOrReloadEditorSkyBox(ddsTextures[selectedSkyBoxTextureIndex_]);
		isEditorSkyBoxEnabled_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove SkyBox")) {
		CreateOrReloadEditorSkyBox("");
		isEditorSkyBoxEnabled_ = false;
	}

	if (!skyBoxTextureFilePath_.empty()) {
		ImGui::Text("Current: %s", skyBoxTextureFilePath_.c_str());
	} else {
		ImGui::Text("Current: None");
	}
#endif
}

std::string BaseScene::MakeUniqueObjectName(const std::string& baseName) const {
	return baseName + "_" + std::to_string(nextObjectId_);
}


