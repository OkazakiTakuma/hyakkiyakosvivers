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
#include "services/PlayerAssembler.h"

Entity* BaseScene::CreateEditorObject(EditorCreateType type, const std::string& modelFilePath) {
	auto object = std::make_unique<Entity>();
	object->SetEditorType(BaseSceneEditorGeometry::EditorCreateTypeName(type));

	/// <summary>生成タイプと初期化処理を対応付ける型消去登録です。</summary>
	struct EditorObjectRegistration {
		EditorCreateType type;
		bool (*initialize)(BaseScene& scene, Entity& object, const std::string& resourceName);
	};

	// 新しい生成タイプはこの表へ登録し、継承階層を増やさず生成手順だけを差し替える。
	static const std::array<EditorObjectRegistration, 14> registrations = {{
		{EditorCreateType::Empty, [](BaseScene& scene, Entity& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("Empty"));
			return true;
		}},
		{EditorCreateType::Object3dSphere, [](BaseScene& scene, Entity& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("Sphere"));
			ModelManager::GetInstance()->LoadModel("sphere.obj");
			object.AddComponent<Object3dComponent>()->SetModel("sphere.obj");
			return true;
		}},
		{EditorCreateType::Object3dCylinder, [](BaseScene& scene, Entity& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("Cylinder"));
			Object3dComponent* object3d = object.AddComponent<Object3dComponent>();
			object3d->CreateCylinder(1.0f, 2.0f, 16, true, true);
			object3d->SetTexture("Resources/gradationLine.png");
			return true;
		}},
		{EditorCreateType::Object3dCylinderOpen, [](BaseScene& scene, Entity& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("OpenCylinder"));
			Object3dComponent* object3d = object.AddComponent<Object3dComponent>();
			object3d->CreateCylinder(1.0f, 2.0f, 16, false, false);
			object3d->SetTexture("Resources/gradationLine.png");
			return true;
		}},
		{EditorCreateType::Sprite, [](BaseScene& scene, Entity& object, const std::string& resourceName) {
			const std::string spriteBaseName = resourceName.empty()
				? "Sprite" : StringUtility::PathToUtf8(StringUtility::Utf8ToPath(resourceName).stem());
			object.SetName(scene.MakeUniqueObjectName(spriteBaseName.empty() ? "Sprite" : spriteBaseName));
			SpriteComponent* sprite = object.AddComponent<SpriteComponent>();
			const std::vector<std::string> textures = CollectResourceTexturePaths();
			const std::string textureFilePath = !resourceName.empty()
				? resourceName
				: textures.empty() ? "Resources/uvChecker.png"
					: textures[(std::min)(scene.selectedTextureIndex_, static_cast<int>(textures.size()) - 1)];
			TextureManager::GetInstance()->LoadTexture(textureFilePath);
			sprite->Initialize(textureFilePath);
			object.GetTransform().scale = {100.0f, 100.0f, 1.0f};
			sprite->SetSize({100.0f, 100.0f});
			return true;
		}},
		{EditorCreateType::Text, [](BaseScene& scene, Entity& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("Text"));
			TextComponent* text = object.AddComponent<TextComponent>();
			text->SetText("Text");
			text->SetFontName("Default");
			text->SetFontSize(32.0f);
			object.GetTransform().translate = {100.0f, 100.0f, 0.0f};
			return true;
		}},
		{EditorCreateType::LoadedModel, [](BaseScene& scene, Entity& object, const std::string& resourceName) {
			if (resourceName.empty() || !ModelManager::GetInstance()->FindModel(resourceName)) {
				return false;
			}
			object.SetName(scene.MakeUniqueObjectName(StringUtility::PathToUtf8(StringUtility::Utf8ToPath(resourceName).stem())));
			object.SetEditorType("LoadedModel:" + resourceName);
			object.AddComponent<Object3dComponent>()->SetModel(resourceName);
			return true;
		}},
		{EditorCreateType::AnimatedModel, [](BaseScene& scene, Entity& object, const std::string& resourceName) {
			if (resourceName.empty() || !ModelManager::GetInstance()->FindModel(resourceName)) {
				return false;
			}
			object.SetName(scene.MakeUniqueObjectName(StringUtility::PathToUtf8(StringUtility::Utf8ToPath(resourceName).stem())));
			object.SetEditorType("AnimatedModel:" + resourceName);
			Object3dComponent* object3d = object.AddComponent<Object3dComponent>();
			object3d->SetModel(resourceName);
			object3d->SetDrawSkeleton(true);
			return true;
		}},
		{EditorCreateType::Camera, [](BaseScene& scene, Entity& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("Camera"));
			object.AddComponent<CameraComponent>()->SetFovY(0.45f);
			object.GetTransform().translate = {0.0f, 4.0f, -10.0f};
			return true;
		}},
		{EditorCreateType::PointLight, [](BaseScene& scene, Entity& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("PointLight"));
			ModelManager::GetInstance()->LoadModel("sphere.obj");
			Object3dComponent* object3d = object.AddComponent<Object3dComponent>();
			object3d->SetModel("sphere.obj");
			object3d->IsPointLightSet(true);
			object3d->SetPointLight({1.0f, 1.0f, 1.0f, 1.0f}, object.GetTransform().translate, 1.0f, 10.0f, 1.0f);
			object.GetTransform().scale = {0.25f, 0.25f, 0.25f};
			return true;
		}},
		{EditorCreateType::ParticleEmitter, [](BaseScene& scene, Entity& object, const std::string& resourceName) {
			object.SetName(scene.MakeUniqueObjectName("ParticleEmitter"));
			ParticleEmitterComponent* emitter = object.AddComponent<ParticleEmitterComponent>();
			const std::string groupName = object.GetName();
			ParticlePresetData preset;
			// プリセット未指定時は、従来と同じ円形テクスチャで新規エミッターを作る。
			preset.textureFilePath = "Resources/circle.png";
			const bool hasPreset = ParticlePresetRepository::Load(resourceName, preset);
			if (!ParticleManager::GetInstance()->HasGroup(groupName)) {
				ParticleManager::GetInstance()->CreateParticleGroup(
					groupName, preset.textureFilePath, preset.meshType);
			}
			emitter->SetGroupName(groupName);
			if (hasPreset) {
				ApplyParticlePreset(emitter, preset);
			} else {
				emitter->SetTexture(preset.textureFilePath);
				emitter->SetMeshType(preset.meshType);
				emitter->SetFrequency(0.0f);
				ParticleEmitParam param = emitter->GetParam();
				param.count = 10;
				param.lifeTime = 1.0f;
				param.scale = {1.0f, 1.0f, 1.0f};
				param.endScale = {0.0f, 0.0f, 0.0f};
				param.randomPositionRange = {0.5f, 0.5f, 0.5f};
				param.randomVelocityRange = {0.5f, 0.5f, 0.5f};
				emitter->SetParam(param);
			}
			return true;
		}},
		{EditorCreateType::Player, [](BaseScene& scene, Entity& object, const std::string& resourceName) {
			const std::string playerTypeName = resourceName.empty() ? "Default" : resourceName;
			PlayerStats playerStats = LoadPlayerStats(playerTypeName);
			object.SetName(scene.MakeUniqueObjectName(playerTypeName.empty() ? "Player" : playerTypeName));
			PlayerComponent* player = object.AddComponent<PlayerComponent>();
			PlayerAttackComponent* attack = object.AddComponent<PlayerAttackComponent>();
			ApplyPlayerDefinition(
				player,
				attack,
				playerTypeName,
				playerStats,
				ApplyPlayerStatusItems(playerStats, LoadPlayerStatusItemDefinitions()),
				LoadPlayerAttackDefinitions());
			object.GetTransform().scale = {playerStats.sizeScale, playerStats.sizeScale, playerStats.sizeScale};
			player->SetSpawnPoint(object.GetTransform().translate);

			Object3dComponent* object3d = object.AddComponent<Object3dComponent>();
			std::string playerModelFilePath = playerStats.modelFilePath;
			if (!playerModelFilePath.empty() && !ModelManager::GetInstance()->FindModel(playerModelFilePath)) {
				// プレイヤー定義のモデル種別と配置先を使い、エディタ生成でも実行時と同じ読込条件にする。
				ModelManager::GetInstance()->LoadModel(playerModelFilePath, playerStats.isAnimationModel, playerStats.modelDirectory);
			}
			if (playerModelFilePath.empty() || !ModelManager::GetInstance()->FindModel(playerModelFilePath)) {
				ModelManager::GetInstance()->LoadModel("sphere.obj");
				playerModelFilePath = "sphere.obj";
				playerStats.modelFilePath = playerModelFilePath;
				player->ApplyStats(
					playerStats,
					ApplyPlayerStatusItems(playerStats, LoadPlayerStatusItemDefinitions()));
			}
			object3d->SetModel(playerModelFilePath);
			Model* playerModel = ModelManager::GetInstance()->FindModel(playerModelFilePath);
			const bool isAnimationModel = playerModel && playerModel->GetIsAnimation();
			object3d->SetDrawSkeleton(isAnimationModel);
			player->SetModelFilePath(playerModelFilePath, isAnimationModel);

			OBBColliderComponent* collider = object.AddComponent<OBBColliderComponent>();
			collider->SetHalfSize({0.5f, 1.0f, 0.5f});
			collider->SetCenterOffset({0.0f, 1.0f, 0.0f});
			collider->SetPushBackEnabled(true);
			CameraComponent* camera = object.AddComponent<CameraComponent>();
			camera->SetLocalOffset({0.0f, 15.0f, 0.0f});
			camera->SetOverrideRotationEnabled(true);
			camera->SetOverrideRotation({MathConstants::kPi * 0.5f, 0.0f, 0.0f});
			camera->SetFovY(0.75f);
			camera->SetFarClip(1000.0f);
			return true;
		}},
		{EditorCreateType::EnemySpawnPoint, [](BaseScene& scene, Entity& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("EnemySpawnPoint"));
			object.AddComponent<EnemySpawnPointComponent>();
			return true;
		}},
		{EditorCreateType::Enemy, [](BaseScene& scene, Entity& object, const std::string& resourceName) {
			const std::string enemyTypeName = resourceName.empty() ? "Default" : resourceName;
			const EnemyStats enemyStats = LoadEnemyStats(enemyTypeName);
			object.SetName(scene.MakeUniqueObjectName(enemyTypeName));
			object.SetEditorType("Enemy");
			EnemyComponent* enemy = object.AddComponent<EnemyComponent>();
			enemy->SetEnemyTypeName(enemyTypeName);
			enemy->ApplyStats(enemyStats);
			ModelManager::GetInstance()->LoadModel("sphere.obj");
			object.AddComponent<Object3dComponent>()->SetModel("sphere.obj");
			const float enemyScale = 0.75f * enemyStats.sizeScale;
			object.GetTransform().scale = {enemyScale, enemyScale, enemyScale};
			OBBColliderComponent* collider = object.AddComponent<OBBColliderComponent>();
			collider->SetHalfSize({0.4f, 0.4f, 0.4f});
			collider->SetPushBackEnabled(true);
			return true;
		}}
	}};

	const EditorObjectRegistration* selectedRegistration = nullptr;
	for (const EditorObjectRegistration& registration : registrations) {
		if (registration.type == type) {
			selectedRegistration = &registration;
			break;
		}
	}
	if (!selectedRegistration || !selectedRegistration->initialize(*this, *object, modelFilePath)) {
		return nullptr;
	}

	object->Update();
	sceneObjects_.push_back(std::move(object));
	sceneEditorState_.SelectLastOrClear(sceneObjects_.size());
	if (type == EditorCreateType::Player || (type == EditorCreateType::Camera && !sceneEditorState_.HasActiveCamera())) {
		SetActiveCameraObject(sceneObjects_.back().get());
	}
	++nextObjectId_;
	return sceneObjects_.back().get();
}

