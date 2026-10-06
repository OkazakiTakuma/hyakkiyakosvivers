#include "PlayerSelectScene.h"

#include "SceneManager.h"
#include "repositories/PlayerStatusRepository.h"
#include <model/ModelManager.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <Xinput.h>

namespace {
constexpr const char* kUiFontName = "Yu Gothic UI";

std::unique_ptr<Sprite> CreateColorSprite() {
	// 白テクスチャへ描画時に色を乗算し、全パネルを同じ仕組みで描画する。
	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize("Resources/human/white.png");
	return sprite;
}

std::unique_ptr<GameObject> CreateTextObject(
	const std::string& text, float fontSize, TextComponent::Anchor anchor = TextComponent::Anchor::Center) {
	auto object = std::make_unique<GameObject>();
	TextComponent* textComponent = object->AddComponent<TextComponent>();
	textComponent->SetText(text);
	textComponent->SetFontName(kUiFontName);
	textComponent->SetFontSize(fontSize);
	textComponent->SetAnchor(anchor);
	return object;
}

void DrawColorSprite(Sprite* sprite, float x, float y, float width, float height, const Vector4& color) {
	EulerTransform transform = sprite->GetTransform();
	transform.translate = {x, y, 0.0f};
	transform.rotate.z = 0.0f;
	sprite->SetAnchorPoint({0.0f, 0.0f});
	sprite->SetTransform(transform);
	sprite->SetSize({width, height});
	sprite->SetColor(color);
	sprite->Update();
	sprite->Draw();
}

const PlayerAttackLevelStats* FindAttackLevel(const PlayerAttackStats& attack, const std::string& level) {
	// 設定に指定レベルがない場合も、先頭レベルへ戻して詳細欄を空にしない。
	for (const PlayerAttackLevelStats& levelStats : attack.levels) {
		if (levelStats.level == level) return &levelStats;
	}
	return attack.levels.empty() ? nullptr : &attack.levels.front();
}

std::string GetModelDirectory(const std::string& modelFilePath) {
	// 現在のモデル配置規則に合わせ、ファイル名から読込元ディレクトリを解決する。
	if (modelFilePath == "miko.gltf") return "/human";
	if (modelFilePath == "neko.gltf") return "/cat";
	if (modelFilePath == "doppelganger.gltf") return "/doppelganger";
	return "/";
}
}

void PlayerSelectScene::Initialize() {
	// エディタ用のDefaultは候補から除き、実際に遊べるキャラクターだけを並べる。
	playerTypeNames_ = LoadPlayerTypeNames();
	playerTypeNames_.erase(std::remove(playerTypeNames_.begin(), playerTypeNames_.end(), "Default"), playerTypeNames_.end());
	selectedPlayerIndex_ = 0;
	if (sceneManager_) {
		// 前回選択が使用可能なら維持し、そうでなければ初期開放キャラクターへ戻す。
		const std::string& currentType = sceneManager_->GetSelectedPlayerTypeName();
		const auto found = std::find(playerTypeNames_.begin(), playerTypeNames_.end(), currentType);
		if (found != playerTypeNames_.end() && sceneManager_->IsPlayerTypeUnlocked(currentType)) {
			selectedPlayerIndex_ = static_cast<int>(std::distance(playerTypeNames_.begin(), found));
		} else {
			const auto initialPlayer = std::find(playerTypeNames_.begin(), playerTypeNames_.end(), "巫女");
			if (initialPlayer != playerTypeNames_.end()) selectedPlayerIndex_ = static_cast<int>(std::distance(playerTypeNames_.begin(), initialPlayer));
		}
	}

	// 3Dモデルを左カラムへ寄せて全身が収まるプレビュー専用カメラを用意する。
	previewCamera_ = std::make_unique<Camera>();
	previewCamera_->SetTranslate({0.0f, 2.2f, -8.5f});
	previewCamera_->SetRotate({0.12f, 0.0f, 0.0f});
	previewCamera_->SetfovY(0.62f);
	previewCamera_->SetNearClip(0.1f);
	previewCamera_->SetFarClip(100.0f);
	CreateUi();
	RefreshSelectionPresentation();
}

