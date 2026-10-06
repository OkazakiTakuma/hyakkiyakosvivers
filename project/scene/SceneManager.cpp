#include "SceneManager.h"

#include "object/Object3dCommon.h"
#include "particle/ParticleManager.h"
#include "BaseScene.h"
#include "Input.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "sky/SkyBoxCommon.h"
#include "GameTime.h"
#include "StringUtility.h"

#include <assert.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <json.hpp>

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
	"Wipe Left",
	"Wipe Right",
	"Curtains",
	"Horizontal Bars"
};

// 外部画像を必要としないよう、遷移色の下地には実行時生成する白1pxテクスチャを使用する。
constexpr const char* kTransitionTextureKey = "__scene_transition_white";
/// <summary>
/// 所持金・ショップ購入レベル・ステージ開放状況を永続化するファイルです。
/// </summary>
constexpr const char* kGameProgressFilePath = "Resources/Data/game_progress.json";
/// <summary>ImGuiから編集するステージ開放条件の設定ファイルです。</summary>
constexpr const char* kStageUnlockConditionsFilePath = "Resources/Data/stage_unlock_conditions.json";
/// <summary>全体強化1レベルあたりの経験値・獲得G上昇率です。</summary>
constexpr float kGlobalGainBonusPercentPerLevel = 10.0f;

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
	for (auto& sprite : transitionSprites_) {
		sprite = std::make_unique<Sprite>();
		sprite->Initialize(kTransitionTextureKey);
		sprite->SetAnchorPoint({0.0f, 0.0f});
	}
}

void SceneManager::LoadGameProgress() {
	std::ifstream input(StringUtility::Utf8ToPath(kGameProgressFilePath));
	if (!input) {
		// 初回起動など、セーブファイルがない場合はメンバーの初期値（所持金0・未購入）を使う。
		return;
	}
	try {
		nlohmann::json root;
		input >> root;
		// 手編集された負の値がゲーム内へ入らないよう、読み込み時に0以上へ補正する。
		money_ = (std::max)(0, root.value("money", 0));
		if (root.contains("shopUpgradeLevels") && root.at("shopUpgradeLevels").is_object()) {
			// JSONオブジェクトのキーは「プレイヤー名:商品ID」、値は購入済みレベルを表す。
			for (auto it = root.at("shopUpgradeLevels").begin(); it != root.at("shopUpgradeLevels").end(); ++it) {
				shopUpgradeLevels_[it.key()] = (std::max)(0, it.value().get<int>());
			}
		}
		if (root.contains("unlockedGameplayStages") && root.at("unlockedGameplayStages").is_array()) {
			// 一度開放されたステージは、あとから開放条件を変更してもプレイヤーから取り上げない。
			for (const auto& stageId : root.at("unlockedGameplayStages")) {
				if (stageId.is_string() && IsValidGameplayStageId(stageId.get<std::string>())) {
					unlockedGameplayStageIds_.insert(stageId.get<std::string>());
				}
			}
		}
		if (root.contains("clearedGameplayStages") && root.at("clearedGameplayStages").is_array()) {
			// クリア履歴も別に保持し、ImGuiで条件を変更した直後の開放判定にも再利用する。
			for (const auto& stageId : root.at("clearedGameplayStages")) {
				if (stageId.is_string() && IsValidGameplayStageId(stageId.get<std::string>())) {
					clearedGameplayStageIds_.insert(stageId.get<std::string>());
				}
			}
		}
		if (root.contains("unlockedPlayerTypes") && root.at("unlockedPlayerTypes").is_array()) {
			// プレイヤー名は日本語も使用するため、空文字だけを除外してそのまま復元する。
			// 設定ファイル側でキャラクターが増えても、名前をキーにすることでセーブ形式を変更せず対応できる。
			for (const auto& playerTypeName : root.at("unlockedPlayerTypes")) {
				if (playerTypeName.is_string() && !playerTypeName.get<std::string>().empty()) {
					unlockedPlayerTypeNames_.insert(playerTypeName.get<std::string>());
				}
			}
		}
		// defaultは新規・旧形式セーブのどちらでも必ずプレイ可能にする。
		unlockedGameplayStageIds_.insert("default");
		// プレイヤー開放情報がない旧形式セーブでも、開始キャラクターの巫女は必ず使用可能にする。
		unlockedPlayerTypeNames_.insert("巫女");
		// この機能追加前に条件ステージをクリア済みなら、その履歴からプレイヤー開放状態を復元する。
		for (const auto& [playerTypeName, prerequisiteStageId] : playerUnlockStagePrerequisites_) {
			if (clearedGameplayStageIds_.contains(prerequisiteStageId)) {
				unlockedPlayerTypeNames_.insert(playerTypeName);
			}
		}
	} catch (...) {
		// 壊れた保存データでゲームを起動不能にせず、未購入状態へ戻して続行する。
		money_ = 0;
		shopUpgradeLevels_.clear();
		unlockedGameplayStageIds_ = {"default"};
		clearedGameplayStageIds_.clear();
		unlockedPlayerTypeNames_ = {"巫女"};
	}
}

