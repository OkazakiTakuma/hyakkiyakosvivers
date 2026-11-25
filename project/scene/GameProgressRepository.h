#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

/// <summary>ゲーム進行JSONへ保存する値だけをまとめた、シーン非依存のデータです。</summary>
struct GameProgressSaveData {
	int money = 0;
	std::unordered_map<std::string, int> shopUpgradeLevels;
	std::unordered_set<std::string> unlockedGameplayStageIds = {"default"};
	std::unordered_set<std::string> clearedGameplayStageIds;
	std::unordered_set<std::string> unlockedPlayerTypeNames = {"巫女"};
};

/// <summary>ファイル不在と破損を区別し、呼び出し側がフォールバック方法を選べるようにします。</summary>
enum class GameProgressLoadStatus {
	Loaded,
	NotFound,
	Invalid
};

namespace GameProgressRepository {

inline constexpr const char* kFilePath = "Resources/Data/game_progress.json";

GameProgressLoadStatus Load(GameProgressSaveData& data);
bool Save(const GameProgressSaveData& data);

} // namespace GameProgressRepository