void PlayerSelectScene::CreateUi() {
	// 画像素材に依存しない単色パネルを個別に所有し、描画順と透明度を制御する。
	backgroundSprite_ = CreateColorSprite();
	modelPanelSprite_ = CreateColorSprite();
	modelPanelAccentSprite_ = CreateColorSprite();
	detailPanelSprite_ = CreateColorSprite();
	statusPanelSprite_ = CreateColorSprite();
	weaponPanelSprite_ = CreateColorSprite();
	weaponIconBackgroundSprite_ = CreateColorSprite();
	// 武器画像が未設定でも枠を空にしないよう、初期状態は既存の円画像を使用する。
	weaponIconSprite_ = std::make_unique<Sprite>();
	weaponIconSprite_->Initialize("Resources/circle.png");
	weaponIconSprite_->SetAnchorPoint({0.5f, 0.5f});

	titleTextObject_ = CreateTextObject("キャラクター選択", 42.0f);
	characterNameTextObject_ = CreateTextObject("", 40.0f);
	characterStateTextObject_ = CreateTextObject("", 20.0f);
	statusHeadingTextObject_ = CreateTextObject("STATUS", 20.0f, TextComponent::Anchor::TopLeft);
	statusTextObject_ = CreateTextObject("", 22.0f, TextComponent::Anchor::TopLeft);
	weaponHeadingTextObject_ = CreateTextObject("INITIAL WEAPON", 20.0f, TextComponent::Anchor::TopLeft);
	weaponNameTextObject_ = CreateTextObject("", 27.0f, TextComponent::Anchor::TopLeft);
	weaponTextObject_ = CreateTextObject("", 18.0f, TextComponent::Anchor::TopLeft);
	pageTextObject_ = CreateTextObject("", 18.0f);
	instructionTextObject_ = CreateTextObject("A / D・左右キー・Pad左右: 選択    Enter・Pad A: 決定    Q・Pad B: 戻る", 18.0f);
	statusHeadingTextObject_->GetComponent<TextComponent>()->SetColor({0.35f, 0.78f, 1.0f, 1.0f});
	weaponHeadingTextObject_->GetComponent<TextComponent>()->SetColor({1.0f, 0.74f, 0.24f, 1.0f});
	instructionTextObject_->GetComponent<TextComponent>()->SetColor({0.68f, 0.75f, 0.86f, 1.0f});
}

void PlayerSelectScene::RefreshSelectionPresentation() {
	if (playerTypeNames_.empty() || selectedPlayerIndex_ < 0 || selectedPlayerIndex_ >= static_cast<int>(playerTypeNames_.size())) return;
	presentedPlayerIndex_ = selectedPlayerIndex_;
	previewTime_ = 0.0f;
	// 選択されたタイプのJSON設定を、右側の全表示項目の共通入力として使う。
	const PlayerStats stats = LoadPlayerStats(playerTypeNames_[selectedPlayerIndex_]);
	const bool unlocked = !sceneManager_ || sceneManager_->IsPlayerTypeUnlocked(stats.name);
	characterNameTextObject_->GetComponent<TextComponent>()->SetText(stats.name);
	characterNameTextObject_->GetComponent<TextComponent>()->SetColor(unlocked ? Vector4{1.0f, 0.94f, 0.72f, 1.0f} : Vector4{0.58f, 0.60f, 0.66f, 1.0f});
	statusTextObject_->GetComponent<TextComponent>()->SetText(unlocked ? MakeStatusDescription(stats) : "？？？\n\n未開放のため能力は表示されません");

	if (unlocked) {
		characterStateTextObject_->GetComponent<TextComponent>()->SetText("◀  SELECTED  ▶");
		characterStateTextObject_->GetComponent<TextComponent>()->SetColor({0.42f, 0.82f, 1.0f, 1.0f});
		weaponNameTextObject_->GetComponent<TextComponent>()->SetText(stats.initialAttackName);
		weaponTextObject_->GetComponent<TextComponent>()->SetText(MakeWeaponDescription(stats));
	} else {
		// 未開放キャラクターは数値を伏せ、解除条件だけをプレイヤーへ案内する。
		std::string prerequisite = sceneManager_ ? sceneManager_->GetPlayerUnlockPrerequisiteStage(stats.name) : "";
		const std::string prerequisiteId = prerequisite;
		if (sceneManager_) prerequisite = sceneManager_->GetGameplayStageDisplayName(prerequisite);
		if (prerequisiteId != "default") {
			std::transform(prerequisite.begin(), prerequisite.end(), prerequisite.begin(), [](unsigned char character) { return static_cast<char>(std::toupper(character)); });
		}
		characterStateTextObject_->GetComponent<TextComponent>()->SetText("LOCKED  /  " + prerequisite + "クリアで開放");
		characterStateTextObject_->GetComponent<TextComponent>()->SetColor({0.86f, 0.38f, 0.38f, 1.0f});
		weaponNameTextObject_->GetComponent<TextComponent>()->SetText("？？？");
		weaponTextObject_->GetComponent<TextComponent>()->SetText("開放後に初期武器の詳細を確認できます");
	}
	pageTextObject_->GetComponent<TextComponent>()->SetText(std::to_string(selectedPlayerIndex_ + 1) + "  /  " + std::to_string(playerTypeNames_.size()));

	// 攻撃共通画像、レベル別画像の順に探し、どちらもなければ仮アイコンへ戻す。
	const PlayerAttackStats attack = LoadPlayerAttackStats(stats.initialAttackName);
	const PlayerAttackLevelStats* level = FindAttackLevel(attack, stats.initialAttackLevel);
	std::string iconPath = attack.choiceTextureFilePath;
	if (iconPath.empty() && level) iconPath = level->choiceTextureFilePath;
	if (iconPath.empty() || !std::filesystem::exists(iconPath)) iconPath = "Resources/circle.png";
	weaponIconSprite_->SetTexture(iconPath);
	weaponIconSprite_->SetColor(unlocked ? Vector4{1.0f, 0.72f, 0.24f, 1.0f} : Vector4{0.30f, 0.32f, 0.36f, 1.0f});
	RebuildCharacterModel(stats);
}

