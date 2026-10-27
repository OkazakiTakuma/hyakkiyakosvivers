#include "PlayerSelectScene.h"

#include "SceneManager.h"
#include "helpers/SceneUiUtility.h"
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
constexpr float kPi = 3.14159265f;
// 自機選択プレビューはゲーム中のモデル向きと反対側から見せる。
constexpr float kPreviewBaseRotationY = 0.18f + kPi;
constexpr Vector3 kPreviewBasePosition = {-2.15f, -1.25f, 0.45f};
constexpr float kPreviewBaseScale = 1.20f;
constexpr float kSelectionConfirmationDuration = 0.85f;

float EaseOutCubic(float value) {
	const float inverse = 1.0f - value;
	return 1.0f - inverse * inverse * inverse;
}

float SmoothStep(float value) {
	return value * value * (3.0f - 2.0f * value);
}

std::unique_ptr<GameObject> CreateTextObject(
	const std::string& text, float fontSize, TextComponent::Anchor anchor = TextComponent::Anchor::Center) {
	return SceneUiUtility::CreateTextObject(text, fontSize, anchor, kUiFontName);
}

using SceneUiUtility::CreateColorSprite;
using SceneUiUtility::DrawColorSprite;

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

std::string GetEntranceAnimationName(const std::string& playerTypeName) {
	if (playerTypeName == "巫女") return "選択登場_巫女";
	if (playerTypeName == "烏天狗") return "選択登場_烏天狗";
	if (playerTypeName == "猫") return "選択登場_猫";
	return "";
}

