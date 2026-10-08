#include "GamePlayScene.h"
#include "SceneManager.h"
#include <cstring>
#include <model/ModelManager.h>
#include "instancing/InstancingModelCommon.h"
#include "GameTime.h"
#include <Xinput.h>
#include <cmath>

namespace {
#ifdef USE_IMGUI
ImVec2 GetPrimaryWorkPos() {
	const ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
	if (platformIO.Monitors.Size > 0) {
		return platformIO.Monitors[0].WorkPos;
	}
	return ImVec2(0.0f, 0.0f);
}

ImVec2 GetPrimaryWorkSize() {
	const ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
	if (platformIO.Monitors.Size > 0) {
		return platformIO.Monitors[0].WorkSize;
	}
	return ImVec2(1280.0f, 720.0f);
}

float ClampFloat(float value, float minValue, float maxValue) {
	if (maxValue < minValue) {
		return minValue;
	}
	if (value < minValue) {
		return minValue;
	}
	if (value > maxValue) {
		return maxValue;
	}
	return value;
}

float ClampLayoutValue(float value, float minValue, float maxValue) {
	if (maxValue < minValue) {
		return maxValue;
	}
	if (value < minValue) {
		return minValue;
	}
	if (value > maxValue) {
		return maxValue;
	}
	return value;
}

ImVec2 ClampWindowPosToWorkArea(const ImVec2& pos, const ImVec2& size) {
	const ImVec2 workPos = GetPrimaryWorkPos();
	const ImVec2 workSize = GetPrimaryWorkSize();
	const float maxX = workPos.x + workSize.x - size.x;
	const float maxY = workPos.y + workSize.y - size.y;
	return ImVec2(
	    ClampFloat(pos.x, workPos.x, maxX),
	    ClampFloat(pos.y, workPos.y, maxY)
	);
}

struct EditorLayout {
	ImVec2 hierarchyPos;
	ImVec2 hierarchySize;
	ImVec2 inspectorPos;
	ImVec2 inspectorSize;
	ImVec2 particlePos;
	ImVec2 particleSize;
};

/// <summary>
/// EditorLayout を生成して返します。
/// </summary>
EditorLayout MakeEditorLayout() {
	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	const float width = displaySize.x > 0.0f ? displaySize.x : 1280.0f;
	const float height = displaySize.y > 0.0f ? displaySize.y : 720.0f;
	const float leftWidth = ClampLayoutValue(width * 0.18f, 140.0f, width * 0.35f);
	const float rightWidth = ClampLayoutValue(width * 0.24f, 180.0f, width * 0.40f);
	const float bottomHeight = ClampLayoutValue(height * 0.24f, 120.0f, height * 0.40f);
	const float centerWidth = width - leftWidth - rightWidth;
	const float mainHeight = height - bottomHeight;

	EditorLayout layout{};
	layout.hierarchyPos = ImVec2(0.0f, 0.0f);
	layout.hierarchySize = ImVec2(leftWidth, mainHeight > 1.0f ? mainHeight : 1.0f);
	layout.inspectorPos = ImVec2(width - rightWidth, 0.0f);
	layout.inspectorSize = ImVec2(rightWidth, height);
	layout.particlePos = ImVec2(leftWidth, mainHeight);
	layout.particleSize = ImVec2(centerWidth > 1.0f ? centerWidth : 1.0f, bottomHeight);
	return layout;
}

bool BeginEditorPanel(const char* name, const ImVec2& pos, const ImVec2& size) {
#ifdef IMGUI_HAS_DOCK
	(void)pos;
	(void)size;
	return ImGui::Begin(name);
#else
	const ImGuiWindowFlags flags =
	    ImGuiWindowFlags_NoMove |
	    ImGuiWindowFlags_NoCollapse |
	    ImGuiWindowFlags_NoSavedSettings;

	ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(size, ImGuiCond_Always);
	return ImGui::Begin(name, nullptr, flags);
#endif
}
#endif

std::unique_ptr<Sprite> CreatePauseColorSprite() {
	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize("Resources/human/white.png");
	return sprite;
}

std::unique_ptr<GameObject> CreatePauseTextObject(const std::string& text, float fontSize) {
	auto object = std::make_unique<GameObject>();
	TextComponent* textComponent = object->AddComponent<TextComponent>();
	textComponent->SetText(text);
	textComponent->SetFontSize(fontSize);
	textComponent->SetAnchor(TextComponent::Anchor::Center);
	return object;
}

void DrawPauseColorSprite(Sprite* sprite, float x, float y, float width, float height, const Vector4& color) {
	EulerTransform transform = sprite->GetTransform();
	transform.translate = {x, y, 0.0f};
	sprite->SetTransform(transform);
	sprite->SetSize({width, height});
	sprite->SetColor(color);
	sprite->Update();
	sprite->Draw();
}
}

