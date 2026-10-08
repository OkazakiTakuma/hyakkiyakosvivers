#pragma once
#include "Component.h"
#include "MathConstants.h"
#include "GameObject.h"
#include "LineDrawer.h"
#include "EnemyProjectileComponent.h"
#include "../base/GameTime.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

/// <summary>敵が使用する移動・攻撃パターンです。</summary>
enum class EnemyBehaviorType {
	Chase,
	Shooter,
	Charger,
	NightSlashBoss,
	SelfDestruct,
	TornadoBoss,
	BurstShooter
};

/// <summary>シーンへ引き渡す敵弾生成パラメーターです。</summary>
struct EnemyShotRequest {
	Vector3 position{};
	Vector3 direction{0.0f, 0.0f, 1.0f};
	float speed = 0.12f;
	float attack = 1.0f;
	float size = 0.22f;
	float lifeTime = 6.0f;
	/// <summary>直進以外の特殊軌道をシーン側へ引き渡す設定です。</summary>
	EnemyProjectileMotionType motionType = EnemyProjectileMotionType::Linear;
	Vector3 orbitCenter{};
	float orbitAngle = 0.0f;
	float orbitInitialRadius = 0.0f;
	float orbitAngularSpeed = 0.0f;
	float orbitRadialSpeed = 0.0f;
	float orbitHeight = 0.0f;
	/// <summary>追尾弾が参照する対象です。要求および生成弾は所有権を持ちません。</summary>
	GameObject* homingTarget = nullptr;
};

/// <summary>敵タイプごとに読み込む能力・行動設定です。</summary>
struct EnemyStats {
	float health = 10.0f;
	float attack = 1.0f;
	float speed = 0.05f;
	float shootingInterval = 1.0f;
	float spawnsPerMinute = 12.0f;
	int experience = 1;
	std::string experienceModelFilePath = "enemy_drop.obj";
	/// <summary>敵撃破時に回復アイテムを落とす確率です（0.0～1.0）。</summary>
	float healthItemDropChance = 0.05f;
	/// <summary>敵撃破時に経験値全回収アイテムを落とす確率です（0.0～1.0）。</summary>
	float collectExperienceItemDropChance = 0.05f;
	/// <summary>この敵が落とす回復アイテムのHP回復量です。</summary>
	float healthItemHealAmount = 25.0f;
	bool shoots = false;
	EnemyBehaviorType behavior = EnemyBehaviorType::Chase;
	// Shooter専用: preferredDistanceを中心とした許容帯の中で停止し、近すぎる場合は後退する。
	float preferredDistance = 7.0f;
	float distanceTolerance = 1.5f;
	// 敵弾はEnemyShotRequestへコピーされ、シーン側で実体化される。
	float projectileSpeed = 0.12f;
	float projectileSize = 0.22f;
	float projectileLifeTime = 6.0f;
	int burstShotCount = 5;
	float burstSpreadAngle = 0.18f;
	// Charger専用: 接近、予兆、方向固定済みの突進、硬直を構成する調整値。
	float chargeTriggerDistance = 7.0f;
	float chargeDuration = 1.2f;
	float dashSpeed = 0.28f;
	float dashDuration = 0.65f;
	float dashRecovery = 1.0f;
	// NightSlashBoss専用: プレイヤーの左右を交互に横切る連続斬りの調整値。
	// TriggerDistance内へ入ると予兆を開始し、DashとSlashPauseを指定回数繰り返す。
	float comboTriggerDistance = 8.0f;
	float comboWindup = 0.8f;
	float comboDashSpeed = 0.48f;
	float comboDashDuration = 0.24f;
	float comboSlashPause = 0.16f;
	float comboRecovery = 1.8f;
	float comboSideOffset = 1.8f;
	int comboDashCount = 4;
	// 最終ダッシュだけに掛ける速度倍率。接触ダメージ倍率はGetContactAttackDamageで管理する。
	float finisherSpeedMultiplier = 1.35f;
	// NightSlashBoss専用: 連続斬りの間に挟む全方位弾幕・扇状弾幕の共通調整値。
	float bossRangedWindup = 0.75f;
	float bossRangedInterval = 0.32f;
	int bossRangedWaves = 3;
	int bossRadialShotCount = 12;
	int bossAimedShotCount = 5;
	float bossAimedSpreadAngle = 0.22f;
	float bossProjectileAttackMultiplier = 0.65f;
	// TornadoBoss専用: ボス中心から外側へ広がる4方向竜巻。
	int bossTornadoCount = 4;
	float bossTornadoInitialRadius = 1.4f;
	float bossTornadoAngularSpeed = 2.4f;
	float bossTornadoRadialSpeed = 0.035f;
	float bossTornadoSize = 0.65f;
	float bossTornadoLifeTime = 5.0f;
	float bossTornadoAttackMultiplier = 0.8f;
	float bossTornadoTriggerDistance = 10.0f;
	float bossTornadoWindup = 1.0f;
	float bossTornadoRecovery = 2.0f;
	// TornadoBoss専用: プレイヤーを囲んで中心へ狭まる包囲竜巻。
	int bossConvergingTornadoCount = 8;
	float bossConvergingTornadoInitialRadius = 9.0f;
	float bossConvergingTornadoAngularSpeed = -2.0f;
	float bossConvergingTornadoRadialSpeed = 0.035f;
	float bossConvergingTornadoSize = 0.55f;
	float bossConvergingTornadoLifeTime = 4.0f;
	float bossConvergingTornadoAttackMultiplier = 0.55f;
	// TornadoBoss専用: プレイヤーを追跡する単体の巨大竜巻。
	float bossGiantTornadoSpeed = 0.035f;
	float bossGiantTornadoSize = 2.2f;
	float bossGiantTornadoLifeTime = 7.0f;
	float bossGiantTornadoAttackMultiplier = 1.2f;
	float bossGiantTornadoSpawnOffset = 2.2f;
	float selfDestructTriggerDistance = 2.2f;
	float selfDestructFuseDuration = 0.8f;
	float selfDestructRadius = 3.0f;
	/// <summary>標準敵を1.0とした表示・コライダー・被弾半径の倍率です。</summary>
	float sizeScale = 1.0f;
};