/// <summary>
/// 現在選択中のエディタオブジェクトを削除します。
/// </summary>
void BaseScene::DeleteSelectedEditorObject() {
	if (!sceneEditorState_.HasValidSelection(sceneObjects_.size())) {
		return;
	}
	const int selectedObjectIndex = sceneEditorState_.GetSelectedObjectIndex();

	const bool deletesActiveCamera =
	    sceneEditorState_.HasActiveCamera() &&
	    sceneObjects_[selectedObjectIndex]->GetName() == sceneEditorState_.GetActiveCameraName();
	const std::string deletedObjectName = sceneObjects_[selectedObjectIndex]->GetName();
	// 所有オブジェクトを破棄する前に、残るComponentから対象への非所有参照を解除する。
	ClearEntityReferences(sceneObjects_[selectedObjectIndex].get());
	sceneObjects_.erase(sceneObjects_.begin() + selectedObjectIndex);
	for (const auto& object : sceneObjects_) {
		if (object->GetParentName() == deletedObjectName) {
			TrySetObjectParent(object.get(), "");
		}
	}
	sceneEditorState_.ClampSelection(sceneObjects_.size());

	if (deletesActiveCamera) {
		Entity* nextCamera = FindFirstCameraObject();
		if (nextCamera) {
			SetActiveCameraObject(nextCamera);
		} else {
			sceneEditorState_.ClearActiveCamera();
			ApplyCamera(fallbackCamera_);
		}
	}
}

