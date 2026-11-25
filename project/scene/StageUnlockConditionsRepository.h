#pragma once

#include <string>
#include <unordered_map>

/// <summary>ステージ開放条件ファイルの読込結果です。</summary>
enum class StageUnlockConditionsLoadStatus {
	Loaded,
	NotFound,
	Invalid
};

namespace StageUnlockConditionsRepository {

inline constexpr const char* kFilePath = "Resources/Data/stage_unlock_conditions.json";

StageUnlockConditionsLoadStatus Load(std::unordered_map<std::string, std::string>& conditions);
bool Save(const std::unordered_map<std::string, std::string>& conditions);

} // namespace StageUnlockConditionsRepository
