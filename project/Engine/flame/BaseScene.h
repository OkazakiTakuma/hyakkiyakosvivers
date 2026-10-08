#pragma once
#include "Audio.h"
#include "camera/Camera.h"
#include "camera/CameraComponent.h"
#include "EnemyComponent.h"
#include "EnemyProjectileComponent.h"
#include "EnemySpawnPointComponent.h"
#include "ExperienceComponent.h"
#include "ItemDropComponent.h"
#include "GameObject.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "Matrix.h"
#include "object/Object3dCommon.h"
#include "particle/ParticleEmitter.h"
#include "particle/ParticleManager.h"
#include "particle/TrailRendererComponent.h"
#include "Resource.h"
#include "sky/SkyBox.h"
#include "sky/SkyBoxCommon.h"
#include "sky/AtmosphereSystem.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "SrvManager.h"
#include "Vector.h"
#include "struct.h"
#include <object/Object3d.h>
#include "object/Object3dComponent.h"
#include "object/PointLightComponent.h"
#include "collision/OBBColliderComponent.h"
#include "particle/ParticleEmitterComponent.h"
#include "particle/GlowBillboardComponent.h"
#include "PlayerAttackComponent.h"
#include "PlayerProjectileComponent.h"
#include "../../Player/Player.h"
#include "collision/SphereColliderComponent.h"
#include "SpriteComponent.h"
#include "StageResultData.h"
#include "TextComponent.h"
#include "instancing/InstancingModel.h"
#include <array>
#include <memory>
#include <string>
#include <vector>
class SceneManager;

/// <summary>
/// シーン内オブジェクトの所有、実行時更新、衝突処理、シリアライズ、エディタ操作を統括します。
/// 個別シーンは仮想関数をオーバーライドし、シーン固有の振る舞いを追加します。
/// </summary>
class BaseScene {
public:
	/// <summary>
	/// ゲームプレイ中の入力先と時間停止理由を表す排他的なモードです。
	/// 複数のboolを同時に立てないことで、ポーズと選択画面の競合を防ぎます。
	/// </summary>
	enum class GameplayMode {
		Playing,
		Paused,
		LevelUpSelection,
		BossReward,
		Finished
	};

	enum class EditorCreateType {
		Empty,
		Object3dSphere,
		Object3dCylinder,
		Object3dCylinderOpen,
		Sprite,
		Text,
		LoadedModel,
		AnimatedModel,
		Camera,
		PointLight,
		ParticleEmitter,
		Player,
		EnemySpawnPoint,
		Enemy
	};

/// <summary>
/// シーンで使用するリソースやオブジェクトを初期化します。
/// </summary>
	virtual void Initialize();
/// <summary>
/// シーンの毎フレーム更新を行います。
/// </summary>
	virtual void Update();
/// <summary>
/// スカイボックスを描画します。
/// </summary>
	virtual void DrawSkyBox();
/// <summary>
/// 2D要素を描画します。
/// </summary>
	virtual void Draw2D();
	/// <summary>HUDより手前へ表示するシーン固有の2Dオーバーレイを描画します。</summary>
	virtual void DrawOverlay2D() {}
/// <summary>
/// 3D要素を描画します。
/// </summary>
	virtual void Draw3D();
/// <summary>
/// シーンが保持しているリソースを解放します。
/// </summary>
	virtual void Finalize();
	virtual ~BaseScene();

