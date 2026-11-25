#include "SceneManager.h"

#include "object/Object3dCommon.h"
#include "particle/ParticleManager.h"
#include "base_scene/BaseScene.h"
#include "helpers/SceneUiUtility.h"
#include "Input.h"
#include "DemoSceneControl.h"
#include "GameProgressRepository.h"
#include "repositories/PlayerStatusRepository.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "StageUnlockConditionsRepository.h"
#include "TextureManager.h"
#include "sky/SkyBoxCommon.h"
#include "GameTime.h"
#include "PostEffect.h"
#include "StringUtility.h"

#include <assert.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <cmath>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {
const char* kSceneNames[] = {
    "TITLE",
	"PLAYER_SELECT",
	"STAGE_SELECT",
	"GAMEPLAY",
	"RESULT",
	"SHOP"
};

const char* kTransitionNames[] = {
	"None",
	"Fade",
	"Fade White",
	"Wipe Left",
	"Wipe Right",
	"Curtains",
	"Horizontal Bars"
};

// 外部画像を必要としないよう、遷移色の下地には実行時生成する白1pxテクスチャを使用する。
constexpr const char* kTransitionTextureKey = "__scene_transition_white";
constexpr const char* kTransitionFogTexturePath = "Resources/Texture/fog_transition.png";
// 自動デモ用の配置パターンは通常のステージ選択には表示しません。
constexpr std::string_view kDemoStageIdPrefix = "demo";

float SmoothStep(float start, float end, float value) {
	const float rate = std::clamp((value - start) / (end - start), 0.0f, 1.0f);
	return rate * rate * (3.0f - 2.0f * rate);
}

void ApplyFadeWhiteFog(float coverage, bool keepBaseFog) {
	// タイトル終了時に濃くした霧と同じ値を逆向きに辿る。
	const float fogRate = SmoothStep(0.0f, 1.0f, coverage);
	// 逆再生の終端では、遷移先へ薄い霧を残さないよう最後だけ穏やかに消す。
	const float visibility = keepBaseFog ? 1.0f : SmoothStep(0.0f, 0.16f, coverage);
	// タイトルでは待機時の粒量まで滑らかに戻し、終端で0から0.08へ跳ねないようにする。
	const float baseParticleAmount = keepBaseFog ? 0.08f : 0.0f;
	const float particleAmount = baseParticleAmount + (1.0f - baseParticleAmount) * fogRate;
	const Vector4 fogColor = {
		0.26f + (1.0f - 0.26f) * fogRate,
		0.31f + (1.0f - 0.31f) * fogRate,
		0.40f + (1.0f - 0.40f) * fogRate,
		(0.82f + (1.0f - 0.82f) * fogRate) * visibility};
	PostEffect::GetInstance()->SetFogSettings(
		true, fogColor,
		0.68f + 1.22f * fogRate,
		0.54f - 0.28f * fogRate,
		3.8f,
		0.48f - 0.16f * fogRate,
		particleAmount,
		0.028f + 0.018f * fogRate,
		4.5f * (1.0f - fogRate),
		52.0f + (9.0f - 52.0f) * fogRate,
		true);
}
/// <summary>
/// SceneIndex を検索して取得します。
/// </summary>
/// <param name="sceneName">対象となるシーン名を指定します。</param>
int FindSceneIndex(const std::string& sceneName) {
	for (int index = 0; index < static_cast<int>(_countof(kSceneNames)); ++index) {
		if (sceneName == kSceneNames[index]) {
			return index;
		}
	}
	return 0;
}
}

SceneManager::SceneManager() {
	// タイトルやショップの初回表示前に、前回終了時の所持金と購入状況を復元する。
	LoadGameProgress();
	// プレイヤーの進行データとは分離して、制作者がImGuiで設定した開放ルールを復元する。
	LoadStageUnlockConditions();
	RefreshGameplayStagePatterns();
	// 白1pxを任意サイズ・任意色へ拡大し、全パターンを同じスプライト群で構成する。
	TextureManager::GetInstance()->CreateTextureFromRGBA(kTransitionTextureKey, 1, 1, {255, 255, 255, 255});
	TextureManager::GetInstance()->LoadTexture(kTransitionFogTexturePath);
	for (auto& sprite : transitionSprites_) {
		sprite = std::make_unique<Sprite>();
		sprite->Initialize(kTransitionTextureKey);
		sprite->SetAnchorPoint({0.0f, 0.0f});
	}
	demoGuideBackgroundSprite_ = std::make_unique<Sprite>();
	demoGuideBackgroundSprite_->Initialize(kTransitionTextureKey);
	demoGuideBackgroundSprite_->SetAnchorPoint({0.0f, 0.0f});
	demoGuideTextObject_ = SceneUiUtility::CreateTextObject(
		"デモプレイ再生中    F1: 停止", 20.0f,
		Vector4{1.0f, 0.94f, 0.72f, 1.0f}, "Yu Gothic UI");
	demoParticleGuideTextObject_ = SceneUiUtility::CreateTextObject(
		"", 19.0f, TextComponent::Anchor::TopLeft, "Yu Gothic UI");
	demoParticleGuideTextObject_->GetComponent<TextComponent>()->SetColor(
		{0.90f, 0.96f, 1.0f, 1.0f});
	demoParticleGuideBackgroundSprite_ = SceneUiUtility::CreateColorSprite();
}

void SceneManager::LoadGameProgress() {
	GameProgressSaveData loaded;
	const GameProgressLoadStatus status = GameProgressRepository::Load(loaded);
	if (status == GameProgressLoadStatus::NotFound) {
		// 初回起動では各クラスの初期値をそのまま使用する。
		return;
	}
	if (status == GameProgressLoadStatus::Invalid) {
		// 破損をファイル不在と区別し、安全な新規ゲーム状態へ明示的に戻す。
		gameProgress_.ResetEconomy();
		gameProgress_.ResetUnlocks();
		return;
	}

	// JSONの解釈はRepositoryへ任せ、ここではゲーム内で有効なIDだけを進行状態へ反映する。
	gameProgress_.RestoreEconomy(loaded.money, loaded.shopUpgradeLevels);
	// 旧形式のキャラクター別レベルはGameProgressが共通レベルへ変換済み。
	// 能力定義ファイルは変更せず、ゲーム開始時に共通レベルから実効値を計算する。
	std::unordered_set<std::string> validUnlockedStageIds;
	std::unordered_set<std::string> validClearedStageIds;
	for (const std::string& stageId : loaded.unlockedGameplayStageIds) {
		if (IsValidGameplayStageId(stageId)) validUnlockedStageIds.insert(stageId);
	}
	for (const std::string& stageId : loaded.clearedGameplayStageIds) {
		if (IsValidGameplayStageId(stageId)) validClearedStageIds.insert(stageId);
	}
	gameProgress_.RestoreUnlocks(
		validUnlockedStageIds, validClearedStageIds,
		loaded.unlockedPlayerTypeNames, stageUnlockCatalog_.GetPlayerPrerequisites());
}

