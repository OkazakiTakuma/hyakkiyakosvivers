#pragma once

#include "components/Enemy/EnemyComponent.h"
#include "components/ItemDrop/ItemDropComponent.h"

#include <random>
#include <string>
#include <vector>

/// <summary>ボス報酬の個数、抽選テーブル、獲得Gを調整する永続設定です。</summary>
struct BossRewardSettings {
	float singleRewardWeight = 70.0f;
	float tripleRewardWeight = 25.0f;
	float quintupleRewardWeight = 5.0f;
	float ownedUpgradeTableWeight = 60.0f;
	float unownedWeaponTableWeight = 20.0f;
	float unownedItemTableWeight = 20.0f;
	int singleRewardMoney = 100;
	int tripleRewardMoney = 300;
	int quintupleRewardMoney = 500;
};

/// <summary>敵の能力値を引き継いで生成する経験値オブジェクトの要求です。</summary>
struct ExperienceDropRequest {
	EnemyStats stats;
	Vector3 position{};
	Entity* target = nullptr;
};

/// <summary>抽選に当選した回復・経験値回収・お金アイテムの生成要求です。</summary>
struct ItemDropRequest {
	ItemDropType type = ItemDropType::Health;
	Vector3 position{};
	Entity* target = nullptr;
	float healAmount = 0.0f;
	int moneyAmount = 0;
};

/// <summary>ボス撃破時に生成する強化報酬と、その適用可能回数です。</summary>
struct BossUpgradeDropRequest {
	Vector3 position{};
	Entity* target = nullptr;
	int upgradeCount = 1;
};

/// <summary>1体の敵を撃破したときに生成すべき全ドロップをまとめた抽選結果です。</summary>
struct LootDropResult {
	// 経験値は敵の種類にかかわらず必ず生成する。
	ExperienceDropRequest experience;
	// 各アイテムは独立抽選のため、同時に複数種類が含まれる場合がある。
	std::vector<ItemDropRequest> items;
	// 通常敵ではfalseとなり、bossUpgradeは使用しない。
	bool hasBossUpgrade = false;
	BossUpgradeDropRequest bossUpgrade;
};

/// <summary>撃破された敵の設定から、生成すべきドロップ要求を抽選します。</summary>
class LootDropService final {
public:
	LootDropService();
	/// <summary>撃破された敵の設定と位置から、生成要求一式を返します。</summary>
	LootDropResult Roll(const EnemyComponent& enemy, const Vector3& position);
	const BossRewardSettings& GetBossRewardSettings() const { return bossRewardSettings_; }
	void SetBossRewardSettings(const BossRewardSettings& settings);
	bool SaveBossRewardSettings() const;
	int GetBossRewardMoney(int rewardCount) const;

private:
	void LoadBossRewardSettings();
	// 毎回シードを作り直さず、サービスの生存期間を通して同じ乱数生成器を使用する。
	std::mt19937 randomEngine_;
	BossRewardSettings bossRewardSettings_;
};
