#include "BaseScene.h"
#include "StringUtility.h"
#include "helpers/BaseSceneEditorGeometry.h"
#include "helpers/BaseSceneResourceCatalog.h"
#include "repositories/EnemyStatusRepository.h"
#include "repositories/ParticlePresetRepository.h"
#include "repositories/PlayerStatusRepository.h"

#include <array>

#ifdef USE_IMGUI
#include "../../../imgui/ImGuizmo.h"
#include <cctype>
#include <limits>

namespace {
constexpr float kProjectThumbnailSize = 64.0f;

/// <summary>Inspectorへ公開するComponentの取得方法と共通UI機能をまとめた型消去登録です。</summary>
struct EditorComponentRegistration {
	const char* label;
	Component* (*find)(GameObject* object);
	bool supportsGravity;
};

template<class T>
Component* FindEditorComponent(GameObject* object) {
	return object ? object->GetComponent<T>() : nullptr;
}

const std::array<EditorComponentRegistration, 13>& GetEditorComponentRegistrations() {
	// 新しいComponentをInspectorへ追加するときは、この表へ1行登録するだけで
	// 選択リスト・有効状態・共通重力UIへ同時に反映される。
	static const std::array<EditorComponentRegistration, 13> registrations = {{
		{"Object3d", &FindEditorComponent<Object3dComponent>, true},
		{"PointLight", &FindEditorComponent<PointLightComponent>, false},
		{"GlowBillboard", &FindEditorComponent<GlowBillboardComponent>, false},
		{"Sprite", &FindEditorComponent<SpriteComponent>, true},
		{"Text", &FindEditorComponent<TextComponent>, true},
		{"Camera", &FindEditorComponent<CameraComponent>, true},
		{"ParticleEmitter", &FindEditorComponent<ParticleEmitterComponent>, true},
		{"Player", &FindEditorComponent<Player>, true},
		{"PlayerAttack", &FindEditorComponent<PlayerAttackComponent>, true},
		{"EnemySpawnPoint", &FindEditorComponent<EnemySpawnPointComponent>, true},
		{"Enemy", &FindEditorComponent<EnemyComponent>, true},
		{"OBBCollider", &FindEditorComponent<OBBColliderComponent>, true},
		{"SphereCollider", &FindEditorComponent<SphereColliderComponent>, true}
	}};
	return registrations;
}

bool InputTextMultilineString(const char* label, std::string& value, const ImVec2& size = ImVec2(0.0f, 72.0f)) {
	std::array<char, 1024> buffer{};
	const size_t copyLength = (std::min)(value.size(), buffer.size() - 1);
	std::memcpy(buffer.data(), value.data(), copyLength);
	if (!ImGui::InputTextMultiline(label, buffer.data(), buffer.size(), size)) {
		return false;
	}
	value = buffer.data();
	return true;
}

bool SelectionTextureCombo(const char* label, std::string& textureFilePath) {
	const std::vector<std::string> textures = CollectResourceTexturePaths();
	std::vector<std::string> labels;
	labels.reserve(textures.size() + 1);
	labels.push_back("None");
	labels.insert(labels.end(), textures.begin(), textures.end());
	int selectedIndex = 0;
	for (int index = 0; index < static_cast<int>(textures.size()); ++index) {
		if (textures[index] == textureFilePath) {
			selectedIndex = index + 1;
			break;
		}
	}
	const std::vector<const char*> labelPointers = MakeLabelPointers(labels);
	if (!ImGui::Combo(label, &selectedIndex, labelPointers.data(), static_cast<int>(labelPointers.size()))) {
		return false;
	}
	textureFilePath = selectedIndex == 0 ? "" : textures[selectedIndex - 1];
	return true;
}

void DrawProjectAssetDragSource(const std::string& label, const char* payloadType, const std::string& path) {
	ImGui::Selectable(label.c_str(), false);
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
		ImGui::SetDragDropPayload(payloadType, path.c_str(), path.size() + 1);
		ImGui::Text("%s", label.c_str());
		ImGui::EndDragDropSource();
	}
}

void DrawProjectTextureDragSource(const std::string& texturePath) {
	TextureManager* textureManager = TextureManager::GetInstance();
	if (!textureManager->LoadTexture(texturePath)) {
		ImGui::TextDisabled("%s (load failed)", texturePath.c_str());
		return;
	}
	const D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = textureManager->GetSRVHandleGPU(texturePath);
	const DirectX::TexMetadata& metadata = textureManager->GetTextureMetadata(texturePath);
	const float width = static_cast<float>(metadata.width);
	const float height = static_cast<float>(metadata.height);
	const float maxSide = (std::max)(width, height);
	const ImVec2 imageSize = maxSide > 0.0f
	    ? ImVec2(kProjectThumbnailSize * width / maxSide, kProjectThumbnailSize * height / maxSide)
	    : ImVec2(kProjectThumbnailSize, kProjectThumbnailSize);
	const std::string fileName = StringUtility::PathToUtf8(StringUtility::Utf8ToPath(texturePath).filename());
	const std::string buttonId = "##SpriteTexture_" + texturePath;

	ImGui::BeginGroup();
	ImGui::PushID(texturePath.c_str());
	ImGui::BeginChild("ThumbnailFrame", ImVec2(kProjectThumbnailSize + 12.0f, kProjectThumbnailSize + 12.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	const ImVec2 cursor = ImGui::GetCursorPos();
	ImGui::SetCursorPos(ImVec2(
	    cursor.x + (kProjectThumbnailSize - imageSize.x) * 0.5f,
	    cursor.y + (kProjectThumbnailSize - imageSize.y) * 0.5f
	));
	ImGui::ImageButton(buttonId.c_str(), static_cast<ImTextureID>(textureHandle.ptr), imageSize);
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
		ImGui::SetDragDropPayload("CG2_ASSET_SPRITE", texturePath.c_str(), texturePath.size() + 1);
		ImGui::Image(static_cast<ImTextureID>(textureHandle.ptr), imageSize);
		ImGui::Text("%s", texturePath.c_str());
		ImGui::EndDragDropSource();
	}
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Image(static_cast<ImTextureID>(textureHandle.ptr), ImVec2(kProjectThumbnailSize * 2.0f, kProjectThumbnailSize * 2.0f));
		ImGui::Text("%s", texturePath.c_str());
		ImGui::Text("%.0f x %.0f", width, height);
		ImGui::EndTooltip();
	}
	ImGui::EndChild();
	ImGui::TextWrapped("%s", fileName.c_str());
	ImGui::PopID();
	ImGui::EndGroup();
}

bool IsSpriteTexturePath(const std::string& path) {
	std::string extension = StringUtility::PathToUtf8(StringUtility::Utf8ToPath(path).extension());
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
		return static_cast<char>(std::tolower(value));
	});
	return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
}
}
#endif