bool SceneManager::SaveGameProgress() const {
	GameProgressSaveData data;
	data.money = gameProgress_.GetMoney();
	data.shopUpgradeLevels = gameProgress_.GetShopUpgradeLevels();
	data.unlockedGameplayStageIds = gameProgress_.GetUnlockedGameplayStageIds();
	data.clearedGameplayStageIds = gameProgress_.GetClearedGameplayStageIds();
	data.unlockedPlayerTypeNames = gameProgress_.GetUnlockedPlayerTypeNames();
	// SceneManagerは保存形式を知らず、現在の進行値をRepositoryへ渡すだけにする。
	return GameProgressRepository::Save(data);
}

bool SceneManager::IsPlayerTypeUnlocked(const std::string& playerTypeName) const {
	// プレイヤー選択画面はこの関数だけを参照し、保存形式や開放条件の詳細には依存させない。
	const std::string prerequisiteStageId = GetPlayerUnlockPrerequisiteStage(playerTypeName);
	return gameProgress_.IsPlayerTypeUnlocked(playerTypeName, prerequisiteStageId);
}

std::string SceneManager::GetPlayerUnlockPrerequisiteStage(const std::string& playerTypeName) const {
	return stageUnlockCatalog_.GetPlayerPrerequisite(playerTypeName);
}

void SceneManager::LoadStageUnlockConditions() {
	std::unordered_map<std::string, std::string> loadedConditions;
	const StageUnlockConditionsLoadStatus status = StageUnlockConditionsRepository::Load(loadedConditions);
	if (status == StageUnlockConditionsLoadStatus::Loaded) {
		// Repositoryが検証した対応表だけを採用し、不在・破損時は既定条件を維持する。
		stageUnlockCatalog_.ReplaceStagePrerequisites(loadedConditions);
	}
}

void SceneManager::SaveStageUnlockConditions() const {
	// SceneManagerは条件表だけを渡し、保存形式と並び順はRepositoryへ任せる。
	StageUnlockConditionsRepository::Save(stageUnlockCatalog_.GetStagePrerequisites());
}

bool SceneManager::IsGameplayStageUnlocked(const std::string& stageId) const {
	const std::string prerequisite = GetGameplayStageUnlockPrerequisite(stageId);
	return gameProgress_.IsGameplayStageUnlocked(stageId, prerequisite);
}

std::string SceneManager::GetGameplayStageUnlockPrerequisite(const std::string& stageId) const {
	return stageUnlockCatalog_.GetStagePrerequisite(stageId);
}

void SceneManager::RecordGameplayStageClear(const std::string& stageId) {
	// 開放ルールの判定はGameProgressへ委譲し、SceneManagerは条件表と保存契機だけを渡す。
	const bool progressChanged = gameProgress_.RecordGameplayStageClear(
		stageId, stageUnlockCatalog_.GetStagePrerequisites(), stageUnlockCatalog_.GetPlayerPrerequisites());
	if (progressChanged) {
		// リザルト画面へ移る前に即時保存し、直後にゲームを閉じても進行を失わないようにする。
		SaveGameProgress();
	}
}

void SceneManager::ResetGameProgress() {
	// ファイルだけでなく実行中の値も初期化し、再起動せずそのまま新しいゲームを始められるようにする。
	gameProgress_.ResetEconomy();
	gameProgress_.ResetUnlocks();
	selectedPlayerTypeName_ = "巫女";
	selectedGameplayStageId_ = "default";
	activeGameplayStageId_ = "default";
	selectedGameplayStageIndex_ = 0;
	stageResultData_ = {};

	// 初期状態で上書きし、以前の進行が次回起動時に復元されないことを保証する。
	SaveGameProgress();
}

void SceneManager::AddMoney(int amount) {
	// 金額の妥当性と加算はGameProgressへ委譲し、変更がない場合は保存しない。
	if (!gameProgress_.AddMoney(amount)) return;
	// 正常終了を待たず、その場で保存して強制終了時に失う進捗を最小限にする。
	SaveGameProgress();
}

bool SceneManager::SpendMoney(int amount) {
	// 不正価格と残高判定をGameProgressへ集約し、SceneManagerは永続化だけを担当する。
	if (!gameProgress_.SpendMoney(amount)) return false;
	// 購入直後の残高を永続化し、次回起動時の所持金表示へ反映する。
	SaveGameProgress();
	return true;
}

ShopPurchaseResult SceneManager::PurchaseGlobalShopUpgrade(
	const std::string& upgradeId, int price, int maxLevel) {
	// 保存に失敗した場合にメモリ上の残高と購入レベルも購入前へ戻せるようスナップショットを保持する。
	const GameProgress previousProgress = gameProgress_;
	const ShopPurchaseResult result = gameProgress_.PurchaseGlobalUpgrade(upgradeId, price, maxLevel);
	if (result != ShopPurchaseResult::Purchased) return result;
	if (SaveGameProgress()) return ShopPurchaseResult::Purchased;
	gameProgress_ = previousProgress;
	return ShopPurchaseResult::SaveFailed;
}

int SceneManager::GetShopUpgradeLevelForPlayer(
	const std::string& playerTypeName, const std::string& upgradeId) const {
	return gameProgress_.GetPlayerUpgradeLevel(playerTypeName, upgradeId);
}

void SceneManager::SetShopUpgradeLevelForPlayer(
	const std::string& playerTypeName, const std::string& upgradeId, int level) {
	const std::vector<std::string> playerTypeNames = LoadPlayerTypeNames();
	if (upgradeId.empty() || playerTypeName == "Default" ||
	    std::find(playerTypeNames.begin(), playerTypeNames.end(), playerTypeName) == playerTypeNames.end()) {
		return;
	}
	// 共通ステータス強化を未開放キャラクターにも適用し、適用済みレベルを同期マーカーとして保存する。
	gameProgress_.SetPlayerUpgradeLevel(playerTypeName, upgradeId, level);
	SaveGameProgress();
}

int SceneManager::GetGlobalShopUpgradeLevel(const std::string& upgradeId) const {
	return gameProgress_.GetGlobalUpgradeLevel(upgradeId);
}