	virtual void SetSceneManager(SceneManager* manager) { sceneManager = manager; }
	/// SceneManagerが共通描画するパーティクルの表示可否を返します。
	virtual bool IsParticleRenderingEnabled() const { return true; }
	void SetSceneName(const std::string& sceneName) { sceneName_ = sceneName; }
	/// <summary>
	/// シーン名から決まる既定の配置JSONではなく、指定した配置JSONを使用します。
	/// 空文字を指定すると既定パスへ戻ります。
	/// </summary>
	void SetSceneObjectFilePathOverride(const std::string& filePath) { sceneObjectFilePathOverride_ = filePath; }
	/// <summary>現在読み書きする配置JSONファイルパスを返します。</summary>
	std::string GetSceneObjectFilePath() const;
	void SetFallbackCamera(Camera* camera) { fallbackCamera_ = camera; }
	/// <summary>配置JSONのPlayer設定を上書きする、ゲーム開始時の選択タイプを設定します。</summary>
	void SetPlayerTypeOverride(const std::string& playerTypeName) { playerTypeOverride_ = playerTypeName; }
	bool IsLevelUpSelectionActive() const {
		return gameplayMode_ == GameplayMode::LevelUpSelection || gameplayMode_ == GameplayMode::BossReward;
	}
	GameplayMode GetGameplayMode() const { return gameplayMode_; }
	/// <summary>最終ボスを倒し、ステージクリア条件を満たしたかを返します。</summary>
	bool IsStageCleared() const { return isStageCleared_; }
	/// <summary>プレイヤーの体力が0になったかを返します。</summary>
	bool IsPlayerDefeated() const;
	/// <summary>現在の装備とプレイ戦績をリザルト表示用に取得します。</summary>
	StageResultData GetStageResultData() const;
protected:
	/// <summary>入力先の切り替えとGameTimeの停止状態を一括して反映します。</summary>
	void SetGameplayMode(GameplayMode mode);
	/// <summary>派生シーン固有の演出で配置オブジェクトを変更するための非所有ポインタ一覧です。</summary>
	std::vector<GameObject*> GetMutableSceneObjects() {
		std::vector<GameObject*> objects;
		objects.reserve(sceneObjects_.size());
		for (const auto& object : sceneObjects_) {
			objects.push_back(object.get());
		}
		return objects;
	}
	/// <summary>派生シーンから配置オブジェクトを読み取るための非所有ポインタ一覧です。</summary>
	std::vector<const GameObject*> GetSceneObjects() const {
		std::vector<const GameObject*> objects;
		objects.reserve(sceneObjects_.size());
		for (const auto& object : sceneObjects_) {
			objects.push_back(object.get());
		}
		return objects;
	}

private:
	friend class SceneManager;
	/// <summary>シーン内オブジェクトの更新と当たり判定を行います。</summary>
	void UpdateSceneObjects();
	/// <summary>エディタ用のカメラ操作とオブジェクト選択を更新します。</summary>
	void UpdateEditorTools();
	/// <summary>シーン内オブジェクトの2D描画を行います。</summary>
	void DrawSceneObjects2D();
	/// <summary>シーン内オブジェクトの3D描画を行います。</summary>
	void DrawSceneObjects3D();
	/// <summary>影を有効にした3Dオブジェクトを影生成パスへ描画します。</summary>
	void DrawSceneObjectShadowMaps();
	/// <summary>エディタ用ImGuiウィンドウを描画します。</summary>
	void DrawEditorImGui();
	/// <summary>エディタで配置したオブジェクト情報をJSONへ保存します。</summary>
	void SaveEditorObjects();
	/// <summary>JSONからエディタ配置オブジェクトを読み込みます。</summary>
	void LoadEditorObjects();