class EnemyComponent;

/// <summary>行動Strategyがシーンへ公開する、敵のモデルと現在の表示状態です。</summary>
struct EnemyBehaviorPresentation {
	std::string modelFilePath;
	Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
	bool overrideColor = false;
	bool addAttackTrail = false;
	bool attackTrailEmitting = false;
};

class INightSlashState;
class NightSlashApproachState;
class NightSlashWindupState;
class NightSlashDashingState;
class NightSlashSlashingState;
class NightSlashRangedWindupState;
class NightSlashRangedFiringState;
class NightSlashRecoveringState;

/// <summary>敵の追跡、射撃、突進ステートと体力を管理します。</summary>
class EnemyComponent : public Component {
public:
	void Initialize() override;

	void Update() override;

	void Draw3D() override;

	void ApplyStats(const EnemyStats& stats);

	const EnemyStats& GetStats() const;
	void SetEnemyTypeName(const std::string& enemyTypeName);
	const std::string& GetEnemyTypeName() const;
	void SetTarget(GameObject* target);
	GameObject* GetTarget() const;
	void SetTargetName(const std::string& targetName);
	const std::string& GetTargetName() const;
	void SetCurrentHealth(float health);
	float GetCurrentHealth() const;
	void SetRuntimeSpawned(bool runtimeSpawned);
	bool GetRuntimeSpawned() const;
	std::vector<EnemyShotRequest> ConsumeShotRequests();
	bool IsChargeWarningActive() const;
	float GetChargeProgress() const;
	bool IsNightSlashWarningActive() const;
	bool IsSelfDestructArmed() const;
	float GetSelfDestructProgress() const;
	bool IsTornadoWarningActive() const;
	int GetTornadoPatternIndex() const;
	float GetTornadoWarningProgress() const;
	bool ConsumeSelfDestructRequest();
	bool IsBossRangedWarningActive() const;
	bool IsBossRangedAttacking() const;
	float GetBossRangedProgress() const;
	bool IsNightSlashAttacking() const;
	/// <summary>予兆の経過率、または完了した連続斬りの割合を0～1で返します。</summary>
	float GetNightSlashProgress() const;
	bool CanDealContactDamage() const;
	/// <summary>現在の攻撃段階を考慮した接触ダメージを返します。</summary>
	float GetContactAttackDamage() const;
	/// <summary>現在の行動と攻撃段階に対応したモデル・色・軌跡設定を返します。</summary>
	EnemyBehaviorPresentation GetBehaviorPresentation();

private:
	friend class NightSlashApproachState;
	friend class NightSlashWindupState;
	friend class NightSlashDashingState;
	friend class NightSlashSlashingState;
	friend class NightSlashRangedWindupState;
	friend class NightSlashRangedFiringState;
	friend class NightSlashRecoveringState;