GameObject* BaseScene::CreateEditorObject(EditorCreateType type, const std::string& modelFilePath) {
	auto object = std::make_unique<GameObject>();
	object->SetEditorType(BaseSceneEditorGeometry::EditorCreateTypeName(type));

	/// <summary>生成タイプと初期化処理を対応付ける型消去登録です。</summary>
	struct EditorObjectRegistration {
		EditorCreateType type;
		bool (*initialize)(BaseScene& scene, GameObject& object, const std::string& resourceName);
	};

	// 新しい生成タイプはこの表へ登録し、継承階層を増やさず生成手順だけを差し替える。
	static const std::array<EditorObjectRegistration, 14> registrations = {{
		{EditorCreateType::Empty, [](BaseScene& scene, GameObject& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("Empty"));
			return true;
		}},
		{EditorCreateType::Object3dSphere, [](BaseScene& scene, GameObject& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("Sphere"));
			ModelManager::GetInstance()->LoadModel("sphere.obj");
			object.AddComponent<Object3dComponent>()->SetModel("sphere.obj");
			return true;
		}},
		{EditorCreateType::Object3dCylinder, [](BaseScene& scene, GameObject& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("Cylinder"));
			Object3dComponent* object3d = object.AddComponent<Object3dComponent>();
			object3d->CreateCylinder(1.0f, 2.0f, 16, true, true);
			object3d->SetTexture("Resources/gradationLine.png");
			return true;
		}},
		{EditorCreateType::Object3dCylinderOpen, [](BaseScene& scene, GameObject& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("OpenCylinder"));
			Object3dComponent* object3d = object.AddComponent<Object3dComponent>();
			object3d->CreateCylinder(1.0f, 2.0f, 16, false, false);
			object3d->SetTexture("Resources/gradationLine.png");
			return true;
		}},
		{EditorCreateType::Sprite, [](BaseScene& scene, GameObject& object, const std::string& resourceName) {
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
		{EditorCreateType::Text, [](BaseScene& scene, GameObject& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("Text"));
			TextComponent* text = object.AddComponent<TextComponent>();
			text->SetText("Text");
			text->SetFontName("Default");
			text->SetFontSize(32.0f);
			object.GetTransform().translate = {100.0f, 100.0f, 0.0f};
			return true;
		}},
		{EditorCreateType::LoadedModel, [](BaseScene& scene, GameObject& object, const std::string& resourceName) {
			if (resourceName.empty() || !ModelManager::GetInstance()->FindModel(resourceName)) {
				return false;
			}
			object.SetName(scene.MakeUniqueObjectName(StringUtility::PathToUtf8(StringUtility::Utf8ToPath(resourceName).stem())));
			object.SetEditorType("LoadedModel:" + resourceName);
			object.AddComponent<Object3dComponent>()->SetModel(resourceName);
			return true;
		}},
		{EditorCreateType::AnimatedModel, [](BaseScene& scene, GameObject& object, const std::string& resourceName) {
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
		{EditorCreateType::Camera, [](BaseScene& scene, GameObject& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("Camera"));
			object.AddComponent<CameraComponent>()->SetFovY(0.45f);
			object.GetTransform().translate = {0.0f, 4.0f, -10.0f};
			return true;
		}},
		{EditorCreateType::PointLight, [](BaseScene& scene, GameObject& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("PointLight"));
			ModelManager::GetInstance()->LoadModel("sphere.obj");
			Object3dComponent* object3d = object.AddComponent<Object3dComponent>();
			object3d->SetModel("sphere.obj");
			object3d->IsPointLightSet(true);
			object3d->SetPointLight({1.0f, 1.0f, 1.0f, 1.0f}, object.GetTransform().translate, 1.0f, 10.0f, 1.0f);
			object.GetTransform().scale = {0.25f, 0.25f, 0.25f};
			return true;
		}},
		{EditorCreateType::ParticleEmitter, [](BaseScene& scene, GameObject& object, const std::string& resourceName) {
			object.SetName(scene.MakeUniqueObjectName("ParticleEmitter"));
			ParticleEmitterComponent* emitter = object.AddComponent<ParticleEmitterComponent>();
			const std::string groupName = object.GetName();
			std::string textureFilePath = "Resources/circle.png";
			ParticleMeshType meshType = kMeshTypeQuad;
			ParticlePresetRepository::GetResourceInfo(resourceName, textureFilePath, meshType);
			if (!ParticleManager::GetInstance()->HasGroup(groupName)) {
				ParticleManager::GetInstance()->CreateParticleGroup(groupName, textureFilePath, meshType);
			}
			emitter->SetGroupName(groupName);
			emitter->SetTexture(textureFilePath);
			emitter->SetMeshType(meshType);
			if (!resourceName.empty()) {
				ParticlePresetRepository::Apply(resourceName, emitter);
			} else {
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
		{EditorCreateType::Player, [](BaseScene& scene, GameObject& object, const std::string& resourceName) {
			const std::string playerTypeName = resourceName.empty() ? "Default" : resourceName;
			PlayerStats playerStats = LoadPlayerStats(playerTypeName);
			object.SetName(scene.MakeUniqueObjectName(playerTypeName.empty() ? "Player" : playerTypeName));
			Player* player = object.AddComponent<Player>();
			player->SetPlayerTypeName(playerTypeName);
			player->ApplyStats(playerStats, ApplyPlayerStatusItems(playerStats));
			object.GetTransform().scale = {playerStats.sizeScale, playerStats.sizeScale, playerStats.sizeScale};
			player->SetSpawnPoint(object.GetTransform().translate);
			ApplyPlayerAttackSlots(object.AddComponent<PlayerAttackComponent>(), playerStats);

			Object3dComponent* object3d = object.AddComponent<Object3dComponent>();
			std::string playerModelFilePath = playerStats.modelFilePath;
			if (!playerModelFilePath.empty() && !ModelManager::GetInstance()->FindModel(playerModelFilePath)) {
				ModelManager::GetInstance()->LoadModel(playerModelFilePath);
			}
			if (playerModelFilePath.empty() || !ModelManager::GetInstance()->FindModel(playerModelFilePath)) {
				ModelManager::GetInstance()->LoadModel("sphere.obj");
				playerModelFilePath = "sphere.obj";
				playerStats.modelFilePath = playerModelFilePath;
				player->ApplyStats(playerStats, ApplyPlayerStatusItems(playerStats));
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
		{EditorCreateType::EnemySpawnPoint, [](BaseScene& scene, GameObject& object, const std::string&) {
			object.SetName(scene.MakeUniqueObjectName("EnemySpawnPoint"));
			object.AddComponent<EnemySpawnPointComponent>();
			return true;
		}},
		{EditorCreateType::Enemy, [](BaseScene& scene, GameObject& object, const std::string& resourceName) {
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
	selectedObjectIndex_ = static_cast<int>(sceneObjects_.size()) - 1;
	if (type == EditorCreateType::Player || (type == EditorCreateType::Camera && activeCameraObjectName_.empty())) {
		SetActiveCameraObject(sceneObjects_.back().get());
	}
	++nextObjectId_;
	return sceneObjects_.back().get();
}

/// <summary>
/// 現在選択中のエディタオブジェクトを削除します。
/// </summary>
void BaseScene::DeleteSelectedEditorObject() {
	if (selectedObjectIndex_ < 0 || selectedObjectIndex_ >= static_cast<int>(sceneObjects_.size())) {
		return;
	}

	const bool deletesActiveCamera =
	    !activeCameraObjectName_.empty() &&
	    sceneObjects_[selectedObjectIndex_]->GetName() == activeCameraObjectName_;
	const std::string deletedObjectName = sceneObjects_[selectedObjectIndex_]->GetName();
	sceneObjects_.erase(sceneObjects_.begin() + selectedObjectIndex_);
	for (const auto& object : sceneObjects_) {
		if (object->GetParentName() == deletedObjectName) {
			object->SetParentName("");
		}
	}
	if (sceneObjects_.empty()) {
		selectedObjectIndex_ = -1;
	} else if (selectedObjectIndex_ >= static_cast<int>(sceneObjects_.size())) {
		selectedObjectIndex_ = static_cast<int>(sceneObjects_.size()) - 1;
	}

	if (deletesActiveCamera) {
		GameObject* nextCamera = FindFirstCameraObject();
		if (nextCamera) {
			SetActiveCameraObject(nextCamera);
		} else {
			activeCameraObjectName_.clear();
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

	DrawEditorSkyBoxControls();
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
	const bool canDelete = selectedObjectIndex_ >= 0 && selectedObjectIndex_ < static_cast<int>(sceneObjects_.size());
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

			const bool isSelected = selectedObjectIndex_ == index;
			std::string label(depth * 2, ' ');
			label += sceneObjects_[index]->GetName();
			if (!activeCameraObjectName_.empty() && sceneObjects_[index]->GetName() == activeCameraObjectName_) {
				label += " [Active Camera]";
			}
			if (ImGui::Selectable(label.c_str(), isSelected)) {
				selectedObjectIndex_ = index;
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
		GameObject* createdObject = nullptr;
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
		if (!activeCameraObjectName_.empty() && object->GetName() == activeCameraObjectName_) {
			currentCameraIndex = static_cast<int>(cameraNames.size()) - 1;
		}
	}

	std::vector<const char*> cameraLabels = MakeLabelPointers(cameraNames);
	if (ImGui::Combo("Active Camera", &currentCameraIndex, cameraLabels.data(), static_cast<int>(cameraLabels.size()))) {
		if (currentCameraIndex == 0) {
			activeCameraObjectName_.clear();
			ApplyCamera(fallbackCamera_);
		} else {
			GameObject* cameraObject = FindObjectByName(cameraNames[currentCameraIndex]);
			SetActiveCameraObject(cameraObject);
			selectedObjectIndex_ = -1;
			for (int index = 0; index < static_cast<int>(sceneObjects_.size()); ++index) {
				if (sceneObjects_[index].get() == cameraObject) {
					selectedObjectIndex_ = index;
					selectedInspectorComponentIndex_ = 0;
					break;
				}
			}
		}
	}

	if (activeCameraObjectName_.empty()) {
		ImGui::Text("Current: Fallback Camera");
	} else {
		ImGui::Text("Current: %s", activeCameraObjectName_.c_str());
	}
#endif
}

void BaseScene::DrawEditorInspector() {
#ifdef USE_IMGUI
	ImGui::Begin("Component Inspector");

	if (selectedObjectIndex_ < 0 || selectedObjectIndex_ >= static_cast<int>(sceneObjects_.size())) {
		ImGui::Text("No object selected");
		ImGui::End();
		return;
	}

	GameObject* selectedObject = sceneObjects_[selectedObjectIndex_].get();
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
		if (object.get() == selectedObject) {
			continue;
		}
		parentLabels.push_back(object->GetName());
		if (selectedObject->GetParentName() == object->GetName()) {
			currentParentIndex = static_cast<int>(parentLabels.size()) - 1;
		}
	}
	std::vector<const char*> parentLabelPtrs = MakeLabelPointers(parentLabels);
	if (ImGui::Combo("Parent", &currentParentIndex, parentLabelPtrs.data(), static_cast<int>(parentLabelPtrs.size()))) {
		selectedObject->SetParentName(currentParentIndex == 0 ? "" : parentLabels[currentParentIndex]);
	}

	std::vector<std::string> componentLabels;
	std::vector<std::pair<const EditorComponentRegistration*, Component*>> registeredComponents;
	componentLabels.push_back("Transform");
	for (const EditorComponentRegistration& registration : GetEditorComponentRegistrations()) {
		if (Component* component = registration.find(selectedObject)) {
			componentLabels.push_back(registration.label);
			registeredComponents.push_back({&registration, component});
		}
	}
	if (selectedInspectorComponentIndex_ >= static_cast<int>(componentLabels.size())) {
		selectedInspectorComponentIndex_ = 0;
	}
	std::vector<const char*> componentLabelPtrs = MakeLabelPointers(componentLabels);
	ImGui::Combo("Edit Component", &selectedInspectorComponentIndex_, componentLabelPtrs.data(), static_cast<int>(componentLabelPtrs.size()));

	const std::string selectedComponentLabel = componentLabels[selectedInspectorComponentIndex_];

	auto drawComponentEnabledCheckbox = [](const char* label, Component* component) {
		if (!component) {
			return;
		}
		bool isComponentEnabled = component->IsEnabled();
		if (ImGui::Checkbox(label, &isComponentEnabled)) {
			component->SetEnabled(isComponentEnabled);
		}
	};
	auto drawComponentGravityControls = [](const char* label, Component* component) {
		if (!component) {
			return;
		}
		if (ImGui::TreeNode(label)) {
			bool isGravityEnabled = component->IsGravityEnabled();
			if (ImGui::Checkbox("Use Gravity", &isGravityEnabled)) {
				component->SetGravityEnabled(isGravityEnabled);
			}
			float gravityStrength = component->GetGravityStrength();
			if (ImGui::DragFloat("Gravity Strength", &gravityStrength, 0.1f, 0.0f, 200.0f)) {
				component->SetGravityStrength(gravityStrength);
			}
			if (ImGui::Button("Reset Gravity Velocity")) {
				component->ResetGravityVelocity();
			}
			ImGui::TreePop();
		}
	};
	ImGui::Separator();
	ImGui::Text("Component Enabled");
	for (const auto& [registration, component] : registeredComponents) {
		const std::string label = std::string(registration->label) + " Enabled";
		drawComponentEnabledCheckbox(label.c_str(), component);
	}
	ImGui::Separator();
	ImGui::Text("Component Gravity");
	for (const auto& [registration, component] : registeredComponents) {
		if (!registration->supportsGravity) {
			continue;
		}
		const std::string label = std::string(registration->label) + " Gravity";
		drawComponentGravityControls(label.c_str(), component);
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

void BaseScene::DrawSelectedComponentInspector(GameObject* selectedObject, const std::string& selectedComponentLabel) {
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

void BaseScene::DrawEnemySpawnPointInspector(GameObject* selectedObject) {
#ifdef USE_IMGUI
	if (EnemySpawnPointComponent* enemySpawnPoint = selectedObject->GetComponent<EnemySpawnPointComponent>()) {
		ImGui::Separator();
		ImGui::Text("EnemySpawnPointComponent");

		std::vector<std::string> targetLabels;
		targetLabels.push_back("Auto First Player");
		int currentTargetIndex = 0;
		for (const auto& object : sceneObjects_) {
			if (!object->GetComponent<Player>()) {
				continue;
			}
			targetLabels.push_back(object->GetName());
			if (enemySpawnPoint->GetTargetName() == object->GetName()) {
				currentTargetIndex = static_cast<int>(targetLabels.size()) - 1;
			}
		}
		std::vector<const char*> targetLabelPtrs = MakeLabelPointers(targetLabels);
		if (ImGui::Combo("Target Player", &currentTargetIndex, targetLabelPtrs.data(), static_cast<int>(targetLabelPtrs.size()))) {
			enemySpawnPoint->SetTargetName(currentTargetIndex == 0 ? "" : targetLabels[currentTargetIndex]);
			enemySpawnPoint->SetTarget(nullptr);
		}

		std::vector<std::string> cameraLabels;
		cameraLabels.push_back("Active Camera");
		int currentCameraIndex = 0;
		for (const auto& object : sceneObjects_) {
			if (!object->GetComponent<CameraComponent>()) {
				continue;
			}
			cameraLabels.push_back(object->GetName());
			if (enemySpawnPoint->GetCameraName() == object->GetName()) {
				currentCameraIndex = static_cast<int>(cameraLabels.size()) - 1;
			}
		}
		std::vector<const char*> cameraLabelPtrs = MakeLabelPointers(cameraLabels);
		if (ImGui::Combo("Reference Camera", &currentCameraIndex, cameraLabelPtrs.data(), static_cast<int>(cameraLabelPtrs.size()))) {
			enemySpawnPoint->SetCameraName(currentCameraIndex == 0 ? "" : cameraLabels[currentCameraIndex]);
			enemySpawnPoint->SetCamera(nullptr);
		}

		std::vector<std::string> enemyTypes = LoadEnemyTypeNames();
		int currentEnemyTypeIndex = 0;
		for (int index = 0; index < static_cast<int>(enemyTypes.size()); ++index) {
			if (enemyTypes[index] == enemySpawnPoint->GetEnemyTypeName()) {
				currentEnemyTypeIndex = index;
				break;
			}
		}
		if (!enemyTypes.empty()) {
			std::vector<const char*> enemyTypeLabels = MakeLabelPointers(enemyTypes);
			if (ImGui::Combo("Spawn Enemy Type", &currentEnemyTypeIndex, enemyTypeLabels.data(), static_cast<int>(enemyTypeLabels.size()))) {
				enemySpawnPoint->SetEnemyTypeName(enemyTypes[currentEnemyTypeIndex]);
				enemySpawnPoint->ResetSpawnTimer();
			}
		}
		bool spawnEnabled = enemySpawnPoint->GetSpawnEnabled();
		if (ImGui::Checkbox("Spawn Enabled", &spawnEnabled)) {
			enemySpawnPoint->SetSpawnEnabled(spawnEnabled);
			enemySpawnPoint->ResetSpawnTimer();
		}

		int spawnCount = enemySpawnPoint->GetSpawnCount();
		if (ImGui::DragInt("Spawn Count", &spawnCount, 1.0f, 1, 64)) {
			enemySpawnPoint->SetSpawnCount(spawnCount);
		}

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Spawn Schedules", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto schedules = enemySpawnPoint->GetSpawnSchedules();
			bool schedulesChanged = false;
			ImGui::Text("Elapsed Time: %.2f sec", enemySpawnPoint->GetElapsedTimeSeconds());
			if (ImGui::Button("Reset Schedule Time")) {
				enemySpawnPoint->ResetSpawnTimer();
			}
			ImGui::SameLine();
			if (ImGui::Button("Add Schedule")) {
				EnemySpawnPointComponent::SpawnSchedule schedule;
				schedule.enemyTypeName = !enemyTypes.empty() ? enemyTypes.front() : enemySpawnPoint->GetEnemyTypeName();
				schedules.push_back(schedule);
				schedulesChanged = true;
			}

			int removeScheduleIndex = -1;
			for (int scheduleIndex = 0; scheduleIndex < static_cast<int>(schedules.size()); ++scheduleIndex) {
				auto& schedule = schedules[scheduleIndex];
				ImGui::PushID(scheduleIndex);
				ImGui::Separator();
				ImGui::Text("Schedule %d", scheduleIndex + 1);

				if (!enemyTypes.empty()) {
					int scheduleEnemyTypeIndex = 0;
					for (int enemyTypeIndex = 0; enemyTypeIndex < static_cast<int>(enemyTypes.size()); ++enemyTypeIndex) {
						if (enemyTypes[enemyTypeIndex] == schedule.enemyTypeName) {
							scheduleEnemyTypeIndex = enemyTypeIndex;
							break;
						}
					}
					std::vector<const char*> scheduleEnemyTypeLabels = MakeLabelPointers(enemyTypes);
					if (ImGui::Combo("Enemy Type", &scheduleEnemyTypeIndex, scheduleEnemyTypeLabels.data(), static_cast<int>(scheduleEnemyTypeLabels.size()))) {
						schedule.enemyTypeName = enemyTypes[scheduleEnemyTypeIndex];
						schedulesChanged = true;
					}
				}

				if (ImGui::DragFloat("Start Time (sec)", &schedule.startTimeSeconds, 0.1f, 0.0f, 36000.0f)) {
					schedule.startTimeSeconds = (std::max)(0.0f, schedule.startTimeSeconds);
					schedule.endTimeSeconds = (std::max)(schedule.startTimeSeconds, schedule.endTimeSeconds);
					schedulesChanged = true;
				}
				if (ImGui::DragFloat("End Time (sec)", &schedule.endTimeSeconds, 0.1f, schedule.startTimeSeconds, 36000.0f)) {
					schedule.endTimeSeconds = (std::max)(schedule.startTimeSeconds, schedule.endTimeSeconds);
					schedulesChanged = true;
				}
				if (ImGui::DragInt("Interval (frames)", &schedule.spawnIntervalFrames, 1.0f, 1, 360000)) {
					schedule.spawnIntervalFrames = (std::max)(1, schedule.spawnIntervalFrames);
					schedulesChanged = true;
				}
				if (ImGui::DragInt("Amount Per Spawn", &schedule.spawnAmount, 1.0f, 1, 64)) {
					schedule.spawnAmount = std::clamp(schedule.spawnAmount, 1, 64);
					schedulesChanged = true;
				}
				if (ImGui::Checkbox("Apply Time Scaling", &schedule.applyTimeScaling)) {
					schedulesChanged = true;
				}
				// 中ボスのような時刻イベントは、一度だけ生成する設定にできる。
				if (ImGui::Checkbox("Spawn Once", &schedule.spawnOnce)) {
					schedulesChanged = true;
				}
				if (ImGui::Button("Remove Schedule")) {
					removeScheduleIndex = scheduleIndex;
				}
				ImGui::PopID();
			}

			if (removeScheduleIndex >= 0) {
				schedules.erase(schedules.begin() + removeScheduleIndex);
				schedulesChanged = true;
			}
			if (schedulesChanged) {
				enemySpawnPoint->SetSpawnSchedules(schedules);
			}

			if (schedules.empty()) {
				ImGui::TextDisabled("No schedules: legacy enemy Spawns Per Minute is used.");
			} else {
				ImGui::TextDisabled("Schedule mode overrides legacy Spawn Enemy Type / Spawns Per Minute.");
			}
		}

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Enemy Time Scaling", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::TextDisabled("Multipliers are fixed when an enabled schedule spawns an enemy.");
			auto tiers = enemySpawnPoint->GetTimeScalingTiers();
			if (ImGui::Button("Add Scaling Tier")) {
				EnemySpawnPointComponent::TimeScalingTier tier;
				if (!tiers.empty()) {
					tier = tiers.back();
					tier.startTimeSeconds += 60.0f;
				}
				tiers.push_back(tier);
			}

			int removeTierIndex = -1;
			bool tiersChanged = false;
			for (int tierIndex = 0; tierIndex < static_cast<int>(tiers.size()); ++tierIndex) {
				auto& tier = tiers[tierIndex];
				ImGui::PushID(10000 + tierIndex);
				ImGui::Separator();
				ImGui::Text("Tier %d", tierIndex + 1);
				tiersChanged |= ImGui::DragFloat("Start Time (sec)", &tier.startTimeSeconds, 1.0f, 0.0f, 36000.0f);
				tiersChanged |= ImGui::DragFloat("Health Multiplier", &tier.multipliers.healthMultiplier, 0.01f, 0.0f, 100.0f);
				tiersChanged |= ImGui::DragFloat("Speed Multiplier", &tier.multipliers.speedMultiplier, 0.01f, 0.0f, 100.0f);
				tiersChanged |= ImGui::DragFloat("Experience Multiplier", &tier.multipliers.experienceMultiplier, 0.01f, 0.0f, 100.0f);
				if (ImGui::Button("Remove Scaling Tier")) {
					removeTierIndex = tierIndex;
				}
				ImGui::PopID();
			}
			if (removeTierIndex >= 0) {
				tiers.erase(tiers.begin() + removeTierIndex);
				tiersChanged = true;
			}
			if (tiersChanged) {
				enemySpawnPoint->SetTimeScalingTiers(tiers);
			}
		}

		ImGui::Separator();
		// 最終ボス専用イベントの時刻、種類、出現座標、プレイヤーワープ座標を編集する。
		if (ImGui::CollapsingHeader("Boss Encounter", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto bossSettings = enemySpawnPoint->GetBossEncounterSettings();
			bool bossSettingsChanged = false;
			bossSettingsChanged |= ImGui::Checkbox("Enable Boss Encounter", &bossSettings.enabled);
			if (ImGui::DragFloat("Boss Trigger Time (sec)", &bossSettings.triggerTimeSeconds, 0.1f, 0.0f, 36000.0f)) {
				bossSettings.triggerTimeSeconds = (std::max)(0.0f, bossSettings.triggerTimeSeconds);
				bossSettingsChanged = true;
			}

			if (!enemyTypes.empty()) {
				int bossEnemyTypeIndex = 0;
				for (int enemyTypeIndex = 0; enemyTypeIndex < static_cast<int>(enemyTypes.size()); ++enemyTypeIndex) {
					if (enemyTypes[enemyTypeIndex] == bossSettings.enemyTypeName) {
						bossEnemyTypeIndex = enemyTypeIndex;
						break;
					}
				}
				std::vector<const char*> bossEnemyTypeLabels = MakeLabelPointers(enemyTypes);
				if (ImGui::Combo("Boss Enemy Type", &bossEnemyTypeIndex, bossEnemyTypeLabels.data(), static_cast<int>(bossEnemyTypeLabels.size()))) {
					bossSettings.enemyTypeName = enemyTypes[bossEnemyTypeIndex];
					bossSettingsChanged = true;
				}
			}
			bossSettingsChanged |= ImGui::DragFloat3("Boss Position", &bossSettings.bossPosition.x, 0.1f);
			bossSettingsChanged |= ImGui::DragFloat3("Player Warp Position", &bossSettings.playerWarpPosition.x, 0.1f);
			const char* bossState = enemySpawnPoint->IsBossEncounterActive()
			    ? "Active (normal spawn timer paused)"
			    : enemySpawnPoint->IsBossEncounterTriggered() ? "Finished" : "Waiting";
			ImGui::Text("State: %s", bossState);
			ImGui::TextDisabled("The player is warped immediately before the boss appears.");
			if (bossSettingsChanged) {
				enemySpawnPoint->SetBossEncounterSettings(bossSettings);
			}
		}

		float outerMargin = enemySpawnPoint->GetOuterMargin();
		if (ImGui::DragFloat("Outside Margin", &outerMargin, 0.1f, 0.0f, 1000.0f)) {
			enemySpawnPoint->SetOuterMargin(outerMargin);
		}
		float minimumRadius = enemySpawnPoint->GetMinimumRadius();
		if (ImGui::DragFloat("Minimum Radius", &minimumRadius, 0.1f, 0.0f, 1000.0f)) {
			enemySpawnPoint->SetMinimumRadius(minimumRadius);
		}
		float groundY = enemySpawnPoint->GetGroundY();
		if (ImGui::DragFloat("Ground Y", &groundY, 0.1f, -1000.0f, 1000.0f)) {
			enemySpawnPoint->SetGroundY(groundY);
		}
		float pointHeight = enemySpawnPoint->GetPointHeight();
		if (ImGui::DragFloat("Point Height", &pointHeight, 0.05f, -1000.0f, 1000.0f)) {
			enemySpawnPoint->SetPointHeight(pointHeight);
		}
		bool drawDebug = enemySpawnPoint->GetDrawDebug();
		if (ImGui::Checkbox("Draw Spawn Debug", &drawDebug)) {
			enemySpawnPoint->SetDrawDebug(drawDebug);
		}
		float debugPointSize = enemySpawnPoint->GetDebugPointSize();
		if (ImGui::DragFloat("Debug Point Size", &debugPointSize, 0.05f, 0.01f, 100.0f)) {
			enemySpawnPoint->SetDebugPointSize(debugPointSize);
		}
		ImGui::Text("Spawn Points: %d", static_cast<int>(enemySpawnPoint->GetSpawnPoints().size()));
	}
#else
	(void)selectedObject;
#endif
}

/// <summary>
/// パーティクルエミッターコンポーネントの編集UIを描画します。
/// </summary>
void BaseScene::DrawEnemyInspector() {
#ifdef USE_IMGUI
	if (!ImGui::Begin("Enemy Inspector")) {
		ImGui::End();
		return;
	}
	if (selectedObjectIndex_ < 0 || selectedObjectIndex_ >= static_cast<int>(sceneObjects_.size())) {
		ImGui::Text("No selected enemy");
		ImGui::End();
		return;
	}

	GameObject* selectedObject = sceneObjects_[selectedObjectIndex_].get();
	EnemyComponent* enemy = selectedObject->GetComponent<EnemyComponent>();
	if (!enemy) {
		ImGui::Text("Selected object has no Enemy component");
		ImGui::End();
		return;
	}

	auto updateTypeNameBuffer = [this](const std::string& typeName) {
		std::fill(enemyTypeNameBuffer_.begin(), enemyTypeNameBuffer_.end(), '\0');
		const size_t copyLength = (std::min)(typeName.size(), enemyTypeNameBuffer_.size() - 1);
		std::memcpy(enemyTypeNameBuffer_.data(), typeName.data(), copyLength);
	};
	if (enemyInspectorObjectName_ != selectedObject->GetName()) {
		enemyInspectorObjectName_ = selectedObject->GetName();
		updateTypeNameBuffer(enemy->GetEnemyTypeName());
	}

	ImGui::Text("%s", selectedObject->GetName().c_str());
	bool isEnabled = enemy->IsEnabled();
	if (ImGui::Checkbox("Enabled", &isEnabled)) {
		enemy->SetEnabled(isEnabled);
	}
	const float maxHealth = enemy->GetStats().health;
	const float healthRate = maxHealth > 0.0f ? enemy->GetCurrentHealth() / maxHealth : 0.0f;
	ImGui::Text("Current Health: %.1f / %.1f", enemy->GetCurrentHealth(), maxHealth);
	ImGui::ProgressBar(healthRate, ImVec2(-1.0f, 0.0f));
	ImGui::Separator();

	std::vector<std::string> targetLabels = {"Auto First Player"};
	int currentTargetIndex = 0;
	for (const auto& object : sceneObjects_) {
		if (!object->GetComponent<Player>()) {
			continue;
		}
		targetLabels.push_back(object->GetName());
		if (enemy->GetTargetName() == object->GetName()) {
			currentTargetIndex = static_cast<int>(targetLabels.size()) - 1;
		}
	}
	std::vector<const char*> targetLabelPointers = MakeLabelPointers(targetLabels);
	if (ImGui::Combo("Target Player", &currentTargetIndex, targetLabelPointers.data(), static_cast<int>(targetLabelPointers.size()))) {
		enemy->SetTargetName(currentTargetIndex == 0 ? "" : targetLabels[currentTargetIndex]);
		enemy->SetTarget(nullptr);
	}

	std::vector<std::string> enemyTypes = LoadEnemyTypeNames();
	int currentEnemyTypeIndex = 0;
	for (int index = 0; index < static_cast<int>(enemyTypes.size()); ++index) {
		if (enemyTypes[index] == enemy->GetEnemyTypeName()) {
			currentEnemyTypeIndex = index;
			break;
		}
	}
	if (!enemyTypes.empty()) {
		std::vector<const char*> enemyTypeLabels = MakeLabelPointers(enemyTypes);
		if (ImGui::Combo("Enemy Type", &currentEnemyTypeIndex, enemyTypeLabels.data(), static_cast<int>(enemyTypeLabels.size()))) {
			const std::string& selectedTypeName = enemyTypes[currentEnemyTypeIndex];
			enemy->SetEnemyTypeName(selectedTypeName);
			const EnemyStats selectedStats = LoadEnemyStats(selectedTypeName);
			enemy->ApplyStats(selectedStats);
			const float enemyScale = 0.75f * selectedStats.sizeScale;
			selectedObject->GetTransform().scale = {enemyScale, enemyScale, enemyScale};
			updateTypeNameBuffer(selectedTypeName);
		}
	}

	EnemyStats stats = enemy->GetStats();
	bool statsChanged = false;
	statsChanged |= ImGui::DragFloat("Health", &stats.health, 0.1f, 0.0f, 10000.0f);
	statsChanged |= ImGui::DragFloat("Attack", &stats.attack, 0.1f, 0.0f, 10000.0f);
	statsChanged |= ImGui::DragFloat("Speed", &stats.speed, 0.001f, 0.0f, 100.0f);
	if (ImGui::DragFloat("Size Scale", &stats.sizeScale, 0.05f, 0.1f, 10.0f)) {
		const float enemyScale = 0.75f * stats.sizeScale;
		selectedObject->GetTransform().scale = {enemyScale, enemyScale, enemyScale};
		statsChanged = true;
	}
	const char* behaviorLabels[] = {
		"Chase", "Shooter", "Charger", "Night Slash Boss", "Self Destruct", "Tornado Boss", "Burst Shooter"};
	int behaviorIndex = static_cast<int>(stats.behavior);
	if (ImGui::Combo("Behavior", &behaviorIndex, behaviorLabels, 7)) {
		stats.behavior = static_cast<EnemyBehaviorType>(behaviorIndex);
		stats.shoots = stats.behavior == EnemyBehaviorType::Shooter;
		statsChanged = true;
	}
	if (stats.behavior == EnemyBehaviorType::Shooter || stats.behavior == EnemyBehaviorType::BurstShooter) {
		statsChanged |= ImGui::DragFloat("Shoot Interval", &stats.shootingInterval, 0.01f, 0.05f, 1000.0f);
		statsChanged |= ImGui::DragFloat("Preferred Distance", &stats.preferredDistance, 0.1f, 0.0f, 1000.0f);
		statsChanged |= ImGui::DragFloat("Distance Tolerance", &stats.distanceTolerance, 0.1f, 0.0f, 1000.0f);
		statsChanged |= ImGui::DragFloat("Projectile Speed", &stats.projectileSpeed, 0.005f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Projectile Size", &stats.projectileSize, 0.01f, 0.01f, 100.0f);
		statsChanged |= ImGui::DragFloat("Projectile Life", &stats.projectileLifeTime, 0.1f, 0.0f, 1000.0f);
		if (stats.behavior == EnemyBehaviorType::BurstShooter) {
			statsChanged |= ImGui::DragInt("Burst Shot Count", &stats.burstShotCount, 1.0f, 1, 31);
			statsChanged |= ImGui::SliderAngle("Burst Shot Spacing", &stats.burstSpreadAngle, 0.0f, 45.0f);
		}
	} else if (stats.behavior == EnemyBehaviorType::Charger) {
		statsChanged |= ImGui::DragFloat("Charge Trigger Distance", &stats.chargeTriggerDistance, 0.1f, 0.0f, 1000.0f);
		statsChanged |= ImGui::DragFloat("Charge Warning Time", &stats.chargeDuration, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Dash Speed", &stats.dashSpeed, 0.005f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Dash Duration", &stats.dashDuration, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Dash Recovery", &stats.dashRecovery, 0.05f, 0.0f, 100.0f);
	} else if (stats.behavior == EnemyBehaviorType::SelfDestruct) {
		statsChanged |= ImGui::DragFloat("Explosion Trigger Distance", &stats.selfDestructTriggerDistance, 0.1f, 0.0f, 1000.0f);
		statsChanged |= ImGui::DragFloat("Explosion Fuse Time", &stats.selfDestructFuseDuration, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Explosion Radius", &stats.selfDestructRadius, 0.1f, 0.0f, 1000.0f);
	} else if (stats.behavior == EnemyBehaviorType::TornadoBoss) {
		statsChanged |= ImGui::DragFloat("Tornado Trigger Distance", &stats.bossTornadoTriggerDistance, 0.1f, 0.0f, 1000.0f);
		statsChanged |= ImGui::DragFloat("Tornado Warning Time", &stats.bossTornadoWindup, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Tornado Recovery Time", &stats.bossTornadoRecovery, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragInt("Tornado Count", &stats.bossTornadoCount, 1.0f, 1, 16);
		statsChanged |= ImGui::DragFloat("Tornado Initial Radius", &stats.bossTornadoInitialRadius, 0.1f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Tornado Angular Speed", &stats.bossTornadoAngularSpeed, 0.1f, -20.0f, 20.0f);
		statsChanged |= ImGui::DragFloat("Tornado Radial Speed", &stats.bossTornadoRadialSpeed, 0.005f, 0.0f, 5.0f);
		statsChanged |= ImGui::DragFloat("Tornado Size", &stats.bossTornadoSize, 0.05f, 0.01f, 10.0f);
		statsChanged |= ImGui::DragFloat("Tornado Life Time", &stats.bossTornadoLifeTime, 0.1f, 0.0f, 30.0f);
		statsChanged |= ImGui::DragFloat("Tornado Attack Multiplier", &stats.bossTornadoAttackMultiplier, 0.05f, 0.0f, 10.0f);
		ImGui::SeparatorText("Converging Tornado Pattern");
		statsChanged |= ImGui::DragInt("Converging Tornado Count", &stats.bossConvergingTornadoCount, 1.0f, 1, 24);
		statsChanged |= ImGui::DragFloat("Converging Initial Radius", &stats.bossConvergingTornadoInitialRadius, 0.1f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Converging Angular Speed", &stats.bossConvergingTornadoAngularSpeed, 0.1f, -20.0f, 20.0f);
		statsChanged |= ImGui::DragFloat("Converging Radial Speed", &stats.bossConvergingTornadoRadialSpeed, 0.005f, 0.0f, 5.0f);
		statsChanged |= ImGui::DragFloat("Converging Tornado Size", &stats.bossConvergingTornadoSize, 0.05f, 0.01f, 10.0f);
		statsChanged |= ImGui::DragFloat("Converging Life Time", &stats.bossConvergingTornadoLifeTime, 0.1f, 0.0f, 30.0f);
		statsChanged |= ImGui::DragFloat("Converging Attack Multiplier", &stats.bossConvergingTornadoAttackMultiplier, 0.05f, 0.0f, 10.0f);
		ImGui::SeparatorText("Giant Homing Tornado Pattern");
		statsChanged |= ImGui::DragFloat("Giant Tornado Speed", &stats.bossGiantTornadoSpeed, 0.005f, 0.0f, 5.0f);
		statsChanged |= ImGui::DragFloat("Giant Tornado Size", &stats.bossGiantTornadoSize, 0.1f, 0.01f, 20.0f);
		statsChanged |= ImGui::DragFloat("Giant Tornado Life Time", &stats.bossGiantTornadoLifeTime, 0.1f, 0.0f, 30.0f);
		statsChanged |= ImGui::DragFloat("Giant Tornado Attack Multiplier", &stats.bossGiantTornadoAttackMultiplier, 0.05f, 0.0f, 10.0f);
		statsChanged |= ImGui::DragFloat("Giant Tornado Spawn Offset", &stats.bossGiantTornadoSpawnOffset, 0.1f, 0.0f, 100.0f);
	} else if (stats.behavior == EnemyBehaviorType::NightSlashBoss) {
		// 連続斬りの開始距離、予兆、移動、切り返し、フィニッシュを個別に調整する。
		statsChanged |= ImGui::DragFloat("Combo Trigger Distance", &stats.comboTriggerDistance, 0.1f, 0.0f, 1000.0f);
		statsChanged |= ImGui::DragFloat("Combo Warning Time", &stats.comboWindup, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Combo Dash Speed", &stats.comboDashSpeed, 0.005f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Combo Dash Duration", &stats.comboDashDuration, 0.01f, 0.01f, 100.0f);
		statsChanged |= ImGui::DragFloat("Slash Pause", &stats.comboSlashPause, 0.01f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Combo Recovery", &stats.comboRecovery, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Side Offset", &stats.comboSideOffset, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragInt("Dash Count", &stats.comboDashCount, 1.0f, 1, 12);
		statsChanged |= ImGui::DragFloat("Finisher Speed Multiplier", &stats.finisherSpeedMultiplier, 0.05f, 1.0f, 5.0f);
		// 連続斬り後に循環する全方位弾幕と扇状弾幕の共通パラメーター。
		ImGui::SeparatorText("Ranged Patterns");
		statsChanged |= ImGui::DragFloat("Ranged Warning Time", &stats.bossRangedWindup, 0.05f, 0.0f, 10.0f);
		statsChanged |= ImGui::DragFloat("Ranged Wave Interval", &stats.bossRangedInterval, 0.01f, 0.01f, 10.0f);
		statsChanged |= ImGui::DragInt("Ranged Wave Count", &stats.bossRangedWaves, 1.0f, 1, 20);
		statsChanged |= ImGui::DragInt("Radial Shot Count", &stats.bossRadialShotCount, 1.0f, 1, 64);
		statsChanged |= ImGui::DragInt("Aimed Fan Shot Count", &stats.bossAimedShotCount, 1.0f, 1, 31);
		statsChanged |= ImGui::SliderAngle("Aimed Shot Spacing", &stats.bossAimedSpreadAngle, 0.0f, 45.0f);
		statsChanged |= ImGui::DragFloat("Projectile Attack Multiplier", &stats.bossProjectileAttackMultiplier, 0.05f, 0.0f, 5.0f);
		statsChanged |= ImGui::DragFloat("Boss Projectile Speed", &stats.projectileSpeed, 0.005f, 0.0f, 5.0f);
		statsChanged |= ImGui::DragFloat("Boss Projectile Size", &stats.projectileSize, 0.01f, 0.01f, 5.0f);
		statsChanged |= ImGui::DragFloat("Boss Projectile Life Time", &stats.projectileLifeTime, 0.1f, 0.0f, 30.0f);
	}
	statsChanged |= ImGui::DragFloat("Spawns Per Minute", &stats.spawnsPerMinute, 0.1f, 0.0f, 10000.0f);
	statsChanged |= ImGui::DragInt("Drop Experience", &stats.experience, 1.0f, 0, 100000);
	statsChanged |= ImGui::SliderFloat("Health Item Drop Chance", &stats.healthItemDropChance, 0.0f, 1.0f, "%.2f");
	statsChanged |= ImGui::SliderFloat("EXP Collector Drop Chance", &stats.collectExperienceItemDropChance, 0.0f, 1.0f, "%.2f");
	statsChanged |= ImGui::DragFloat("Health Item Heal Amount", &stats.healthItemHealAmount, 1.0f, 0.0f, 100000.0f);

	const std::vector<std::string> experienceModels = CollectAllLoadedModelNames();
	if (!experienceModels.empty()) {
		int experienceModelIndex = 0;
		for (int index = 0; index < static_cast<int>(experienceModels.size()); ++index) {
			if (experienceModels[index] == stats.experienceModelFilePath) {
				experienceModelIndex = index;
				break;
			}
		}
		std::vector<std::string> experienceModelLabels;
		experienceModelLabels.reserve(experienceModels.size());
		for (const std::string& modelName : experienceModels) {
			Model* model = ModelManager::GetInstance()->FindModel(modelName);
			experienceModelLabels.push_back(model && model->GetIsAnimation() ? "[Anim] " + modelName : "[Model] " + modelName);
		}
		std::vector<const char*> experienceModelLabelPointers = MakeLabelPointers(experienceModelLabels);
		if (ImGui::Combo("Experience Model", &experienceModelIndex, experienceModelLabelPointers.data(), static_cast<int>(experienceModelLabelPointers.size()))) {
			stats.experienceModelFilePath = experienceModels[experienceModelIndex];
			statsChanged = true;
		}
	}
	if (statsChanged) {
		enemy->ApplyStats(stats);
	}

	float currentHealth = enemy->GetCurrentHealth();
	if (ImGui::DragFloat("Edit Current Health", &currentHealth, 0.1f, 0.0f, stats.health)) {
		enemy->SetCurrentHealth(currentHealth);
	}

	ImGui::Separator();
	ImGui::InputText("Status Name", enemyTypeNameBuffer_.data(), enemyTypeNameBuffer_.size());
	if (ImGui::Button("Save Enemy Type")) {
		const std::string saveTypeName = enemyTypeNameBuffer_.data();
		if (!saveTypeName.empty()) {
			enemy->SetEnemyTypeName(saveTypeName);
		}
		SaveEnemyStats(enemy->GetEnemyTypeName(), enemy->GetStats());
	}

	ImGui::End();
#endif
}

void BaseScene::DrawPlayerInspector() {
#ifdef USE_IMGUI
	if (!ImGui::Begin("Player Inspector")) {
		ImGui::End();
		return;
	}
	if (selectedObjectIndex_ < 0 || selectedObjectIndex_ >= static_cast<int>(sceneObjects_.size())) {
		ImGui::Text("No selected player");
		ImGui::End();
		return;
	}

	GameObject* selectedObject = sceneObjects_[selectedObjectIndex_].get();
	Player* player = selectedObject->GetComponent<Player>();
	if (!player) {
		ImGui::Text("Selected object has no Player component");
		ImGui::End();
		return;
	}

	ImGui::Text("%s", selectedObject->GetName().c_str());
	const float currentHealth = player->GetCurrentHealth();
	const float maxHealth = player->GetMaxHealth();
	const float healthRate = maxHealth > 0.0f ? currentHealth / maxHealth : 0.0f;
	ImGui::Text("Current Health: %.1f / %.1f", currentHealth, maxHealth);
	ImGui::ProgressBar(healthRate, ImVec2(-1.0f, 0.0f));
	ImGui::Separator();

	std::vector<std::string> playerTypes = LoadPlayerTypeNames();
	int currentPlayerTypeIndex = 0;
	for (int index = 0; index < static_cast<int>(playerTypes.size()); ++index) {
		if (playerTypes[index] == player->GetPlayerTypeName()) {
			currentPlayerTypeIndex = index;
			break;
		}
	}
	if (!playerTypes.empty()) {
		std::vector<const char*> playerTypeLabels = MakeLabelPointers(playerTypes);
		if (ImGui::Combo("Player Type", &currentPlayerTypeIndex, playerTypeLabels.data(), static_cast<int>(playerTypeLabels.size()))) {
			const std::string& selectedTypeName = playerTypes[currentPlayerTypeIndex];
			player->SetPlayerTypeName(selectedTypeName);
			std::fill(playerTypeNameBuffer_.begin(), playerTypeNameBuffer_.end(), '\0');
			const size_t copyLength = (std::min)(selectedTypeName.size(), playerTypeNameBuffer_.size() - 1);
			std::memcpy(playerTypeNameBuffer_.data(), selectedTypeName.data(), copyLength);
			PlayerStats stats = LoadPlayerStats(selectedTypeName);
			player->ApplyStats(stats, ApplyPlayerStatusItems(stats));
			selectedObject->GetTransform().scale = {stats.sizeScale, stats.sizeScale, stats.sizeScale};
			if (PlayerAttackComponent* attack = selectedObject->GetComponent<PlayerAttackComponent>()) {
				ApplyPlayerAttackSlots(attack, stats);
			}
			if (Object3dComponent* object3dComponent = selectedObject->GetComponent<Object3dComponent>()) {
				if (!stats.modelFilePath.empty() && ModelManager::GetInstance()->FindModel(stats.modelFilePath)) {
					object3dComponent->SetModel(stats.modelFilePath);
					object3dComponent->SetDrawSkeleton(stats.isAnimationModel);
				}
			}
		}
	}

	Vector3 spawnPoint = player->GetSpawnPoint();
	if (ImGui::DragFloat3("Spawn Point", &spawnPoint.x, 0.1f)) {
		player->SetSpawnPoint(spawnPoint);
	}
	if (ImGui::Button("Set Spawn To Current")) {
		player->SetSpawnPoint(selectedObject->GetTransform().translate);
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset To Spawn")) {
		player->ResetToSpawnPoint();
	}
	if (ImGui::Button("Save Spawn Point")) {
		SaveEditorObjects();
	}

	DrawPlayerStatsInspector(selectedObject, player);

	ImGui::End();
#endif
}

void BaseScene::DrawPlayerStatsInspector(GameObject* selectedObject, Player* player) {
#ifdef USE_IMGUI
	PlayerStats stats = player->GetBaseStats();
	bool statsChanged = false;
	bool statusSlotsChanged = false;
	if (playerTypeNameBuffer_[0] == '\0') {
		const std::string& currentTypeName = player->GetPlayerTypeName();
		const size_t copyLength = (std::min)(currentTypeName.size(), playerTypeNameBuffer_.size() - 1);
		std::memcpy(playerTypeNameBuffer_.data(), currentTypeName.data(), copyLength);
		playerTypeNameBuffer_[copyLength] = '\0';
	}
	ImGui::InputText("Player Type Name", playerTypeNameBuffer_.data(), playerTypeNameBuffer_.size());
	statsChanged |= ImGui::DragFloat("Base Health", &stats.baseHealth, 1.0f, 0.0f, 100000.0f);
	statsChanged |= ImGui::DragFloat("Health %", &stats.health, 1.0f, 0.0f, 100000.0f);
	statsChanged |= ImGui::DragFloat("Attack", &stats.attack, 1.0f, 0.0f, 100000.0f);
	statsChanged |= ImGui::DragFloat("Defense", &stats.defense, 1.0f, 0.0f, 10000.0f);
	statsChanged |= ImGui::DragFloat("Base Speed", &stats.baseSpeed, 0.001f, 0.0f, 100.0f);
	statsChanged |= ImGui::DragFloat("Speed %", &stats.speed, 1.0f, 0.0f, 100000.0f);
	statsChanged |= ImGui::DragFloat("Attack Speed", &stats.attackSpeed, 1.0f, 0.0f, 100000.0f);
	statsChanged |= ImGui::DragFloat("Attack Size", &stats.attackSize, 1.0f, 0.0f, 100000.0f);
	if (ImGui::DragFloat("Size Scale", &stats.sizeScale, 0.05f, 0.1f, 10.0f)) {
		selectedObject->GetTransform().scale = {stats.sizeScale, stats.sizeScale, stats.sizeScale};
		statsChanged = true;
	}
	statsChanged |= ImGui::DragFloat("Damage Invincibility (sec)", &stats.damageInvincibilityDuration, 0.05f, 0.0f, 60.0f);
	statsChanged |= ImGui::DragInt("Level", &stats.level, 1.0f, 1, 100000);
	statsChanged |= ImGui::DragInt("Experience", &stats.experience, 1.0f, 0, 100000000);
	const int nextLevelExperience = Player::GetRequiredExperienceForNextLevel(stats.level);
	if (nextLevelExperience == (std::numeric_limits<int>::max)()) {
		ImGui::Text("Next Level EXP: Max");
	} else {
		ImGui::Text("Next Level EXP: %d / %d", stats.experience, nextLevelExperience);
	}
	statsChanged |= ImGui::DragFloat("Experience Correction %", &stats.experienceCorrection, 1.0f, 0.0f, 100000.0f);
	ImGui::Text("Max Health: %.1f", stats.baseHealth * (stats.health / 100.0f));
	ImGui::Text("Effective Speed: %.3f", stats.baseSpeed * (stats.speed / 100.0f));
	float editedCurrentHealth = player->GetCurrentHealth();
	if (ImGui::DragFloat("Edit Current Health", &editedCurrentHealth, 1.0f, 0.0f, player->GetMaxHealth())) {
		player->SetCurrentHealth(editedCurrentHealth);
	}
	if (!isPlayerAttackCacheLoaded_) {
		ReloadPlayerAttackInspectorCache();
	}
	std::vector<std::string> attackLevels = GetPlayerAttackLevels();
	const std::vector<std::string>& attackNames = cachedPlayerAttackNames_;
	if (!attackNames.empty()) {
		std::vector<std::string> attackSlotLabels;
		attackSlotLabels.reserve(attackNames.size() + 1);
		attackSlotLabels.push_back("None");
		attackSlotLabels.insert(attackSlotLabels.end(), attackNames.begin(), attackNames.end());
		std::vector<const char*> attackLabels = MakeLabelPointers(attackSlotLabels);
		std::vector<const char*> attackLevelLabels = MakeLabelPointers(attackLevels);
		if (ImGui::CollapsingHeader("Attack Slots", ImGuiTreeNodeFlags_DefaultOpen)) {
			for (int slotIndex = 0; slotIndex < static_cast<int>(stats.attackSlots.size()); ++slotIndex) {
				ImGui::PushID(slotIndex);
				PlayerAttackSlot& slot = stats.attackSlots[slotIndex];
				ImGui::Separator();
				ImGui::Text("Slot %d", slotIndex + 1);
				if (ImGui::Checkbox("Enabled", &slot.enabled)) {
					statsChanged = true;
				}
				int attackIndex = 0;
				for (int index = 0; index < static_cast<int>(attackNames.size()); ++index) {
					if (attackNames[index] == slot.attackName) {
						attackIndex = index + 1;
						break;
					}
				}
				if (ImGui::Combo("Attack", &attackIndex, attackLabels.data(), static_cast<int>(attackLabels.size()))) {
					slot.attackName = attackIndex <= 0 ? "" : attackNames[attackIndex - 1];
					if (slot.attackName.empty()) {
						slot.enabled = false;
					}
					statsChanged = true;
				}
				int attackLevelIndex = 0;
				for (int index = 0; index < static_cast<int>(attackLevels.size()); ++index) {
					if (attackLevels[index] == slot.attackLevel) {
						attackLevelIndex = index;
						break;
					}
				}
				if (ImGui::Combo("Level", &attackLevelIndex, attackLevelLabels.data(), static_cast<int>(attackLevelLabels.size()))) {
					slot.attackLevel = attackLevels[attackLevelIndex];
					statsChanged = true;
				}
				ImGui::PopID();
			}
		}
	}

	if (ImGui::CollapsingHeader("Status Slots", ImGuiTreeNodeFlags_DefaultOpen)) {
		std::vector<std::string> statusItemNames = LoadPlayerStatusItemNames();
		std::vector<std::string> statusSlotLabels;
		statusSlotLabels.reserve(statusItemNames.size() + 1);
		statusSlotLabels.push_back("None");
		statusSlotLabels.insert(statusSlotLabels.end(), statusItemNames.begin(), statusItemNames.end());
		std::vector<const char*> statusLabels = MakeLabelPointers(statusSlotLabels);
		std::vector<std::string> statusLevels = GetPlayerStatusItemLevels();
		std::vector<const char*> statusLevelLabels = MakeLabelPointers(statusLevels);
		for (int slotIndex = 0; slotIndex < static_cast<int>(stats.statusSlots.size()); ++slotIndex) {
			ImGui::PushID(1000 + slotIndex);
			PlayerStatusSlot& slot = stats.statusSlots[slotIndex];
			ImGui::Separator();
			ImGui::Text("Status Slot %d", slotIndex + 1);
			if (ImGui::Checkbox("Enabled", &slot.enabled)) {
				if (slot.statusName.empty()) {
					slot.enabled = false;
				}
				statsChanged = true;
				statusSlotsChanged = true;
			}

			int statusIndex = 0;
			for (int index = 0; index < static_cast<int>(statusItemNames.size()); ++index) {
				if (statusItemNames[index] == slot.statusName) {
					statusIndex = index + 1;
					break;
				}
			}
			if (ImGui::Combo("Status Item", &statusIndex, statusLabels.data(), static_cast<int>(statusLabels.size()))) {
				slot.statusName = statusIndex <= 0 ? "" : statusItemNames[statusIndex - 1];
				if (slot.statusName.empty()) {
					slot.enabled = false;
				} else {
					slot.enabled = true;
				}
				statsChanged = true;
				statusSlotsChanged = true;
			}
			int statusLevelIndex = 0;
			for (int index = 0; index < static_cast<int>(statusLevels.size()); ++index) {
				if (statusLevels[index] == slot.level) {
					statusLevelIndex = index;
					break;
				}
			}
			if (ImGui::Combo("Level", &statusLevelIndex, statusLevelLabels.data(), static_cast<int>(statusLevelLabels.size()))) {
				slot.level = statusLevels[statusLevelIndex];
				if (!slot.statusName.empty()) {
					slot.enabled = true;
				}
				statsChanged = true;
				statusSlotsChanged = true;
			}
			ImGui::PopID();
		}
	}
	const PlayerStats appliedStatusStats = ApplyPlayerStatusItems(stats);
	if (ImGui::CollapsingHeader("Applied Status", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Base Health: %.1f", appliedStatusStats.baseHealth);
		ImGui::Text("Health %%: %.1f", appliedStatusStats.health);
		ImGui::Text("Max Health: %.1f", appliedStatusStats.baseHealth * (appliedStatusStats.health / 100.0f));
		ImGui::Text("Attack: %.1f", appliedStatusStats.attack);
		ImGui::Text("Defense: %.1f", appliedStatusStats.defense);
		ImGui::Text("Base Speed: %.3f", appliedStatusStats.baseSpeed);
		ImGui::Text("Speed %%: %.1f", appliedStatusStats.speed);
		ImGui::Text("Effective Speed: %.3f", appliedStatusStats.baseSpeed * (appliedStatusStats.speed / 100.0f));
		ImGui::Text("Attack Speed: %.1f", appliedStatusStats.attackSpeed);
		ImGui::Text("Attack Size: %.1f", appliedStatusStats.attackSize);
		ImGui::Text("Size Scale: %.2f", appliedStatusStats.sizeScale);
		ImGui::Text("Damage Invincibility: %.2f sec", appliedStatusStats.damageInvincibilityDuration);
		ImGui::Text("Experience Correction %%: %.1f", appliedStatusStats.experienceCorrection);
	}

	DrawPlayerPersistenceInspector(selectedObject, player, stats, statsChanged, statusSlotsChanged);
#else
	(void)selectedObject;
	(void)player;
#endif
}

void BaseScene::DrawPlayerPersistenceInspector(GameObject* selectedObject, Player* player, PlayerStats& stats, bool& statsChanged, bool statusSlotsChanged) {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("Status Item Settings")) {
		static int selectedStatusItemIndex = 0;
		static std::array<char, 128> statusItemNameBuffer{};
		static PlayerStatusItemStats editingStatusItemStats{};
		static std::string editingStatusItemName;
		static bool isEditingStatusItemLoaded = false;
		std::vector<std::string> statusItemNames = LoadPlayerStatusItemNames();
		if (selectedStatusItemIndex >= static_cast<int>(statusItemNames.size())) {
			selectedStatusItemIndex = 0;
		}
		std::vector<const char*> statusItemLabels = MakeLabelPointers(statusItemNames);
		if (!statusItemLabels.empty() && ImGui::Combo("Item", &selectedStatusItemIndex, statusItemLabels.data(), static_cast<int>(statusItemLabels.size()))) {
			std::fill(statusItemNameBuffer.begin(), statusItemNameBuffer.end(), '\0');
			isEditingStatusItemLoaded = false;
		}

		const std::string selectedItemName = statusItemNames.empty() ? "AttackUp" : statusItemNames[selectedStatusItemIndex];
		if (!isEditingStatusItemLoaded || editingStatusItemName != selectedItemName) {
			editingStatusItemStats = LoadPlayerStatusItemStats(selectedItemName);
			editingStatusItemName = selectedItemName;
			isEditingStatusItemLoaded = true;
		}
		if (statusItemNameBuffer[0] == '\0') {
			const size_t copyLength = (std::min)(editingStatusItemStats.name.size(), statusItemNameBuffer.size() - 1);
			std::memcpy(statusItemNameBuffer.data(), editingStatusItemStats.name.data(), copyLength);
			statusItemNameBuffer[copyLength] = '\0';
		}
		ImGui::InputText("Item Name", statusItemNameBuffer.data(), statusItemNameBuffer.size());

		const char* typeLabels[] = {"Attack", "HP", "AttackSpeed", "Speed", "Defense", "AttackSize", "Experience"};
		int typeIndex = static_cast<int>(editingStatusItemStats.type);
		if (ImGui::Combo("Type", &typeIndex, typeLabels, _countof(typeLabels))) {
			editingStatusItemStats.type = static_cast<PlayerStatusItemType>(typeIndex);
		}
		for (int levelIndex = 0; levelIndex < static_cast<int>(editingStatusItemStats.levelAmounts.size()); ++levelIndex) {
			// 効果量と同じレベル単位で、選択カードの文章・画像も編集する。
			ImGui::PushID(2000 + levelIndex);
			ImGui::DragFloat(("Lv" + std::to_string(levelIndex + 1) + " Amount").c_str(), &editingStatusItemStats.levelAmounts[levelIndex], 1.0f, 0.0f, 100000.0f);
			InputTextMultilineString(("Lv" + std::to_string(levelIndex + 1) + " Selection Text").c_str(), editingStatusItemStats.levelDescriptions[levelIndex]);
			SelectionTextureCombo(("Lv" + std::to_string(levelIndex + 1) + " Selection Texture").c_str(), editingStatusItemStats.levelTextureFilePaths[levelIndex]);
			ImGui::PopID();
		}
		if (ImGui::Button("Save Status Item")) {
			const std::string saveItemName = statusItemNameBuffer.data();
			editingStatusItemStats.name = saveItemName.empty() ? selectedItemName : saveItemName;
			SavePlayerStatusItemStats(editingStatusItemStats.name, editingStatusItemStats);
			// 保存直後の画像を右上HUDへ反映するため、ステータス画像キャッシュを破棄する。
			playerStatusSlotTextureKeys_.fill({});
			playerStatusSlotTexturePaths_.fill({});
			player->ApplyStats(stats, ApplyPlayerStatusItems(stats));
			std::fill(statusItemNameBuffer.begin(), statusItemNameBuffer.end(), '\0');
			isEditingStatusItemLoaded = false;
		}
	}

	const std::vector<std::string> loadedModels = CollectAllLoadedModelNames();
	if (!loadedModels.empty()) {
		// 登録済みモデルから選択し、設定値とシーン上の表示モデルを同時に更新する。
		int modelIndex = 0;
		for (int index = 0; index < static_cast<int>(loadedModels.size()); index++) {
			if (loadedModels[index] == stats.modelFilePath) {
				modelIndex = index;
				break;
			}
		}

		std::vector<std::string> modelLabels;
		modelLabels.reserve(loadedModels.size());
		for (const std::string& modelName : loadedModels) {
			Model* model = ModelManager::GetInstance()->FindModel(modelName);
			modelLabels.push_back(model && model->GetIsAnimation() ? "[Anim] " + modelName : "[Model] " + modelName);
		}
		std::vector<const char*> loadedModelLabels = MakeLabelPointers(modelLabels);
		if (ImGui::Combo("Player Model", &modelIndex, loadedModelLabels.data(), static_cast<int>(loadedModelLabels.size()))) {
			const std::string& modelFilePath = loadedModels[modelIndex];
			Model* model = ModelManager::GetInstance()->FindModel(modelFilePath);
			const bool isAnimationModel = model && model->GetIsAnimation();
			stats.modelFilePath = modelFilePath;
			stats.isAnimationModel = isAnimationModel;
			statsChanged = true;
			if (Object3dComponent* object3dComponent = selectedObject->GetComponent<Object3dComponent>()) {
				object3dComponent->SetModel(modelFilePath);
				object3dComponent->SetDrawSkeleton(isAnimationModel);
			}
		}
		ImGui::Text("Current Player Model: %s", stats.modelFilePath.empty() ? "None" : stats.modelFilePath.c_str());
	} else {
		ImGui::Text("No loaded models");
	}

	if (statsChanged) {
		stats.initialAttackName = stats.attackSlots[0].attackName;
		stats.initialAttackLevel = stats.attackSlots[0].attackLevel;
		// ステータススロットはJSONへ即時保存せず、現在シーン上のプレイヤーにだけ反映する。
		player->ApplyStats(stats, ApplyPlayerStatusItems(stats));
		if (PlayerAttackComponent* attack = selectedObject->GetComponent<PlayerAttackComponent>()) {
			ApplyPlayerAttackSlots(attack, stats);
		}
	}
	if (statusSlotsChanged) {
		ImGui::Text("Status slots applied to current player only.");
	}
	const std::string currentTypeName = player->GetPlayerTypeName();
	const std::string editedTypeName = playerTypeNameBuffer_.data();
	if (ImGui::Button("Save / Rename Player Type")) {
		if (editedTypeName.empty()) {
			playerTypeEditMessage_ = "Player type name cannot be empty.";
		} else {
			PlayerStats saveStats = player->GetBaseStats();
			saveStats.name = editedTypeName;
			player->ApplyStats(saveStats, ApplyPlayerStatusItems(saveStats));
			if (editedTypeName == currentTypeName) {
				// 名前が同じ場合は能力値や使用モデルなどの編集内容だけを保存する。
				SavePlayerStats(currentTypeName, saveStats);
				playerTypeEditMessage_ = "Saved player type: " + currentTypeName;
			} else if (RenamePlayerStats(currentTypeName, editedTypeName, saveStats)) {
				// シーン内にある同タイプのプレイヤー参照も新しい名前へ揃える。
				for (const auto& object : sceneObjects_) {
					if (Player* scenePlayer = object->GetComponent<Player>();
					    scenePlayer && scenePlayer->GetPlayerTypeName() == currentTypeName) {
						scenePlayer->SetPlayerTypeName(editedTypeName);
					}
				}
				playerTypeEditMessage_ = "Renamed player type to: " + editedTypeName;
			} else {
				playerTypeEditMessage_ = "Rename failed. The new name may already exist.";
			}
		}
	}
	ImGui::SameLine();
	// Default は設定ファイル欠損時にも使う基準タイプなので削除させない。
	const bool canDeletePlayerType = currentTypeName != "Default";
	if (!canDeletePlayerType) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Delete Player Type")) {
		ImGui::OpenPopup("Delete Player Type?");
	}
	if (!canDeletePlayerType) {
		ImGui::EndDisabled();
	}
	if (ImGui::BeginPopupModal("Delete Player Type?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Delete \"%s\"?", currentTypeName.c_str());
		ImGui::TextDisabled("Players using it will be changed to Default.");
		if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f))) {
			if (DeletePlayerStats(currentTypeName)) {
				// 削除したタイプへの参照を残さないよう、対象プレイヤーへDefault設定を適用する。
				const PlayerStats defaultStats = LoadPlayerStats("Default");
				for (const auto& object : sceneObjects_) {
					Player* scenePlayer = object->GetComponent<Player>();
					if (!scenePlayer || scenePlayer->GetPlayerTypeName() != currentTypeName) {
						continue;
					}
					scenePlayer->SetPlayerTypeName("Default");
					scenePlayer->ApplyStats(defaultStats, ApplyPlayerStatusItems(defaultStats));
					if (PlayerAttackComponent* attack = object->GetComponent<PlayerAttackComponent>()) {
						ApplyPlayerAttackSlots(attack, defaultStats);
					}
					if (Object3dComponent* object3d = object->GetComponent<Object3dComponent>()) {
						if (!defaultStats.modelFilePath.empty() &&
						    !ModelManager::GetInstance()->FindModel(defaultStats.modelFilePath)) {
							ModelManager::GetInstance()->LoadModel(defaultStats.modelFilePath);
						}
						if (Model* model = ModelManager::GetInstance()->FindModel(defaultStats.modelFilePath)) {
							object3d->SetModel(defaultStats.modelFilePath);
							object3d->SetDrawSkeleton(model->GetIsAnimation());
						}
					}
				}
				std::fill(playerTypeNameBuffer_.begin(), playerTypeNameBuffer_.end(), '\0');
				const std::string defaultTypeName = "Default";
				std::memcpy(playerTypeNameBuffer_.data(), defaultTypeName.data(), defaultTypeName.size());
				selectedPlayerTypeIndex_ = 0;
				playerTypeEditMessage_ = "Deleted player type: " + currentTypeName;
			} else {
				playerTypeEditMessage_ = "Delete failed.";
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	if (!playerTypeEditMessage_.empty()) {
		ImGui::TextWrapped("%s", playerTypeEditMessage_.c_str());
	}

#else
	(void)selectedObject;
	(void)player;
	(void)stats;
	(void)statsChanged;
	(void)statusSlotsChanged;
#endif
}

void BaseScene::ReloadPlayerAttackInspectorCache() {
	cachedPlayerAttackNames_ = LoadPlayerAttackNames();
	if (cachedPlayerAttackNames_.empty()) {
		cachedPlayerAttackNames_.push_back("Straight");
	}
	cachedPlayerAttackStats_.clear();
	cachedPlayerAttackStats_.reserve(cachedPlayerAttackNames_.size());
	for (const std::string& attackName : cachedPlayerAttackNames_) {
		cachedPlayerAttackStats_.push_back(LoadPlayerAttackStats(attackName));
	}
	if (selectedPlayerAttackTypeIndex_ >= static_cast<int>(cachedPlayerAttackNames_.size())) {
		selectedPlayerAttackTypeIndex_ = 0;
	}
	std::fill(playerAttackNameBuffer_.begin(), playerAttackNameBuffer_.end(), '\0');
	isPlayerAttackCacheLoaded_ = true;
}

PlayerAttackStats* BaseScene::FindCachedPlayerAttackStats(const std::string& attackName) {
	if (!isPlayerAttackCacheLoaded_) {
		ReloadPlayerAttackInspectorCache();
	}
	const std::string targetName = attackName.empty() ? "Straight" : attackName;
	for (PlayerAttackStats& stats : cachedPlayerAttackStats_) {
		if (stats.name == targetName) {
			return &stats;
		}
	}
	return nullptr;
}

void BaseScene::DrawPlayerAttackInspector() {
#ifdef USE_IMGUI
	if (!ImGui::Begin("Player Attack Inspector")) {
		ImGui::End();
		return;
	}

	if (!isPlayerAttackCacheLoaded_) {
		ReloadPlayerAttackInspectorCache();
	}
	if (ImGui::Button("Load")) {
		ReloadPlayerAttackInspectorCache();
	}
	ImGui::SameLine();
	ImGui::Text("Source: %s", kPlayerAttackStatusFilePath);

	if (selectedPlayerAttackTypeIndex_ >= static_cast<int>(cachedPlayerAttackNames_.size())) {
		selectedPlayerAttackTypeIndex_ = 0;
	}

	std::vector<const char*> attackLabels = MakeLabelPointers(cachedPlayerAttackNames_);
	if (ImGui::Combo("Attack Type", &selectedPlayerAttackTypeIndex_, attackLabels.data(), static_cast<int>(attackLabels.size()))) {
		std::fill(playerAttackNameBuffer_.begin(), playerAttackNameBuffer_.end(), '\0');
	}
	PlayerAttackStats* attackStats = selectedPlayerAttackTypeIndex_ < static_cast<int>(cachedPlayerAttackStats_.size()) ? &cachedPlayerAttackStats_[selectedPlayerAttackTypeIndex_] : nullptr;
	if (!attackStats) {
		ImGui::Text("No attack status");
		ImGui::End();
		return;
	}

	if (playerAttackNameBuffer_[0] == '\0') {
		const size_t copyLength = (std::min)(attackStats->name.size(), playerAttackNameBuffer_.size() - 1);
		std::memcpy(playerAttackNameBuffer_.data(), attackStats->name.data(), copyLength);
		playerAttackNameBuffer_[copyLength] = '\0';
	}
	ImGui::InputText("Attack Name", playerAttackNameBuffer_.data(), playerAttackNameBuffer_.size());

	std::vector<std::string> levelNames = GetPlayerAttackLevels();
	if (selectedPlayerAttackLevelIndex_ >= static_cast<int>(levelNames.size())) {
		selectedPlayerAttackLevelIndex_ = 0;
	}
	std::vector<const char*> levelLabels = MakeLabelPointers(levelNames);
	ImGui::Combo("Level", &selectedPlayerAttackLevelIndex_, levelLabels.data(), static_cast<int>(levelLabels.size()));

	PlayerAttackLevelStats* selectedLevel = nullptr;
	for (PlayerAttackLevelStats& levelStats : attackStats->levels) {
		if (levelStats.level == levelNames[selectedPlayerAttackLevelIndex_]) {
			selectedLevel = &levelStats;
			break;
		}
	}
	if (!selectedLevel) {
		attackStats->levels.push_back(MakeDefaultPlayerAttackLevelStats(levelNames[selectedPlayerAttackLevelIndex_]));
		selectedLevel = &attackStats->levels.back();
	}

	bool changed = false;
	if (ImGui::CollapsingHeader("Super Condition", ImGuiTreeNodeFlags_DefaultOpen)) {
		std::vector<std::string> statusItemNames = LoadPlayerStatusItemNames();
		std::vector<std::string> statusItemLabels;
		statusItemLabels.reserve(statusItemNames.size() + 1);
		statusItemLabels.push_back("None");
		statusItemLabels.insert(statusItemLabels.end(), statusItemNames.begin(), statusItemNames.end());
		int statusItemIndex = 0;
		for (int index = 0; index < static_cast<int>(statusItemNames.size()); ++index) {
			if (statusItemNames[index] == attackStats->superConditionStatusName) {
				statusItemIndex = index + 1;
				break;
			}
		}
		std::vector<const char*> statusItemLabelPointers = MakeLabelPointers(statusItemLabels);
		if (ImGui::Combo("Required Status Item", &statusItemIndex, statusItemLabelPointers.data(), static_cast<int>(statusItemLabelPointers.size()))) {
			attackStats->superConditionStatusName = statusItemIndex == 0 ? "" : statusItemNames[statusItemIndex - 1];
			changed = true;
		}

		std::vector<std::string> statusLevels = GetPlayerStatusItemLevels();
		int statusLevelIndex = 0;
		for (int index = 0; index < static_cast<int>(statusLevels.size()); ++index) {
			if (statusLevels[index] == attackStats->superConditionStatusLevel) {
				statusLevelIndex = index;
				break;
			}
		}
		std::vector<const char*> statusLevelLabels = MakeLabelPointers(statusLevels);
		if (ImGui::Combo("Required Status Level", &statusLevelIndex, statusLevelLabels.data(), static_cast<int>(statusLevelLabels.size()))) {
			attackStats->superConditionStatusLevel = statusLevels[statusLevelIndex];
			changed = true;
		}
		if (attackStats->superConditionStatusName.empty()) {
			ImGui::TextDisabled("Super promotion is disabled.");
		} else {
			ImGui::TextWrapped("Uses the super level when the player equips %s at level %s or higher.", attackStats->superConditionStatusName.c_str(), attackStats->superConditionStatusLevel.c_str());
		}
	}
	ImGui::Separator();
	changed |= InputTextMultilineString("Selection Text", selectedLevel->choiceDescription);
	if (selectedLevel->level == "super") {
		// Superは見た目を区別できるよう、通常レベルとは別の画像を保持する。
		changed |= SelectionTextureCombo("Super Selection Texture", selectedLevel->choiceTextureFilePath);
	} else {
		// Lv1～5は攻撃ごとの共通画像を編集し、レベル変更による画像差を作らない。
		changed |= SelectionTextureCombo("Selection Texture (Lv1-5)", attackStats->choiceTextureFilePath);
	}
	ImGui::TextDisabled("Shown when this level is offered. Empty text uses the default description.");
	changed |= ImGui::DragFloat("Attack", &selectedLevel->attack, 1.0f, 0.0f, 100000.0f);
	changed |= ImGui::DragFloat("Speed", &selectedLevel->speed, 0.01f, 0.0f, 100.0f);
	changed |= ImGui::DragFloat("Size %", &selectedLevel->size, 1.0f, 0.0f, 100000.0f);
	changed |= ImGui::DragInt("Shot Count", &selectedLevel->shotCount, 1.0f, 1, 32);
	// 弾数を変更した直後も、角度と発射位置を同じ弾番号で編集できるよう配列数を同期する。
	while (static_cast<int>(selectedLevel->angles.size()) < selectedLevel->shotCount) {
		selectedLevel->angles.push_back(0.0f);
	}
	while (static_cast<int>(selectedLevel->angles.size()) > selectedLevel->shotCount) {
		selectedLevel->angles.pop_back();
	}
	while (static_cast<int>(selectedLevel->spawnOffsets.size()) < selectedLevel->shotCount) {
		selectedLevel->spawnOffsets.push_back(
		    selectedLevel->spawnOffsets.empty() ? Vector3{0.0f, 0.5f, 1.2f} : selectedLevel->spawnOffsets.back());
	}
	while (static_cast<int>(selectedLevel->spawnOffsets.size()) > selectedLevel->shotCount) {
		selectedLevel->spawnOffsets.pop_back();
	}
	for (int index = 0; index < selectedLevel->shotCount; ++index) {
		// 角度と位置を弾ごとに隣接表示し、同じ番号の設定であることを分かりやすくする。
		std::string label = "Shot Angle " + std::to_string(index + 1);
		changed |= ImGui::DragFloat(label.c_str(), &selectedLevel->angles[index], 1.0f, -180.0f, 180.0f);
		label = "Shot " + std::to_string(index + 1) + " Spawn Offset";
		changed |= ImGui::DragFloat3(label.c_str(), &selectedLevel->spawnOffsets[index].x, 0.05f, -100.0f, 100.0f);
	}
	ImGui::TextDisabled("Local position: X = right, Y = up, Z = forward");

	const std::vector<std::string> loadedModels = CollectAllLoadedModelNames();
	if (!loadedModels.empty()) {
		int modelIndex = 0;
		for (int index = 0; index < static_cast<int>(loadedModels.size()); ++index) {
			if (loadedModels[index] == selectedLevel->modelFilePath) {
				modelIndex = index;
				break;
			}
		}
		std::vector<const char*> modelLabels = MakeLabelPointers(loadedModels);
		if (ImGui::Combo("Attack Model", &modelIndex, modelLabels.data(), static_cast<int>(modelLabels.size()))) {
			selectedLevel->modelFilePath = loadedModels[modelIndex];
			changed = true;
		}
	} else {
		ImGui::Text("No loaded models");
	}
	changed |= ImGui::Checkbox("Homing", &selectedLevel->homing);
	changed |= ImGui::SliderFloat("Homing Accuracy", &selectedLevel->homingAccuracy, 0.0f, 1.0f);
	changed |= ImGui::DragFloat("Attack Interval", &selectedLevel->attackInterval, 0.01f, 0.01f, 100.0f);
	changed |= ImGui::DragFloat("Life Time", &selectedLevel->lifeTime, 0.01f, 0.01f, 100.0f);
	changed |= ImGui::DragFloat("Travel Distance", &selectedLevel->travelDistance, 0.1f, 0.1f, 1000.0f);
	changed |= ImGui::DragInt("Pierce Count", &selectedLevel->pierceCount, 1.0f, 0, 1000);
	changed |= ImGui::Checkbox("Infinite Pierce", &selectedLevel->infinitePierce);

	const std::string currentAttackName = cachedPlayerAttackNames_[selectedPlayerAttackTypeIndex_];
	if (changed) {
		attackStats->name = currentAttackName;
		for (const auto& object : sceneObjects_) {
			PlayerAttackComponent* attack = object->GetComponent<PlayerAttackComponent>();
			if (attack) {
				attack->UpdateAttackStatsByName(currentAttackName, *attackStats);
			}
		}
	}
	if (ImGui::Button("Save Attack Type")) {
		const std::string saveName = playerAttackNameBuffer_.data();
		const std::string finalName = saveName.empty() ? currentAttackName : saveName;
		attackStats->name = finalName;
		SavePlayerAttackStats(finalName, *attackStats);
		playerAttackSlotTextureKeys_.fill({});
		playerAttackSlotTexturePaths_.fill({});
		cachedPlayerAttackNames_[selectedPlayerAttackTypeIndex_] = finalName;
		std::fill(playerAttackNameBuffer_.begin(), playerAttackNameBuffer_.end(), '\0');
	}

	ImGui::End();
#endif
}

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
		atmosphere.sunDirection = NormalizeReturnVector(atmosphere.sunDirection);
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