void SceneManager::SetGlobalShopUpgradeLevel(const std::string& upgradeId, int level) {
	gameProgress_.SetGlobalUpgradeLevel(upgradeId, level);
	// 購入直後に保存し、ゲームを閉じても全体強化レベルが失われないようにする。
	SaveGameProgress();
}

float SceneManager::GetGlobalExperienceBonusPercent() const {
	return gameProgress_.GetGlobalExperienceBonusPercent();
}

int SceneManager::ApplyGlobalGoldBonus(int baseAmount) const {
	return gameProgress_.ApplyGlobalGoldBonus(baseAmount);
}

PlayerStats SceneManager::ApplyGlobalShopStatUpgrades(const PlayerStats& stats) const {
	PlayerStats effective = stats;
	// 商品定義JSONではなく進行スナップショットから実効値を計算し、新規ゲームで基礎値へ戻せるようにする。
	effective.baseHealth += 10.0f * gameProgress_.GetGlobalUpgradeLevel("health");
	effective.attack += 5.0f * gameProgress_.GetGlobalUpgradeLevel("attack");
	effective.attackSpeed += 5.0f * gameProgress_.GetGlobalUpgradeLevel("attack_speed");
	effective.defense += 2.0f * gameProgress_.GetGlobalUpgradeLevel("defense");
	effective.baseSpeed += 0.01f * gameProgress_.GetGlobalUpgradeLevel("speed");
	return effective;
}

/// <summary>
/// 毎フレームの状態更新を行います。
/// </summary>
void SceneManager::Update() {
	// 起動直後の最初のシーンだけは演出を待たずに読み込みます。
	if (nextScene_ && transitionPhase_ == TransitionPhase::Idle) {
		SwitchToNextScene();
	}
	UpdateTransition();
	UpdateDemoPlay();

	if (scene_) {
		// FadeWhiteのOut中は退出演出を進め、タイトルへ戻るIn中は黒幕を開いて減少する深度霧を見せます。
		const bool updatesBehindFadeWhite =
			transitionPhase_ == TransitionPhase::Out && activeTransitionType_ == SceneTransitionType::FadeWhite;
		const bool revealsTitleBehindFadeWhite =
			transitionPhase_ == TransitionPhase::In && activeTransitionType_ == SceneTransitionType::FadeWhite &&
			currentSceneName_ == "TITLE";
		const bool revealsDemo = demoDirector_.IsActive() && currentSceneName_ == "GAMEPLAY" &&
			transitionPhase_ == TransitionPhase::In;
		const bool shouldUpdateScene =
			(transitionPhase_ == TransitionPhase::Idle || updatesBehindFadeWhite || revealsTitleBehindFadeWhite || revealsDemo) &&
		    (isScenePlaying_ || isFrameStepRequested_);
		if (shouldUpdateScene) {
			scene_->Update();
			// ポーズ中もシーン固有のメニュー入力は受け付けるが、ゲーム世界は進めない。
			if (!GameTime::IsPaused()) {
				scene_->UpdateSceneObjects();
				// エディタ配置を含む全エミッターが生成したパーティクルを共通経路で更新します。
				ParticleManager::GetInstance()->Update();
			} else {
				scene_->UpdateEditorTools();
			}
		} else {
			scene_->UpdateEditorTools();
		}
		isFrameStepRequested_ = false;
		if (!demoDirector_.IsActive()) {
			scene_->DrawEditorImGui();
			// 共通ツールバーとステージパターン操作を最後に描き、シーン固有パネルより前面に表示します。
			DrawEditorImGui();
		}
	}
}

void SceneManager::UpdateDemoPlay() {
	Input* input = Input::GetInstance();
	if (input && input->TriggerKey(DIK_F1)) {
		demoToggleRequested_ = !demoToggleRequested_;
	}
	if (transitionPhase_ != TransitionPhase::Idle || nextScene_) return;

	if (demoToggleRequested_) {
		demoToggleRequested_ = false;
		isScenePlaying_ = true;
		GameTime::SetPaused(false);
		if (demoDirector_.Toggle()) {
			stageBeforeDemo_ = selectedGameplayStageId_;
			SetSelectedGameplayStageId("stage2");
		} else {
			SetSelectedGameplayStageId(stageBeforeDemo_);
			ChangeScene("TITLE", SceneTransitionType::Fade);
			return;
		}
	}
	if (demoDirector_.ShouldStartGameplay(currentSceneName_, GameTime::GetUnscaledDeltaTime())) {
		ChangeScene("GAMEPLAY", SceneTransitionType::Fade, 0.35f, 0.35f);
	}
}

/// <summary>
/// スカイボックスの描画処理を行います。
/// </summary>
void SceneManager::DrawSkyBox() {
	if (scene_) {
		scene_->DrawSkyBox();
	}
}

/// <summary>
/// 2D 要素の描画処理を行います。
/// </summary>
void SceneManager::Draw2D() {
	if (scene_) {
		scene_->Draw2D();
		SpriteCommon::GetInstance()->SetDraw(kBlendModeNone);
		scene_->DrawSceneObjects2D();
		scene_->DrawOverlay2D();
	}
	// シーン内の2D要素より後に描き、遷移色を常にゲーム画面の最前面へ置く。
	DrawTransition();
	// デモ停止方法は遷移中も見失わないよう、遷移オーバーレイよりさらに手前へ表示する。
	DrawDemoPlayGuide();
}

