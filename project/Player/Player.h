#pragma once
#include "core/Component.h"
#include "Vector.h"
#include <array>
#include <cmath>
#include <memory>
#include <string>

class Object3d;

/// <summary>プレイヤーが装備している攻撃スロット1枠分の設定です。</summary>
struct PlayerAttackSlot {
	/// <summary>このスロットの攻撃を実行するかどうかです。</summary>
	bool enabled = false;
	/// <summary>player_attack_status.json に保存されている攻撃タイプ名です。</summary>
	std::string attackName;
	/// <summary>攻撃タイプ内で使用するレベルです。1～5 または super を指定します。</summary>
	std::string attackLevel = "1";
};

/// <summary>プレイヤーへ加算適用するステータスアップアイテムのスロット設定です。</summary>
struct PlayerStatusSlot {
	/// <summary>このスロットの補正を適用するかどうかです。</summary>
	bool enabled = false;
	/// <summary>player_status_item_status.json に保存されているステータスアップアイテム名です。</summary>
	std::string statusName;
	/// <summary>ステータスアップアイテムの効果量レベルです。1～5 を指定します。</summary>
	std::string level = "1";
};

/// <summary>プレイヤータイプごとの基礎能力、装備、保存対象の状態をまとめたデータです。</summary>
struct PlayerStats {
	/// <summary>プレイヤータイプ名です。</summary>
	std::string name = "Default";
	/// <summary>HP計算の基準値です。</summary>
	float baseHealth = 100.0f;
	/// <summary>baseHealth に掛けるHP倍率です。100で等倍です。</summary>
	float health = 100.0f;
	/// <summary>攻撃力倍率です。100で等倍です。</summary>
	float attack = 100.0f;
	/// <summary>被ダメージ軽減率です。1増えるごとに受けるダメージを1%軽減します。</summary>
	float defense = 0.0f;
	/// <summary>移動速度計算の基準値です。</summary>
	float baseSpeed = 0.1f;
	/// <summary>baseSpeed に掛ける速度倍率です。100で等倍です。</summary>
	float speed = 100.0f;
	/// <summary>攻撃間隔に掛ける攻撃速度倍率です。100で等倍です。</summary>
	float attackSpeed = 100.0f;
	/// <summary>攻撃弾サイズに掛ける倍率です。100で等倍です。</summary>
	float attackSize = 100.0f;
	/// <summary>プレイヤーモデルとコライダーに掛ける一様なサイズ倍率です。</summary>
	float sizeScale = 1.0f;
	/// <summary>被ダメージ後に再度ダメージを受けない時間です。</summary>
	float damageInvincibilityDuration = 1.0f;
	/// <summary>現在レベルです。</summary>
	int level = 1;
	/// <summary>現在経験値です。</summary>
	int experience = 0;
	/// <summary>獲得経験値へ掛ける倍率です。100で等倍です。</summary>
	float experienceCorrection = 100.0f;
	/// <summary>プレイヤー表示に使用するモデルファイルパスです。</summary>
	std::string modelFilePath;
	/// <summary>モデルをアニメーションモデルとして扱うかどうかです。</summary>
	bool isAnimationModel = false;
	/// <summary>プレイヤーが同時に装備できる攻撃スロットです。</summary>
	std::array<PlayerAttackSlot, 5> attackSlots{};
	/// <summary>一時的または恒久的に能力へ加算するステータスアップアイテムスロットです。</summary>
	std::array<PlayerStatusSlot, 5> statusSlots{};
};

class Player : public Component {
public:
	~Player() override;

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
	float GetMaxHealth() const { return effectiveStats_.baseHealth * (effectiveStats_.health / 100.0f); }
	float GetEffectiveMoveSpeed() const { return effectiveStats_.baseSpeed * (effectiveStats_.speed / 100.0f); }
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
	/// 烏天狗用の刀モデルを必要になった時点で一度だけ生成します。
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
