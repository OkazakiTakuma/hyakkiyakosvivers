#pragma once

#include <string>
#include <unordered_map>

/// <summary>ステージとキャラクターの開放条件を所有し、条件表の不変条件を守ります。</summary>
class StageUnlockCatalog {
public:
	std::string GetStagePrerequisite(const std::string& stageId) const;
	std::string GetPlayerPrerequisite(const std::string& playerTypeName) const;
	const std::unordered_map<std::string, std::string>& GetStagePrerequisites() const {
		return stagePrerequisites_;
	}
	const std::unordered_map<std::string, std::string>& GetPlayerPrerequisites() const {
		return playerPrerequisites_;
	}

	/// <summary>条件表全体が循環しない場合だけ置き換え、成功時にtrueを返します。</summary>
	bool ReplaceStagePrerequisites(const std::unordered_map<std::string, std::string>& prerequisites);
	void RemoveStagePrerequisite(const std::string& stageId);
	/// <summary>循環しない場合だけ条件を設定し、成功時にtrueを返します。</summary>
	bool SetStagePrerequisite(const std::string& stageId, const std::string& prerequisiteStageId);
	bool WouldCreateCycle(const std::string& stageId, const std::string& prerequisiteStageId) const;

private:
	std::unordered_map<std::string, std::string> playerPrerequisites_ = {
		{"猫", "default"},
		{"烏天狗", "stage2"}
	};
	std::unordered_map<std::string, std::string> stagePrerequisites_ = {{"stage2", "default"}};
};