void SceneManager::DrawDemoPlayGuide() {
	if (!demoDirector_.IsActive() || !demoGuideBackgroundSprite_ || !demoGuideTextObject_) {
		return;
	}
	DirectXCommon* dxCommon = SpriteCommon::GetInstance()->GetDxCommon();
	if (!dxCommon) {
		return;
	}

	const float screenWidth = static_cast<float>(dxCommon->GetRenderWidth());
	const float screenHeight = static_cast<float>(dxCommon->GetRenderHeight());
	const float panelWidth = 330.0f;
	const float panelHeight = 46.0f;
	const float panelX = (std::max)(16.0f, screenWidth - panelWidth - 20.0f);
	const bool isGameplay = currentSceneName_ == "GAMEPLAY";
	const float panelY = isGameplay ? (std::max)(18.0f, screenHeight - panelHeight - 56.0f) : 18.0f;
	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	SceneUiUtility::DrawColorSprite(
		demoGuideBackgroundSprite_.get(), panelX, panelY, panelWidth, panelHeight,
		{0.02f, 0.03f, 0.06f, 0.82f});
	demoGuideTextObject_->GetTransform().translate = {
		panelX + panelWidth * 0.5f, panelY + panelHeight * 0.5f, 0.0f};
	demoGuideTextObject_->Draw2D();

	if (!isGameplay || !demoParticleGuideTextObject_) {
		return;
	}

	const auto describeParticleLocation = [](const std::string& groupName) -> std::string {
		if (groupName == "MagatamaGlow") return "飛んでいる勾玉の周囲（発光）";
		if (groupName == "RuntimePlayerHitSpark") return "攻撃が敵に命中した位置";
		if (groupName == "EnemyDeathBurst") return "敵・ボスを倒した位置";
		if (groupName == "RuntimeDropSparkle") return "ドロップアイテムの周囲";
		if (groupName == "RuntimeTornadoWispParticle") return "竜巻攻撃の軌道";
		if (groupName == "RuntimeSelfDestructCore") return "自爆敵の中心（閃光）";
		if (groupName == "RuntimeSelfDestructFlame") return "自爆敵の周囲（爆炎）";
		if (groupName == "RuntimeSelfDestructSpark") return "自爆敵から外向き（火花）";
		if (groupName == "RuntimeSelfDestructShockwave") return "自爆地点の地面（衝撃波）";
		if (groupName.rfind("GlowBillboard_", 0) == 0) return "発光オブジェクトの位置";
		return groupName;
	};

	const auto activeGroups = ParticleManager::GetInstance()->GetActiveParticleGroups();
	std::string guideText = "パーティクル使用箇所  /  発生中\n";
	if (activeGroups.empty()) {
		guideText += "・発生待ち（攻撃・命中・撃破時に表示）";
	} else {
		constexpr size_t kMaxGuideGroups = 5;
		const size_t groupCount = (std::min)(activeGroups.size(), kMaxGuideGroups);
		for (size_t index = 0; index < groupCount; ++index) {
			guideText += "・" + describeParticleLocation(activeGroups[index].first) +
				"  ×" + std::to_string(activeGroups[index].second);
			if (index + 1 < groupCount) guideText += "\n";
		}
	}
	if (const auto* gameplay = dynamic_cast<const IDemoGameplayControl*>(scene_.get())) {
		guideText += "\n";
		guideText += gameplay->GetDemoParticleCue();
	}

	constexpr float particlePanelX = 20.0f;
	constexpr float particlePanelY = 70.0f;
	constexpr float particlePanelWidth = 520.0f;
	const float particlePanelHeight = 76.0f + static_cast<float>((std::min)(activeGroups.size(), size_t{5})) * 25.0f;
	SceneUiUtility::DrawColorSprite(
		demoParticleGuideBackgroundSprite_.get(), particlePanelX, particlePanelY,
		particlePanelWidth, particlePanelHeight, {0.015f, 0.035f, 0.075f, 0.88f});
	TextComponent* guideTextComponent = demoParticleGuideTextObject_->GetComponent<TextComponent>();
	guideTextComponent->SetText(guideText);
	guideTextComponent->SetLayoutBounds(particlePanelWidth - 32.0f, particlePanelHeight - 24.0f);
	demoParticleGuideTextObject_->GetTransform().translate = {
		particlePanelX + 16.0f, particlePanelY + 12.0f, 0.0f};
	demoParticleGuideTextObject_->Draw2D();
}

/// <summary>
/// 3D 要素の描画処理を行います。
/// </summary>
void SceneManager::Draw3D() {
	if (scene_) {
		scene_->Draw3D();
		Object3dCommon::GetInstance()->SetDraw();
		scene_->DrawSceneObjects3D();
		// 個別シーンで描画を呼び忘れないよう、現在のカメラを使ってここで一括描画します。
		Camera* camera = Object3dCommon::GetInstance()->GetDefaultCamera();
		if (camera && scene_->IsParticleRenderingEnabled()) {
			ParticleManager::GetInstance()->Draw(camera);
		}
	}
}

void SceneManager::DrawShadowMap() {
	if (scene_) scene_->DrawSceneObjectShadowMaps();
}