void PlayerSelectScene::RebuildCharacterModel(const PlayerStats& stats) {
	// 選択変更時は旧モデルを破棄し、共有ModelManagerから新しい表示モデルを参照する。
	characterModelObject_.reset();
	if (stats.modelFilePath.empty()) return;
	if (!ModelManager::GetInstance()->FindModel(stats.modelFilePath)) {
		ModelManager::GetInstance()->LoadModel(stats.modelFilePath, stats.isAnimationModel, GetModelDirectory(stats.modelFilePath));
	}
	if (!ModelManager::GetInstance()->FindModel(stats.modelFilePath)) return;

	characterModelObject_ = std::make_unique<GameObject>();
	characterModelObject_->SetName("PlayerSelectPreview");
	EulerTransform& transform = characterModelObject_->GetTransform();
	// UIの左カラム中央へ見えるよう、カメラ正面より左へモデルを配置する。
	transform.translate = {-2.15f, -1.25f, 0.45f};
	transform.rotate = {0.0f, 0.18f, 0.0f};
	transform.scale = {1.20f, 1.20f, 1.20f};
	Object3dComponent* object3d = characterModelObject_->AddComponent<Object3dComponent>();
	object3d->SetModel(stats.modelFilePath);
	object3d->SetCamera(previewCamera_.get());
	object3d->SetDrawSkeleton(false);
	object3d->SetAnimationPlaying(stats.isAnimationModel);
	object3d->SetDirectionalLight({1.0f, 0.92f, 0.80f, 1.0f}, {-0.35f, -0.75f, 0.55f}, 1.35f);
	// プレビューでは衣装や肌の色を読み取りやすくし、環境マップ由来の鏡面反射をほぼ抑える。
	object3d->SetPointLight({0.30f, 0.62f, 1.0f, 1.0f}, {-3.2f, 2.8f, -2.0f}, 0.75f, 9.0f, 1.6f);
	object3d->SetEnvironmentMultiplier(0.02f);
	characterModelObject_->Update();
}

std::string PlayerSelectScene::MakeStatusDescription(const PlayerStats& stats) const {
	std::ostringstream stream;
	stream << "HP                         " << static_cast<int>(stats.baseHealth * stats.health / 100.0f)
	       << "\n攻撃力                     " << static_cast<int>(stats.attack)
	       << "\n防御力                     " << static_cast<int>(stats.defense)
	       << "\n移動速度                   " << std::fixed << std::setprecision(2) << stats.baseSpeed * stats.speed / 100.0f
	       << "\n攻撃速度                   " << static_cast<int>(stats.attackSpeed) << "%"
	       << "\n攻撃サイズ                 " << static_cast<int>(stats.attackSize) << "%";
	return stream.str();
}

