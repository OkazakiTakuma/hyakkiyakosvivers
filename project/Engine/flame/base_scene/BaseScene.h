#pragma once
#include "Audio.h"
#include "camera/Camera.h"
#include "camera/CameraComponent.h"
#include "components/Enemy/EnemyComponent.h"
#include "components/EnemyAttackEntity/EnemyAttackEntityComponent.h"
#include "components/EnemySpawnPoint/EnemySpawnPointComponent.h"
#include "components/Experience/ExperienceComponent.h"
#include "components/Gravity/GravityComponent.h"
#include "components/ItemDrop/ItemDropComponent.h"
#include "core/Entity.h"
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
#include "components/PlayerAttack/PlayerAttackComponent.h"
#include "components/PlayerAttackEntity/PlayerAttackEntityComponent.h"
#include "../../../Player/PlayerComponent.h"
#include "collision/SphereColliderComponent.h"
#include "SpriteComponent.h"
#include "base_scene/StageResultData.h"
#include "base_scene/runtime/GameplaySession.h"
#include "base_scene/runtime/GameplayHud.h"
#include "base_scene/runtime/SceneWorld.h"
#include "base_scene/editor/SceneEditorSkyBox.h"
#include "base_scene/editor/SceneEditorState.h"
#include "base_scene/runtime/EnemySpawnSystem.h"
#include "base_scene/runtime/CombatSystem.h"
#include "services/LootDropService.h"
#include "services/PlayerStatsCalculator.h"
#include "services/PlayerAttackAssembler.h"
#include "services/ParticlePresetAssembler.h"
#include "TextComponent.h"
#include "instancing/InstancingModel.h"
#include "helpers/CollisionSpatialGrid.h"
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
	using GameplayMode = GameplaySession::Mode;

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

	virtual void SetSceneManager(SceneManager* manager) { sceneManager_ = manager; }
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
		return gameplaySession_.IsSelectionActive();
	}
	GameplayMode GetGameplayMode() const { return gameplaySession_.GetMode(); }
	/// <summary>最終ボスを倒し、ステージクリア条件を満たしたかを返します。</summary>
	bool IsStageCleared() const { return gameplaySession_.IsStageCleared(); }
	/// <summary>プレイヤーの体力が0になったかを返します。</summary>
	bool IsPlayerDefeated() const;
	/// <summary>現在の装備とプレイ戦績をリザルト表示用に取得します。</summary>
	StageResultData GetStageResultData() const;
protected:
	/// <summary>派生シーンが遷移や共有進行へアクセスするための、基底で一元管理された参照です。</summary>
	SceneManager* GetSceneManager() const { return sceneManager_; }
	/// <summary>全派生シーンで共有するSceneManagerへの唯一の非所有参照です。</summary>
	SceneManager* sceneManager_ = nullptr;
	/// <summary>入力先の切り替えとGameTimeの停止状態を一括して反映します。</summary>
	void SetGameplayMode(GameplayMode mode);
	Entity* CreateRuntimeEnemy(
		const std::string& enemyTypeName, const Vector3& position, Entity* target,
		const EnemySpawnPointComponent::EnemyStatMultipliers& statMultipliers = {});
	Entity* CreateRuntimePlayerAttackEntity(const PlayerAttackShotRequest& request);
	void BeginRuntimeBossEncounter(const Entity& boss);
	Entity* FindObjectByName(const std::string& name) const;
	/// <summary>派生シーン固有の演出で配置オブジェクトを変更するための非所有ポインタ一覧です。</summary>
	std::vector<Entity*> GetMutableSceneObjects() {
		std::vector<Entity*> objects;
		objects.reserve(sceneObjects_.size());
		for (const auto& object : sceneObjects_) {
			objects.push_back(object.get());
		}
		return objects;
	}
	/// <summary>派生シーンから配置オブジェクトを読み取るための非所有ポインタ一覧です。</summary>
	std::vector<const Entity*> GetSceneObjects() const {
		std::vector<const Entity*> objects;
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
	/// <summary>削除対象を参照する全Componentの非所有ポインタを解除します。</summary>
	void ClearEntityReferences(Entity* target);
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
		/// <summary>この候補と進化条件で対応し、現在所持している武器・アイテムの画像です。</summary>
		std::vector<std::string> correspondingOwnedTextureFilePaths;
	};
/// <summary>
/// 指定された種類のエディタオブジェクトを生成します。
/// </summary>
	Entity* CreateEditorObject(EditorCreateType type, const std::string& modelFilePath = "");
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
	void DrawSelectedComponentInspector(Entity* selectedObject, const std::string& selectedComponentLabel);
	/// <summary>
	/// EnemySpawnPointComponentの生成条件とスケジュール設定を描画します。
	/// </summary>
	void DrawEnemySpawnPointInspector(Entity* selectedObject);
	void DrawPlayerInspector();
	/// <summary>
	/// 選択中プレイヤーの能力値、装備スロット、モデルと保存操作を描画します。
	/// </summary>
	void DrawPlayerStatsInspector(Entity* selectedObject, PlayerComponent* player);
	/// <summary>
	/// プレイヤーのステータスアイテム定義、モデル選択、変更反映と保存操作を描画します。
	/// </summary>
	void DrawPlayerPersistenceInspector(Entity* selectedObject, PlayerComponent* player, PlayerStats& stats, bool& statsChanged, bool statusSlotsChanged);
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
	void DrawCameraInspector(Entity* selectedObject);