float GetEntranceAnimationDuration(const std::string& playerTypeName) {
	// bin内の専用クリップと同じ長さにし、骨格と画面内移動を同時に決めポーズへ到達させる。
	if (playerTypeName == "巫女") return 1.60f;
	if (playerTypeName == "烏天狗") return 1.45f;
	if (playerTypeName == "猫") return 1.75f;
	return 1.20f;
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
		if (found != playerTypeNames_.end() &&
			(sceneManager_->IsDemoPlayActive() || sceneManager_->IsPlayerTypeUnlocked(currentType))) {
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
	isSelectionConfirming_ = false;
	selectionConfirmationElapsed_ = 0.0f;
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
	confirmationFlashSprite_ = CreateColorSprite();

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
	// 自動デモでは進行データを変更せず、未開放キャラクターも順番に検証できるよう表示を開放する。
	const bool unlocked = !sceneManager_ || sceneManager_->IsDemoPlayActive() || sceneManager_->IsPlayerTypeUnlocked(stats.name);
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
	presentedPlayerTypeName_ = stats.name;
	if (stats.modelFilePath.empty()) return;
	if (!ModelManager::GetInstance()->FindModel(stats.modelFilePath)) {
		ModelManager::GetInstance()->LoadModel(stats.modelFilePath, stats.isAnimationModel, GetModelDirectory(stats.modelFilePath));
	}
	if (!ModelManager::GetInstance()->FindModel(stats.modelFilePath)) return;

	characterModelObject_ = std::make_unique<GameObject>();
	characterModelObject_->SetName("PlayerSelectPreview");
	EulerTransform& transform = characterModelObject_->GetTransform();
	// UIの左カラム中央へ見えるよう、カメラ正面より左へモデルを配置する。
	transform.translate = kPreviewBasePosition;
	transform.rotate = {0.0f, kPreviewBaseRotationY, 0.0f};
	transform.scale = {kPreviewBaseScale, kPreviewBaseScale, kPreviewBaseScale};
	Object3dComponent* object3d = characterModelObject_->AddComponent<Object3dComponent>();
	object3d->SetModel(stats.modelFilePath);
	object3d->SetCamera(previewCamera_.get());
	object3d->SetDrawSkeleton(false);
	// セレクト画面では歩行を使わず、新規の専用クリップだけを一度再生して決めポーズを維持する。
	const std::string expectedEntranceAnimationName = GetEntranceAnimationName(stats.name);
	std::string resolvedEntranceAnimationName;
	if (stats.isAnimationModel) {
		const std::vector<std::string>& animationNames = object3d->GetAnimationNames();
		const auto expectedAnimation = std::find(animationNames.begin(), animationNames.end(), expectedEntranceAnimationName);
		if (expectedAnimation != animationNames.end()) {
			resolvedEntranceAnimationName = *expectedAnimation;
		} else if (animationNames.size() >= 2) {
			// Assimp側で名前が変換された場合も、追加した2番目のクリップを歩行の代わりに使用する。
			resolvedEntranceAnimationName = animationNames[1];
		}
	}
	if (!resolvedEntranceAnimationName.empty() && object3d->SetAnimation(resolvedEntranceAnimationName, true)) {
		object3d->SetAnimationLooping(false);
	} else {
		// 専用クリップを読み込めなかった場合も、既定の歩行へ戻さず初期姿勢で表示する。
		object3d->ResetAnimationPoseToInitial();
	}
	object3d->SetDirectionalLight({1.0f, 0.92f, 0.80f, 1.0f}, {-0.35f, -0.75f, 0.55f}, 1.35f);
	// プレビューでは衣装や肌の色を読み取りやすくし、環境マップ由来の鏡面反射をほぼ抑える。
	object3d->SetPointLight({0.30f, 0.62f, 1.0f, 1.0f}, {-3.2f, 2.8f, -2.0f}, 0.75f, 9.0f, 1.6f);
	object3d->SetEnvironmentMultiplier(0.02f);
	UpdateCharacterEntranceAnimation();
}

void PlayerSelectScene::UpdateCharacterEntranceAnimation() {
	if (!characterModelObject_) return;
	EulerTransform& transform = characterModelObject_->GetTransform();
	transform.translate = kPreviewBasePosition;
	transform.rotate = {0.0f, kPreviewBaseRotationY, 0.0f};
	transform.scale = {kPreviewBaseScale, kPreviewBaseScale, kPreviewBaseScale};

	const float entranceDuration = GetEntranceAnimationDuration(presentedPlayerTypeName_);
	const float normalizedTime = (std::min)(previewTime_ / entranceDuration, 1.0f);
	const float remaining = 1.0f - normalizedTime;

	if (presentedPlayerTypeName_ == "巫女") {
		// 巫女は上方から静かに降り、正面へ向き直りながら着地する。
		const float progress = SmoothStep(normalizedTime);
		transform.translate.y += remaining * 2.15f;
		transform.rotate.y += remaining * 0.58f;
		const float scale = kPreviewBaseScale * (0.84f + progress * 0.16f);
		transform.scale = {scale, scale, scale};
	} else if (presentedPlayerTypeName_ == "烏天狗") {
		// 烏天狗は左奥から一気に飛び込み、鋭く正面へ旋回する。
		const float progress = EaseOutCubic(normalizedTime);
		transform.translate.x -= (1.0f - progress) * 3.4f;
		transform.translate.y += (1.0f - progress) * 0.55f;
		transform.translate.z += (1.0f - progress) * 0.9f;
		transform.rotate.y -= (1.0f - progress) * 1.15f;
	} else if (presentedPlayerTypeName_ == "猫") {
		// 猫は右下から弧を描いて跳び込み、空中で伸びてから着地時の大きさへ戻る。
		const float progress = EaseOutCubic(normalizedTime);
		const float jumpArc = std::sin(normalizedTime * kPi);
		transform.translate.x += (1.0f - progress) * 1.35f;
		transform.translate.y += -(1.0f - progress) * 1.75f + jumpArc * 0.95f;
		transform.rotate.z = (1.0f - progress) * -0.20f;
		transform.scale.x = kPreviewBaseScale * (0.74f + progress * 0.26f + jumpArc * 0.07f);
		transform.scale.y = kPreviewBaseScale * (0.66f + progress * 0.34f + jumpArc * 0.18f);
		transform.scale.z = transform.scale.x;
	} else {
		// 将来追加されるタイプも、専用設定がなくても軽いポップ表示で破綻させない。
		const float progress = EaseOutCubic(normalizedTime);
		const float scale = kPreviewBaseScale * (0.72f + progress * 0.28f);
		transform.scale = {scale, scale, scale};
	}

	// 登場完了後は従来の小さな往復回転へ位相0からつなぎ、切り替わりを目立たせない。
	if (normalizedTime >= 1.0f) {
		transform.rotate.y += std::sin((previewTime_ - entranceDuration) * 0.75f) * 0.16f;
	}
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
	if (isSelectionConfirming_) {
		UpdateSelectionConfirmation();
		return;
	}
	Input* input = Input::GetInstance();
	const int playerCount = static_cast<int>(playerTypeNames_.size());
	if (input->TriggerKey(DIK_A) || input->TriggerKey(DIK_LEFT) || input->TriggerGamepadLeft()) selectedPlayerIndex_ = (selectedPlayerIndex_ + playerCount - 1) % playerCount;
	if (input->TriggerKey(DIK_D) || input->TriggerKey(DIK_RIGHT) || input->TriggerGamepadRight()) selectedPlayerIndex_ = (selectedPlayerIndex_ + 1) % playerCount;
	// 入力で位置が変わったフレームだけ、モデルと文字テクスチャを作り直す。
	if (selectedPlayerIndex_ != presentedPlayerIndex_) RefreshSelectionPresentation();

	// 選択変更直後はタイプ固有の登場モーションを流し、完了後は小さな待機回転へつなぐ。
	previewTime_ += GameTime::GetDeltaTime();
	if (characterModelObject_) {
		UpdateCharacterEntranceAnimation();
		characterModelObject_->Update();
	}
	if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) || input->TriggerGamepadButton(XINPUT_GAMEPAD_A)) {
		const std::string& selectedPlayerType = playerTypeNames_[selectedPlayerIndex_];
		if (sceneManager_->IsPlayerTypeUnlocked(selectedPlayerType)) {
			BeginSelectionConfirmation();
		}
		return;
	}
	if (input->TriggerKey(DIK_Q) || input->TriggerGamepadButton(XINPUT_GAMEPAD_B)) {
		sceneManager_->ChangeScene("STAGE_SELECT");
	}
}

