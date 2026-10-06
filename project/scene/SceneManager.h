#pragma once
#include "AbstractSceneFactory.h"
#include "StageResultData.h"
#include "Vector.h"

#include <memory>
#include <array>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

class BaseScene;
class Camera;
class Sprite;

/// <summary>シーン切り替え時に使用する画面演出です。</summary>
enum class SceneTransitionType {
	None,           ///< 演出を行わず、次の更新で即時切り替えます。
	Fade,           ///< 指定色の透明度を変化させて画面全体を覆います。
	WipeLeft,       ///< 画面左端から右方向へ覆います。
	WipeRight,      ///< 画面右端から左方向へ覆います。
	Curtains,       ///< 画面の左右から中央へ向けて閉じます。
	HorizontalBars  ///< 横帯を左右交互の方向から伸ばします。
};

/// <summary>シーン遷移の既定値です。ChangeSceneごとに種類だけ上書きすることもできます。</summary>
struct SceneTransitionSettings {
	SceneTransitionType type = SceneTransitionType::Fade; ///< ChangeSceneで使用する既定パターンです。
	float outDuration = 0.35f;                            ///< 現在のシーンを覆い切るまでの秒数です。
	float inDuration = 0.35f;                             ///< 新しいシーンを表示し切るまでの秒数です。
	Vector4 color = {0.0f, 0.0f, 0.0f, 1.0f};            ///< 遷移オーバーレイのRGBA色です。
};

/// <summary>
/// 現在のシーンと遷移先シーンを所有し、ライフサイクルと描画呼び出しを管理します。
/// シーン生成そのものはAbstractSceneFactoryへ委譲します。
/// </summary>
class SceneManager {
private:
	/// <summary>Idle → Out → シーン交換 → In → Idleの順で遷移します。</summary>
	enum class TransitionPhase { Idle, Out, In };

public:
	SceneManager();
	/// <summary>
	/// 破棄時に必要な解放処理を行います。
	/// </summary>
	~SceneManager();

	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	/// <summary>
	/// 次のシーンへの切り替えを予約します。
	/// </summary>
	/// <param name="sceneName">対象となるシーン名を指定します。</param>
	void ChangeScene(const std::string& sceneName);
	/// <summary>この切り替えに限り、指定した遷移パターンを使用します。</summary>
	void ChangeScene(const std::string& sceneName, SceneTransitionType transitionType);
	/// <summary>以降のシーン切り替えに使用する既定設定を更新します。</summary>
	void SetTransitionSettings(const SceneTransitionSettings& settings);
	/// <summary>現在のシーン遷移設定を取得します。</summary>
	const SceneTransitionSettings& GetTransitionSettings() const { return transitionSettings_; }
	/// <summary>フェードアウトまたはフェードインの途中かどうかを返します。</summary>
	bool IsTransitioning() const { return transitionPhase_ != TransitionPhase::Idle; }
	/// <summary>現在表示中のシーン名を返します。</summary>
	const std::string& GetCurrentSceneName() const { return currentSceneName_; }
	/// <summary>選択画面で決定したプレイヤータイプを、ゲームプレイ生成時まで保持します。</summary>
	void SetSelectedPlayerTypeName(const std::string& playerTypeName) { selectedPlayerTypeName_ = playerTypeName; }
	/// <summary>現在選択されているプレイヤータイプ名を返します。</summary>
	const std::string& GetSelectedPlayerTypeName() const { return selectedPlayerTypeName_; }
	/// <summary>指定したプレイヤータイプが現在選択可能かを返します。</summary>
	bool IsPlayerTypeUnlocked(const std::string& playerTypeName) const;
	/// <summary>指定プレイヤーの開放に必要なクリア済みステージIDを返します。条件なしなら空文字です。</summary>
	std::string GetPlayerUnlockPrerequisiteStage(const std::string& playerTypeName) const;
	/// <summary>指定したプレイヤータイプを開放し、進行データへ即時保存します。</summary>
	void UnlockPlayerType(const std::string& playerTypeName);

	/// <summary>
	/// 毎フレームの状態更新を行います。
	/// </summary>
	void Update();
	/// <summary>
	/// スカイボックスの描画処理を行います。
	/// </summary>
	void DrawSkyBox();
	/// <summary>
	/// 2D 要素の描画処理を行います。
	/// </summary>
	void Draw2D();
	/// <summary>
	/// 3D 要素の描画処理を行います。
	/// </summary>
	void Draw3D();
	/// <summary>現在のシーンをライト視点の影テクスチャへ描画します。</summary>
	void DrawShadowMap();
	void DrawEditorImGui();