/// <summary>
/// シーン内オブジェクトの階層ウィンドウを描画します。
/// </summary>
void BaseScene::DrawEditorHierarchy() {
#ifdef USE_IMGUI
	ImGui::Begin("Component Manager");

	sceneEditorSkyBox_.DrawControls();
	ImGui::Separator();
	DrawEditorCameraSelector();
	ImGui::Separator();

	const char* createLabels[] = {"Empty", "Sphere", "Cylinder Capped", "Cylinder Open", "Sprite", "Text", "Model", "Animation Model", "Camera", "Point Light", "Particle Emitter", "Player", "Enemy Spawn Point", "Enemy"};
	int createTypeIndex = static_cast<int>(createType_);
	if (ImGui::Combo("Type", &createTypeIndex, createLabels, _countof(createLabels))) {
		createType_ = static_cast<EditorCreateType>(createTypeIndex);
	}

	std::string selectedModelFilePath;
	std::string selectedParticlePresetName;
	std::string selectedEnemyTypeName = "Default";
	std::string selectedPlayerTypeName = "Default";
	if (createType_ == EditorCreateType::Sprite) {
		const std::vector<std::string> textures = CollectResourceTexturePaths();
		if (textures.empty()) {
			ImGui::Text("No textures");
		} else {
			if (selectedTextureIndex_ >= static_cast<int>(textures.size())) {
				selectedTextureIndex_ = 0;
			}
			std::vector<const char*> textureLabels = MakeLabelPointers(textures);
			ImGui::Combo("Texture", &selectedTextureIndex_, textureLabels.data(), static_cast<int>(textureLabels.size()));
		}
	}
	if (createType_ == EditorCreateType::LoadedModel) {
		const std::vector<std::string> loadedModels = CollectLoadedModelNames(false);
		if (loadedModels.empty()) {
			ImGui::Text("No loaded models");
		} else {
			if (selectedLoadedModelIndex_ >= static_cast<int>(loadedModels.size())) {
				selectedLoadedModelIndex_ = 0;
			}

			std::vector<const char*> loadedModelLabels = MakeLabelPointers(loadedModels);
			ImGui::Combo("Model", &selectedLoadedModelIndex_, loadedModelLabels.data(), static_cast<int>(loadedModelLabels.size()));
			selectedModelFilePath = loadedModels[selectedLoadedModelIndex_];
		}
	}
	if (createType_ == EditorCreateType::AnimatedModel) {
		const std::vector<std::string> loadedModels = CollectLoadedModelNames(true);
		if (loadedModels.empty()) {
			ImGui::Text("No loaded animation models");
		} else {
			if (selectedAnimatedModelIndex_ >= static_cast<int>(loadedModels.size())) {
				selectedAnimatedModelIndex_ = 0;
			}

			std::vector<const char*> loadedModelLabels = MakeLabelPointers(loadedModels);
			ImGui::Combo("Animation Model", &selectedAnimatedModelIndex_, loadedModelLabels.data(), static_cast<int>(loadedModelLabels.size()));
			selectedModelFilePath = loadedModels[selectedAnimatedModelIndex_];
		}
	}
	if (createType_ == EditorCreateType::ParticleEmitter) {
		const std::vector<std::string> particlePresets = ParticlePresetRepository::LoadNames();
		if (particlePresets.empty()) {
			ImGui::Text("No particle presets");
		} else {
			if (selectedParticlePresetIndex_ >= static_cast<int>(particlePresets.size())) {
				selectedParticlePresetIndex_ = 0;
			}

			std::vector<const char*> presetLabels;
			presetLabels.reserve(particlePresets.size());
			for (const std::string& presetName : particlePresets) {
				presetLabels.push_back(presetName.c_str());
			}
			ImGui::Combo("Particle Preset", &selectedParticlePresetIndex_, presetLabels.data(), static_cast<int>(presetLabels.size()));
			selectedParticlePresetName = particlePresets[selectedParticlePresetIndex_];
		}
	}
	if (createType_ == EditorCreateType::Player) {
		const std::vector<std::string> playerTypes = LoadPlayerTypeNames();
		if (playerTypes.empty()) {
			ImGui::Text("No player types");
		} else {
			if (selectedPlayerTypeIndex_ >= static_cast<int>(playerTypes.size())) {
				selectedPlayerTypeIndex_ = 0;
			}
			std::vector<const char*> playerTypeLabels = MakeLabelPointers(playerTypes);
			ImGui::Combo("Create Player Type", &selectedPlayerTypeIndex_, playerTypeLabels.data(), static_cast<int>(playerTypeLabels.size()));
			selectedPlayerTypeName = playerTypes[selectedPlayerTypeIndex_];
			const PlayerStats previewStats = LoadPlayerStats(selectedPlayerTypeName);
			ImGui::Text("Model: %s", previewStats.modelFilePath.empty() ? "None" : previewStats.modelFilePath.c_str());
			ImGui::Text("Health: %.1f  Attack: %.1f  Speed: %.3f",
			    previewStats.baseHealth * (previewStats.health / 100.0f),
			    previewStats.attack,
			    previewStats.baseSpeed * (previewStats.speed / 100.0f));
		}
	}
	if (createType_ == EditorCreateType::Enemy) {
		const std::vector<std::string> enemyTypes = LoadEnemyTypeNames();
		if (enemyTypes.empty()) {
			ImGui::Text("No enemy types");
		} else {
			if (selectedEnemyTypeIndex_ >= static_cast<int>(enemyTypes.size())) {
				selectedEnemyTypeIndex_ = 0;
			}
			std::vector<const char*> enemyTypeLabels = MakeLabelPointers(enemyTypes);
			ImGui::Combo("Enemy Type", &selectedEnemyTypeIndex_, enemyTypeLabels.data(), static_cast<int>(enemyTypeLabels.size()));
			selectedEnemyTypeName = enemyTypes[selectedEnemyTypeIndex_];
		}
	}

	const char* createButtonLabel = createType_ == EditorCreateType::Player ? "Create Player" : "Create";
	if (ImGui::Button(createButtonLabel)) {
		std::string createArgument = selectedModelFilePath;
		if (createType_ == EditorCreateType::ParticleEmitter) {
			createArgument = selectedParticlePresetName;
		} else if (createType_ == EditorCreateType::Player) {
			createArgument = selectedPlayerTypeName;
		} else if (createType_ == EditorCreateType::Enemy) {
			createArgument = selectedEnemyTypeName;
		}
		CreateEditorObject(createType_, createArgument);
	}
	ImGui::SameLine();
	const bool canDelete = sceneEditorState_.HasValidSelection(sceneObjects_.size());
	if (!canDelete) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Delete")) {
		DeleteSelectedEditorObject();
	}
	if (!canDelete) {
		ImGui::EndDisabled();
	}

	if (ImGui::Button("Save Layout")) {
		SaveEditorObjects();
	}
	ImGui::SameLine();
	if (ImGui::Button("Load Layout")) {
		LoadEditorObjects();
	}

	ImGui::Separator();
	std::function<void(const std::string&, int)> drawChildren = [&](const std::string& parentName, int depth) {
		for (int index = 0; index < static_cast<int>(sceneObjects_.size()); ++index) {
			if (sceneObjects_[index]->GetParentName() != parentName) {
				continue;
			}

			const bool isSelected = sceneEditorState_.GetSelectedObjectIndex() == index;
			std::string label(depth * 2, ' ');
			label += sceneObjects_[index]->GetName();
			if (sceneEditorState_.HasActiveCamera() && sceneObjects_[index]->GetName() == sceneEditorState_.GetActiveCameraName()) {
				label += " [Active Camera]";
			}
			if (ImGui::Selectable(label.c_str(), isSelected)) {
				sceneEditorState_.SelectObject(index);
				selectedInspectorComponentIndex_ = 0;
			}
			drawChildren(sceneObjects_[index]->GetName(), depth + 1);
		}
	};
	drawChildren("", 0);

	ImGui::End();