std::string PlayerSelectScene::MakeWeaponDescription(const PlayerStats& stats) const {
	const PlayerAttackStats attack = LoadPlayerAttackStats(stats.initialAttackName);
	const PlayerAttackLevelStats* level = FindAttackLevel(attack, stats.initialAttackLevel);
	if (!level) return "武器データがありません";
	std::ostringstream stream;
	stream << (level->choiceDescription.empty() ? "初期装備として使用する攻撃です" : level->choiceDescription)
	       << "\n\n威力  " << static_cast<int>(level->attack)
	       << "    発射数  " << level->shotCount
	       << "    間隔  " << std::fixed << std::setprecision(2) << level->attackInterval << "秒";
	return stream.str();
}

void PlayerSelectScene::Update() {
	if (playerTypeNames_.empty() || !sceneManager_) return;
	Input* input = Input::GetInstance();
	const int playerCount = static_cast<int>(playerTypeNames_.size());
	if (input->TriggerKey(DIK_A) || input->TriggerKey(DIK_LEFT) || input->TriggerGamepadLeft()) selectedPlayerIndex_ = (selectedPlayerIndex_ + playerCount - 1) % playerCount;
	if (input->TriggerKey(DIK_D) || input->TriggerKey(DIK_RIGHT) || input->TriggerGamepadRight()) selectedPlayerIndex_ = (selectedPlayerIndex_ + 1) % playerCount;
	// 入力で位置が変わったフレームだけ、モデルと文字テクスチャを作り直す。
	if (selectedPlayerIndex_ != presentedPlayerIndex_) RefreshSelectionPresentation();

	// 静止画に見えない程度の小さな往復回転をプレビューモデルへ与える。
	previewTime_ += GameTime::GetDeltaTime();
	if (characterModelObject_) {
		characterModelObject_->GetTransform().rotate.y = 0.18f + std::sin(previewTime_ * 0.75f) * 0.16f;
		characterModelObject_->Update();
	}
	if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) || input->TriggerGamepadButton(XINPUT_GAMEPAD_A)) {
		const std::string& selectedPlayerType = playerTypeNames_[selectedPlayerIndex_];
		if (sceneManager_->IsPlayerTypeUnlocked(selectedPlayerType)) {
			sceneManager_->SetSelectedPlayerTypeName(selectedPlayerType);
			sceneManager_->ChangeScene("STAGE_SELECT");
		}
		return;
	}
	if (input->TriggerKey(DIK_Q) || input->TriggerGamepadButton(XINPUT_GAMEPAD_B)) sceneManager_->ChangeScene("TITLE");
}

void PlayerSelectScene::Draw3D() {
	if (!previewCamera_ || !characterModelObject_) return;
	// ウィンドウ比率が変わってもモデルが横へ伸びないよう、描画直前に射影比を更新する。
	DirectXCommon* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	if (dxCommon && dxCommon->GetRenderHeight() > 0) previewCamera_->SetAspectRatio(static_cast<float>(dxCommon->GetRenderWidth()) / static_cast<float>(dxCommon->GetRenderHeight()));
	previewCamera_->Update();
	if (Object3dComponent* object3d = characterModelObject_->GetComponent<Object3dComponent>()) object3d->SetCamera(previewCamera_.get());
	characterModelObject_->Update();
	characterModelObject_->Draw3D();
}