void SceneManager::DrawEditorImGui() {
#ifdef USE_IMGUI
	Input* input = Input::GetInstance();
	if (!ImGui::GetIO().WantCaptureKeyboard) {
		if (input && input->TriggerKey(DIK_P)) {
			isScenePlaying_ = !isScenePlaying_;
		}
		if (input && input->TriggerKey(DIK_O)) {
			isFrameStepRequested_ = true;
			isScenePlaying_ = false;
		}
	}

	ImGui::Begin("Editor Toolbar", nullptr, ImGuiWindowFlags_NoCollapse);

	ImGui::Text("Scene: %s", currentSceneName_.c_str());
	ImGui::SameLine();
	if (nextScene_) {
		ImGui::Text("Next: %s", nextSceneName_.c_str());
		ImGui::SameLine();
	}

	ImGui::SetNextItemWidth(160.0f);
	ImGui::Combo("##Scene", &selectedSceneIndex_, kSceneNames, _countof(kSceneNames));
	ImGui::SameLine();

	const bool canChange = !nextScene_ && currentSceneName_ != kSceneNames[selectedSceneIndex_];
	if (!canChange) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Change Scene")) {
		ChangeScene(kSceneNames[selectedSceneIndex_]);
	}
	if (!canChange) {
		ImGui::EndDisabled();
	}
	ImGui::SameLine();
	if (ImGui::Button(isScenePlaying_ ? "Pause" : "Play")) {
		isScenePlaying_ = !isScenePlaying_;
	}
	ImGui::SameLine();
	const bool canStepFrame = !isScenePlaying_;
	if (!canStepFrame) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Step")) {
		isFrameStepRequested_ = true;
	}
	if (!canStepFrame) {
		ImGui::EndDisabled();
	}
	ImGui::SameLine();
	ImGui::Text("%s", isScenePlaying_ ? "Playing" : "Paused");

	int transitionType = static_cast<int>(transitionSettings_.type);
	// エディターから既定パターン、往復時間、色を実行中に調整できるようにする。
	ImGui::SetNextItemWidth(150.0f);
	if (ImGui::Combo("Transition", &transitionType, kTransitionNames, _countof(kTransitionNames))) {
		transitionSettings_.type = static_cast<SceneTransitionType>(transitionType);
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.0f);
	ImGui::DragFloat("Out sec", &transitionSettings_.outDuration, 0.01f, 0.0f, 5.0f, "%.2f");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.0f);
	ImGui::DragFloat("In sec", &transitionSettings_.inDuration, 0.01f, 0.0f, 5.0f, "%.2f");
	ImGui::SameLine();
	ImGui::ColorEdit4("Color", &transitionSettings_.color.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
	SetTransitionSettings(transitionSettings_);

	ImGui::End();
	if (currentSceneName_ != "GAMEPLAY") {
		return;
	}

	// ゲームプレイシーンの左右パネルに隠れないよう、中央上部へ独立表示します。
	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	const float displayWidth = displaySize.x > 0.0f ? displaySize.x : 1280.0f;
	const float leftPanelWidth = displayWidth * 0.18f;
	const float rightPanelWidth = displayWidth * 0.24f;
	const float availableStagePanelWidth = displayWidth - leftPanelWidth - rightPanelWidth;
	const float stagePanelWidth = availableStagePanelWidth > 420.0f ? availableStagePanelWidth : 420.0f;
	const float stagePanelHeight = currentSceneName_ == "GAMEPLAY" ? 166.0f : 82.0f;
	ImGui::SetNextWindowPos(ImVec2(leftPanelWidth, 0.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(stagePanelWidth, stagePanelHeight), ImGuiCond_Always);
	const ImGuiWindowFlags stagePanelFlags =
	    ImGuiWindowFlags_NoMove |
	    ImGuiWindowFlags_NoCollapse |
	    ImGuiWindowFlags_NoSavedSettings;
	ImGui::Begin("Gameplay Stage Patterns", nullptr, stagePanelFlags);

	if (gameplayStageIds_.empty()) {
		RefreshGameplayStagePatterns();
	}
	std::vector<const char*> stageNames;
	stageNames.reserve(gameplayStageIds_.size());
	for (const std::string& stageId : gameplayStageIds_) {
		stageNames.push_back(stageId.c_str());
	}
	ImGui::Text("Gameplay Stage Pattern");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(180.0f);
	if (ImGui::Combo("##GameplayStagePattern", &selectedGameplayStageIndex_, stageNames.data(), static_cast<int>(stageNames.size()))) {
		selectedGameplayStageId_ = gameplayStageIds_[selectedGameplayStageIndex_];
	}
	ImGui::SameLine();
	if (ImGui::Button("Rescan Patterns")) {
		RefreshGameplayStagePatterns();
	}

	// 選択中ステージを「常時開放」または別ステージのクリアで開放するよう設定する。
	std::vector<std::string> prerequisiteIds = {""};
	std::vector<std::string> prerequisiteLabels = {"None (Always Unlocked)"};
	// 自分自身は前提にできないため候補から除外し、先頭の空IDを「条件なし」として扱う。
	for (const std::string& stageId : gameplayStageIds_) {
		if (stageId != selectedGameplayStageId_) {
			prerequisiteIds.push_back(stageId);
			prerequisiteLabels.push_back(stageId);
		}
	}
	std::vector<const char*> prerequisiteLabelPointers;
	prerequisiteLabelPointers.reserve(prerequisiteLabels.size());
	// ImGui::Comboへ渡すポインターは、このフレーム中存続する文字列配列を参照させる。
	for (const std::string& label : prerequisiteLabels) {
		prerequisiteLabelPointers.push_back(label.c_str());
	}
	const std::string currentPrerequisite = GetGameplayStageUnlockPrerequisite(selectedGameplayStageId_);
	int prerequisiteIndex = 0;
	const auto prerequisiteFound = std::find(prerequisiteIds.begin(), prerequisiteIds.end(), currentPrerequisite);
	if (prerequisiteFound != prerequisiteIds.end()) {
		prerequisiteIndex = static_cast<int>(std::distance(prerequisiteIds.begin(), prerequisiteFound));
	}
	ImGui::SetNextItemWidth(180.0f);
	if (selectedGameplayStageId_ == "default") {
		// 最初から遊べる入口がなくなることを防ぐため、defaultの開放条件は編集不可にする。
		ImGui::BeginDisabled();
	}
	if (ImGui::Combo("Unlock After Clear", &prerequisiteIndex, prerequisiteLabelPointers.data(),
	    static_cast<int>(prerequisiteLabelPointers.size()))) {
		const std::string& newPrerequisite = prerequisiteIds[prerequisiteIndex];
		if (newPrerequisite.empty()) {
			// 対応表から削除されたステージは、IsGameplayStageUnlockedで常時開放として扱われる。
			stageUnlockCatalog_.RemoveStagePrerequisite(selectedGameplayStageId_);
			SaveStageUnlockConditions();
			gameplayStageMessage_ = "Always unlocked: " + selectedGameplayStageId_;
		} else if (!stageUnlockCatalog_.SetStagePrerequisite(selectedGameplayStageId_, newPrerequisite)) {
			gameplayStageMessage_ = "Unlock condition would create a cycle.";
		} else {
			// 編集結果は決定したフレームで保存し、ゲーム再起動後も同じルールを使用する。
			SaveStageUnlockConditions();
			gameplayStageMessage_ = "Unlock " + selectedGameplayStageId_ + " after clearing " + newPrerequisite;
		}
	}
	if (selectedGameplayStageId_ == "default") {
		ImGui::EndDisabled();
	}
	if (currentSceneName_ == "GAMEPLAY" && scene_) {
		ImGui::SameLine();
		const bool canLoadPattern = activeGameplayStageId_ != selectedGameplayStageId_;
		if (!canLoadPattern) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Load Selected Pattern")) {
			activeGameplayStageId_ = selectedGameplayStageId_;
			ApplyGameplayStagePath(scene_.get(), activeGameplayStageId_);
			scene_->LoadEditorObjects();
			gameplayStageMessage_ = "Loaded: " + activeGameplayStageId_;
		}
		if (!canLoadPattern) {
			ImGui::EndDisabled();
		}

		ImGui::SetNextItemWidth(180.0f);
		ImGui::InputTextWithHint("##NewGameplayStageId", "new pattern id", newGameplayStageIdBuffer_.data(), newGameplayStageIdBuffer_.size());
		ImGui::SameLine();
		if (ImGui::Button("Duplicate Current Pattern")) {
			const std::string newStageId = newGameplayStageIdBuffer_.data();
			if (!IsValidGameplayStageId(newStageId) || newStageId == "default") {
				gameplayStageMessage_ = "Use letters, numbers, '-' or '_' (not 'default').";
			} else if (std::filesystem::exists(StringUtility::Utf8ToPath(GetGameplayStageJsonPath(newStageId)))) {
				gameplayStageMessage_ = "Pattern already exists: " + newStageId;
			} else {
				selectedGameplayStageId_ = newStageId;
				activeGameplayStageId_ = newStageId;
				ApplyGameplayStagePath(scene_.get(), activeGameplayStageId_);
				scene_->SaveEditorObjects();
				RefreshGameplayStagePatterns();
				newGameplayStageIdBuffer_.fill('\0');
				gameplayStageMessage_ = "Created: " + newStageId;
			}
		}
		ImGui::SameLine();
		ImGui::Text("Active: %s", activeGameplayStageId_.c_str());
		ImGui::SameLine();
		if (ImGui::Button("Save Current Pattern")) {
			scene_->SaveEditorObjects();
			gameplayStageMessage_ = "Saved: " + activeGameplayStageId_;
		}
	}
	if (!gameplayStageMessage_.empty()) {
		ImGui::TextUnformatted(gameplayStageMessage_.c_str());
	}

	ImGui::End();
#endif
}