/// <summary>
/// 必要なリソースを準備し、オブジェクトを初期化します。
/// </summary>
void GamePlayScene::Initialize() {
	LoadSceneModels();
	InitializePauseMenu();

	std::vector<std::string> allTextures = {
			"Resources/circle.png",
			"Resources/uvChecker.png",
			"Resources/rostock_laage_airport_4k.dds",
			"Resources/gradationLine.png",
			"Resources/terrain/grass_replacement.png",
			"Resources/sand/sand.png"
	};

	availableTextures_.clear();

	for (const auto& path : allTextures) {
		TextureManager::GetInstance()->LoadTexture(path);

		if (path.find(".png") != std::string::npos) {
			availableTextures_.push_back(path);
		}
	}

	sprite = nullptr;
	object3d = nullptr;
	sphereObject = nullptr;
	cylinderObject = nullptr;
	currentParticleIndex_ = 0;
	ParticleManager::GetInstance()->ClearGroups();
}

/// <summary>
/// SceneModels を読み込み、内部データへ反映します。
/// </summary>
void GamePlayScene::LoadSceneModels() {
	ModelManager::GetInstance()->LoadModel("walk.gltf", true, "/human");
	ModelManager::GetInstance()->LoadModel("miko.gltf", true, "/human");
	ModelManager::GetInstance()->LoadModel("neko.gltf", true, "/cat");
	// 烏天狗とステージ2ボスで共有する歩行アニメーション付きモデル。
	ModelManager::GetInstance()->LoadModel("doppelganger.gltf", true, "/doppelganger");
	ModelManager::GetInstance()->LoadModel("enemy_drop.obj");
	ModelManager::GetInstance()->LoadModel("sphere.obj");
	ModelManager::GetInstance()->LoadModel("terrain.obj", false, "/terrain");
	ModelManager::GetInstance()->LoadModel("sand.obj", false, "/sand");
	// 烏天狗の右手へ装備する刀を専用フォルダから事前読み込みする。
	ModelManager::GetInstance()->LoadModel("brade.obj", false, "/brade");
	ModelManager::GetInstance()->LoadModel("grass.obj");

	// 草の板ポリゴンをインスタンシング描画用に一度だけ配置する。
	// grass.obj の原点は地面側にあるため、translate.y = 0 で地面に接する。
	Model* grassModel = ModelManager::GetInstance()->FindModel("grass.obj");
	if (grassModel) {
		// 草原の広さを維持したまま、従来の約5倍の密度にする。
		constexpr int kGrassSide = 537;
		constexpr int kGrassInstanceCount = kGrassSide * kGrassSide;
		constexpr float kSpacing = 0.1427f;
		instancingModel_ = std::make_unique<InstancingModel>();
		instancingModel_->Initialize(grassModel, kGrassInstanceCount);

		const float offset = (kGrassSide - 1) * kSpacing * 0.5f;
		for (int z = 0; z < kGrassSide; ++z) {
			for (int x = 0; x < kGrassSide; ++x) {
				EulerTransform transform{};
				// 格子を少し崩し、人工的な並びに見えないようにする。
				const float jitterX = std::sin(x * 12.9898f + z * 78.233f) * 0.05f;
				const float jitterZ = std::sin(x * 39.3467f + z * 11.135f) * 0.05f;
				transform.translate = {
					 x * kSpacing - offset + jitterX,
					 0.0f,
					 z * kSpacing - offset + jitterZ
				};
				transform.rotate = {0.0f, std::sin(x * 4.1f + z * 7.3f) * 3.14159265f, 0.0f};
				const float scale = 0.65f + (std::sin(x * 3.7f + z * 5.9f) * 0.10f + 0.10f);
				transform.scale = {scale, scale, scale};
				instancingModel_->AddInstance(transform);
			}
		}
	}
}