void PlayerSelectScene::Draw2D() {
	DirectXCommon* dxCommon = SpriteCommon::GetInstance()->GetDxCommon();
	if (!dxCommon || !backgroundSprite_) return;
	const float width = static_cast<float>(dxCommon->GetRenderWidth());
	const float height = static_cast<float>(dxCommon->GetRenderHeight());
	// 画面幅に応じた余白を確保し、左48%をモデル、残りを詳細表示へ割り当てる。
	const float margin = (std::max)(28.0f, width * 0.032f);
	const float top = 112.0f;
	const float bottom = height - 74.0f;
	const float contentHeight = (std::max)(300.0f, bottom - top);
	const float gap = (std::max)(20.0f, width * 0.018f);
	const float leftWidth = (width - margin * 2.0f - gap) * 0.48f;
	const float rightX = margin + leftWidth + gap;
	const float rightWidth = width - margin - rightX;

	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	DrawColorSprite(backgroundSprite_.get(), 0.0f, 0.0f, width, height, {0.018f, 0.028f, 0.055f, 0.46f});
	DrawColorSprite(modelPanelSprite_.get(), margin, top, leftWidth, contentHeight, {0.025f, 0.075f, 0.13f, 0.28f});
	DrawColorSprite(modelPanelAccentSprite_.get(), margin, top, 5.0f, contentHeight, {0.22f, 0.70f, 1.0f, 0.92f});
	DrawColorSprite(detailPanelSprite_.get(), rightX, top, rightWidth, contentHeight, {0.035f, 0.055f, 0.095f, 0.96f});

	// 右カラムを能力欄と初期武器欄へ縦に分割する。
	const float innerMargin = 25.0f;
	const float sectionWidth = rightWidth - innerMargin * 2.0f;
	const float statusY = top + 58.0f;
	const float statusHeight = contentHeight * 0.45f;
	const float weaponY = statusY + statusHeight + 18.0f;
	const float weaponHeight = bottom - weaponY - 20.0f;
	DrawColorSprite(statusPanelSprite_.get(), rightX + innerMargin, statusY, sectionWidth, statusHeight, {0.045f, 0.095f, 0.145f, 0.96f});
	DrawColorSprite(weaponPanelSprite_.get(), rightX + innerMargin, weaponY, sectionWidth, weaponHeight, {0.12f, 0.075f, 0.035f, 0.96f});

	// 武器欄の高さが小さい解像度では、枠内に収まるようアイコンも縮小する。
	const float iconSize = (std::min)(96.0f, weaponHeight - 34.0f);
	const float iconX = rightX + innerMargin + 18.0f;
	const float iconY = weaponY + (weaponHeight - iconSize) * 0.5f;
	DrawColorSprite(weaponIconBackgroundSprite_.get(), iconX, iconY, iconSize, iconSize, {0.035f, 0.045f, 0.070f, 1.0f});
	EulerTransform iconTransform = weaponIconSprite_->GetTransform();
	iconTransform.translate = {iconX + iconSize * 0.5f, iconY + iconSize * 0.5f, 0.0f};
	weaponIconSprite_->SetTransform(iconTransform);
	weaponIconSprite_->SetSize({iconSize * 0.70f, iconSize * 0.70f});
	weaponIconSprite_->Update();
	weaponIconSprite_->Draw();

	titleTextObject_->GetTransform().translate = {width * 0.5f, 52.0f, 0.0f};
	characterNameTextObject_->GetTransform().translate = {margin + leftWidth * 0.5f, top + 45.0f, 0.0f};
	characterStateTextObject_->GetTransform().translate = {margin + leftWidth * 0.5f, bottom - 46.0f, 0.0f};
	statusHeadingTextObject_->GetTransform().translate = {rightX + innerMargin + 18.0f, top + 31.0f, 0.0f};
	statusTextObject_->GetTransform().translate = {rightX + innerMargin + 22.0f, statusY + 26.0f, 0.0f};
	weaponHeadingTextObject_->GetTransform().translate = {rightX + innerMargin + 18.0f, weaponY - 11.0f, 0.0f};
	weaponNameTextObject_->GetTransform().translate = {iconX + iconSize + 20.0f, weaponY + 30.0f, 0.0f};
	weaponTextObject_->GetTransform().translate = {iconX + iconSize + 20.0f, weaponY + 72.0f, 0.0f};
	pageTextObject_->GetTransform().translate = {margin + leftWidth * 0.5f, bottom - 18.0f, 0.0f};
	instructionTextObject_->GetTransform().translate = {width * 0.5f, height - 30.0f, 0.0f};
	titleTextObject_->Draw2D();
	characterNameTextObject_->Draw2D();
	characterStateTextObject_->Draw2D();
	statusHeadingTextObject_->Draw2D();
	statusTextObject_->Draw2D();
	weaponHeadingTextObject_->Draw2D();
	weaponNameTextObject_->Draw2D();
	weaponTextObject_->Draw2D();
	pageTextObject_->Draw2D();
	instructionTextObject_->Draw2D();
}

void PlayerSelectScene::Finalize() {
	characterModelObject_.reset();
	previewCamera_.reset();
	backgroundSprite_.reset();
	modelPanelSprite_.reset();
	modelPanelAccentSprite_.reset();
	detailPanelSprite_.reset();
	statusPanelSprite_.reset();
	weaponPanelSprite_.reset();
	weaponIconBackgroundSprite_.reset();
	weaponIconSprite_.reset();
	titleTextObject_.reset();
	characterNameTextObject_.reset();
	characterStateTextObject_.reset();
	statusHeadingTextObject_.reset();
	statusTextObject_.reset();
	weaponHeadingTextObject_.reset();
	weaponNameTextObject_.reset();
	weaponTextObject_.reset();
	pageTextObject_.reset();
	instructionTextObject_.reset();
	playerTypeNames_.clear();
	BaseScene::Finalize();
}