bool SceneManager::ReloadCurrentSceneJson() {
	// 遷移予約中に現在シーンを再構築すると所有権交換と競合するため、この間は見送る。
	if (!scene_ || nextScene_) {
		return false;
	}
	scene_->LoadEditorObjects();
	return true;
}

std::string SceneManager::GetCurrentSceneJsonPath() const {
	if (!scene_ || currentSceneName_ == "None") {
		return {};
	}
	// GAMEPLAYの派生ステージなど、固定命名ではなくシーンが実際に参照中のパスを監視する。
	return scene_->GetSceneObjectFilePath();
}

void SceneManager::RefreshGameplayStagePatterns() {
	const std::string keepSelectedId = selectedGameplayStageId_;
	gameplayStageIds_.clear();
	gameplayStageIds_.push_back("default");

	const std::filesystem::path sceneDirectory = StringUtility::Utf8ToPath("Resources/Data/Scenes");
	std::error_code error;
	for (std::filesystem::directory_iterator iterator(sceneDirectory, error), end; !error && iterator != end; iterator.increment(error)) {
		if (!iterator->is_regular_file() || iterator->path().extension() != ".json") {
			continue;
		}
		const std::string fileName = StringUtility::PathToUtf8(iterator->path().filename());
		constexpr const char* prefix = "GAMEPLAY_";
		constexpr const char* suffix = "_objects.json";
		if (fileName == "GAMEPLAY_objects.json" || fileName.size() <= std::strlen(prefix) + std::strlen(suffix)) {
			continue;
		}
		if (fileName.compare(0, std::strlen(prefix), prefix) != 0 ||
		    fileName.compare(fileName.size() - std::strlen(suffix), std::strlen(suffix), suffix) != 0) {
			continue;
		}
		const std::string stageId = fileName.substr(
		    std::strlen(prefix),
		    fileName.size() - std::strlen(prefix) - std::strlen(suffix));
		if (IsValidGameplayStageId(stageId)) {
			gameplayStageIds_.push_back(stageId);
		}
	}
	std::sort(gameplayStageIds_.begin() + 1, gameplayStageIds_.end());
	gameplayStageIds_.erase(std::unique(gameplayStageIds_.begin(), gameplayStageIds_.end()), gameplayStageIds_.end());
	selectedGameplayStageIndex_ = FindGameplayStageIndex(keepSelectedId);
	selectedGameplayStageId_ = gameplayStageIds_[selectedGameplayStageIndex_];
}

void SceneManager::SetSelectedGameplayStageId(const std::string& stageId) {
	if (!IsValidGameplayStageId(stageId)) {
		return;
	}
	RefreshGameplayStagePatterns();
	const int index = FindGameplayStageIndex(stageId);
	if (gameplayStageIds_[index] == stageId) {
		selectedGameplayStageId_ = stageId;
		selectedGameplayStageIndex_ = index;
	}
}

std::string SceneManager::GetGameplayStageDisplayName(const std::string& stageId) const {
	// 配置ファイルやセーブデータでは内部IDを維持し、画面表示だけを日本語名へ置き換える。
	if (stageId == "default") return "平原";
	if (stageId == "stage2") return "里";
	return stageId;
}

std::vector<std::string> SceneManager::GetSelectableGameplayStageIds() const {
	std::vector<std::string> selectableStageIds;
	selectableStageIds.reserve(gameplayStageIds_.size());
	for (const std::string& stageId : gameplayStageIds_) {
		if (stageId.rfind(kDemoStageIdPrefix, 0) != 0) {
			selectableStageIds.push_back(stageId);
		}
	}
	return selectableStageIds;
}

std::string SceneManager::GetGameplayStageJsonPath(const std::string& stageId) const {
	if (demoDirector_.IsActive() && stageId == "stage2") {
		return "Resources/Data/Scenes/GAMEPLAY_demo_stage2_objects.json";
	}
	if (stageId.empty() || stageId == "default") {
		return "Resources/Data/Scenes/GAMEPLAY_objects.json";
	}
	return "Resources/Data/Scenes/GAMEPLAY_" + stageId + "_objects.json";
}

void SceneManager::ApplyGameplayStagePath(BaseScene* scene, const std::string& stageId) const {
	if (scene) {
		scene->SetSceneObjectFilePathOverride(GetGameplayStageJsonPath(stageId));
	}
}

bool SceneManager::IsValidGameplayStageId(const std::string& stageId) const {
	if (stageId.empty()) {
		return false;
	}
	return std::all_of(stageId.begin(), stageId.end(), [](unsigned char character) {
		return std::isalnum(character) || character == '-' || character == '_';
	});
}

int SceneManager::FindGameplayStageIndex(const std::string& stageId) const {
	const auto found = std::find(gameplayStageIds_.begin(), gameplayStageIds_.end(), stageId);
	return found == gameplayStageIds_.end() ? 0 : static_cast<int>(std::distance(gameplayStageIds_.begin(), found));
}

/// <summary>
/// 破棄時に必要な解放処理を行います。
/// </summary>
SceneManager::~SceneManager() {
	if (scene_) {
		ApplyFallbackCamera();
		scene_->Finalize();
		scene_.reset();
	}
}

/// <summary>
/// FallbackCamera を現在の状態へ反映します。
/// </summary>
void SceneManager::ApplyFallbackCamera() {
	if (!fallbackCamera_) {
		return;
	}

	Object3dCommon::GetInstance()->SetDefaultCamera(fallbackCamera_);
	SkyBoxCommon::GetInstance()->SetDefaultCamera(fallbackCamera_);
	ParticleManager::GetInstance()->SetCamera(fallbackCamera_);
}

/// <summary>
/// 次のシーンへの切り替えを予約します。
/// </summary>
/// <param name="sceneName">対象となるシーン名を指定します。</param>
void SceneManager::ChangeScene(const std::string& sceneName) {
	// パターン指定のない呼び出しは、エディターまたはAPIで設定された既定値を使用する。
	ChangeScene(sceneName, transitionSettings_.type);
}