/// <summary>
/// 毎フレームの状態更新を行います。
/// </summary>
void GamePlayScene::Update() {
	// クリアと死亡のどちらでも、シーン破棄前に最終戦績を確定してリザルトへ渡す。
	if (IsStageCleared() || IsPlayerDefeated()) {
		// 終了処理の多重実行を避けるため、結果を確定するフレームで入力モードも終了へ移す。
		SetGameplayMode(GameplayMode::Finished);
		const StageResultData resultData = GetStageResultData();
		if (resultData.stageCleared) {
			// クリア時だけ履歴と新規開放を記録する。死亡終了はクリア条件を満たさない。
			sceneManager->RecordGameplayStageClear(sceneManager->GetSelectedGameplayStageId());
		}
		// Gは取得時点で所持金へ保存済みなので、ここではリザルト表示用の戦績だけを渡す。
		// 終了時にAddMoneyを呼ばないことで、取得時と終了時の二重加算を防止する。
		sceneManager->SetStageResultData(resultData);
		sceneManager->ChangeScene("RESULT");
		return;
	}
	if (IsLevelUpSelectionActive()) {
		ImGuiUpdate();
		return;
	}
	Input* input = Input::GetInstance();
	if (GetGameplayMode() == GameplayMode::Paused) {
		UpdatePauseMenu();
		ImGuiUpdate();
		return;
	}
	if (input->TriggerKey(DIK_ESCAPE) || input->TriggerGamepadButton(XINPUT_GAMEPAD_START)) {
		SetPauseMenuOpen(true);
		ImGuiUpdate();
		return;
	}
	if (skyBox) {
		skyBox->Update();
	}

	const float deltaTime = GameTime::GetDeltaTime();

	for (auto& emitter : emitters_) {
		emitter->Update(deltaTime);
	}
	Object3dCommon::GetInstance()->GetDefaultCamera()->Update();
	if (object3dObject_) {
		object3dObject_->Update();
	}

	if (cylinderGameObject_) {
		cylinderGameObject_->Update();
	}
	if (spriteObject_) {
		spriteObject_->Update();
	}
	for (auto& spriteObject : spriteObjects_) {
		spriteObject->Update();
	}
	for (auto& s : sprites) {
		s->SetSize({ 100.0f, 100.0f });
	}
	if (sphereGameObject_) {
		sphereGameObject_->Update();
	}
	if (Input::GetInstance()->TriggerKey(DIK_A)) {
		Vector3 targetPos = { 0.0f, -5.0f, 10.0f };

		for (auto& emitter : emitters_) {
			if (emitter->GetGroupName() == "Lightning") {
				emitter->SetTranslate({ 0.0f, 10.0f, 10.0f });
				emitter->EmitLightning(targetPos);
			}
		}
	}
	if (Input::GetInstance()->TriggerKey(DIK_S)) {
		for (auto& emitter : emitters_) {
			if (emitter->GetGroupName() == "Slash") {
				emitter->SetTranslate({0.0f, 2.0f, 5.0f});
				emitter->Emit();
			}
		}
	}
	ImGuiUpdate();
}

void GamePlayScene::InitializePauseMenu() {
	SetGameplayMode(GameplayMode::Playing);
	selectedPauseMenuItem_ = static_cast<int>(PauseMenuItem::Resume);
	pauseOverlaySprite_ = CreatePauseColorSprite();
	pausePanelSprite_ = CreatePauseColorSprite();
	pauseSelectionSprite_ = CreatePauseColorSprite();
	pauseTitleTextObject_ = CreatePauseTextObject("PAUSE", 52.0f);
	pauseMenuTextObjects_.clear();
	pauseMenuTextObjects_.push_back(CreatePauseTextObject("ゲームに戻る", 29.0f));
	pauseMenuTextObjects_.push_back(CreatePauseTextObject("ステージをやり直す", 29.0f));
	pauseMenuTextObjects_.push_back(CreatePauseTextObject("ステージ選択へ戻る", 29.0f));
	pauseInstructionTextObject_ = CreatePauseTextObject(
	    "W / S・上下キー・Pad上下: 選択    Space / Enter・Pad A: 決定    Esc・Pad Start / B: 戻る", 17.0f);
}

void GamePlayScene::SetPauseMenuOpen(bool isOpen) {
	if (isOpen) {
		selectedPauseMenuItem_ = static_cast<int>(PauseMenuItem::Resume);
	}
	// ポーズ状態も共通GameplayModeへ集約し、レベルアップ画面との同時成立を防ぐ。
	SetGameplayMode(isOpen ? GameplayMode::Paused : GameplayMode::Playing);
}