#endif
}

void BaseScene::DrawEditorProjectAssets() {
#ifdef USE_IMGUI
	if (!ImGui::Begin("Project")) {
		ImGui::End();
		return;
	}

	if (ImGui::CollapsingHeader("Loaded Models", ImGuiTreeNodeFlags_DefaultOpen)) {
		const std::vector<std::string> loadedModels = CollectAllLoadedModelNames();
		if (loadedModels.empty()) {
			ImGui::Text("No loaded models");
		}
		for (const std::string& modelName : loadedModels) {
			Model* model = ModelManager::GetInstance()->FindModel(modelName);
			const bool isAnimation = model && model->GetIsAnimation();
			const std::string label = std::string(isAnimation ? "[Anim] " : "[Model] ") + modelName;
			DrawProjectAssetDragSource(label, isAnimation ? "CG2_ASSET_ANIM_MODEL" : "CG2_ASSET_MODEL", modelName);
		}
	}

	if (ImGui::CollapsingHeader("Sprites / Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
		const std::vector<std::string> textures = CollectResourceTexturePaths();
		bool hasSpriteTexture = false;
		for (const std::string& texturePath : textures) {
			if (!IsSpriteTexturePath(texturePath)) {
				continue;
			}
			hasSpriteTexture = true;
			DrawProjectTextureDragSource(texturePath);
			const float windowContentRight = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
			const float nextItemRight = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + kProjectThumbnailSize + 12.0f;
			if (nextItemRight < windowContentRight) {
				ImGui::SameLine();
			}
		}
		if (!hasSpriteTexture) {
			ImGui::Text("No sprite textures");
		}
	}

	ImGui::End();
#endif
}

void BaseScene::HandleGameViewAssetDrop() {
#ifdef USE_IMGUI
	ImGuiManager::DroppedAssetPayload payload;
	while (ImGuiManager::GetInstance()->ConsumeDroppedAsset(payload)) {
		Entity* createdObject = nullptr;
		switch (payload.type) {
		case ImGuiManager::DroppedAssetPayload::Type::Model:
			createdObject = CreateEditorObject(EditorCreateType::LoadedModel, payload.path);
			break;
		case ImGuiManager::DroppedAssetPayload::Type::AnimatedModel:
			createdObject = CreateEditorObject(EditorCreateType::AnimatedModel, payload.path);
			break;
		case ImGuiManager::DroppedAssetPayload::Type::SpriteTexture:
			createdObject = CreateEditorObject(EditorCreateType::Sprite, payload.path);
			break;
		case ImGuiManager::DroppedAssetPayload::Type::None:
		default:
			break;
		}

		if (createdObject) {
			createdObject->Update();
		}
	}
#endif
}

/// <summary>
/// 使用するカメラを選択するUIを描画します。
/// </summary>
void BaseScene::DrawEditorCameraSelector() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("Main Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	std::vector<std::string> cameraNames;
	cameraNames.push_back("Fallback Camera");
	int currentCameraIndex = 0;
	for (const auto& object : sceneObjects_) {
		CameraComponent* cameraComponent = object ? object->GetComponent<CameraComponent>() : nullptr;
		if (!cameraComponent || !cameraComponent->IsEnabled()) {
			continue;
		}
		cameraNames.push_back(object->GetName());
		if (sceneEditorState_.HasActiveCamera() && object->GetName() == sceneEditorState_.GetActiveCameraName()) {
			currentCameraIndex = static_cast<int>(cameraNames.size()) - 1;
		}
	}

	std::vector<const char*> cameraLabels = MakeLabelPointers(cameraNames);
	if (ImGui::Combo("Active Camera", &currentCameraIndex, cameraLabels.data(), static_cast<int>(cameraLabels.size()))) {
		if (currentCameraIndex == 0) {
			sceneEditorState_.ClearActiveCamera();
			ApplyCamera(fallbackCamera_);
		} else {
			Entity* cameraObject = FindObjectByName(cameraNames[currentCameraIndex]);
			SetActiveCameraObject(cameraObject);
			sceneEditorState_.ClearSelection();
			for (int index = 0; index < static_cast<int>(sceneObjects_.size()); ++index) {
				if (sceneObjects_[index].get() == cameraObject) {
					sceneEditorState_.SelectObject(index);
					selectedInspectorComponentIndex_ = 0;
					break;
				}
			}
		}
	}

	if (!sceneEditorState_.HasActiveCamera()) {
		ImGui::Text("Current: Fallback Camera");
	} else {
		ImGui::Text("Current: %s", sceneEditorState_.GetActiveCameraName().c_str());
	}
#endif
}

void BaseScene::DrawEditorInspector() {
#ifdef USE_IMGUI
	ImGui::Begin("Component Inspector");

	if (!sceneEditorState_.HasValidSelection(sceneObjects_.size())) {
		ImGui::Text("No object selected");
		ImGui::End();
		return;
	}

	Entity* selectedObject = sceneObjects_[sceneEditorState_.GetSelectedObjectIndex()].get();
	ImGui::Text("%s", selectedObject->GetName().c_str());
	ImGui::Separator();

	EulerTransform& transform = selectedObject->GetTransform();
	ImGui::DragFloat3("Position", &transform.translate.x, 0.1f);
	ImGui::SliderAngle("Rotate X", &transform.rotate.x);
	ImGui::SliderAngle("Rotate Y", &transform.rotate.y);
	ImGui::SliderAngle("Rotate Z", &transform.rotate.z);
	ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f, 0.01f, 1000.0f);
	ImGui::Checkbox("Enable Gizmo", &isGizmoEnabled_);
	const char* gizmoOperationLabels[] = {"Move", "Rotate", "Scale"};
	ImGui::Combo("Gizmo Operation", &gizmoOperationIndex_, gizmoOperationLabels, _countof(gizmoOperationLabels));

	std::vector<std::string> parentLabels;
	parentLabels.push_back("None");
	int currentParentIndex = 0;
	for (const auto& object : sceneObjects_) {
		if (!CanSetObjectParent(selectedObject, object->GetName())) {
			continue;
		}
		parentLabels.push_back(object->GetName());
		if (selectedObject->GetParentName() == object->GetName()) {
			currentParentIndex = static_cast<int>(parentLabels.size()) - 1;
		}
	}
	std::vector<const char*> parentLabelPtrs = MakeLabelPointers(parentLabels);
	if (ImGui::Combo("Parent", &currentParentIndex, parentLabelPtrs.data(), static_cast<int>(parentLabelPtrs.size()))) {
		// 自己参照と子孫への接続を検証するHierarchy API以外から親を書き換えない。
		TrySetObjectParent(selectedObject, currentParentIndex == 0 ? "" : parentLabels[currentParentIndex]);
	}

	std::vector<std::string> componentLabels;
	std::vector<std::pair<const EditorComponentRegistration*, EntityComponent*>> registeredComponents;
	componentLabels.push_back("Transform");
	for (const EditorComponentRegistration& registration : GetEditorComponentRegistrations()) {
		if (EntityComponent* component = registration.find(selectedObject)) {
			componentLabels.push_back(registration.metadata->displayName);
			registeredComponents.push_back({&registration, component});
		}
	}
	if (selectedInspectorComponentIndex_ >= static_cast<int>(componentLabels.size())) {
		selectedInspectorComponentIndex_ = 0;
	}
	std::vector<const char*> componentLabelPtrs = MakeLabelPointers(componentLabels);
	ImGui::Combo("Edit EntityComponent", &selectedInspectorComponentIndex_, componentLabelPtrs.data(), static_cast<int>(componentLabelPtrs.size()));

	const std::string selectedComponentLabel = componentLabels[selectedInspectorComponentIndex_];

	auto drawComponentEnabledCheckbox = [](const char* label, EntityComponent* component) {
		if (!component) {
			return;
		}
		bool isComponentEnabled = component->IsEnabled();
		if (ImGui::Checkbox(label, &isComponentEnabled)) {
			component->SetEnabled(isComponentEnabled);
		}
	};
	ImGui::Separator();
	ImGui::Text("EntityComponent Enabled");
	for (const auto& [registration, component] : registeredComponents) {
		// 表示名はComponentTypeMetadataを正本とし、Inspector固有の重複ラベルを持たない。
		const std::string label = std::string(registration->metadata->displayName) + " Enabled";
		drawComponentEnabledCheckbox(label.c_str(), component);
	}
	ImGui::Separator();
	ImGui::Text("Entity Gravity");
	GravityComponent* gravity = selectedObject->GetComponent<GravityComponent>();
	bool useGravity = gravity != nullptr;
	if (ImGui::Checkbox("Use Gravity", &useGravity)) {
		if (useGravity) {
			gravity = selectedObject->AddComponent<GravityComponent>();
		} else {
			selectedObject->RemoveComponent<GravityComponent>();
			gravity = nullptr;
		}
	}
	if (gravity) {
		float gravityStrength = gravity->GetStrength();
		if (ImGui::DragFloat("Gravity Strength", &gravityStrength, 0.1f, 0.0f, 200.0f)) {
			gravity->SetStrength(gravityStrength);
		}
		if (ImGui::Button("Reset Gravity Velocity")) {
			gravity->ResetVelocity();
		}
	}

	if (componentLabels.size() == 1) {
		ImGui::Text("No optional components");
	}
	DrawSelectedComponentInspector(selectedObject, selectedComponentLabel);
	DrawPointLightInspector(selectedObject);
	DrawGlowBillboardInspector(selectedObject);
	DrawEnemySpawnPointInspector(selectedObject);
	DrawOBBColliderInspector(selectedObject);

	ImGui::End();
#endif
}