void SceneManager::ChangeScene(const std::string& sceneName, SceneTransitionType transitionType) {
	ChangeScene(sceneName, transitionType, transitionSettings_.outDuration, transitionSettings_.inDuration);
}

void SceneManager::ChangeScene(const std::string& sceneName, SceneTransitionType transitionType,
	float outDuration, float inDuration) {
	assert(sceneFactory_);
	assert(nextScene_ == nullptr);
	QueueNextScene(sceneName);

	activeTransitionType_ = transitionType;
	activeTransitionOutDuration_ = (std::max)(outDuration, 0.0f);
	activeTransitionInDuration_ = (std::max)(inDuration, 0.0f);
	transitionElapsed_ = 0.0f;
	transitionFogElapsed_ = 0.0f;
	transitionCoverage_ = 0.0f;
	transitionInWarmupFrames_ = 0;
	// 初回ロード、演出なし、またはOut時間0の場合はUpdateで即時交換する。
	if (!scene_ || transitionType == SceneTransitionType::None || activeTransitionOutDuration_ <= 0.0f) {
		transitionPhase_ = TransitionPhase::Idle;
		return;
	}
	transitionPhase_ = TransitionPhase::Out;
}

void SceneManager::SetTransitionSettings(const SceneTransitionSettings& settings) {
	transitionSettings_ = settings;
	// 負の時間や色範囲外の値が、状態更新やブレンド計算へ入らないよう正規化する。
	transitionSettings_.outDuration = (std::max)(0.0f, transitionSettings_.outDuration);
	transitionSettings_.inDuration = (std::max)(0.0f, transitionSettings_.inDuration);
	transitionSettings_.color.x = std::clamp(transitionSettings_.color.x, 0.0f, 1.0f);
	transitionSettings_.color.y = std::clamp(transitionSettings_.color.y, 0.0f, 1.0f);
	transitionSettings_.color.z = std::clamp(transitionSettings_.color.z, 0.0f, 1.0f);
	transitionSettings_.color.w = std::clamp(transitionSettings_.color.w, 0.0f, 1.0f);
}

void SceneManager::QueueNextScene(const std::string& sceneName) {
	// シーン生成は遷移開始時に済ませ、画面が覆われた瞬間は所有権の交換だけにする。
	nextScene_ = sceneFactory_->CreateScene(sceneName);
	assert(nextScene_);
	nextSceneName_ = sceneName;
	nextScene_->SetSceneManager(this);
	if (sceneName == "GAMEPLAY") {
		RefreshGameplayStagePatterns();
		ApplyGameplayStagePath(nextScene_.get(), selectedGameplayStageId_);
		// 配置JSONに保存されたPlayer設定より、選択画面で決定したタイプを優先させる。
		nextScene_->SetPlayerTypeOverride(demoDirector_.IsActive() ? "巫女" : selectedPlayerTypeName_);
	}
}

void SceneManager::SwitchToNextScene() {
	if (!nextScene_) {
		return;
	}
	if (scene_) {
		// 旧シーンが設定したカメラ参照を、破棄前にエンジン共通カメラへ戻す。
		ApplyFallbackCamera();
		scene_->Finalize();
		scene_.reset();
	}

	scene_ = std::move(nextScene_);
	currentSceneName_ = nextSceneName_;
	selectedSceneIndex_ = FindSceneIndex(currentSceneName_);
	scene_->SetSceneName(currentSceneName_);
	if (currentSceneName_ == "GAMEPLAY") {
		activeGameplayStageId_ = selectedGameplayStageId_;
		ApplyGameplayStagePath(scene_.get(), activeGameplayStageId_);
	}
	scene_->SetFallbackCamera(fallbackCamera_);
	// 完全被覆中に初期化とJSONロードを終え、未初期化の画面が見えないようにする。
	scene_->Initialize();
	scene_->LoadEditorObjects();
}

void SceneManager::UpdateTransition() {
	if (transitionPhase_ == TransitionPhase::Idle) {
		return;
	}
	const float deltaTime = GameTime::GetDeltaTime();
	transitionElapsed_ += deltaTime;
	// フェードの濃さが停止する完全白・準備フレーム中も、霧の流れだけは継続します。
	if (activeTransitionType_ == SceneTransitionType::FadeWhite) {
		transitionFogElapsed_ += deltaTime;
	}
	if (transitionPhase_ == TransitionPhase::Out) {
		// Outでは被覆率を0→1へ増やし、完全に隠れたフレームでシーンを交換する。
		const float duration = (std::max)(activeTransitionOutDuration_, 0.0001f);
		transitionCoverage_ = std::clamp(transitionElapsed_ / duration, 0.0f, 1.0f);
		if (activeTransitionType_ == SceneTransitionType::FadeWhite) {
			ApplyFadeWhiteFog(transitionCoverage_, currentSceneName_ == "TITLE");
		}
		if (transitionCoverage_ >= 1.0f) {
			SwitchToNextScene();
			transitionElapsed_ = 0.0f;
			transitionInWarmupFrames_ = 0;
			if (activeTransitionInDuration_ <= 0.0f) {
				transitionCoverage_ = 0.0f;
				transitionPhase_ = TransitionPhase::Idle;
			} else {
				transitionPhase_ = TransitionPhase::In;
			}
		}
	} else {
		// Inでは新しいシーンを描画しながら被覆率を1→0へ戻す。
		// 初期化直後の描画先が安定するまで完全な白を維持し、黒い準備フレームを露出させません。
		if (transitionInWarmupFrames_ < 2) {
			++transitionInWarmupFrames_;
			transitionElapsed_ = 0.0f;
			transitionCoverage_ = 1.0f;
			return;
		}
		// 白い濃霧からの復帰は、霧が薄くなる過程を読める長さへ延ばします。
		const float requestedDuration = activeTransitionType_ == SceneTransitionType::FadeWhite
			? (std::max)(activeTransitionInDuration_, 1.35f)
			: activeTransitionInDuration_;
		const float duration = (std::max)(requestedDuration, 0.0001f);
		transitionCoverage_ = 1.0f - std::clamp(transitionElapsed_ / duration, 0.0f, 1.0f);
		if (activeTransitionType_ == SceneTransitionType::FadeWhite) {
			// Outで増加させた深度霧を、同じcoverageに沿って高密度から通常状態へ減少させます。
			ApplyFadeWhiteFog(transitionCoverage_, currentSceneName_ == "TITLE");
		}
		if (transitionCoverage_ <= 0.0f) {
			transitionCoverage_ = 0.0f;
			transitionPhase_ = TransitionPhase::Idle;
			if (activeTransitionType_ == SceneTransitionType::FadeWhite) {
				PostEffect::GetInstance()->SetFogSettings(
					currentSceneName_ == "TITLE", {0.26f, 0.31f, 0.40f, 0.82f}, 0.68f, 0.54f,
					3.8f, 0.48f, 0.08f, 0.028f, 4.5f, 52.0f, false);
			}
		}
	}
}