void GamePlayScene::UpdatePauseMenu() {
	Input* input = Input::GetInstance();
	const int itemCount = static_cast<int>(PauseMenuItem::Count);
	if (input->TriggerKey(DIK_W) || input->TriggerKey(DIK_UP) || input->TriggerGamepadUp()) {
		selectedPauseMenuItem_ = (selectedPauseMenuItem_ + itemCount - 1) % itemCount;
	}
	if (input->TriggerKey(DIK_S) || input->TriggerKey(DIK_DOWN) || input->TriggerGamepadDown()) {
		selectedPauseMenuItem_ = (selectedPauseMenuItem_ + 1) % itemCount;
	}
	if (input->TriggerKey(DIK_ESCAPE) || input->TriggerGamepadButton(XINPUT_GAMEPAD_START) ||
	    input->TriggerGamepadButton(XINPUT_GAMEPAD_B)) {
		SetPauseMenuOpen(false);
		return;
	}
	if (!input->TriggerKey(DIK_SPACE) && !input->TriggerKey(DIK_RETURN) &&
	    !input->TriggerGamepadButton(XINPUT_GAMEPAD_A)) {
		return;
	}

	switch (static_cast<PauseMenuItem>(selectedPauseMenuItem_)) {
	case PauseMenuItem::Resume:
		SetPauseMenuOpen(false);
		break;
	case PauseMenuItem::Retry:
		SetPauseMenuOpen(false);
		if (sceneManager) sceneManager->ChangeScene("GAMEPLAY");
		break;
	case PauseMenuItem::StageSelect:
		SetPauseMenuOpen(false);
		if (sceneManager) sceneManager->ChangeScene("STAGE_SELECT");
		break;
	case PauseMenuItem::Count:
		break;
	}
}

/// <summary>
/// スカイボックスの描画処理を行います。
/// </summary>
void GamePlayScene::DrawSkyBox() {
	BaseScene::DrawSkyBox();
	if (isShowSkyBox_ && skyBox) {
		 skyBox->Draw();
	}
}

/// <summary>
/// 2D 要素の描画処理を行います。
/// </summary>
void GamePlayScene::Draw2D() {
	if (isShowSprite_ && spriteObject_) {
		spriteObject_->Draw();
	}

	if (isShowSprites_) {
		for (auto& spriteObject : spriteObjects_) {
			spriteObject->Draw();
		}
	}
}

void GamePlayScene::DrawOverlay2D() {
	if (GetGameplayMode() != GameplayMode::Paused || !pauseOverlaySprite_ || !pausePanelSprite_ || !pauseSelectionSprite_) {
		return;
	}
	DirectXCommon* dxCommon = SpriteCommon::GetInstance()->GetDxCommon();
	if (!dxCommon) return;
	const float screenWidth = static_cast<float>(dxCommon->GetRenderWidth());
	const float screenHeight = static_cast<float>(dxCommon->GetRenderHeight());
	const float panelWidth = (std::min)(520.0f, screenWidth - 50.0f);
	const float panelHeight = (std::min)(500.0f, screenHeight - 40.0f);
	const float panelX = (screenWidth - panelWidth) * 0.5f;
	const float panelY = (screenHeight - panelHeight) * 0.5f;
	const float itemWidth = panelWidth - 80.0f;
	const float itemHeight = 62.0f;
	const float firstItemY = panelY + 145.0f;
	const float itemGap = 18.0f;

	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	DrawPauseColorSprite(pauseOverlaySprite_.get(), 0.0f, 0.0f, screenWidth, screenHeight, {0.0f, 0.0f, 0.0f, 0.68f});
	DrawPauseColorSprite(pausePanelSprite_.get(), panelX, panelY, panelWidth, panelHeight, {0.025f, 0.08f, 0.15f, 0.98f});
	const float selectedY = firstItemY + selectedPauseMenuItem_ * (itemHeight + itemGap);
	DrawPauseColorSprite(pauseSelectionSprite_.get(), panelX + 40.0f, selectedY, itemWidth, itemHeight,
	    {0.08f, 0.48f, 0.62f, 0.98f});

	pauseTitleTextObject_->GetTransform().translate = {screenWidth * 0.5f, panelY + 72.0f, 0.0f};
	pauseTitleTextObject_->GetComponent<TextComponent>()->SetColor({0.55f, 0.95f, 1.0f, 1.0f});
	pauseTitleTextObject_->Draw2D();
	for (int index = 0; index < static_cast<int>(pauseMenuTextObjects_.size()); ++index) {
		GameObject* textObject = pauseMenuTextObjects_[index].get();
		textObject->GetTransform().translate = {
		    screenWidth * 0.5f, firstItemY + index * (itemHeight + itemGap) + itemHeight * 0.5f, 0.0f};
		textObject->GetComponent<TextComponent>()->SetColor(
		    index == selectedPauseMenuItem_ ? Vector4{0.95f, 1.0f, 1.0f, 1.0f} : Vector4{0.72f, 0.79f, 0.88f, 1.0f});
		textObject->Draw2D();
	}
	pauseInstructionTextObject_->GetTransform().translate = {screenWidth * 0.5f, panelY + panelHeight - 35.0f, 0.0f};
	pauseInstructionTextObject_->Draw2D();
}