	void SetSceneFactory(AbstractSceneFactory* factory) { sceneFactory_ = factory; }
	void SetFallbackCamera(Camera* camera) { fallbackCamera_ = camera; }
	void SetScenePlaying(bool isPlaying) { isScenePlaying_ = isPlaying; }
	bool IsScenePlaying() const { return isScenePlaying_; }
	void RequestFrameStep() { isFrameStepRequested_ = true; }
	/// <summary>シーン遷移中でなければ、現在の配置JSONからシーンを再構築します。</summary>
	bool ReloadCurrentSceneJson();
	/// <summary>現在のシーンまたはステージパターンが実際に使用しているJSONパスを返します。</summary>
	std::string GetCurrentSceneJsonPath() const;
	void RefreshGameplayStagePatterns();
	void SetSelectedGameplayStageId(const std::string& stageId);
	const std::string& GetSelectedGameplayStageId() const { return selectedGameplayStageId_; }
	/// <summary>内部ステージIDをプレイヤー向けの表示名へ変換します。</summary>
	std::string GetGameplayStageDisplayName(const std::string& stageId) const;
	const std::vector<std::string>& GetGameplayStageIds() const { return gameplayStageIds_; }
	/// <summary>指定したステージが現在の進行度で選択可能かを返します。</summary>
	bool IsGameplayStageUnlocked(const std::string& stageId) const;
	/// <summary>指定ステージの開放に必要なクリア済みステージIDを返します。条件なしなら空文字です。</summary>
	std::string GetGameplayStageUnlockPrerequisite(const std::string& stageId) const;
	/// <summary>ステージクリアに応じて次のステージを開放し、進行データへ保存します。</summary>
	void RecordGameplayStageClear(const std::string& stageId);
	/// <summary>ゲームプレイ終了時の戦績をシーン遷移後まで保持します。</summary>
	void SetStageResultData(const StageResultData& resultData) { stageResultData_ = resultData; }
	/// <summary>最後に確定した戦績を返します。</summary>
	const StageResultData& GetStageResultData() const { return stageResultData_; }
	/// <summary>全シーンで共有する現在の所持金を返します。</summary>
	int GetMoney() const { return money_; }
	/// <summary>保存済みのゲーム進行と実行中の進行状態を初期状態へ戻します。</summary>
	void ResetGameProgress();
	/// <summary>獲得金を所持金へ加算し、進行データへ保存します。</summary>
	void AddMoney(int amount);
	/// <summary>残高が足りる場合だけ所持金を消費し、進行データへ保存します。</summary>
	bool SpendMoney(int amount);
	/// <summary>選択中プレイヤーの商品購入レベルを取得します。</summary>
	int GetShopUpgradeLevel(const std::string& upgradeId) const;
	/// <summary>選択中プレイヤーの商品購入レベルを更新して保存します。</summary>
	void SetShopUpgradeLevel(const std::string& upgradeId, int level);
	/// <summary>指定プレイヤーの商品購入レベルを取得します。ショップ内の対象切り替えで使用します。</summary>
	int GetShopUpgradeLevelForPlayer(const std::string& playerTypeName, const std::string& upgradeId) const;
	/// <summary>指定プレイヤーの商品購入レベルを更新し、進行データへ保存します。</summary>
	void SetShopUpgradeLevelForPlayer(const std::string& playerTypeName, const std::string& upgradeId, int level);
	/// <summary>全キャラクター共通商品の購入レベルを取得します。</summary>
	int GetGlobalShopUpgradeLevel(const std::string& upgradeId) const;
	/// <summary>全キャラクター共通商品の購入レベルを更新して保存します。</summary>
	void SetGlobalShopUpgradeLevel(const std::string& upgradeId, int level);
	/// <summary>全体強化による獲得経験値の加算率を百分率で返します。</summary>
	float GetGlobalExperienceBonusPercent() const;
	/// <summary>全体強化倍率を適用したステージ獲得Gを返します。</summary>
	int ApplyGlobalGoldBonus(int baseAmount) const;

private:
	/// <summary>
	/// FallbackCamera を現在の状態へ反映します。
	/// </summary>
	void ApplyFallbackCamera();
	/// <summary>遷移先シーンを生成し、交換まで待機させます。</summary>
	void QueueNextScene(const std::string& sceneName);
	/// <summary>現在のシーンを終了し、待機中のシーンを初期化して交換します。</summary>
	void SwitchToNextScene();
	/// <summary>経過時間から遷移のフェーズと画面被覆率を更新します。</summary>
	void UpdateTransition();
	/// <summary>選択中のパターンに従って最前面の遷移オーバーレイを描画します。</summary>
	void DrawTransition();
	/// <summary>共有する単色スプライトの1枚を、指定矩形へ配置して描画します。</summary>
	void SetTransitionSpriteRect(size_t index, float x, float y, float width, float height, float alpha);
	std::string GetGameplayStageJsonPath(const std::string& stageId) const;
	void ApplyGameplayStagePath(BaseScene* scene, const std::string& stageId) const;
	/// <summary>所持金とショップ購入レベルを進行データJSONから読み込みます。</summary>
	void LoadGameProgress();
	/// <summary>所持金とショップ購入レベルを進行データJSONへ書き込みます。</summary>
	void SaveGameProgress() const;
	/// <summary>ステージごとの開放条件を編集用JSONから読み込みます。</summary>
	void LoadStageUnlockConditions();
	/// <summary>ImGuiで編集したステージ開放条件をJSONへ保存します。</summary>
	void SaveStageUnlockConditions() const;
	/// <summary>開放条件の変更によって循環参照が発生するかを判定します。</summary>
	bool WouldCreateStageUnlockCycle(const std::string& stageId, const std::string& prerequisiteStageId) const;
	/// <summary>プレイヤーごとに商品レベルを分けるための保存キーを生成します。</summary>
	std::string MakeShopUpgradeKey(const std::string& upgradeId) const;
	/// <summary>ショップ対象を明示して商品レベルの保存キーを生成します。</summary>
	std::string MakeShopUpgradeKey(const std::string& playerTypeName, const std::string& upgradeId) const;
	/// <summary>全キャラクター共通商品の保存キーを生成します。</summary>
	std::string MakeGlobalShopUpgradeKey(const std::string& upgradeId) const;
	bool IsValidGameplayStageId(const std::string& stageId) const;
	int FindGameplayStageIndex(const std::string& stageId) const;