	enum class LevelUpChoiceType {
		AttackLevelUp,
		AttackSuper,
		NewAttack,
		StatusLevelUp,
		NewStatus,
		Decline
	};
	struct LevelUpChoice {
		LevelUpChoiceType type = LevelUpChoiceType::AttackLevelUp;
		std::string name;
		std::string title;
		std::string description;
		int slotIndex = -1;
		std::string textureFilePath;
	};
/// <summary>
/// 指定された種類のエディタオブジェクトを生成します。
/// </summary>
	GameObject* CreateEditorObject(EditorCreateType type, const std::string& modelFilePath = "");
/// <summary>
/// 現在選択中のエディタオブジェクトを削除します。
/// </summary>
	void DeleteSelectedEditorObject();
/// <summary>
/// シーン内オブジェクトの階層ウィンドウを描画します。
/// </summary>
	void DrawEditorHierarchy();
	void DrawEditorProjectAssets();
	void HandleGameViewAssetDrop();
	void DrawEditorCameraSelector();
/// <summary>
/// 選択中オブジェクトのインスペクタを描画します。
/// </summary>
	void DrawEditorInspector();
	/// <summary>
	/// 選択中オブジェクトのComponent固有設定を描画します。
	/// </summary>
	void DrawSelectedComponentInspector(GameObject* selectedObject, const std::string& selectedComponentLabel);
	/// <summary>
	/// EnemySpawnPointComponentの生成条件とスケジュール設定を描画します。
	/// </summary>
	void DrawEnemySpawnPointInspector(GameObject* selectedObject);
	void DrawPlayerInspector();
	/// <summary>
	/// 選択中プレイヤーの能力値、装備スロット、モデルと保存操作を描画します。
	/// </summary>
	void DrawPlayerStatsInspector(GameObject* selectedObject, Player* player);
	/// <summary>
	/// プレイヤーのステータスアイテム定義、モデル選択、変更反映と保存操作を描画します。
	/// </summary>
	void DrawPlayerPersistenceInspector(GameObject* selectedObject, Player* player, PlayerStats& stats, bool& statsChanged, bool statusSlotsChanged);
	void DrawEnemyInspector();
	void DrawPlayerAttackInspector();
	void ReloadPlayerAttackInspectorCache();
	PlayerAttackStats* FindCachedPlayerAttackStats(const std::string& attackName);
/// <summary>
/// 選択中オブジェクトを操作するギズモを描画します。
/// </summary>
	void DrawEditorGizmo();
/// <summary>
/// カメラコンポーネントの編集UIを描画します。
/// </summary>
	void DrawCameraInspector(GameObject* selectedObject);
/// <summary>
/// コライダーコンポーネントの編集UIを描画します。
/// </summary>
	void DrawOBBColliderInspector(GameObject* selectedObject);
	/// <summary>PointLightComponentの追加、削除、光源設定を描画します。</summary>
	void DrawPointLightInspector(GameObject* selectedObject);
	/// <summary>GlowBillboardComponentの追加、削除、Halo設定を描画します。</summary>
	void DrawGlowBillboardInspector(GameObject* selectedObject);
/// <summary>
/// パーティクルエミッターコンポーネントの編集UIを描画します。
/// </summary>
	void DrawParticleEmitterInspector(GameObject* selectedObject);
/// <summary>
/// マウスクリックによるエディタオブジェクト選択を更新します。
/// </summary>
	void UpdateEditorObjectPicking();
/// <summary>
/// エディタ用カメラのマウス操作を更新します。
/// </summary>
	void UpdateEditorCameraControl();
/// <summary>
/// 有効なコライダー同士の当たり判定と押し戻しを行います。
/// </summary>
	void UpdateColliderCollisions();
/// <summary>
/// 指定したカメラを描画用の既定カメラへ反映します。
/// </summary>
	void ApplyCamera(Camera* camera);
/// <summary>
/// 現在選択されているアクティブカメラを反映します。
/// </summary>
	void ApplyActiveCamera();
	/// <summary>シーン内の有効なPointLightComponentを全3Dオブジェクトへ反映します。</summary>
	void ApplyScenePointLight();
/// <summary>
/// カメラの追従対象リンクを名前から解決します。
/// </summary>
	void ResolveCameraLinks();
	void ResolveEnemySpawnPointLinks();
	void ResolveEnemyLinks();
	void UpdateEnemySpawning();
	/// <summary>ループステージで動的オブジェクト群の座標を連続したまま折り返します。</summary>
	void UpdateStageBoundaryWrapping();
	void UpdateEnemyAttacks();
	void UpdateEnemyProjectileHits();
	void UpdatePlayerAttacks();
	void UpdatePlayerProjectileHits();
	/// <summary>近接する同単位の通常経験値を、合計値を保ったまま次の単位へ圧縮します。</summary>
	void UpdateExperienceCompression();
	void UpdateItemDrops();
	void UpdateBossUpgradeRewards();
	void CleanupExpiredPlayerProjectiles();
	void UpdatePlayerHealthHud();
	/// <summary>ステージ中に回収したGの累計を左上HUDへ反映します。</summary>
	void UpdateChallengeMoneyHud();
	/// <summary>ステージ中の今回獲得Gを左上へ描画します。</summary>
	void DrawChallengeMoneyHud();
	/// <summary>現在レベル内の経験値進捗を計算し、画面下部のバーと表示文字列を更新します。</summary>
	void UpdatePlayerExperienceHud();
	/// <summary>画面下部へ次のレベルアップまでの経験値バーを描画します。</summary>
	void DrawPlayerExperienceHud();
	/// <summary>攻撃・ステータススロットの背景、画像、表示キャッシュを更新します。</summary>
	void UpdatePlayerSlotHud();
	/// <summary>右上へ攻撃5枠とステータス5枠を描画します。</summary>
	void DrawPlayerSlotHud();
	void UpdateLevelUpSelection();
	bool BuildLevelUpChoices(Player* player);
	/// <summary>購入済みの場合、通常レベルアップ候補を1回だけ再抽選します。</summary>
	void RerollLevelUpChoices();
	void ApplyLevelUpChoice(int choiceIndex);
	void EnsureLevelUpSelectionSprites();
	void DrawLevelUpSelection2D();
/// <summary>
/// 指定したオブジェクトのカメラをアクティブカメラに設定します。
/// </summary>
	void SetActiveCameraObject(GameObject* object);
/// <summary>
/// 最初に見つかった有効なカメラオブジェクトを返します。
/// </summary>
	GameObject* FindFirstCameraObject();
/// <summary>
/// 名前に一致するシーンオブジェクトを検索します。
/// </summary>
	GameObject* FindObjectByName(const std::string& name) const;
/// <summary>
/// 既存名と重複しないオブジェクト名を生成します。
/// </summary>
	std::string MakeUniqueObjectName(const std::string& baseName) const;
	GameObject* CreateRuntimeEnemy(
		const std::string& enemyTypeName, const Vector3& position, GameObject* target,
		const EnemySpawnPointComponent::EnemyStatMultipliers& statMultipliers = {});
	GameObject* CreateRuntimeEnemyProjectile(const EnemyShotRequest& request);
	GameObject* CreateRuntimeExperience(const EnemyStats& enemyStats, const Vector3& position, GameObject* target);
	GameObject* CreateRuntimeItemDrop(ItemDropType type, const Vector3& position, GameObject* target, float healAmount = 0.0f, int moneyAmount = 0);
	/// <summary>指定回数の強化を内包したボス報酬を1個だけ生成します。</summary>
	void CreateRuntimeBossUpgradeDrop(const Vector3& position, GameObject* target, int upgradeCount);
	/// <summary>装備中の攻撃・アイテムから強化可能な対象を抽選し、実際に適用できた回数を返します。</summary>
	int ApplyRandomBossUpgrades(Player* player, int upgradeCount);
	/// <summary>強化先がなかった残り回数について、未所持装備の取得確認を予約します。</summary>
	void QueueBossAcquisitionOffers(Player* player, int offerCount);
	/// <summary>予約された取得候補を1件ずつ確認画面へ表示します。</summary>
	bool ShowNextBossAcquisitionOffer();
	GameObject* CreateRuntimePlayerProjectile(const PlayerAttackShotRequest& request);
	GameObject* FindNearestEnemy(const Vector3& position) const;
/// <summary>
/// 文字列からエディタ生成タイプへ変換します。
/// </summary>
	EditorCreateType EditorCreateTypeFromName(const std::string& typeName) const;
	void CreateOrReloadEditorSkyBox(const std::string& textureFilePath);
	void DrawEditorSkyBoxControls();