/// <summary>
/// 3D 要素の描画処理を行います。
/// </summary>
void GamePlayScene::Draw3D() {
	if (isShowObject3D_ && object3dObject_) {
		object3dObject_->Draw();
	}

	if (isShowCylinder_ && cylinderGameObject_) {
		cylinderGameObject_->Draw();
	}

	if (isShowSphere_ && sphereGameObject_) {
		sphereGameObject_->Draw();
	}

	if (isShowInstancing_ && instancingModel_) {
		InstancingModelCommon::GetInstance()->SetDraw();

		instancingModel_->Draw(Object3dCommon::GetInstance()->GetDefaultCamera());
	}

}

/// <summary>
/// 確保したリソースを解放し、終了処理を行います。
/// </summary>
void GamePlayScene::Finalize() {
	SetPauseMenuOpen(false);
	pauseOverlaySprite_.reset();
	pausePanelSprite_.reset();
	pauseSelectionSprite_.reset();
	pauseTitleTextObject_.reset();
	pauseMenuTextObjects_.clear();
	pauseInstructionTextObject_.reset();
	BaseScene::Finalize();
	if (audio_) {
		audio_->Finalize();
		audio_.reset();
	}
	sprite = nullptr;
	sprites.clear();
	emitters_.clear();
	object3d = nullptr;
	sphereObject = nullptr;
	cylinderObject = nullptr;
	spriteObject_.reset();
	spriteObjects_.clear();
	emitterObjects_.clear();
	ParticleManager::GetInstance()->ClearGroups();
	object3dObject_.reset();
	sphereGameObject_.reset();
	cylinderGameObject_.reset();
	instancingModel_.reset();
	skyBox.reset();
}

