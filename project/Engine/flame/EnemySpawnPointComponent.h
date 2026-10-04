#pragma once
#include "../base/GameTime.h"
#include "camera/Camera.h"
#include "Component.h"
#include "MathConstants.h"
#include "GameObject.h"
#include "LineDrawer.h"
#include "Matrix.h"
#include "object/Object3dCommon.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

/// <summary>カメラ表示範囲の外側に敵の出現候補点を作り、時間帯別の生成要求を発行します。</summary>
class EnemySpawnPointComponent : public Component {
public:
	/// <summary>敵生成時に基礎能力へ掛ける時間帯別倍率です。</summary>
	struct EnemyStatMultipliers {
		float healthMultiplier = 1.0f;
		float speedMultiplier = 1.0f;
		float experienceMultiplier = 1.0f;
	};
	/// <summary>指定時刻以降に使用する雑魚敵の能力倍率です。</summary>
	struct TimeScalingTier {
		float startTimeSeconds = 0.0f;
		EnemyStatMultipliers multipliers{};
	};
	/// <summary>一定時間帯に適用する敵タイプ、生成間隔、生成数の設定です。</summary>
	struct SpawnSchedule {
		float startTimeSeconds = 0.0f;
		float endTimeSeconds = 60.0f;
		std::string enemyTypeName = "Default";
		int spawnIntervalFrames = 60;
		int spawnAmount = 1;
		/// <summary>trueの場合、生成時点の時間帯倍率をこの敵へ適用します。</summary>
		bool applyTimeScaling = false;
		/// <summary>trueの場合、時間帯内で最初の生成要求だけを発行します。</summary>
		bool spawnOnce = false;
		/// <summary>実行中だけ使用する生成間隔カウンターです。JSONには保存しません。</summary>
		int frameCounter = 0;
		/// <summary>spawnOnceスケジュールが生成済みかを表す実行時フラグです。</summary>
		bool hasSpawned = false;
	};
	/// <summary>シーン側へ渡す、生成位置が確定した敵生成要求です。</summary>
	struct ScheduledSpawnRequest {
		std::string enemyTypeName;
		Vector3 position{};
		EnemyStatMultipliers statMultipliers{};
	};
	/// <summary>指定時刻に一度だけ発生するボス戦の設定です。</summary>
	struct BossEncounterSettings {
		bool enabled = false;
		float triggerTimeSeconds = 300.0f;
		std::string enemyTypeName = "MidBoss";
		Vector3 bossPosition{0.0f, 0.0f, 0.0f};
		Vector3 playerWarpPosition{0.0f, 0.0f, 6.0f};
	};

	void Update() override;

	void Draw3D() override;

	void SetTarget(GameObject* target);
	GameObject* GetTarget() const;
	void SetTargetName(const std::string& name);
	const std::string& GetTargetName() const;

	void SetCamera(Camera* camera);
	void SetCameraName(const std::string& name);
	const std::string& GetCameraName() const;

	void SetSpawnCount(int count);
	int GetSpawnCount() const;
	void SetOuterMargin(float margin);
	float GetOuterMargin() const;
	void SetMinimumRadius(float radius);
	float GetMinimumRadius() const;
	void SetGroundY(float groundY);
	float GetGroundY() const;
	void SetPointHeight(float height);
	float GetPointHeight() const;
	void SetDrawDebug(bool drawDebug);
	bool GetDrawDebug() const;
	void SetDebugPointSize(float size);
	float GetDebugPointSize() const;
	void SetEnemyTypeName(const std::string& enemyTypeName);
	const std::string& GetEnemyTypeName() const;
	void SetSpawnEnabled(bool spawnEnabled);
	bool GetSpawnEnabled() const;
	void ResetSpawnTimer();
	float GetElapsedTimeSeconds() const;
	const std::vector<SpawnSchedule>& GetSpawnSchedules() const;
	void SetSpawnSchedules(const std::vector<SpawnSchedule>& schedules);
	const std::vector<TimeScalingTier>& GetTimeScalingTiers() const;
	void SetTimeScalingTiers(const std::vector<TimeScalingTier>& tiers);
	EnemyStatMultipliers GetCurrentTimeScaling() const;
	const BossEncounterSettings& GetBossEncounterSettings() const;
	void SetBossEncounterSettings(const BossEncounterSettings& settings);
	bool ConsumeBossEncounterRequest();
	bool IsBossEncounterTriggered() const;
	void SetBossEncounterActive(bool active);
	bool IsBossEncounterActive() const;

	std::vector<ScheduledSpawnRequest> ConsumeScheduledSpawnRequests();

	bool ConsumeSpawnRequest(float spawnsPerMinute, Vector3& outPosition);

	const std::vector<Vector3>& GetSpawnPoints() const;

private:
	Vector3 TransformCoord(const Vector3& vector, const Matrix4x4& matrix) const;

	bool IntersectCameraRayToGround(Camera* camera, const Vector2& ndc, Vector3& outPoint) const;

	/// <summary>カメラの地面投影範囲を基に、画面外の生成候補点を再計算します。</summary>
	void RecalculateSpawnPoints();

	/// <summary>生成位置の基準となる対象への非所有参照です。</summary>
	GameObject* target_ = nullptr;
	Camera* camera_ = nullptr;
	std::string enemyTypeName_ = "Default";
	std::string targetName_;
	std::string cameraName_;
	/// <summary>画面外周に配置した敵生成候補位置です。</summary>
	std::vector<Vector3> spawnPoints_;
	/// <summary>カメラ視錐台を地面へ投影した四隅です。</summary>
	std::vector<Vector3> groundViewCorners_;
	/// <summary>候補点を均等に使用するための次回位置です。</summary>
	size_t nextSpawnIndex_ = 0;
	int spawnCount_ = 8;
	float spawnTimerSeconds_ = 0.0f;
	float elapsedTimeSeconds_ = 0.0f;
	/// <summary>ゲーム時間帯ごとの敵生成設定です。</summary>
	std::vector<SpawnSchedule> spawnSchedules_;
	/// <summary>開始時刻の早い順に適用する、生成時能力の段階倍率です。</summary>
	std::vector<TimeScalingTier> timeScalingTiers_;
	BossEncounterSettings bossEncounterSettings_;
	bool bossEncounterTriggered_ = false;
	bool bossEncounterActive_ = false;
	float outerMargin_ = 5.0f;
	float minimumRadius_ = 8.0f;
	float groundY_ = 0.0f;
	float pointHeight_ = 0.2f;
	float debugPointSize_ = 0.5f;
	bool drawDebug_ = true;
	bool spawnEnabled_ = true;
};