	SceneManager* sceneManager = nullptr;
	std::string sceneName_ = "None";
	std::string sceneObjectFilePathOverride_;
	/// <summary>空でない場合、Playerの能力・モデル・初期装備をこのタイプから読み込みます。</summary>
	std::string playerTypeOverride_;
	std::vector<std::unique_ptr<GameObject>> sceneObjects_;
	/// <summary>生存中のボス戦用ランタイム敵名です。空なら通常スポーンを再開します。</summary>
	std::string activeBossEncounterObjectName_;
	/// <summary>最終ボス撃破後、リザルトへ遷移するまで保持するクリアフラグです。</summary>
	bool isStageCleared_ = false;
	/// <summary>今回のゲームプレイでHPを0にした敵の累計です。</summary>
	int defeatedEnemyCount_ = 0;
	/// <summary>現在のゲームプレイで回収済みのコインとボス報酬の合計金額です。</summary>
	int challengeMoneyEarned_ = 0;
	/// <summary>GameTimeが進行しているフレームだけ加算する生存時間です。</summary>
	float survivalTimeSeconds_ = 0.0f;
	int selectedObjectIndex_ = -1;
	int nextObjectId_ = 1;
	EditorCreateType createType_ = EditorCreateType::Object3dSphere;
	int selectedLoadedModelIndex_ = 0;
	int selectedAnimatedModelIndex_ = 0;
	int selectedTextureIndex_ = 0;
	int selectedParticlePresetIndex_ = 0;
	int selectedEnemyTypeIndex_ = 0;
	int selectedPlayerModelIndex_ = 0;
	int selectedPlayerTypeIndex_ = 0;
	int selectedPlayerAttackTypeIndex_ = 0;
	int selectedPlayerAttackLevelIndex_ = 0;
	int selectedSkyBoxTextureIndex_ = 0;
	int selectedInspectorComponentIndex_ = 0;
	std::array<char, 64> particlePresetNameBuffer_ = {};
	std::array<char, 64> enemyTypeNameBuffer_ = {};
	std::string enemyInspectorObjectName_;
	std::array<char, 64> playerTypeNameBuffer_ = {};
	std::string playerTypeEditMessage_;
	std::array<char, 64> playerAttackNameBuffer_ = {};
	std::array<char, 512> textEditBuffer_ = {};
	std::vector<std::string> cachedPlayerAttackNames_;
	std::vector<PlayerAttackStats> cachedPlayerAttackStats_;
	bool isPlayerAttackCacheLoaded_ = false;
	bool isGizmoEnabled_ = true;
	bool isEditorSkyBoxEnabled_ = false;
	int gizmoOperationIndex_ = 0;
	std::string activeCameraObjectName_;
	std::string skyBoxTextureFilePath_;
	std::unique_ptr<SkyBox> editorSkyBox_;
	std::unique_ptr<Sprite> playerHealthBarBackground_;
	std::unique_ptr<Sprite> playerHealthBarFill_;
	bool isPlayerHealthHudVisible_ = false;
	/// <summary>今回の挑戦で取得した、全体強化適用後のG累計を表示するHUD文字です。</summary>
	std::unique_ptr<GameObject> challengeMoneyTextObject_;
	bool isChallengeMoneyHudVisible_ = false;
	/// <summary>経験値バーの背景、進捗部分、レベル・経験値表示を構成するHUD要素です。</summary>
	std::unique_ptr<Sprite> playerExperienceBarBackground_;
	std::unique_ptr<Sprite> playerExperienceBarFill_;
	std::unique_ptr<GameObject> playerExperienceTextObject_;
	/// <summary>現在レベル開始時を0、次レベル到達時を1とした経験値進捗率です。</summary>
	float playerExperienceRate_ = 0.0f;
	bool isPlayerExperienceHudVisible_ = false;
	/// <summary>上段の攻撃5枠、下段のステータス5枠を構成する背景とアイコンです。</summary>
	std::array<std::unique_ptr<Sprite>, 5> playerAttackSlotBackgroundSprites_;
	std::array<std::unique_ptr<Sprite>, 5> playerAttackSlotIconSprites_;
	std::array<std::unique_ptr<Sprite>, 5> playerStatusSlotBackgroundSprites_;
	std::array<std::unique_ptr<Sprite>, 5> playerStatusSlotIconSprites_;
	/// <summary>各スロット下端でレベル文字の可読性を確保する半透明帯です。</summary>
	std::array<std::unique_ptr<Sprite>, 5> playerAttackSlotLevelBackgroundSprites_;
	std::array<std::unique_ptr<Sprite>, 5> playerStatusSlotLevelBackgroundSprites_;
	/// <summary>装備中の武器・ステータスの現在レベル表示です。</summary>
	std::array<std::unique_ptr<GameObject>, 5> playerAttackSlotLevelTextObjects_;
	std::array<std::unique_ptr<GameObject>, 5> playerStatusSlotLevelTextObjects_;
	std::array<bool, 5> playerAttackSlotIconVisible_{};
	std::array<bool, 5> playerStatusSlotIconVisible_{};
	std::array<bool, 5> playerAttackSlotLevelVisible_{};
	std::array<bool, 5> playerStatusSlotLevelVisible_{};
	/// <summary>名前とレベルが変化したスロットだけJSONを再読込するためのキャッシュです。</summary>
	std::array<std::string, 5> playerAttackSlotTextureKeys_{};
	std::array<std::string, 5> playerAttackSlotTexturePaths_{};
	std::array<std::string, 5> playerStatusSlotTextureKeys_{};
	std::array<std::string, 5> playerStatusSlotTexturePaths_{};
	std::unique_ptr<GameObject> playerAttackSlotLabelObject_;
	std::unique_ptr<GameObject> playerStatusSlotLabelObject_;
	bool isPlayerSlotHudVisible_ = false;
	GameplayMode gameplayMode_ = GameplayMode::Playing;
	Player* levelUpPlayer_ = nullptr;
	int selectedLevelUpChoiceIndex_ = 0;
	std::vector<LevelUpChoice> levelUpChoices_;
	/// <summary>現在の通常レベルアップ選択でリロールを消費したかを示します。</summary>
	bool levelUpRerollUsed_ = false;
	/// <summary>ボス報酬で提示する未所持攻撃・アイテムの待機列です。</summary>
	std::vector<LevelUpChoice> bossAcquisitionOfferQueue_;
	/// <summary>取得確認の反映先となるプレイヤーへの非所有参照です。</summary>
	Player* bossAcquisitionPlayer_ = nullptr;
	std::unique_ptr<Sprite> levelUpOverlaySprite_;
	std::unique_ptr<Sprite> levelUpPanelSprite_;
	std::array<std::unique_ptr<Sprite>, 3> levelUpChoiceBorderSprites_;
	std::array<std::unique_ptr<Sprite>, 3> levelUpChoiceSprites_;
	std::array<std::unique_ptr<Sprite>, 3> levelUpChoiceIconSprites_;
	std::unique_ptr<GameObject> levelUpTitleTextObject_;
	std::unique_ptr<GameObject> levelUpInstructionTextObject_;
	std::array<std::unique_ptr<GameObject>, 3> levelUpChoiceTextObjects_;
	Camera* fallbackCamera_ = nullptr;
};