void SceneManager::SetTransitionSpriteRect(size_t index, float x, float y, float width, float height, float alpha) {
	// 被覆幅0のワイプなどは描画せず、不正な矩形のGPU送信も避ける。
	if (index >= transitionSprites_.size() || !transitionSprites_[index] || width <= 0.0f || height <= 0.0f) {
		return;
	}
	Sprite* sprite = transitionSprites_[index].get();
	EulerTransform transform = sprite->GetTransform();
	transform.translate = {x, y, 0.0f};
	sprite->SetTransform(transform);
	sprite->SetSize({width, height});
	sprite->SetColor({transitionSettings_.color.x, transitionSettings_.color.y, transitionSettings_.color.z,
	    std::clamp(alpha * transitionSettings_.color.w, 0.0f, 1.0f)});
	sprite->Update();
	sprite->Draw();
}

void SceneManager::DrawTransition() {
	if (transitionPhase_ == TransitionPhase::Idle || activeTransitionType_ == SceneTransitionType::None || transitionCoverage_ <= 0.0f) {
		return;
	}
	DirectXCommon* dxCommon = SpriteCommon::GetInstance()->GetDxCommon();
	if (!dxCommon) {
		return;
	}
	const float width = static_cast<float>(dxCommon->GetRenderWidth());
	const float height = static_cast<float>(dxCommon->GetRenderHeight());
	const float coverage = std::clamp(transitionCoverage_, 0.0f, 1.0f);
	// Fadeの半透明描画にも対応するため、通常アルファブレンドを使用する。
	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	// 霧境界に使う6枚だけを必要な間テクスチャへ切り替え、他の遷移パターンへ残さないようにします。
	const bool useFogBoundary = activeTransitionType_ == SceneTransitionType::FadeWhite;
	for (size_t index = 2; index < transitionSprites_.size(); ++index) {
		const char* desiredTexture = useFogBoundary ? kTransitionFogTexturePath : kTransitionTextureKey;
		if (transitionSprites_[index]->GetTextureFilePath() != desiredTexture) {
			transitionSprites_[index]->SetTexture(desiredTexture);
		}
	}

	switch (activeTransitionType_) {
	case SceneTransitionType::Fade:
		SetTransitionSpriteRect(0, 0.0f, 0.0f, width, height, coverage);
		break;
	case SceneTransitionType::FadeWhite: {
		// OutとInで同じ被覆率を使い、粒状霧の増加と減少を完全に対称にします。
		const float easedCoverage = coverage * coverage * (3.0f - 2.0f * coverage);
		const float solidAlpha = SmoothStep(0.70f, 1.0f, easedCoverage);
		// 深度霧を先に増やし、前面の霧画像はフェード進行が約8割に達してから現れるようにします。
		const float mistAlpha = SmoothStep(0.80f, 0.96f, coverage);
		const auto drawFogLayer = [&](size_t index, float alpha, float scaleX, float scaleY,
			float speedX, float speedY) {
			if (index >= transitionSprites_.size() || alpha <= 0.0f) return;
			Sprite* sprite = transitionSprites_[index].get();
			EulerTransform transform = sprite->GetTransform();
			transform.translate = {0.0f, 0.0f, 0.0f};
			sprite->SetTransform(transform);
			sprite->SetSize({width, height});
			sprite->SetColor({1.0f, 1.0f, 1.0f, std::clamp(alpha, 0.0f, 1.0f)});
			if (index >= 2) {
				EulerTransform uvTransform = sprite->GetUVTransform();
				uvTransform.scale = {scaleX, scaleY, 1.0f};
				uvTransform.translate = {
					transitionFogElapsed_ * speedX,
					transitionFogElapsed_ * speedY,
					0.0f};
				sprite->SetUVTransform(uvTransform);
			}
			sprite->Update();
			sprite->Draw();
		};
		// 白い下地を敷かず、霧画像の隙間から粒状の深度霧を見せます。
		// 白幕は霧が十分濃くなった終盤だけ重ね、途中の粒が隠れないようにします。
		// 大きな霧は右上、中層は左上、細かな霧は右下へ流して静止画像に見えないようにします。
		drawFogLayer(2, mistAlpha * 0.64f, 0.88f, 1.35f, 0.18f, 0.11f);
		drawFogLayer(3, mistAlpha * 0.48f, 1.34f, 0.92f, -0.15f, 0.17f);
		drawFogLayer(4, mistAlpha * 0.30f, 1.82f, 1.58f, 0.24f, -0.09f);
		drawFogLayer(0, solidAlpha, 1.0f, 1.0f, 0.0f, 0.0f);
		break;
	}
	case SceneTransitionType::WipeLeft:
		SetTransitionSpriteRect(0, 0.0f, 0.0f, width * coverage, height, 1.0f);
		break;
	case SceneTransitionType::WipeRight: {
		const float coveredWidth = width * coverage;
		SetTransitionSpriteRect(0, width - coveredWidth, 0.0f, coveredWidth, height, 1.0f);
		break;
	}
	case SceneTransitionType::Curtains: {
		const float panelWidth = width * 0.5f * coverage;
		SetTransitionSpriteRect(0, 0.0f, 0.0f, panelWidth, height, 1.0f);
		SetTransitionSpriteRect(1, width - panelWidth, 0.0f, panelWidth, height, 1.0f);
		break;
	}
	case SceneTransitionType::HorizontalBars: {
		// 偶数段は左から、奇数段は右から伸ばし、交互方向の動きを作る。
		const float barHeight = height / static_cast<float>(kTransitionSpriteCount);
		const float coveredWidth = width * coverage;
		for (size_t index = 0; index < kTransitionSpriteCount; ++index) {
			const float x = (index % 2 == 0) ? 0.0f : width - coveredWidth;
			const float y = barHeight * static_cast<float>(index);
			const float h = index + 1 == kTransitionSpriteCount ? height - y : std::ceil(barHeight);
			SetTransitionSpriteRect(index, x, y, coveredWidth, h, 1.0f);
		}
		break;
	}
	case SceneTransitionType::None:
		break;
	}
}