void BaseScene::DrawSelectedComponentInspector(Entity* selectedObject, const std::string& selectedComponentLabel) {
#ifdef USE_IMGUI
	if (Object3dComponent* object3dComponent = selectedObject->GetComponent<Object3dComponent>()) {
		if (selectedComponentLabel == "Object3d" || object3dComponent->HasModel() || object3dComponent->HasSkeleton()) {
			ImGui::Separator();
			const bool object3dHeaderOpen = ImGui::CollapsingHeader("Object3d / Model Settings", ImGuiTreeNodeFlags_DefaultOpen);
			if (object3dHeaderOpen) {
				Vector4 materialColor = object3dComponent->GetColor();
				if (ImGui::ColorEdit4("Material Color", &materialColor.x)) {
					object3dComponent->SetColor(materialColor);
				}
				Vector3 emissionColor = object3dComponent->GetEmissionColor();
				float emissionIntensity = object3dComponent->GetEmissionIntensity();
				bool emissionChanged = ImGui::ColorEdit3("Emission Color", &emissionColor.x);
				emissionChanged |= ImGui::DragFloat("Emission Intensity", &emissionIntensity, 0.05f, 0.0f, 100.0f);
				if (emissionChanged) {
					object3dComponent->SetEmission(emissionColor, emissionIntensity);
				}
				bool receiveLighting = object3dComponent->GetLightingEnabled();
				if (ImGui::Checkbox("Receive Lighting", &receiveLighting)) {
					object3dComponent->SetLightingEnabled(receiveLighting);
				}
				bool castShadow = object3dComponent->GetShadowEnabled();
				if (ImGui::Checkbox("Cast Shadow", &castShadow)) {
					object3dComponent->SetShadowEnabled(castShadow);
				}
				if (object3dComponent->HasSkeleton()) {
					ImGui::Text("Skeleton: Available");
					bool isDrawSkeleton = object3dComponent->GetDrawSkeleton();
					if (ImGui::Checkbox("Draw Bone Debug", &isDrawSkeleton)) {
						object3dComponent->SetDrawSkeleton(isDrawSkeleton);
					}
				} else {
					ImGui::Text("Skeleton: None");
				}
				if (object3dComponent->HasAnimation()) {
					// モデル内の全クリップを列挙し、オブジェクト単位で再生クリップを切り替える。
					const std::vector<std::string>& animationNames = object3dComponent->GetAnimationNames();
					if (!animationNames.empty()) {
						int currentAnimationIndex = 0;
						for (int animationIndex = 0; animationIndex < static_cast<int>(animationNames.size()); ++animationIndex) {
							if (animationNames[animationIndex] == object3dComponent->GetAnimationName()) {
								currentAnimationIndex = animationIndex;
								break;
							}
						}
						std::vector<const char*> animationLabels = MakeLabelPointers(animationNames);
						if (ImGui::Combo("Animation Clip", &currentAnimationIndex, animationLabels.data(), static_cast<int>(animationLabels.size()))) {
							object3dComponent->SetAnimation(animationNames[currentAnimationIndex], true);
						}
						ImGui::Text("Animation Clips: %d", static_cast<int>(animationNames.size()));
					}
					bool isAnimationPlaying = object3dComponent->GetAnimationPlaying();
					if (ImGui::Checkbox("Play Animation", &isAnimationPlaying)) {
						object3dComponent->SetAnimationPlaying(isAnimationPlaying);
					}
					ImGui::SameLine();
					if (ImGui::Button("Restart Animation")) {
						object3dComponent->RestartAnimation();
					}
					ImGui::SameLine();
					if (ImGui::Button("Initial Pose")) {
						object3dComponent->SetAnimationPlaying(false);
						object3dComponent->ResetAnimationPoseToInitial();
					}
					ImGui::Text(
					    "Animation Time: %.2f / %.2f",
					    object3dComponent->GetAnimationTime(),
					    object3dComponent->GetAnimationDuration()
					);
				}

				const std::vector<std::string> textures = CollectResourceTexturePaths();
				if (!textures.empty()) {
					int textureIndex = 0;
					const std::string currentTexture = object3dComponent->GetModelTextureFilePath();
					for (int index = 0; index < static_cast<int>(textures.size()); index++) {
						if (textures[index] == currentTexture) {
							textureIndex = index;
							break;
						}
					}
					std::vector<const char*> textureLabels = MakeLabelPointers(textures);
					if (ImGui::Combo("Model Texture", &textureIndex, textureLabels.data(), static_cast<int>(textureLabels.size()))) {
						object3dComponent->SetModelTexture(textures[textureIndex]);
					}
					if (currentTexture.empty()) {
						ImGui::Text("Current Model Texture: None");
					} else {
						ImGui::Text("Current Model Texture: %s", currentTexture.c_str());
					}
				} else {
					ImGui::Text("No selectable textures");
				}

				if (selectedComponentLabel == "Object3d") {
					bool isPointLight = object3dComponent->GetIsPointLightSet();
					if (ImGui::Checkbox("Use Point Light", &isPointLight)) {
						object3dComponent->IsPointLightSet(isPointLight);
					}
					Vector4 pointColor = object3dComponent->GetPointLightColor();
					Vector3 pointPosition = object3dComponent->GetPointLightPosition();
					float pointIntensity = object3dComponent->GetPointLightIntensity();
					float pointRadius = object3dComponent->GetPointLightRadius();
					float pointDecay = object3dComponent->GetPointLightDecay();
					bool pointChanged = false;
					pointChanged |= ImGui::ColorEdit4("Point Color", &pointColor.x);
					pointChanged |= ImGui::DragFloat3("Point Position", &pointPosition.x, 0.05f);
					pointChanged |= ImGui::DragFloat("Point Intensity", &pointIntensity, 0.05f, 0.0f, 100.0f);
					pointChanged |= ImGui::DragFloat("Point Radius", &pointRadius, 0.05f, 0.0f, 1000.0f);
					pointChanged |= ImGui::DragFloat("Point Decay", &pointDecay, 0.01f, 0.0f, 10.0f);
					if (pointChanged) {
						object3dComponent->SetPointLight(pointColor, pointPosition, pointIntensity, pointRadius, pointDecay);
					}
				}
			}
			// ボーンポーズ編集はモデル設定に埋め込まず、右側インスペクターの独立セクションとして表示する。
			// これによりモデル設定を閉じても、選択中モデルのポーズ操作へすぐアクセスできる。
			if (object3dComponent->HasSkeleton()) {
				ImGui::Separator();
				if (ImGui::CollapsingHeader("Bone Pose Editor", ImGuiTreeNodeFlags_DefaultOpen)) {
					const std::vector<std::string> jointNames = object3dComponent->GetJointNames();
					static int selectedJointIndex = 0;
					if (selectedJointIndex >= static_cast<int>(jointNames.size())) {
						selectedJointIndex = 0;
					}
					if (!jointNames.empty()) {
						std::vector<const char*> jointLabels = MakeLabelPointers(jointNames);
						ImGui::Combo("Bone", &selectedJointIndex, jointLabels.data(), static_cast<int>(jointLabels.size()));
						const std::string& selectedJointName = jointNames[selectedJointIndex];
						Vector3 jointRotation = object3dComponent->GetJointRotationOffsetEuler(selectedJointName);
						bool rotationChanged = false;
						rotationChanged |= ImGui::SliderAngle("Bone Rotate X", &jointRotation.x, -180.0f, 180.0f);
						rotationChanged |= ImGui::SliderAngle("Bone Rotate Y", &jointRotation.y, -180.0f, 180.0f);
						rotationChanged |= ImGui::SliderAngle("Bone Rotate Z", &jointRotation.z, -180.0f, 180.0f);
						if (rotationChanged) {
							object3dComponent->SetJointRotationOffsetEuler(selectedJointName, jointRotation);
						}
						if (ImGui::Button("Clear Bone Pose Overrides")) {
							object3dComponent->ClearJointRotationOffsets();
						}
					} else {
						ImGui::TextDisabled("No joints found");
					}
				}
			}
		}
	}
	if (SpriteComponent* spriteComponent = selectedObject->GetComponent<SpriteComponent>(); spriteComponent && selectedComponentLabel == "Sprite") {
		ImGui::Separator();
		ImGui::Text("SpriteComponent");
		const std::vector<std::string> textures = CollectResourceTexturePaths();
		if (!textures.empty()) {
			int textureIndex = 0;
			for (int index = 0; index < static_cast<int>(textures.size()); index++) {
				if (textures[index] == spriteComponent->GetTextureFilePath()) {
					textureIndex = index;
					break;
				}
			}
			std::vector<const char*> textureLabels = MakeLabelPointers(textures);
			if (ImGui::Combo("Texture", &textureIndex, textureLabels.data(), static_cast<int>(textureLabels.size()))) {
				spriteComponent->SetTexture(textures[textureIndex]);
			}
		}
		Vector4 color = spriteComponent->GetColor();
		if (ImGui::ColorEdit4("Color", &color.x)) {
			spriteComponent->SetColor(color);
		}
		Vector2 size = spriteComponent->GetSize();
		if (ImGui::DragFloat2("Size", &size.x, 1.0f, 1.0f, 4096.0f)) {
			spriteComponent->SetSize(size);
		}
	}
	if (TextComponent* textComponent = selectedObject->GetComponent<TextComponent>()) {
		ImGui::Separator();
		if (ImGui::CollapsingHeader("TextComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
			const std::string currentText = textComponent->GetText();
			const size_t copyLength = (std::min)(currentText.size(), textEditBuffer_.size() - 1);
			std::memcpy(textEditBuffer_.data(), currentText.data(), copyLength);
			textEditBuffer_[copyLength] = '\0';
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputTextMultiline("Text", textEditBuffer_.data(), textEditBuffer_.size(), ImVec2(0.0f, 80.0f))) {
				textComponent->SetText(textEditBuffer_.data());
			}

			std::vector<std::string> fontNames = ImGuiManager::GetInstance()->GetAvailableFontNames();
			if (fontNames.empty()) {
				fontNames.push_back("Default");
			}
			int fontIndex = 0;
			for (int index = 0; index < static_cast<int>(fontNames.size()); ++index) {
				if (fontNames[index] == textComponent->GetFontName()) {
					fontIndex = index;
					break;
				}
			}
			std::vector<const char*> fontLabels = MakeLabelPointers(fontNames);
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::Combo("Font", &fontIndex, fontLabels.data(), static_cast<int>(fontLabels.size()))) {
				textComponent->SetFontName(fontNames[fontIndex]);
			}

			float fontSize = textComponent->GetFontSize();
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::DragFloat("Font Size", &fontSize, 1.0f, 1.0f, 256.0f)) {
				textComponent->SetFontSize(fontSize);
			}

			const char* anchorLabels[] = {
			    "Top Left",
			    "Top Center",
			    "Top Right",
			    "Center Left",
			    "Center",
			    "Center Right",
			    "Bottom Left",
			    "Bottom Center",
			    "Bottom Right"
			};
			int anchorIndex = static_cast<int>(textComponent->GetAnchor());
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::Combo("Anchor", &anchorIndex, anchorLabels, _countof(anchorLabels))) {
				textComponent->SetAnchor(static_cast<TextComponent::Anchor>(anchorIndex));
			}

			Vector4 color = textComponent->GetColor();
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::ColorEdit4("Color", &color.x)) {
				textComponent->SetColor(color);
			}
		}
	}
	if (selectedObject->GetComponent<CameraComponent>() && selectedComponentLabel == "Camera") {
		DrawCameraInspector(selectedObject);
	}
	if (selectedObject->GetComponent<ParticleEmitterComponent>()) {
		DrawParticleEmitterInspector(selectedObject);
	}

#else
	(void)selectedObject;
	(void)selectedComponentLabel;
#endif
}


