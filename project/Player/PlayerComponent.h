#pragma once
#include "core/EntityComponent.h"
#include "data/PlayerStatsData.h"
#include "Vector.h"
#include <cmath>
#include <memory>
#include <string>

class Object3d;

class PlayerComponent : public EntityComponent {
public:
	~PlayerComponent() override;

	/// <summary>
	/// 毎フレーム WASD 入力を見て、XZ 平面上でプレイヤーを移動します。
	/// </summary>
	void Update() override;
	/// <summary>
	/// プレイヤー固有の3D装備を描画します。
	/// </summary>
	void Draw3D() override;
	/// <summary>
	/// プレイヤーが所有する装備リソースを解放します。
	/// </summary>
	void Finalize() override;

	/// <summary>
	/// 現在位置をスポーンポイントへ戻します。
	/// </summary>
	void ResetToSpawnPoint();
	/// <summary>基礎能力と実効能力を同じ値として適用します。</summary>
	void ApplyStats(const PlayerStats& stats);
	/// <summary>JSON保存用の基礎能力と、アイテム反映後の実効能力を分けて適用します。</summary>
	void ApplyStats(const PlayerStats& baseStats, const PlayerStats& effectiveStats);
	/// <summary>防御力と無敵時間を考慮してダメージを適用し、実際に減った整数ダメージを返します。</summary>
	int TakeDamage(float rawDamage);
	void SetDemoMove(const Vector3& velocity) { demoControlled_ = true; demoMoveVelocity_ = velocity; }
	float GetMaxHealth() const { return effectiveStats_.baseHealth * (effectiveStats_.health / 100.0f); }
	/// <summary>キャラクター特性を含めた速度倍率です。100を等倍として扱います。</summary>
	float GetEffectiveSpeedPercent() const;
	float GetEffectiveMoveSpeed() const { return effectiveStats_.baseSpeed * (GetEffectiveSpeedPercent() / 100.0f); }
	/// <summary>攻撃対象までの距離に応じたキャラクター固有の与ダメージ倍率を返します。</summary>
	float GetOutgoingDamageMultiplier(float targetDistance) const;
	const PlayerStats& GetStats() const { return effectiveStats_; }
	const PlayerStats& GetBaseStats() const { return stats_; }
	/// <summary>経験値補正を掛けた経験値を加算し、必要ならレベルアップ待ち回数を増やします。</summary>
	void AddExperience(int experience);
	/// <summary>ショップの全体強化による獲得経験値上昇率を設定します。</summary>
	void SetGlobalExperienceBonusPercent(float bonusPercent) { globalExperienceBonusPercent_ = bonusPercent < 0.0f ? 0.0f : bonusPercent; }
	bool ConsumePendingLevelUp() {
		if (pendingLevelUpCount_ <= 0) {
			return false;
		}
		--pendingLevelUpCount_;
		return true;
	}
	int GetPendingLevelUpCount() const { return pendingLevelUpCount_; }
	/// <summary>指定レベルから次レベルへ上がるために必要な累積経験値を返します。</summary>
	static int GetRequiredExperienceForNextLevel(int level);
	void SetExperience(int experience) {
		const int clampedExperience = experience < 0 ? 0 : experience;
		stats_.experience = clampedExperience;
		effectiveStats_.experience = clampedExperience;
	}
	void SetLevel(int level) {
		const int clampedLevel = level < 1 ? 1 : level;
		stats_.level = clampedLevel;
		effectiveStats_.level = clampedLevel;
	}
	void SetPlayerTypeName(const std::string& playerTypeName) { playerTypeName_ = playerTypeName.empty() ? "Default" : playerTypeName; }
	const std::string& GetPlayerTypeName() const { return playerTypeName_; }
	void SetCurrentHealth(float currentHealth) {
		const float maxHealth = GetMaxHealth();
		currentHealth_ = currentHealth < 0.0f ? 0.0f : (currentHealth > maxHealth ? maxHealth : currentHealth);
	}
	float GetCurrentHealth() const { return currentHealth_; }

	/// <summary>
	/// スポーンポイントを設定します。
	/// </summary>
	/// <param name="spawnPoint">リロード時やリセット時に戻す位置を指定します。</param>
	void SetSpawnPoint(const Vector3& spawnPoint) { spawnPoint_ = spawnPoint; }
	const Vector3& GetSpawnPoint() const { return spawnPoint_; }

	/// <summary>
	/// 1 フレームあたりの移動速度を設定します。
	/// </summary>
	/// <param name="moveSpeed">WASD 入力時に加算する移動量を指定します。</param>
	void SetMoveSpeed(float moveSpeed) { moveSpeed_ = moveSpeed < 0.0f ? 0.0f : moveSpeed; }
	float GetMoveSpeed() const { return moveSpeed_; }

	/// <summary>
	/// プレイヤー表示に使用するモデル情報を設定します。
	/// </summary>
	/// <param name="modelFilePath">ModelManager に読み込まれているモデル名を指定します。</param>
	/// <param name="isAnimationModel">アニメーションモデルの場合 true を指定します。</param>
	void SetModelFilePath(const std::string& modelFilePath, bool isAnimationModel) {
		modelFilePath_ = modelFilePath;
		isAnimationModel_ = isAnimationModel;
	}
	const std::string& GetModelFilePath() const { return modelFilePath_; }
	bool GetIsAnimationModel() const { return isAnimationModel_; }

private:
	/// <summary>
	/// プレイヤー定義の右手装備モデルを必要になった時点で一度だけ生成します。
	/// </summary>
	void EnsureBladeEquipment();
	/// <summary>現在経験値からレベルを再計算し、上昇回数をレベルアップ処理用に蓄積します。</summary>
	void UpdateLevelFromExperience();
	/// <summary>一定時間ごとにHPを少量回復します。</summary>
	void UpdateHealthRegeneration(float deltaTime);
	Vector3 spawnPoint_{0.0f, 0.0f, 0.0f};
	/// <summary>移動開始・停止を滑らかにするための現在速度です。</summary>
	Vector3 currentMoveVelocity_{0.0f, 0.0f, 0.0f};
	float moveSpeed_ = 0.1f;
	/// <summary>現在HPです。最大HPは effectiveStats_ から計算します。</summary>
	float currentHealth_ = 100.0f;
	bool demoControlled_ = false;
	Vector3 demoMoveVelocity_{};
	/// <summary>生成直後の初回適用だけ、購入強化反映後の最大HPで開始するための判定です。</summary>
	bool hasAppliedStats_ = false;
	/// <summary>被ダメージ後の残り無敵時間です。</summary>
	float damageInvincibilityTimer_ = 0.0f;
	/// <summary>ショップの全体強化による獲得経験値の加算率です。</summary>
	float globalExperienceBonusPercent_ = 0.0f;
	/// <summary>自動回復の経過時間です。</summary>
	float healthRegenerationTimer_ = 0.0f;
	/// <summary>保存対象となる基礎ステータスです。</summary>
	PlayerStats stats_;
	/// <summary>ステータスアップアイテム反映後にゲーム中で参照するステータスです。</summary>
	PlayerStats effectiveStats_;
	std::string playerTypeName_ = "Default";
	std::string modelFilePath_;
	bool isAnimationModel_ = false;
	int pendingLevelUpCount_ = 0;
	// シーンオブジェクトとは別にPlayerが所有し、右手ボーンへ追従させる刀の描画オブジェクト。
	std::unique_ptr<Object3d> bladeObject_;
};
