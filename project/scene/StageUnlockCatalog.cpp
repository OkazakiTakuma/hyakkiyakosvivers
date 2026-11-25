#include "StageUnlockCatalog.h"

#include <unordered_set>

std::string StageUnlockCatalog::GetStagePrerequisite(const std::string& stageId) const {
	const auto found = stagePrerequisites_.find(stageId);
	return found == stagePrerequisites_.end() ? std::string{} : found->second;
}

std::string StageUnlockCatalog::GetPlayerPrerequisite(const std::string& playerTypeName) const {
	const auto found = playerPrerequisites_.find(playerTypeName);
	return found == playerPrerequisites_.end() ? std::string{} : found->second;
}

bool StageUnlockCatalog::ReplaceStagePrerequisites(
	const std::unordered_map<std::string, std::string>& prerequisites) {
	const std::unordered_map<std::string, std::string> previous = stagePrerequisites_;
	stagePrerequisites_ = prerequisites;
	for (const auto& [stageId, prerequisiteStageId] : stagePrerequisites_) {
		if (stageId.empty() || prerequisiteStageId.empty() || stageId == prerequisiteStageId ||
			WouldCreateCycle(stageId, prerequisiteStageId)) {
			// 一部だけ採用すると条件の意味が変わるため、表全体を直前の状態へ戻す。
			stagePrerequisites_ = previous;
			return false;
		}
	}
	return true;
}

void StageUnlockCatalog::RemoveStagePrerequisite(const std::string& stageId) {
	stagePrerequisites_.erase(stageId);
}

bool StageUnlockCatalog::SetStagePrerequisite(
	const std::string& stageId, const std::string& prerequisiteStageId) {
	if (stageId.empty() || prerequisiteStageId.empty() || stageId == prerequisiteStageId ||
		WouldCreateCycle(stageId, prerequisiteStageId)) {
		return false;
	}
	stagePrerequisites_[stageId] = prerequisiteStageId;
	return true;
}

bool StageUnlockCatalog::WouldCreateCycle(
	const std::string& stageId, const std::string& prerequisiteStageId) const {
	std::unordered_set<std::string> visited;
	std::string current = prerequisiteStageId;
	// 新しい前提から既存の条件をたどり、設定対象へ戻る場合は循環と判定する。
	while (!current.empty() && visited.insert(current).second) {
		if (current == stageId) return true;
		current = GetStagePrerequisite(current);
	}
	return false;
}