void SceneManager::SaveGameProgress() const {
	// 初回保存時にも書き込めるよう、Resources/Dataが存在しなければ生成する。
	std::filesystem::create_directories(StringUtility::Utf8ToPath(kGameProgressFilePath).parent_path());
	nlohmann::json root;
	// unordered_mapはJSONオブジェクトへ変換されるため、商品ごとのレベルをキー付きで復元できる。
	root["money"] = money_;
	root["shopUpgradeLevels"] = shopUpgradeLevels_;
	// unordered_setの列挙順は不定なので、差分確認しやすいようIDを並べ替えて保存する。
	std::vector<std::string> unlockedStages(unlockedGameplayStageIds_.begin(), unlockedGameplayStageIds_.end());
	std::sort(unlockedStages.begin(), unlockedStages.end());
	root["unlockedGameplayStages"] = unlockedStages;
	std::vector<std::string> clearedStages(clearedGameplayStageIds_.begin(), clearedGameplayStageIds_.end());
	std::sort(clearedStages.begin(), clearedStages.end());
	root["clearedGameplayStages"] = clearedStages;
	std::vector<std::string> unlockedPlayerTypes(unlockedPlayerTypeNames_.begin(), unlockedPlayerTypeNames_.end());
	// 実行結果に影響しないunordered_setの順序差で、セーブJSONの差分が毎回変わらないよう整列する。
	std::sort(unlockedPlayerTypes.begin(), unlockedPlayerTypes.end());
	root["unlockedPlayerTypes"] = unlockedPlayerTypes;
	std::ofstream output(StringUtility::Utf8ToPath(kGameProgressFilePath));
	if (output) {
		output << std::setw(4) << root << std::endl;
	}
}

bool SceneManager::IsPlayerTypeUnlocked(const std::string& playerTypeName) const {
	// プレイヤー選択画面はこの関数だけを参照し、保存形式や開放条件の詳細には依存させない。
	const std::string prerequisiteStageId = GetPlayerUnlockPrerequisiteStage(playerTypeName);
	return unlockedPlayerTypeNames_.contains(playerTypeName) ||
	    (!prerequisiteStageId.empty() && clearedGameplayStageIds_.contains(prerequisiteStageId));
}

std::string SceneManager::GetPlayerUnlockPrerequisiteStage(const std::string& playerTypeName) const {
	const auto found = playerUnlockStagePrerequisites_.find(playerTypeName);
	return found == playerUnlockStagePrerequisites_.end() ? std::string{} : found->second;
}

void SceneManager::UnlockPlayerType(const std::string& playerTypeName) {
	// ステージクリア・ショップ購入など、呼び出し元を限定しない共通の開放入口として使用する。
	if (playerTypeName.empty()) {
		return;
	}
	// 初回開放時だけ保存し、同じ条件が複数回成立しても不要なファイル書き込みを行わない。
	if (unlockedPlayerTypeNames_.insert(playerTypeName).second) {
		// 開放した瞬間に永続化し、キャラクター選択画面へ移る前に終了しても進行を保持する。
		SaveGameProgress();
	}
}