/// <summary>
/// コライダーコンポーネントの編集UIを描画します。
/// </summary>
	void DrawOBBColliderInspector(Entity* selectedObject);
	/// <summary>PointLightComponentの追加、削除、光源設定を描画します。</summary>
	void DrawPointLightInspector(Entity* selectedObject);
	/// <summary>GlowBillboardComponentの追加、削除、Halo設定を描画します。</summary>
	void DrawGlowBillboardInspector(Entity* selectedObject);
/// <summary>
/// パーティクルエミッターコンポーネントの編集UIを描画します。
/// </summary>
	void DrawParticleEmitterInspector(Entity* selectedObject);
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
	/// <summary>ボス戦中だけ存在する、地形1枚分の閉鎖壁を生成します。</summary>
	void CreateBossArenaWalls(const EnemySpawnPointComponent::BossEncounterSettings& settings);
	/// <summary>ボス撃破を検知したら実行時アリーナ壁を取り除きます。</summary>
	void UpdateBossArenaWalls();
	/// <summary>ボス戦中のプレイヤーを、コライダー全体がアリーナ内に収まる位置へ補正します。</summary>
	void ConstrainPlayersToBossArena();
	void RemoveBossArenaWalls();
	/// <summary>プレイヤーのワールド座標は変えず、周囲の地形タイルだけを再配置します。</summary>
	void UpdateLoopingTerrain();
	void UpdateEnemyAttacks();
	void UpdatePlayerAttacks();
	void UpdatePlayerAttackEntityHits();
	/// <summary>命中地点の敵頭上へ、上昇しながら消えるダメージ数値を追加します。</summary>
	void CreateDamageNumber(float damage, const Vector3& enemyPosition, float enemySizeScale);
	void UpdateDamageNumbers();
	void DrawDamageNumbers();
	/// <summary>近接する同単位の通常経験値を、合計値を保ったまま次の単位へ圧縮します。</summary>
	void UpdateExperienceCompression();
	void UpdateItemDrops();
	void UpdateBossUpgradeRewards();
	void UpdateBossRewardPresentation();
	void CleanupExpiredPlayerAttackEntities();
	/// <summary>シーン内で最初に見つかった有効なプレイヤーを返します。</summary>
	PlayerComponent* FindActivePlayer() const;
	void UpdateGameplayHud();
	void UpdateLevelUpSelection();
	bool BuildLevelUpChoices(PlayerComponent* player);
	/// <summary>候補の進化条件に対応する所持装備の画像を設定します。</summary>
	void PopulateCorrespondingOwnedIcons(LevelUpChoice& choice, const PlayerStats& stats) const;
	/// <summary>購入済みの場合、通常レベルアップ候補を1回だけ再抽選します。</summary>
	void RerollLevelUpChoices();
	void ApplyLevelUpChoice(int choiceIndex);
	void EnsureLevelUpSelectionSprites();
	void DrawLevelUpSelection2D();
	void EnsureBossRewardPresentationSprites();
	void DrawBossRewardPresentation2D();
/// <summary>
/// 指定したオブジェクトのカメラをアクティブカメラに設定します。
/// </summary>
	void SetActiveCameraObject(Entity* object);
/// <summary>
/// 最初に見つかった有効なカメラオブジェクトを返します。
/// </summary>
	Entity* FindFirstCameraObject();
/// <summary>
/// 名前に一致するシーンオブジェクトを検索します。
/// </summary>
/// <summary>指定した親が存在し、自己参照や循環を作らないか検証します。</summary>
bool CanSetObjectParent(const Entity* child, const std::string& parentName) const;
/// <summary>検証に成功した場合だけ親子関係を更新します。</summary>
bool TrySetObjectParent(Entity* child, const std::string& parentName);
/// <summary>
/// 既存名と重複しないオブジェクト名を生成します。
/// </summary>
	std::string MakeUniqueObjectName(const std::string& baseName) const;
	Entity* CreateRuntimeEnemyAttackEntity(const EnemyShotRequest& request);
	Entity* CreateRuntimeExperience(const EnemyStats& enemyStats, const Vector3& position, Entity* target);
	Entity* CreateRuntimeItemDrop(ItemDropType type, const Vector3& position, Entity* target, float healAmount = 0.0f, int moneyAmount = 0);
	/// <summary>指定回数の強化を内包したボス報酬を1個だけ生成します。</summary>
	void CreateRuntimeBossUpgradeDrop(const Vector3& position, Entity* target, int upgradeCount);
	/// <summary>設定された3テーブルから指定数を抽選し、所持品強化と未所持品候補を作成します。</summary>
	void RollBossRewards(PlayerComponent* player, int rewardCount);
	/// <summary>予約された取得候補を1件ずつ確認画面へ表示します。</summary>
	bool ShowNextBossAcquisitionOffer();
	Entity* FindNearestEnemy(const Vector3& position) const;
