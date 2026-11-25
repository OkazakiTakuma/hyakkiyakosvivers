#pragma once
#include "core/EntityComponent.h"
#include "data/EnemyStatsData.h"
#include "MathConstants.h"
#include "core/Entity.h"
#include "LineDrawer.h"
#include "components/EnemyAttackEntity/EnemyAttackEntityComponent.h"
#include "GameTime.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

/// <summary>シーンへ引き渡す敵弾生成パラメーターです。</summary>
struct EnemyShotRequest {
	Vector3 position{};
	Vector3 direction{0.0f, 0.0f, 1.0f};
	float speed = 0.12f;
	float attack = 1.0f;
	float size = 0.22f;
	float lifeTime = 6.0f;
	/// <summary>直進以外の特殊軌道をシーン側へ引き渡す設定です。</summary>
	EnemyAttackEntityMotionType motionType = EnemyAttackEntityMotionType::Linear;
	Vector3 orbitCenter{};
	float orbitAngle = 0.0f;
	float orbitInitialRadius = 0.0f;
	float orbitAngularSpeed = 0.0f;
	float orbitRadialSpeed = 0.0f;
	float orbitHeight = 0.0f;
	/// <summary>追尾弾が参照する対象です。要求および生成弾は所有権を持ちません。</summary>
	Entity* homingTarget = nullptr;
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

/// <summary>夜叉ボスの接近、近接連撃、射撃、硬直を表す状態です。</summary>
enum class NightSlashState {
	Approach,
	Windup,
	Dashing,
	Slashing,
	RangedWindup,
	RangedFiring,
	Recovering
};

class INightSlashState;

/// <summary>敵の追跡、射撃、突進ステートと体力を管理します。</summary>
class EnemyComponent : public EntityComponent {
public:
	void Initialize() override;

	void Update() override;

	void Draw3D() override;

	void ApplyStats(const EnemyStats& stats);

	const EnemyStats& GetStats() const;
	void SetEnemyTypeName(const std::string& enemyTypeName);
	const std::string& GetEnemyTypeName() const;
	void SetTarget(Entity* target);
	Entity* GetTarget() const;
	void SetTargetName(const std::string& targetName);
	const std::string& GetTargetName() const;
	void SetCurrentHealth(float health);
	float GetCurrentHealth() const;
	/// <summary>ダメージを与え、短時間の被弾カラー演出を開始します。</summary>
	void TakeDamage(float damage);
	/// <summary>武器段階から自身の耐性を引いた実効段階に応じ、指定方向へ押し出します。</summary>
	void ApplyKnockback(const Vector3& direction, int level);
	bool IsDamageFlashActive() const;
	/// <summary>HPが0になってから死亡演出を再生しているかを返します。</summary>
	bool IsDying() const;
	/// <summary>死亡演出が終了し、シーンから削除できる状態かを返します。</summary>
	bool IsDeathEffectFinished() const;
	/// <summary>撃破時に専用の倒れ込み演出を使うボスかを返します。</summary>
	bool IsBoss() const;
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
	bool IsBarrierWarningActive() const;
	float GetBarrierWarningProgress() const;
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
	/// <summary>接近、溜め、方向固定済みの突進、攻撃後硬直から成る突進敵の状態です。</summary>
	enum class ChargeState { Approach, Charging, Dashing, Recovering };
	enum class TornadoBossState { Approach, Windup, Recovering };
	enum class BarrierBossState { Approach, Windup, Active, Recovering };

	void UpdateShooter(EulerTransform& transform, const Vector3& direction, float distance);

	void UpdateCharger(EulerTransform& transform, const Vector3& direction, float distance);

	/// <summary>適正距離を保ちながら、プレイヤー方向を中心とした扇状弾を一斉発射します。</summary>
	void UpdateBurstShooter(EulerTransform& transform, const Vector3& direction, float distance);

	void UpdateSelfDestruct(EulerTransform& transform, const Vector3& direction, float distance);

	void UpdateTornadoBoss(EulerTransform& transform, const Vector3& direction, float distance);

	void UpdateBarrierBoss(EulerTransform& transform, const Vector3& direction, float distance);

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

	/// <summary>死亡時の初期状態を記録し、撃破パーティクルを一度だけ発生させます。</summary>
	void BeginDeathEffect(EulerTransform& transform);
	/// <summary>敵を回転・縮小・沈下させる死亡モーションを更新します。</summary>
	void UpdateDeathEffect(EulerTransform& transform);
	float GetDeathEffectDuration() const;
	/// <summary>最終ボス用の時間停止・スロー・ブラー演出を使うかを返します。</summary>
	bool UsesDefeatCinematic() const;
	/// <summary>開始位置から目標位置までノックバックを線形補間します。</summary>
	void UpdateKnockback(EulerTransform& transform, float deltaTime);

	std::string enemyTypeName_ = "Default";
	std::string targetName_;
	EnemyStats stats_;
	/// <summary>行動対象となるGameObjectへの非所有参照です。</summary>
	Entity* target_ = nullptr;
	float currentHealth_ = 10.0f;
	/// <summary>被弾時にモデルを赤く表示する残り時間（秒）です。</summary>
	float damageFlashTimer_ = 0.0f;
	static constexpr float kDamageFlashDuration = 0.12f;
	static constexpr float kDeathEffectDuration = 0.62f;
	// 両ボスのbin内撃破クリップ（約1.5秒）を最後まで再生してから削除します。
	static constexpr float kBossDeathEffectDuration = 1.52f;
	float deathEffectTimer_ = 0.0f;
	bool deathEffectStarted_ = false;
	/// <summary>モデルのbinに含まれる専用撃破クリップを再生できたかを表します。</summary>
	bool usesAuthoredDeathAnimation_ = false;
	/// <summary>時間倍率に影響されず進む、ボス撃破時のヒットストップ演出時間です。</summary>
	float bossDefeatPresentationTimer_ = 0.0f;
	Vector3 deathStartPosition_{};
	Vector3 deathStartRotation_{};
	Vector3 deathStartScale_{1.0f, 1.0f, 1.0f};
	/// <summary>ノックバック補間の開始位置と到達位置です。</summary>
	Vector3 knockbackStartPosition_{};
	Vector3 knockbackTargetPosition_{};
	float knockbackElapsedSeconds_ = 0.0f;
	static constexpr float kKnockbackDuration = 0.16f;
	bool knockbackActive_ = false;
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
	BarrierBossState barrierBossState_ = BarrierBossState::Approach;
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