void SceneManager::LoadStageUnlockConditions() {
	std::ifstream input(StringUtility::Utf8ToPath(kStageUnlockConditionsFilePath));
	if (!input) {
		// 設定ファイル追加前と同じく、defaultクリアでstage2を開放する既定値を使用する。
		return;
	}
	try {
		nlohmann::json root;
		input >> root;
		std::unordered_map<std::string, std::string> loadedConditions;
		// JSONは「開放されるステージID: 先にクリアするステージID」の対応表として読む。
		const auto& conditions = root.at("stageUnlockPrerequisites");
		if (!conditions.is_object()) {
			return;
		}
		for (auto it = conditions.begin(); it != conditions.end(); ++it) {
			// 不正なIDと、自分自身のクリアを要求する成立不能な条件は読み飛ばす。
			if (it.value().is_string() && IsValidGameplayStageId(it.key())) {
				const std::string prerequisite = it.value().get<std::string>();
				if (IsValidGameplayStageId(prerequisite) && prerequisite != it.key()) {
					loadedConditions[it.key()] = prerequisite;
				}
			}
		}
		stageUnlockPrerequisites_ = std::move(loadedConditions);
	} catch (...) {
		// 壊れた設定では既定条件を維持し、ステージ選択を起動不能にしない。
	}
}

void SceneManager::SaveStageUnlockConditions() const {
	std::filesystem::create_directories(StringUtility::Utf8ToPath(kStageUnlockConditionsFilePath).parent_path());
	nlohmann::json root;
	root["stageUnlockPrerequisites"] = nlohmann::json::object();
	std::vector<std::string> stageIds;
	stageIds.reserve(stageUnlockPrerequisites_.size());
	for (const auto& [stageId, prerequisite] : stageUnlockPrerequisites_) {
		stageIds.push_back(stageId);
	}
	// 設定ファイルを手作業やGit差分でも読みやすくするため、キーを名前順で出力する。
	std::sort(stageIds.begin(), stageIds.end());
	for (const std::string& stageId : stageIds) {
		root["stageUnlockPrerequisites"][stageId] = stageUnlockPrerequisites_.at(stageId);
	}
	std::ofstream output(StringUtility::Utf8ToPath(kStageUnlockConditionsFilePath));
	if (output) {
		output << std::setw(4) << root << std::endl;
	}
}

bool SceneManager::IsGameplayStageUnlocked(const std::string& stageId) const {
	const std::string prerequisite = GetGameplayStageUnlockPrerequisite(stageId);
	// 条件なし・過去に開放済み・条件ステージをクリア済み、のいずれかなら選択可能とする。
	return prerequisite.empty() || unlockedGameplayStageIds_.contains(stageId) ||
	    clearedGameplayStageIds_.contains(prerequisite);
}

std::string SceneManager::GetGameplayStageUnlockPrerequisite(const std::string& stageId) const {
	const auto found = stageUnlockPrerequisites_.find(stageId);
	return found == stageUnlockPrerequisites_.end() ? std::string{} : found->second;
}

void SceneManager::RecordGameplayStageClear(const std::string& stageId) {
	// クリアした本人の履歴と、そのステージを条件にしている全ステージの開放を同時に確定する。
	bool progressChanged = clearedGameplayStageIds_.insert(stageId).second;
	for (const auto& [lockedStageId, prerequisiteStageId] : stageUnlockPrerequisites_) {
		if (prerequisiteStageId == stageId) {
			progressChanged |= unlockedGameplayStageIds_.insert(lockedStageId).second;
		}
	}
	// defaultクリアで猫、stage2クリアで烏天狗など、同じクリア通知からプレイヤー開放も確定する。
	for (const auto& [playerTypeName, prerequisiteStageId] : playerUnlockStagePrerequisites_) {
		if (prerequisiteStageId == stageId) {
			progressChanged |= unlockedPlayerTypeNames_.insert(playerTypeName).second;
		}
	}
	if (progressChanged) {
		// リザルト画面へ移る前に即時保存し、直後にゲームを閉じても進行を失わないようにする。
		SaveGameProgress();
	}
}

void SceneManager::ResetGameProgress() {
	// ファイルだけでなく実行中の値も初期化し、再起動せずそのまま新しいゲームを始められるようにする。
	money_ = 0;
	shopUpgradeLevels_.clear();
	unlockedGameplayStageIds_ = {"default"};
	clearedGameplayStageIds_.clear();
	unlockedPlayerTypeNames_ = {"巫女"};
	selectedPlayerTypeName_ = "巫女";
	selectedGameplayStageId_ = "default";
	activeGameplayStageId_ = "default";
	selectedGameplayStageIndex_ = 0;
	stageResultData_ = {};

	// 初期状態で上書きし、以前の進行が次回起動時に復元されないことを保証する。
	SaveGameProgress();
}