void PlayerSelectScene::BeginSelectionConfirmation() {
	if (isSelectionConfirming_ || !sceneManager_ || selectedPlayerIndex_ < 0 ||
		selectedPlayerIndex_ >= static_cast<int>(playerTypeNames_.size())) return;

	isSelectionConfirming_ = true;
	selectionConfirmationElapsed_ = 0.0f;
	sceneManager_->SetSelectedPlayerTypeName(playerTypeNames_[selectedPlayerIndex_]);
	if (characterModelObject_) {
		selectionConfirmationBaseTransform_ = characterModelObject_->GetTransform();
	}
	instructionTextObject_->GetComponent<TextComponent>()->SetText("");
}

bool PlayerSelectScene::BeginDemoPlayerConfirmation(const std::string& playerTypeName) {
	if (isSelectionConfirming_ || !sceneManager_) {
		return false;
	}
	const auto playerType = std::find(playerTypeNames_.begin(), playerTypeNames_.end(), playerTypeName);
	if (playerType == playerTypeNames_.end()) {
		return false;
	}
	// デモ専用経路では通常の開放条件を変更せず、全キャラクターを検証対象にする。
	selectedPlayerIndex_ = static_cast<int>(std::distance(playerTypeNames_.begin(), playerType));
	if (selectedPlayerIndex_ != presentedPlayerIndex_) {
		RefreshSelectionPresentation();
	}
	BeginSelectionConfirmation();
	return isSelectionConfirming_;
}

void PlayerSelectScene::UpdateSelectionConfirmation() {
	selectionConfirmationElapsed_ += GameTime::GetDeltaTime();
	const float progress = (std::clamp)(selectionConfirmationElapsed_ / kSelectionConfirmationDuration, 0.0f, 1.0f);
	const float impact = std::sin(progress * kPi);

	if (characterModelObject_) {
		EulerTransform transform = selectionConfirmationBaseTransform_;
		const float scale = 1.0f + impact * 0.13f;
		transform.scale = scale * selectionConfirmationBaseTransform_.scale;
		transform.translate.z -= EaseOutCubic(progress) * 0.42f;
		transform.translate.y += impact * 0.10f;
		transform.rotate.y += impact * 0.12f;
		characterModelObject_->GetTransform() = transform;
		characterModelObject_->Update();
	}

	if (selectionConfirmationElapsed_ >= kSelectionConfirmationDuration) {
		sceneManager_->ChangeScene("GAMEPLAY");
	}
}

void PlayerSelectScene::Draw3D() {
	if (!previewCamera_ || !characterModelObject_) return;
	// ウィンドウ比率が変わってもモデルが横へ伸びないよう、描画直前に射影比を更新する。
	DirectXCommon* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	if (dxCommon && dxCommon->GetRenderHeight() > 0) previewCamera_->SetAspectRatio(static_cast<float>(dxCommon->GetRenderWidth()) / static_cast<float>(dxCommon->GetRenderHeight()));
	previewCamera_->Update();
	if (Object3dComponent* object3d = characterModelObject_->GetComponent<Object3dComponent>()) object3d->SetCamera(previewCamera_.get());
	// モデル更新はUpdateだけで行う。描画時にも更新すると骨格だけが倍速になり、フェード中にも進んでしまう。
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
	const float iconSize = (std::min)({96.0f, weaponHeight - 34.0f, sectionWidth * 0.24f});
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
	// 説明文は右端で自動改行し、縦にも収まらない場合は読みやすさを保てる範囲で縮小する。
	const float weaponTextRight = rightX + innerMargin + sectionWidth - 18.0f;
	const float weaponTextBottom = weaponY + weaponHeight - 14.0f;
	weaponNameTextObject_->GetComponent<TextComponent>()->SetLayoutBounds(
		(std::max)(1.0f, weaponTextRight - weaponNameTextObject_->GetTransform().translate.x), 34.0f);
	weaponTextObject_->GetComponent<TextComponent>()->SetLayoutBounds(
		(std::max)(1.0f, weaponTextRight - weaponTextObject_->GetTransform().translate.x),
		(std::max)(1.0f, weaponTextBottom - weaponTextObject_->GetTransform().translate.y));
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
	if (isSelectionConfirming_) DrawSelectionConfirmation(width, height);
}

void PlayerSelectScene::DrawSelectionConfirmation(float width, float height) {
	if (!confirmationFlashSprite_) return;
	const float progress = (std::clamp)(selectionConfirmationElapsed_ / kSelectionConfirmationDuration, 0.0f, 1.0f);
	const float flashOut = 1.0f - SmoothStep((std::min)(progress / 0.24f, 1.0f));

	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	DrawColorSprite(confirmationFlashSprite_.get(), 0.0f, 0.0f, width, height,
		{1.0f, 0.72f, 0.20f, flashOut * 0.30f});
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
	confirmationFlashSprite_.reset();
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
	presentedPlayerTypeName_.clear();
	BaseScene::Finalize();
}
