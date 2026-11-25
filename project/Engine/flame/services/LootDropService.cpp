#include "services/LootDropService.h"

#include "StringUtility.h"
#include <json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace {
// お金は敵データに確率を持たない共通ドロップとして扱う。
constexpr float kMoneyItemDropChance = 0.20f;
constexpr int kMoneyDropMinimum = 5;
constexpr int kMoneyDropMaximum = 15;

constexpr const char* kBossRewardSettingsFilePath = "Resources/Data/boss_reward_settings.json";
}

// サービス生成時に一度だけシードを設定する。
LootDropService::LootDropService()
	: randomEngine_(std::random_device{}()) {
	LoadBossRewardSettings();
}

LootDropResult LootDropService::Roll(const EnemyComponent& enemy, const Vector3& position) {
	LootDropResult result;
	const EnemyStats& stats = enemy.GetStats();
	Entity* target = enemy.GetTarget();
	result.experience = {stats, position, target};

	// 回復・全経験値回収・お金は互いに独立して抽選する。
	std::uniform_real_distribution<float> itemDropDistribution(0.0f, 1.0f);
	if (itemDropDistribution(randomEngine_) < stats.healthItemDropChance) {
		result.items.push_back({ItemDropType::Health, position, target, stats.healthItemHealAmount, 0});
	}
	if (itemDropDistribution(randomEngine_) < stats.collectExperienceItemDropChance) {
		result.items.push_back({ItemDropType::CollectAllExperience, position, target, 0.0f, 0});
	}
	if (itemDropDistribution(randomEngine_) < kMoneyItemDropChance) {
		std::uniform_int_distribution<int> moneyDistribution(kMoneyDropMinimum, kMoneyDropMaximum);
		result.items.push_back({ItemDropType::Money, position, target, 0.0f, moneyDistribution(randomEngine_)});
	}

	// 対象ボスの場合のみ、強化回数を別途抽選する。
	if (stats.dropsBossUpgrade) {
		const float singleWeight = (std::max)(0.0f, bossRewardSettings_.singleRewardWeight);
		const float tripleWeight = (std::max)(0.0f, bossRewardSettings_.tripleRewardWeight);
		const float quintupleWeight = (std::max)(0.0f, bossRewardSettings_.quintupleRewardWeight);
		const float totalWeight = singleWeight + tripleWeight + quintupleWeight;
		int rewardCount = 1;
		if (totalWeight > 0.0f) {
			std::uniform_real_distribution<float> rewardDistribution(0.0f, totalWeight);
			const float roll = rewardDistribution(randomEngine_);
			rewardCount = roll < singleWeight ? 1 : (roll < singleWeight + tripleWeight ? 3 : 5);
		}
		result.hasBossUpgrade = true;
		result.bossUpgrade = {
			position,
			target,
			rewardCount};
	}
	return result;
}

void LootDropService::SetBossRewardSettings(const BossRewardSettings& settings) {
	bossRewardSettings_ = settings;
	bossRewardSettings_.singleRewardWeight = (std::max)(0.0f, bossRewardSettings_.singleRewardWeight);
	bossRewardSettings_.tripleRewardWeight = (std::max)(0.0f, bossRewardSettings_.tripleRewardWeight);
	bossRewardSettings_.quintupleRewardWeight = (std::max)(0.0f, bossRewardSettings_.quintupleRewardWeight);
	bossRewardSettings_.ownedUpgradeTableWeight = (std::max)(0.0f, bossRewardSettings_.ownedUpgradeTableWeight);
	bossRewardSettings_.unownedWeaponTableWeight = (std::max)(0.0f, bossRewardSettings_.unownedWeaponTableWeight);
	bossRewardSettings_.unownedItemTableWeight = (std::max)(0.0f, bossRewardSettings_.unownedItemTableWeight);
	bossRewardSettings_.singleRewardMoney = (std::max)(0, bossRewardSettings_.singleRewardMoney);
	bossRewardSettings_.tripleRewardMoney = (std::max)(0, bossRewardSettings_.tripleRewardMoney);
	bossRewardSettings_.quintupleRewardMoney = (std::max)(0, bossRewardSettings_.quintupleRewardMoney);
}

void LootDropService::LoadBossRewardSettings() {
	std::ifstream input(StringUtility::Utf8ToPath(kBossRewardSettingsFilePath));
	if (!input.is_open()) return;
	try {
		nlohmann::json json;
		input >> json;
		BossRewardSettings loaded;
		loaded.singleRewardWeight = json.value("singleRewardWeight", loaded.singleRewardWeight);
		loaded.tripleRewardWeight = json.value("tripleRewardWeight", loaded.tripleRewardWeight);
		loaded.quintupleRewardWeight = json.value("quintupleRewardWeight", loaded.quintupleRewardWeight);
		loaded.ownedUpgradeTableWeight = json.value("ownedUpgradeTableWeight", loaded.ownedUpgradeTableWeight);
		loaded.unownedWeaponTableWeight = json.value("unownedWeaponTableWeight", loaded.unownedWeaponTableWeight);
		loaded.unownedItemTableWeight = json.value("unownedItemTableWeight", loaded.unownedItemTableWeight);
		loaded.singleRewardMoney = json.value("singleRewardMoney", loaded.singleRewardMoney);
		loaded.tripleRewardMoney = json.value("tripleRewardMoney", loaded.tripleRewardMoney);
		loaded.quintupleRewardMoney = json.value("quintupleRewardMoney", loaded.quintupleRewardMoney);
		SetBossRewardSettings(loaded);
	} catch (...) {
		// 壊れた設定はゲームを停止させず、既定値を使用する。
	}
}

bool LootDropService::SaveBossRewardSettings() const {
	std::filesystem::create_directories(StringUtility::Utf8ToPath(kBossRewardSettingsFilePath).parent_path());
	std::ofstream output(StringUtility::Utf8ToPath(kBossRewardSettingsFilePath));
	if (!output.is_open()) return false;
	nlohmann::json json;
	json["singleRewardWeight"] = bossRewardSettings_.singleRewardWeight;
	json["tripleRewardWeight"] = bossRewardSettings_.tripleRewardWeight;
	json["quintupleRewardWeight"] = bossRewardSettings_.quintupleRewardWeight;
	json["ownedUpgradeTableWeight"] = bossRewardSettings_.ownedUpgradeTableWeight;
	json["unownedWeaponTableWeight"] = bossRewardSettings_.unownedWeaponTableWeight;
	json["unownedItemTableWeight"] = bossRewardSettings_.unownedItemTableWeight;
	json["singleRewardMoney"] = bossRewardSettings_.singleRewardMoney;
	json["tripleRewardMoney"] = bossRewardSettings_.tripleRewardMoney;
	json["quintupleRewardMoney"] = bossRewardSettings_.quintupleRewardMoney;
	output << json.dump(4);
	return output.good();
}

int LootDropService::GetBossRewardMoney(int rewardCount) const {
	if (rewardCount >= 5) return bossRewardSettings_.quintupleRewardMoney;
	if (rewardCount >= 3) return bossRewardSettings_.tripleRewardMoney;
	return bossRewardSettings_.singleRewardMoney;
}
