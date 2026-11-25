#include "GameProgress.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
/// <summary>全体強化1レベルあたりの経験値・獲得G上昇率です。</summary>
constexpr float kGlobalGainBonusPercentPerLevel = 10.0f;
constexpr std::array<const char*, 5> kSharedStatUpgradeIds = {{
	"health", "attack", "attack_speed", "defense", "speed",
}};
}

void GameProgress::RestoreEconomy(
	int money, const std::unordered_map<std::string, int>& shopUpgradeLevels) {
	// 手編集された保存値が実行時ルールを破らないよう、復元時にも0以上へ補正する。
	money_ = (std::max)(0, money);
	shopUpgradeLevels_.clear();
	for (const auto& [key, level] : shopUpgradeLevels) {
		shopUpgradeLevels_[key] = (std::max)(0, level);
	}
	// 旧セーブのキャラクター別レベルは、各項目の最大値を全キャラクター共通レベルとして引き継ぐ。
	for (const char* upgradeId : kSharedStatUpgradeIds) {
		int sharedLevel = GetGlobalUpgradeLevel(upgradeId);
		const std::string suffix = std::string(":") + upgradeId;
		for (const auto& [key, level] : shopUpgradeLevels) {
			if (key.size() >= suffix.size() && key.ends_with(suffix)) {
				sharedLevel = (std::max)(sharedLevel, level);
			}
		}
		SetGlobalUpgradeLevel(upgradeId, sharedLevel);
	}
}

void GameProgress::ResetEconomy() {
	money_ = 0;
	shopUpgradeLevels_.clear();
}

void GameProgress::RestoreUnlocks(
	const std::unordered_set<std::string>& unlockedStageIds,
	const std::unordered_set<std::string>& clearedStageIds,
	const std::unordered_set<std::string>& unlockedPlayerTypeNames,
	const std::unordered_map<std::string, std::string>& playerUnlockPrerequisites) {
	unlockedGameplayStageIds_ = unlockedStageIds;
	clearedGameplayStageIds_ = clearedStageIds;
	unlockedPlayerTypeNames_ = unlockedPlayerTypeNames;
	// 旧形式セーブでも開始対象を失わず、クリア履歴から追加キャラクターを再構築する。
	unlockedGameplayStageIds_.insert("default");
	unlockedPlayerTypeNames_.insert("巫女");
	for (const auto& [playerTypeName, prerequisiteStageId] : playerUnlockPrerequisites) {
		if (clearedGameplayStageIds_.contains(prerequisiteStageId)) {
			unlockedPlayerTypeNames_.insert(playerTypeName);
		}
	}
}

void GameProgress::ResetUnlocks() {
	unlockedGameplayStageIds_ = {"default"};
	clearedGameplayStageIds_.clear();
	unlockedPlayerTypeNames_ = {"巫女"};
}

bool GameProgress::AddMoney(int amount) {
	if (amount <= 0) return false;
	money_ += amount;
	return true;
}

bool GameProgress::SpendMoney(int amount) {
	if (amount < 0 || money_ < amount) return false;
	money_ -= amount;
	return true;
}

ShopPurchaseResult GameProgress::PurchaseGlobalUpgrade(
	const std::string& upgradeId, int price, int maxLevel) {
	if (upgradeId.empty() || price < 0 || maxLevel <= 0) return ShopPurchaseResult::InvalidRequest;
	const int currentLevel = GetGlobalUpgradeLevel(upgradeId);
	if (currentLevel >= maxLevel) return ShopPurchaseResult::MaxLevel;
	if (money_ < price) return ShopPurchaseResult::InsufficientFunds;

	// 購入単位では残高とレベルを必ず同時に変更し、途中状態を外部へ公開しない。
	money_ -= price;
	SetGlobalUpgradeLevel(upgradeId, currentLevel + 1);
	return ShopPurchaseResult::Purchased;
}

std::string GameProgress::MakePlayerUpgradeKey(
	const std::string& playerTypeName, const std::string& upgradeId) {
	return playerTypeName + ":" + upgradeId;
}

std::string GameProgress::MakeGlobalUpgradeKey(const std::string& upgradeId) {
	return "global:" + upgradeId;
}

int GameProgress::GetPlayerUpgradeLevel(
	const std::string& playerTypeName, const std::string& upgradeId) const {
	const auto found = shopUpgradeLevels_.find(MakePlayerUpgradeKey(playerTypeName, upgradeId));
	return found == shopUpgradeLevels_.end() ? 0 : found->second;
}

void GameProgress::SetPlayerUpgradeLevel(
	const std::string& playerTypeName, const std::string& upgradeId, int level) {
	shopUpgradeLevels_[MakePlayerUpgradeKey(playerTypeName, upgradeId)] = (std::max)(0, level);
}

int GameProgress::GetGlobalUpgradeLevel(const std::string& upgradeId) const {
	const auto found = shopUpgradeLevels_.find(MakeGlobalUpgradeKey(upgradeId));
	return found == shopUpgradeLevels_.end() ? 0 : found->second;
}

void GameProgress::SetGlobalUpgradeLevel(const std::string& upgradeId, int level) {
	shopUpgradeLevels_[MakeGlobalUpgradeKey(upgradeId)] = (std::max)(0, level);
}

float GameProgress::GetGlobalExperienceBonusPercent() const {
	return static_cast<float>(GetGlobalUpgradeLevel("experience_gain")) * kGlobalGainBonusPercentPerLevel;
}

int GameProgress::ApplyGlobalGoldBonus(int baseAmount) const {
	if (baseAmount <= 0) return 0;
	const double bonusRate = 1.0 +
		static_cast<double>(GetGlobalUpgradeLevel("gold_gain")) *
		static_cast<double>(kGlobalGainBonusPercentPerLevel) / 100.0;
	const double boostedAmount = std::ceil(static_cast<double>(baseAmount) * bonusRate);
	// 不正な巨大値でもintを越えないよう、表現可能な最大値へ丸める。
	const double maxAmount = static_cast<double>((std::numeric_limits<int>::max)());
	return boostedAmount >= maxAmount ? (std::numeric_limits<int>::max)() : static_cast<int>(boostedAmount);
}

bool GameProgress::IsGameplayStageUnlocked(
	const std::string& stageId, const std::string& prerequisiteStageId) const {
	return prerequisiteStageId.empty() || unlockedGameplayStageIds_.contains(stageId) ||
		clearedGameplayStageIds_.contains(prerequisiteStageId);
}

bool GameProgress::IsPlayerTypeUnlocked(
	const std::string& playerTypeName, const std::string& prerequisiteStageId) const {
	return unlockedPlayerTypeNames_.contains(playerTypeName) ||
		(!prerequisiteStageId.empty() && clearedGameplayStageIds_.contains(prerequisiteStageId));
}

bool GameProgress::RecordGameplayStageClear(
	const std::string& stageId,
	const std::unordered_map<std::string, std::string>& stageUnlockPrerequisites,
	const std::unordered_map<std::string, std::string>& playerUnlockPrerequisites) {
	bool changed = clearedGameplayStageIds_.insert(stageId).second;
	for (const auto& [lockedStageId, prerequisiteStageId] : stageUnlockPrerequisites) {
		if (prerequisiteStageId == stageId) {
			changed |= unlockedGameplayStageIds_.insert(lockedStageId).second;
		}
	}
	for (const auto& [playerTypeName, prerequisiteStageId] : playerUnlockPrerequisites) {
		if (prerequisiteStageId == stageId) {
			changed |= unlockedPlayerTypeNames_.insert(playerTypeName).second;
		}
	}
	return changed;
}