bool SceneManager::WouldCreateStageUnlockCycle(
	const std::string& stageId, const std::string& prerequisiteStageId) const {
	std::unordered_set<std::string> visited;
	std::string current = prerequisiteStageId;
	// 新しい条件から前提条件を順にたどり、設定対象へ戻ってきた場合は循環と判断する。
	while (!current.empty() && visited.insert(current).second) {
		if (current == stageId) {
			return true;
		}
		current = GetGameplayStageUnlockPrerequisite(current);
	}
	return false;
}

void SceneManager::AddMoney(int amount) {
	// 0以下の値を無視し、報酬処理から所持金が減ることを防ぐ。
	if (amount <= 0) return;
	money_ += amount;
	// 正常終了を待たず、その場で保存して強制終了時に失う進捗を最小限にする。
	SaveGameProgress();
}

bool SceneManager::SpendMoney(int amount) {
	// 不正な価格または残高不足の場合は、保存データを変更しない。
	if (amount < 0 || money_ < amount) return false;
	money_ -= amount;
	// 購入直後の残高を永続化し、次回起動時の所持金表示へ反映する。
	SaveGameProgress();
	return true;
}

std::string SceneManager::MakeShopUpgradeKey(const std::string& upgradeId) const {
	// 同じ商品でも使用キャラクターごとに独立した購入レベルを持たせる。
	return MakeShopUpgradeKey(selectedPlayerTypeName_, upgradeId);
}

std::string SceneManager::MakeShopUpgradeKey(
	const std::string& playerTypeName, const std::string& upgradeId) const {
	return playerTypeName + ":" + upgradeId;
}

int SceneManager::GetShopUpgradeLevel(const std::string& upgradeId) const {
	const auto found = shopUpgradeLevels_.find(MakeShopUpgradeKey(upgradeId));
	return found == shopUpgradeLevels_.end() ? 0 : found->second;
}

void SceneManager::SetShopUpgradeLevel(const std::string& upgradeId, int level) {
	SetShopUpgradeLevelForPlayer(selectedPlayerTypeName_, upgradeId, level);
}

int SceneManager::GetShopUpgradeLevelForPlayer(
	const std::string& playerTypeName, const std::string& upgradeId) const {
	const auto found = shopUpgradeLevels_.find(MakeShopUpgradeKey(playerTypeName, upgradeId));
	return found == shopUpgradeLevels_.end() ? 0 : found->second;
}

void SceneManager::SetShopUpgradeLevelForPlayer(
	const std::string& playerTypeName, const std::string& upgradeId, int level) {
	if (!IsPlayerTypeUnlocked(playerTypeName) || upgradeId.empty()) {
		return;
	}
	// ショップ画面の表示対象を変更しても、ゲーム開始用のselectedPlayerTypeName_へ影響させず保存する。
	shopUpgradeLevels_[MakeShopUpgradeKey(playerTypeName, upgradeId)] = (std::max)(0, level);
	SaveGameProgress();
}

std::string SceneManager::MakeGlobalShopUpgradeKey(const std::string& upgradeId) const {
	// プレイヤー名を含めない固定接頭辞により、キャラクターを変更しても同じ購入レベルを参照する。
	return "global:" + upgradeId;
}

int SceneManager::GetGlobalShopUpgradeLevel(const std::string& upgradeId) const {
	const auto found = shopUpgradeLevels_.find(MakeGlobalShopUpgradeKey(upgradeId));
	// 全体強化追加前のセーブデータにはキーがないため、未購入のLv.0として後方互換を保つ。
	return found == shopUpgradeLevels_.end() ? 0 : found->second;
}

void SceneManager::SetGlobalShopUpgradeLevel(const std::string& upgradeId, int level) {
	shopUpgradeLevels_[MakeGlobalShopUpgradeKey(upgradeId)] = (std::max)(0, level);
	// 購入直後に保存し、ゲームを閉じても全体強化レベルが失われないようにする。
	SaveGameProgress();
}

float SceneManager::GetGlobalExperienceBonusPercent() const {
	// 例: Lv.3なら 3 * 10 = +30% をPlayer側の経験値計算へ渡す。
	return static_cast<float>(GetGlobalShopUpgradeLevel("experience_gain")) * kGlobalGainBonusPercentPerLevel;
}