/// <summary>
/// ImGui によるデバッグ用 UI の表示と編集処理を行います。
/// </summary>
void GamePlayScene::ImGuiUpdate() {
#ifdef USE_IMGUI
	cameraPosition = Object3dCommon::GetInstance()->GetDefaultCamera()->GetTranslate();
	cameraRotate = Object3dCommon::GetInstance()->GetDefaultCamera()->GetRotate();
	if (!ImGui::GetIO().WantCaptureMouse) {
		const float wheelZoomSpeed = 0.01f;
		cameraPosition.z += static_cast<float>(Input::GetInstance()->GetMouseWheelDelta()) * wheelZoomSpeed;
	}

	const EditorLayout editorLayout = MakeEditorLayout();

	if (BeginEditorPanel("Hierarchy", editorLayout.hierarchyPos, editorLayout.hierarchySize)) {
		ImGui::Text("Scene Visibility");
		ImGui::Separator();

		ImGui::Checkbox("Show SkyBox", &isShowSkyBox_);
		ImGui::Checkbox("Show Main Sprite", &isShowSprite_);
		ImGui::Checkbox("Show Array Sprites", &isShowSprites_);
		ImGui::Checkbox("Show Plane", &isShowObject3D_);
		ImGui::Checkbox("Show Grass", &isShowInstancing_);
		ImGui::Checkbox("Show Sphere", &isShowSphere_);
		ImGui::Checkbox("Show Cylinder", &isShowCylinder_);
		ImGui::Checkbox("Show Particles", &isShowParticles_);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("System Info");
		ImGui::Text("Frame Rate : %.1f FPS", ImGui::GetIO().Framerate);
		ImGui::Text("Frame Time : %.3f ms", 1000.0f / ImGui::GetIO().Framerate);

		static bool showDemoWindow = false;
		ImGui::Checkbox("Show ImGui Demo Window", &showDemoWindow);
		if (showDemoWindow) {
			ImGui::ShowDemoWindow(&showDemoWindow);
		}
	}
	ImGui::End();

	if (BeginEditorPanel("Inspector", editorLayout.inspectorPos, editorLayout.inspectorSize)) {
		if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat3("camera pos", &cameraPosition.x, 0.1f);
			ImGui::SliderAngle("camera rotate x", &cameraRotate.x);
			ImGui::SliderAngle("camera rotate y", &cameraRotate.y);
			ImGui::SliderAngle("camera rotate z", &cameraRotate.z);
		}
	}
	ImGui::End();

	if (BeginEditorPanel("Particle Editor", editorLayout.particlePos, editorLayout.particleSize)) {
		if (emitters_.empty()) {
			ImGui::Text("No particle emitters");
		} else {
			std::vector<const char*> emitterNames;
			for (const auto& emitter : emitters_) {
				emitterNames.push_back(emitter->GetGroupName().c_str());
			}

			if (currentParticleIndex_ >= static_cast<int>(emitters_.size())) {
				currentParticleIndex_ = 0;
			}
			ImGui::Combo("Target Particle", &currentParticleIndex_, emitterNames.data(), static_cast<int>(emitterNames.size()));
			ImGui::Separator();

			auto& targetEmitter = emitters_[currentParticleIndex_];
			ParticleEmitParam param = targetEmitter->GetParam();
			ImGui::Text("Editing: %s", targetEmitter->GetGroupName().c_str());
			ImGui::DragFloat("Life Time", &param.lifeTime, 0.01f, 0.1f, 20.0f);
			ImGui::DragFloat3("Start Scale", &param.scale.x, 0.01f);
			ImGui::DragFloat3("End Scale", &param.endScale.x, 0.01f);
			ImGui::ColorEdit4("Start Color", &param.color.x);
			ImGui::ColorEdit4("End Color", &param.endColor.x);
			targetEmitter->SetParam(param);

			if (ImGui::Button("Test Emit", ImVec2(120, 30))) {
				targetEmitter->Emit();
			}
		}
	}
	ImGui::End();

	Object3dCommon::GetInstance()->GetDefaultCamera()->SetTranslate(cameraPosition);
	Object3dCommon::GetInstance()->GetDefaultCamera()->SetRotate(cameraRotate);
	return;

	trsprite = sprite->GetTransform();
	trspriteUV = sprite->GetUVTransform();
	spriteColor = sprite->GetColor();
	spriteSize = sprite->GetSize();
	anchor = sprite->GetAnchorPoint();
	isFlipX = sprite->GetIsFlipX();
	isFlipY = sprite->GetIsFlipY();
	textureLeftTop = sprite->GetTextureLeftTop();
	textureSize = sprite->GetTextureSize();

	modelPosition = object3d->GetTranslate();
	modelRotate = object3d->GetRotate();
	modelScale = object3d->GetScale();
	lightColor = object3d->GetLightColor();
	lightDirection = object3d->GetLightDirection();
	lightIntensity = object3d->GetLightIntensity();
	float	environmentMultiplier = object3d->GetEnvironmentMultiplier();

	Vector3 spherepos = sphereObject->GetTranslate();
	Vector3 sphererot = sphereObject->GetRotate();
	Vector3 spherescl = sphereObject->GetScale();

	Vector4 pointLightColor = object3d->GetPointLightColor();
	Vector3 pointLightPosition = object3d->GetPointLightPosition();
	float pointLightIntensity = object3d->GetPointLightIntensity();
	float pointLightRadius = object3d->GetPointLightRadius();
	float pointLightDecay = object3d->GetPointLightDecay();
	bool isPointLightSet = object3d->GetIsPointLightSet();
	const EditorLayout layout = MakeEditorLayout();

	if (BeginEditorPanel("Hierarchy", layout.hierarchyPos, layout.hierarchySize)) {
		ImGui::Text("Scene Visibility");
		ImGui::Separator();

		ImGui::Checkbox("Show SkyBox", &isShowSkyBox_);
		ImGui::Checkbox("Show Main Sprite", &isShowSprite_);
		ImGui::Checkbox("Show Array Sprites", &isShowSprites_);
		ImGui::Checkbox("Show Plane", &isShowObject3D_);
		ImGui::Checkbox("Show InstancingModel", &isShowInstancing_);
		ImGui::Checkbox("Show Sphere", &isShowSphere_);
		ImGui::Checkbox("Show Cylinder", &isShowCylinder_);
		ImGui::Checkbox("Show Particles", &isShowParticles_);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("System Info");
		ImGui::Text("Frame Rate : %.1f FPS", ImGui::GetIO().Framerate);
		ImGui::Text("Frame Time : %.3f ms", 1000.0f / ImGui::GetIO().Framerate);

		static bool showDemoWindow = false;
		ImGui::Checkbox("Show ImGui Demo Window", &showDemoWindow);
		if (showDemoWindow) {
			ImGui::ShowDemoWindow(&showDemoWindow);
		}
	}
	ImGui::End();

	if (BeginEditorPanel("Inspector", layout.inspectorPos, layout.inspectorSize)) {
		if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat3("camera pos", &cameraPosition.x, 0.1f);
			ImGui::SliderAngle("camera rotate x", &cameraRotate.x);
			ImGui::SliderAngle("camera rotate y", &cameraRotate.y);
			ImGui::SliderAngle("camera rotate z", &cameraRotate.z);
		}

		if (ImGui::CollapsingHeader("Model Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat3("model pos", &modelPosition.x, 0.1f);
			ImGui::SliderAngle("model rotate x", &modelRotate.x);
			ImGui::SliderAngle("model rotate y", &modelRotate.y);
			ImGui::SliderAngle("model rotate z", &modelRotate.z);
			ImGui::DragFloat3("model scale", &modelScale.x, 0.1f);
			ImGui::DragFloat("environment multiplier", &environmentMultiplier, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat3("sphere pos", &spherepos.x, 0.1f);
			ImGui::DragFloat3("sphere rotate", &sphererot.x, 0.1f);
			ImGui::DragFloat3("sphere scale", &spherescl.x, 0.1f);
		}

		if (ImGui::CollapsingHeader("Sprite Settings")) {
			ImGui::DragFloat2("sprite pos", &trsprite.translate.x, 0.3f);
			ImGui::SliderAngle("sprite rotate", &trsprite.rotate.z);
			ImGui::DragFloat2("sprite scale", &spriteSize.x, 0.3f);
			ImGui::ColorEdit4("sprite color", &spriteColor.x);
			ImGui::DragFloat2("anchor point", &anchor.x, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat2("texture left top", &textureLeftTop.x, 1.0f, 0.0f, 512.0f);
			ImGui::DragFloat2("texture size", &textureSize.x, 1.0f, 0.0f, 512.0f);
			ImGui::Checkbox("Flip X", &isFlipX);
			ImGui::Checkbox("Flip Y", &isFlipY);
			ImGui::DragFloat2("UV translate", &trspriteUV.translate.x, 0.01f, -10.0f, 10.0f);
			ImGui::DragFloat2("UV scale", &trspriteUV.scale.x, 0.01f, 0.0f, 10.0f);
			ImGui::SliderAngle("UV rotate", &trspriteUV.rotate.z);
		}

		if (ImGui::CollapsingHeader("Light Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::ColorEdit4("light color", &lightColor.x);
			ImGui::DragFloat3("light direction", &lightDirection.x, 0.1f, -1.0f, 1.0f);
			ImGui::DragFloat("light intensity", &lightIntensity, 0.1f, 0.0f, 10.0f);
			ImGui::Checkbox("point light set", &isPointLightSet);
			ImGui::ColorEdit4("point light color", &pointLightColor.x);
			ImGui::DragFloat3("point light position", &pointLightPosition.x, 0.1f);
			ImGui::DragFloat("point light intensity", &pointLightIntensity, 0.1f, 0.0f, 10.0f);
			ImGui::DragFloat("point light radius", &pointLightRadius, 0.1f, 0.0f, 20.0f);
			ImGui::DragFloat("point light decay", &pointLightDecay, 0.1f, 0.0f, 10.0f);
			ImGui::DragFloat("environment multiplier", &environmentMultiplier, 0.01f, 0.0f, 1.0f);
		}
	}
	ImGui::End();

	BeginEditorPanel("Particle Editor", layout.particlePos, layout.particleSize);

	std::vector<const char*> emitterNames;
	for (const auto& emitter : emitters_) {
		emitterNames.push_back(emitter->GetGroupName().c_str());
	}

	ImGui::Combo("Target Particle", &currentParticleIndex_, emitterNames.data(), static_cast<int>(emitterNames.size()));

	ImGui::Separator();

	if (!emitters_.empty() && currentParticleIndex_ < emitters_.size()) {
		auto& targetEmitter = emitters_[currentParticleIndex_];

		ParticleEmitParam param = targetEmitter->GetParam();

		ImGui::Text("Editing: %s", targetEmitter->GetGroupName().c_str());

		if (ImGui::CollapsingHeader("1. Basic Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			bool isActive = targetEmitter->GetIsActive();
			if (ImGui::Checkbox("Active (Draw/Update ON)", &isActive)) {
				targetEmitter->SetIsActive(isActive);
			}

			int count = static_cast<int>(param.count);
			if (ImGui::DragInt("Emit Count", &count, 1, 0, static_cast<int>(ParticleManager::kMaxParticle))) {
				param.count = static_cast<uint32_t>(count);
			}

			float frequency = targetEmitter->GetFrequency();
			if (ImGui::DragFloat("Emit Frequency (sec)", &frequency, 0.01f, 0.01f, 10.0f)) {
				targetEmitter->SetFrequency(frequency);
			}
			ImGui::Separator();

			ImGui::DragFloat("Life Time", &param.lifeTime, 0.01f, 0.1f, 20.0f);
			
			ImGui::Checkbox("Is Billboard", &param.isBillboard);

			ImGui::DragFloat3("Random Pos Range", &param.randomPositionRange.x, 0.01f);
			ImGui::Text("Texture");

			std::vector<const char*> textureNames;
			for (const auto& path : availableTextures_) {
				textureNames.push_back(path.c_str());
			}

			int currentTexIndex = 0;
			std::string currentPath = targetEmitter->GetTextureFilePath();
			for (int i = 0; i < availableTextures_.size(); ++i) {
				if (availableTextures_[i] == currentPath) {
					currentTexIndex = i;
					break;
				}
			}

			if (ImGui::Combo("Particle Texture", &currentTexIndex, textureNames.data(), static_cast<int>(textureNames.size()))) {
				targetEmitter->SetTexture(availableTextures_[currentTexIndex]);
			}

			ImGui::Separator();
			ImGui::Text("Blend Mode");
			const char* blendModeNames[] = { "None", "Normal", "Add", "Subtract", "Multiply", "Screen" };

			int currentBlendMode = static_cast<int>(targetEmitter->GetBlendMode());

			if (ImGui::Combo("##BlendMode", &currentBlendMode, blendModeNames, IM_ARRAYSIZE(blendModeNames))) {
				BlendMode newMode = static_cast<BlendMode>(currentBlendMode);
				targetEmitter->SetBlendMode(newMode);

				ParticleManager::GetInstance()->SetGroupBlendMode(targetEmitter->GetGroupName(), newMode);
			}
			ImGui::Text("Mesh Type");
			const char* meshTypeNames[] = { "Quad (Square)", "Ring" };

			int currentMeshType = static_cast<int>(targetEmitter->GetMeshType());

			if (ImGui::Combo("##MeshType", &currentMeshType, meshTypeNames, IM_ARRAYSIZE(meshTypeNames))) {
				ParticleMeshType newType = static_cast<ParticleMeshType>(currentMeshType);
				targetEmitter->SetMeshType(newType);

			}
			ImGui::Separator();
		}

		if (ImGui::CollapsingHeader("2. Movement & Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat3("Base Velocity", &param.baseVelocity.x, 0.01f);
			ImGui::DragFloat3("Random Vel Range", &param.randomVelocityRange.x, 0.01f);

			ImGui::DragFloat3("Acceleration", &param.acceleration.x, 0.001f);
		}

		if (ImGui::CollapsingHeader("3. Scale & Color", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("Scale");
			ImGui::DragFloat3("Start Scale", &param.scale.x, 0.01f);
			ImGui::DragFloat3("End Scale", &param.endScale.x, 0.01f);
			ImGui::DragFloat3("Random Scale Range", &param.randomScaleRange.x, 0.01f);

			ImGui::Spacing();

			ImGui::Text("Color");
			ImGui::ColorEdit4("Start Color", &param.color.x);
			ImGui::ColorEdit4("End Color", &param.endColor.x);
		}

		if (ImGui::CollapsingHeader("4. Rotation")) {
			ImGui::DragFloat3("Base Rotate", &param.baseRotate.x, 0.01f);
			ImGui::Checkbox("Enable Random Rotate", &param.isRandomRotate);
			if (param.isRandomRotate) {
				ImGui::DragFloat3("Random Rot Range", &param.randomRotateRange.x, 0.01f);
			}
		}

		targetEmitter->SetParam(param);

		ImGui::Separator();

		if (ImGui::Button("Test Emit", ImVec2(120, 30))) {
			targetEmitter->Emit();
		}
	}

	ImGui::Separator();

	if (ImGui::Button("Save All to JSON", ImVec2(150, 40))) {
		for (auto& emitter : emitters_) {
			emitter->SaveToJson("Resources/Data/emit_status.json");
		}
	}

	ImGui::End();

	sphereObject->SetTranslate(spherepos);
	sphereObject->SetRotate(sphererot);
	sphereObject->SetScale(spherescl);
	object3d->IsPointLightSet(isPointLightSet);
	sphereObject->IsPointLightSet(isPointLightSet);

	object3d->SetPointLight(pointLightColor, pointLightPosition, pointLightIntensity, pointLightRadius, pointLightDecay);
	sphereObject->SetPointLight(pointLightColor, pointLightPosition, pointLightIntensity, pointLightRadius, pointLightDecay);
	object3d->SetEnvironmentMultiplier(environmentMultiplier);

	Object3dCommon::GetInstance()->GetDefaultCamera()->SetTranslate(cameraPosition);
	Object3dCommon::GetInstance()->GetDefaultCamera()->SetRotate(cameraRotate);
	object3d->SetTranslate(modelPosition);
	object3d->SetRotate(modelRotate);
	object3d->SetScale(modelScale);
	sprite->SetIsFlipX(isFlipX);
	sprite->SetIsFlipY(isFlipY);
	sprite->SetAnchorPoint(anchor);
	sprite->SetTransform(trsprite);
	sprite->SetUVTransform(trspriteUV);
	sprite->SetColor(spriteColor);
	sprite->SetSize(spriteSize);
	sprite->SetTextureLeftTop(textureLeftTop);
	sprite->SetTextureSize(textureSize);
	object3d->SetDirectionalLight(lightColor, lightDirection, lightIntensity);
	sphereObject->SetDirectionalLight(lightColor, lightDirection, lightIntensity);
#endif
}


