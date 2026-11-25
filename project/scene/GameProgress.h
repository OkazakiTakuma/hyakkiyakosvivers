#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

enum class ShopPurchaseResult {
	Purchased,
	InvalidRequest,
	MaxLevel,
	InsufficientFunds,
	SaveFailed
};

/// <summary>シーン遷移から独立して、所持金と永続強化の実行時状態・ルールを管理します。</summary>
class GameProgress {
public:
	int GetMoney() const { return money_; }
	const std::unordered_map<std::string, int>& GetShopUpgradeLevels() const { return shopUpgradeLevels_; }
	const std::unordered_set<std::string>& GetUnlockedGameplayStageIds() const { return unlockedGameplayStageIds_; }
	const std::unordered_set<std::string>& GetClearedGameplayStageIds() const { return clearedGameplayStageIds_; }
	const std::unordered_set<std::string>& GetUnlockedPlayerTypeNames() const { return unlockedPlayerTypeNames_; }

	/// <summary>保存データから復元した経済状態を、検証済みの値として取り込みます。</summary>
	void RestoreEconomy(int money, const std::unordered_map<std::string, int>& shopUpgradeLevels);
	/// <summary>所持金と全強化レベルを新規ゲームの状態へ戻します。</summary>
	void ResetEconomy();
	/// <summary>保存済みの開放状態を復元し、必須の初期ステージと初期キャラクターを保証します。</summary>
	void RestoreUnlocks(
		const std::unordered_set<std::string>& unlockedStageIds,
		const std::unordered_set<std::string>& clearedStageIds,
		const std::unordered_set<std::string>& unlockedPlayerTypeNames,
		const std::unordered_map<std::string, std::string>& playerUnlockPrerequisites);
	/// <summary>ステージ・キャラクターの進行状態を新規ゲームへ戻します。</summary>
	void ResetUnlocks();
	/// <summary>正の金額だけを加算し、変更できた場合にtrueを返します。</summary>
	bool AddMoney(int amount);
	/// <summary>非負の金額を残高内で消費し、成功した場合にtrueを返します。</summary>
	bool SpendMoney(int amount);
	/// <summary>残高と全体強化レベルを一つの実行時状態として同時に更新します。</summary>
	ShopPurchaseResult PurchaseGlobalUpgrade(const std::string& upgradeId, int price, int maxLevel);

	int GetPlayerUpgradeLevel(const std::string& playerTypeName, const std::string& upgradeId) const;
	void SetPlayerUpgradeLevel(const std::string& playerTypeName, const std::string& upgradeId, int level);
	int GetGlobalUpgradeLevel(const std::string& upgradeId) const;
	void SetGlobalUpgradeLevel(const std::string& upgradeId, int level);
	float GetGlobalExperienceBonusPercent() const;
	int ApplyGlobalGoldBonus(int baseAmount) const;
	bool IsGameplayStageUnlocked(const std::string& stageId, const std::string& prerequisiteStageId) const;
	bool IsPlayerTypeUnlocked(const std::string& playerTypeName, const std::string& prerequisiteStageId) const;
	/// <summary>クリア履歴を記録し、条件が一致するステージとキャラクターを開放します。</summary>
	bool RecordGameplayStageClear(
		const std::string& stageId,
		const std::unordered_map<std::string, std::string>& stageUnlockPrerequisites,
		const std::unordered_map<std::string, std::string>& playerUnlockPrerequisites);

private:
	/// <summary>プレイヤー別強化を永続化する複合キーを一箇所で生成します。</summary>
	static std::string MakePlayerUpgradeKey(const std::string& playerTypeName, const std::string& upgradeId);
	/// <summary>全キャラクター共通強化を識別する保存キーを生成します。</summary>
	static std::string MakeGlobalUpgradeKey(const std::string& upgradeId);

	int money_ = 0;
	std::unordered_map<std::string, int> shopUpgradeLevels_;
	std::unordered_set<std::string> unlockedGameplayStageIds_ = {"default"};
	std::unordered_set<std::string> clearedGameplayStageIds_;
	std::unordered_set<std::string> unlockedPlayerTypeNames_ = {"巫女"};
};