	/// <summary>接近、溜め、方向固定済みの突進、攻撃後硬直から成る突進敵の状態です。</summary>
	enum class ChargeState { Approach, Charging, Dashing, Recovering };
	/// <summary>接近、斬撃予兆、連続斬り、射撃予兆、射撃、硬直から成るボス行動状態です。</summary>
	enum class NightSlashState { Approach, Windup, Dashing, Slashing, RangedWindup, RangedFiring, Recovering };
	enum class TornadoBossState { Approach, Windup, Recovering };

	void UpdateShooter(EulerTransform& transform, const Vector3& direction, float distance);

	void UpdateCharger(EulerTransform& transform, const Vector3& direction, float distance);

	/// <summary>適正距離を保ちながら、プレイヤー方向を中心とした扇状弾を一斉発射します。</summary>
	void UpdateBurstShooter(EulerTransform& transform, const Vector3& direction, float distance);

	void UpdateSelfDestruct(EulerTransform& transform, const Vector3& direction, float distance);

	void UpdateTornadoBoss(EulerTransform& transform, const Vector3& direction, float distance);

	void UpdateNightSlashBoss(EulerTransform& transform, const Vector3& direction, float distance);
	/// <summary>夜叉ボスの現在Stateを切り替え、状態固有タイマーを初期化します。</summary>
	void ChangeNightSlashState(NightSlashState state);

	/// <summary>猫ボスの姿勢と歩行アニメーションを現在の攻撃状態へ同期します。</summary>
	void UpdateNightSlashBossMotion(EulerTransform& transform);

	void EmitBossRangedWave(const EulerTransform& transform);

	void EmitBossTornadoPattern(const EulerTransform& transform);

	void EmitBossConvergingTornadoPattern(const EulerTransform& transform);

	void EmitBossGiantTornadoPattern(const EulerTransform& transform);

	void BeginNightSlashDash(const EulerTransform& transform);

	std::string enemyTypeName_ = "Default";
	std::string targetName_;
	EnemyStats stats_;
	/// <summary>行動対象となるGameObjectへの非所有参照です。</summary>
	GameObject* target_ = nullptr;
	float currentHealth_ = 10.0f;
	float shootTimer_ = 0.0f;
	float stateTimer_ = 0.0f;
	Vector3 dashDirection_{0.0f, 0.0f, 1.0f};
	ChargeState chargeState_ = ChargeState::Approach;
	NightSlashState nightSlashState_ = NightSlashState::Approach;
	/// <summary>
	/// 夜叉ボスの状態固有処理です。Stateは共有可能なステートレスオブジェクトなので、
	/// ボスごとのヒープ確保を行わず非所有ポインターで参照します。
	/// </summary>
	const INightSlashState* nightSlashStateObject_ = nullptr;
	NightSlashState previousNightSlashMotionState_ = NightSlashState::Approach;
	TornadoBossState tornadoBossState_ = TornadoBossState::Approach;
	Vector3 nightSlashBaseScale_{1.0f, 1.0f, 1.0f};
	float nightSlashMotionTime_ = 0.0f;
	bool nightSlashMotionInitialized_ = false;
	/// <summary>0=拡散、1=包囲収束、2=巨大追尾を表す次回攻撃番号です。</summary>
	int tornadoPatternIndex_ = 0;
	/// <summary>現在実行中の連続斬り番号です。0始まりで最終斬り判定にも使用します。</summary>
	int comboDashIndex_ = 0;
	/// <summary>0=連続斬り、1=全方位弾幕、2=扇状弾幕を表す次回攻撃番号です。</summary>
	int bossPatternIndex_ = 0;
	/// <summary>現在発射済みの射撃ウェーブ番号です。</summary>
	int rangedWaveIndex_ = 0;
	bool selfDestructArmed_ = false;
	bool selfDestructRequested_ = false;
	/// <summary>シーン側で敵弾へ変換される保留中の射撃要求です。</summary>
	std::vector<EnemyShotRequest> pendingShotRequests_;
	bool runtimeSpawned_ = false;
	bool hasAppliedStats_ = false;
};