	std::unique_ptr<BaseScene> scene_ = nullptr;
	std::unique_ptr<BaseScene> nextScene_ = nullptr;
	/// <summary>HorizontalBarsで使用する横帯の数であり、共有スプライトの最大数です。</summary>
	static constexpr size_t kTransitionSpriteCount = 8;
	/// <summary>実行時生成した1px白テクスチャを拡大して描く遷移用スプライトです。</summary>
	std::array<std::unique_ptr<Sprite>, kTransitionSpriteCount> transitionSprites_{};
	TransitionPhase transitionPhase_ = TransitionPhase::Idle;
	SceneTransitionSettings transitionSettings_{};
	/// <summary>遷移開始時に確定し、途中の設定変更によるパターン切り替わりを防ぎます。</summary>
	SceneTransitionType activeTransitionType_ = SceneTransitionType::Fade;
	/// <summary>現在フェーズが開始してからの経過秒数です。</summary>
	float transitionElapsed_ = 0.0f;
	/// <summary>画面が遷移色で覆われている割合です。0が表示、1が完全被覆です。</summary>
	float transitionCoverage_ = 0.0f;
	AbstractSceneFactory* sceneFactory_ = nullptr;
	Camera* fallbackCamera_ = nullptr;
	std::string currentSceneName_ = "None";
	std::string nextSceneName_ = "None";
	int selectedSceneIndex_ = 0;
	bool isScenePlaying_ = true;
	bool isFrameStepRequested_ = false;
	/// <summary>シーンをまたいで引き継ぐプレイヤータイプです。初回は唯一開放済みの巫女を使用します。</summary>
	std::string selectedPlayerTypeName_ = "巫女";
	std::vector<std::string> gameplayStageIds_ = {"default"};
	std::string selectedGameplayStageId_ = "default";
	std::string activeGameplayStageId_ = "default";
	int selectedGameplayStageIndex_ = 0;
	std::array<char, 64> newGameplayStageIdBuffer_{};
	std::string gameplayStageMessage_;
	/// <summary>GamePlaySceneからResultSceneへ受け渡す戦績のスナップショットです。</summary>
	StageResultData stageResultData_;
	/// <summary>ゲーム全体で共有し、シーンをまたいで保持する所持金です。</summary>
	int money_ = 0;
	/// <summary>「プレイヤー名:商品ID」をキーとする購入済みレベルです。</summary>
	std::unordered_map<std::string, int> shopUpgradeLevels_;
	/// <summary>クリア進行によって開放済みになったゲームプレイステージIDです。</summary>
	std::unordered_set<std::string> unlockedGameplayStageIds_ = {"default"};
	/// <summary>一度でもクリアしたステージIDです。開放条件を編集した場合も判定へ再利用します。</summary>
	std::unordered_set<std::string> clearedGameplayStageIds_;
	/// <summary>選択画面で使用可能なプレイヤータイプ名です。新規・旧形式セーブでは巫女だけを開放します。</summary>
	std::unordered_set<std::string> unlockedPlayerTypeNames_ = {"巫女"};
	/// <summary>キーのプレイヤーを開放するためにクリアが必要なステージIDです。</summary>
	std::unordered_map<std::string, std::string> playerUnlockStagePrerequisites_ = {
		{"猫", "default"},
		{"烏天狗", "stage2"}
	};
	/// <summary>キーのステージを開放するためにクリアが必要なステージIDです。</summary>
	std::unordered_map<std::string, std::string> stageUnlockPrerequisites_ = {{"stage2", "default"}};
};