/// <summary>
/// 文字列からエディタ生成タイプへ変換します。
/// </summary>
	EditorCreateType EditorCreateTypeFromName(const std::string& typeName) const;

	std::string sceneName_ = "None";
	std::string sceneObjectFilePathOverride_;
	/// <summary>空でない場合、Playerの能力・モデル・初期装備をこのタイプから読み込みます。</summary>
	std::string playerTypeOverride_;
	/// <summary>シーン内Entityの所有と共通更新・描画を担当します。</summary>
	SceneWorld sceneObjects_;
	/// <summary>空間分割を用いた衝突候補抽出と戦闘命中処理を担当します。</summary>
	CombatSystem combatSystem_;
	/// <summary>通常敵・ボスの生成要求と、ボス戦終了条件を管理します。</summary>
	EnemySpawnSystem enemySpawnSystem_;
	/// <summary>敵撃破時の経験値・アイテム・ボス報酬の抽選を担当します。</summary>
	LootDropService lootDropService_;
	/// <summary>今回のプレイに属するモード・戦績・クリア状態の所有者です。</summary>
	GameplaySession gameplaySession_;
	struct DamageNumberPopup {
		std::unique_ptr<Entity> textObject;
		Vector3 worldPosition{};
		float elapsedSeconds = 0.0f;
		float horizontalOffset = 0.0f;
	};
	std::vector<DamageNumberPopup> damageNumberPopups_;
	uint32_t nextDamageNumberIndex_ = 0;
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
	int gizmoOperationIndex_ = 0;
	/// <summary>エディタの選択対象とアクティブカメラ指定を管理します。</summary>
	SceneEditorState sceneEditorState_;
	/// <summary>エディタ用SkyBoxのリソースと編集状態を管理します。</summary>
	SceneEditorSkyBox sceneEditorSkyBox_;
	/// <summary>戦闘HUDの表示状態と描画リソースを管理します。</summary>
	GameplayHud gameplayHud_;
	PlayerComponent* levelUpPlayer_ = nullptr;
	int selectedLevelUpChoiceIndex_ = 0;
	std::vector<LevelUpChoice> levelUpChoices_;
	/// <summary>現在の通常レベルアップ選択でリロールを消費したかを示します。</summary>
	bool levelUpRerollUsed_ = false;
	/// <summary>ボス報酬で提示する未所持攻撃・アイテムの待機列です。</summary>
	std::vector<LevelUpChoice> bossAcquisitionOfferQueue_;
	/// <summary>取得確認の反映先となるプレイヤーへの非所有参照です。</summary>
	PlayerComponent* bossAcquisitionPlayer_ = nullptr;
	std::unique_ptr<Sprite> levelUpOverlaySprite_;
	std::unique_ptr<Sprite> levelUpPanelSprite_;
	std::array<std::unique_ptr<Sprite>, 2> levelUpPanelAccentSprites_;
	std::array<std::unique_ptr<Sprite>, 3> levelUpChoiceBorderSprites_;
	std::array<std::unique_ptr<Sprite>, 3> levelUpChoiceSprites_;
	std::array<std::unique_ptr<Sprite>, 3> levelUpChoiceAccentSprites_;
	std::array<std::unique_ptr<Sprite>, 3> levelUpChoiceIconSprites_;
	std::array<std::array<std::unique_ptr<Sprite>, kPlayerEquipmentSlotCount>, 3> levelUpChoiceCorrespondingIconSprites_;
	std::unique_ptr<Entity> levelUpTitleTextObject_;
	std::unique_ptr<Entity> levelUpInstructionTextObject_;
	std::array<std::unique_ptr<Entity>, 3> levelUpChoiceTextObjects_;
	/// <summary>開封演出の最後に表示する全抽選結果です。</summary>
	std::vector<LevelUpChoice> bossRewardResults_;
	float bossRewardPresentationTimer_ = 0.0f;
	bool bossRewardPresentationReady_ = false;
	int bossRewardPresentationMoney_ = 0;
	std::unique_ptr<Sprite> bossRewardOverlaySprite_;
	std::unique_ptr<Sprite> bossRewardFlashSprite_;
	std::unique_ptr<Sprite> bossRewardChestBackSprite_;
	std::unique_ptr<Sprite> bossRewardChestFrontSprite_;
	std::unique_ptr<Sprite> bossRewardChestLidSprite_;
	std::array<std::unique_ptr<Sprite>, 16> bossRewardBurstSprites_;
	std::array<std::unique_ptr<Sprite>, 5> bossRewardIconBackSprites_;
	std::array<std::unique_ptr<Sprite>, 5> bossRewardIconSprites_;
	std::unique_ptr<Entity> bossRewardTitleTextObject_;
	std::unique_ptr<Entity> bossRewardMoneyTextObject_;
	std::unique_ptr<Entity> bossRewardInstructionTextObject_;
	Camera* fallbackCamera_ = nullptr;
};