int SceneManager::ApplyGlobalGoldBonus(int baseAmount) const {
	if (baseAmount <= 0) return 0;
	// 基礎額 × (1 + レベル × 10%) で計算する。端数は獲得側に有利な切り上げとする。
	const double bonusRate = 1.0 +
	    static_cast<double>(GetGlobalShopUpgradeLevel("gold_gain")) *
	    static_cast<double>(kGlobalGainBonusPercentPerLevel) / 100.0;
	const double boostedAmount = std::ceil(static_cast<double>(baseAmount) * bonusRate);
	// 大量取得や不正データでもintをオーバーフローさせず、表現可能な最大値へ丸める。
	const double maxAmount = static_cast<double>((std::numeric_limits<int>::max)());
	return boostedAmount >= maxAmount ? (std::numeric_limits<int>::max)() : static_cast<int>(boostedAmount);
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

	if (scene_) {
		// 遷移中はゲーム進行を止め、閉じる前後でシーン状態が変化しないようにする。
		const bool shouldUpdateScene = transitionPhase_ == TransitionPhase::Idle &&
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
		scene_->DrawEditorImGui();
		// 共通ツールバーとステージパターン操作を最後に描き、シーン固有パネルより前面に表示します。
		DrawEditorImGui();
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
			stageUnlockPrerequisites_.erase(selectedGameplayStageId_);
			SaveStageUnlockConditions();
			gameplayStageMessage_ = "Always unlocked: " + selectedGameplayStageId_;
		} else if (WouldCreateStageUnlockCycle(selectedGameplayStageId_, newPrerequisite)) {
			gameplayStageMessage_ = "Unlock condition would create a cycle.";
		} else {
			// 編集結果は決定したフレームで保存し、ゲーム再起動後も同じルールを使用する。
			stageUnlockPrerequisites_[selectedGameplayStageId_] = newPrerequisite;
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
	// 配置ファイルやセーブデータではdefault IDを維持し、画面表示だけを日本語名へ置き換える。
	return stageId == "default" ? "平原" : stageId;
}

std::string SceneManager::GetGameplayStageJsonPath(const std::string& stageId) const {
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
	assert(sceneFactory_);
	assert(nextScene_ == nullptr);
	QueueNextScene(sceneName);

	activeTransitionType_ = transitionType;
	transitionElapsed_ = 0.0f;
	transitionCoverage_ = 0.0f;
	// 初回ロード、演出なし、またはOut時間0の場合はUpdateで即時交換する。
	if (!scene_ || transitionType == SceneTransitionType::None || transitionSettings_.outDuration <= 0.0f) {
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
		nextScene_->SetPlayerTypeOverride(selectedPlayerTypeName_);
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
	transitionElapsed_ += GameTime::GetDeltaTime();
	if (transitionPhase_ == TransitionPhase::Out) {
		// Outでは被覆率を0→1へ増やし、完全に隠れたフレームでシーンを交換する。
		const float duration = (std::max)(transitionSettings_.outDuration, 0.0001f);
		transitionCoverage_ = std::clamp(transitionElapsed_ / duration, 0.0f, 1.0f);
		if (transitionCoverage_ >= 1.0f) {
			SwitchToNextScene();
			transitionElapsed_ = 0.0f;
			if (transitionSettings_.inDuration <= 0.0f) {
				transitionCoverage_ = 0.0f;
				transitionPhase_ = TransitionPhase::Idle;
			} else {
				transitionPhase_ = TransitionPhase::In;
			}
		}
	} else {
		// Inでは新しいシーンを描画しながら被覆率を1→0へ戻す。
		const float duration = (std::max)(transitionSettings_.inDuration, 0.0001f);
		transitionCoverage_ = 1.0f - std::clamp(transitionElapsed_ / duration, 0.0f, 1.0f);
		if (transitionCoverage_ <= 0.0f) {
			transitionCoverage_ = 0.0f;
			transitionPhase_ = TransitionPhase::Idle;
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

	switch (activeTransitionType_) {
	case SceneTransitionType::Fade:
		SetTransitionSpriteRect(0, 0.0f, 0.0f, width, height, coverage);
		break;
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
