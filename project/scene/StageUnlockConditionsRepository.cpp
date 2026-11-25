#include "StageUnlockConditionsRepository.h"

#include "StringUtility.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <json.hpp>
#include <utility>
#include <vector>

namespace {
// ファイル名にも使用するIDへ危険な記号が混ざらないよう、許可文字だけを受け付ける。
bool IsValidStageId(const std::string& stageId) {
	if (stageId.empty()) return false;
	return std::all_of(stageId.begin(), stageId.end(), [](unsigned char character) {
		return std::isalnum(character) || character == '-' || character == '_';
	});
}
}

namespace StageUnlockConditionsRepository {

StageUnlockConditionsLoadStatus Load(std::unordered_map<std::string, std::string>& conditions) {
	std::ifstream input(StringUtility::Utf8ToPath(kFilePath));
	if (!input) return StageUnlockConditionsLoadStatus::NotFound;

	try {
		nlohmann::json root;
		input >> root;
		if (!root.is_object() || !root.contains("stageUnlockPrerequisites") ||
			!root.at("stageUnlockPrerequisites").is_object()) {
			return StageUnlockConditionsLoadStatus::Invalid;
		}

		std::unordered_map<std::string, std::string> loaded;
		for (auto it = root.at("stageUnlockPrerequisites").begin();
			it != root.at("stageUnlockPrerequisites").end(); ++it) {
			if (!it.value().is_string()) continue;
			const std::string prerequisite = it.value().get<std::string>();
			// 自分自身を前提にする成立不能な条件と、不正なIDは取り込まない。
			if (IsValidStageId(it.key()) && IsValidStageId(prerequisite) && prerequisite != it.key()) {
				loaded[it.key()] = prerequisite;
			}
		}
		conditions = std::move(loaded);
		return StageUnlockConditionsLoadStatus::Loaded;
	} catch (...) {
		return StageUnlockConditionsLoadStatus::Invalid;
	}
}

bool Save(const std::unordered_map<std::string, std::string>& conditions) {
	nlohmann::json root;
	root["stageUnlockPrerequisites"] = nlohmann::json::object();
	std::vector<std::string> stageIds;
	stageIds.reserve(conditions.size());
	for (const auto& [stageId, prerequisite] : conditions) {
		stageIds.push_back(stageId);
	}
	// unordered_mapの順序差で保存ファイルが毎回変化しないよう、IDを整列する。
	std::sort(stageIds.begin(), stageIds.end());
	for (const std::string& stageId : stageIds) {
		root["stageUnlockPrerequisites"][stageId] = conditions.at(stageId);
	}

	std::filesystem::create_directories(StringUtility::Utf8ToPath(kFilePath).parent_path());
	std::ofstream output(StringUtility::Utf8ToPath(kFilePath));
	if (!output) return false;
	output << std::setw(4) << root << std::endl;
	return output.good();
}

} // namespace StageUnlockConditionsRepository
