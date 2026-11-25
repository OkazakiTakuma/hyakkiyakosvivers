#include "GameProgressRepository.h"

#include "StringUtility.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <json.hpp>
#include <utility>
#include <vector>
#include <Windows.h>

namespace {
// 文字列配列だけを集合へ取り込み、不正な要素は保存データへ混入させない。
void ReadStringSet(
	const nlohmann::json& root, const char* key, std::unordered_set<std::string>& destination) {
	if (!root.contains(key) || !root.at(key).is_array()) return;
	for (const auto& value : root.at(key)) {
		if (value.is_string() && !value.get<std::string>().empty()) {
			destination.insert(value.get<std::string>());
		}
	}
}

// unordered_setの列挙順に依存せず、Git差分を安定させるため整列して返す。
std::vector<std::string> MakeSortedValues(const std::unordered_set<std::string>& values) {
	std::vector<std::string> sorted(values.begin(), values.end());
	std::sort(sorted.begin(), sorted.end());
	return sorted;
}
}

namespace GameProgressRepository {

GameProgressLoadStatus Load(GameProgressSaveData& data) {
	std::ifstream input(StringUtility::Utf8ToPath(kFilePath));
	if (!input) return GameProgressLoadStatus::NotFound;

	try {
		nlohmann::json root;
		input >> root;
		if (!root.is_object()) return GameProgressLoadStatus::Invalid;

		GameProgressSaveData loaded;
		loaded.money = (std::max)(0, root.value("money", 0));
		if (root.contains("shopUpgradeLevels") && root.at("shopUpgradeLevels").is_object()) {
			for (auto it = root.at("shopUpgradeLevels").begin(); it != root.at("shopUpgradeLevels").end(); ++it) {
				loaded.shopUpgradeLevels[it.key()] = (std::max)(0, it.value().get<int>());
			}
		}
		ReadStringSet(root, "unlockedGameplayStages", loaded.unlockedGameplayStageIds);
		ReadStringSet(root, "clearedGameplayStages", loaded.clearedGameplayStageIds);
		ReadStringSet(root, "unlockedPlayerTypes", loaded.unlockedPlayerTypeNames);
		data = std::move(loaded);
		return GameProgressLoadStatus::Loaded;
	} catch (...) {
		return GameProgressLoadStatus::Invalid;
	}
}

bool Save(const GameProgressSaveData& data) {
	nlohmann::json root;
	root["money"] = data.money;
	root["shopUpgradeLevels"] = data.shopUpgradeLevels;
	root["unlockedGameplayStages"] = MakeSortedValues(data.unlockedGameplayStageIds);
	root["clearedGameplayStages"] = MakeSortedValues(data.clearedGameplayStageIds);
	root["unlockedPlayerTypes"] = MakeSortedValues(data.unlockedPlayerTypeNames);

	const std::filesystem::path destinationPath = StringUtility::Utf8ToPath(kFilePath);
	const std::filesystem::path temporaryPath = destinationPath.wstring() + L".tmp";
	std::filesystem::create_directories(destinationPath.parent_path());
	{
		std::ofstream output(temporaryPath, std::ios::trunc);
		if (!output) return false;
		output << std::setw(4) << root << std::endl;
		if (!output.good()) {
			output.close();
			std::error_code cleanupError;
			std::filesystem::remove(temporaryPath, cleanupError);
			return false;
		}
	}

	// 完成した一時ファイルだけを置換し、書込み途中のJSONが正規ファイルとして残らないようにする。
	if (!MoveFileExW(
			temporaryPath.c_str(), destinationPath.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		std::error_code cleanupError;
		std::filesystem::remove(temporaryPath, cleanupError);
		return false;
	}
	return true;
}

} // namespace GameProgressRepository
