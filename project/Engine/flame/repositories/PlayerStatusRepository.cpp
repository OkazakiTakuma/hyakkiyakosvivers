#include "PlayerStatusRepository.h"
#include "../helpers/SceneJsonUtility.h"
#include "../../base/StringUtility.h"
#include "../../math/MathConstants.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#pragma warning(push)
#pragma warning(disable: 26495)
#include <json.hpp>
#pragma warning(pop)
#include <iomanip>

std::vector<std::string> GetPlayerStatusItemLevels() {
	// ステータスアップアイテムは固定でLv1～Lv5を扱う。
	return {"1", "2", "3", "4", "5"};
}

const char* PlayerStatusItemTypeToName(PlayerStatusItemType type) {
	// JSONへ保存する文字列は、インスペクター表示名と同じ英語名に統一する。
	switch (type) {
	case PlayerStatusItemType::Attack:
		return "Attack";
	case PlayerStatusItemType::Health:
		return "HP";
	case PlayerStatusItemType::AttackSpeed:
		return "AttackSpeed";
	case PlayerStatusItemType::Speed:
		return "Speed";
	case PlayerStatusItemType::Defense:
		return "Defense";
	case PlayerStatusItemType::AttackSize:
		return "AttackSize";
	case PlayerStatusItemType::Experience:
		return "Experience";
	}
	return "Attack";
}

PlayerStatusItemType PlayerStatusItemTypeFromName(const std::string& typeName) {
	// 旧データ互換のため、HPとHealthは同じ体力補正として扱う。
	if (typeName == "HP" || typeName == "Health") {
		return PlayerStatusItemType::Health;
	}
	if (typeName == "AttackSpeed") {
		return PlayerStatusItemType::AttackSpeed;
	}
	if (typeName == "Speed") {
		return PlayerStatusItemType::Speed;
	}
	if (typeName == "Defense") {
		return PlayerStatusItemType::Defense;
	}
	if (typeName == "AttackSize") {
		return PlayerStatusItemType::AttackSize;
	}
	if (typeName == "Experience") {
		return PlayerStatusItemType::Experience;
	}
	return PlayerStatusItemType::Attack;
}

PlayerStatusItemStats MakeDefaultPlayerStatusItemStats() {
	return {};
}

nlohmann::json PlayerStatusItemStatsToJson(const PlayerStatusItemStats& stats) {
	nlohmann::json json;
	json["name"] = stats.name;
	json["type"] = PlayerStatusItemTypeToName(stats.type);
	// 効果量・説明・画像を同じレベル文字列で保存し、配列番号との対応を明確にする。
	for (int index = 0; index < static_cast<int>(stats.levelAmounts.size()); ++index) {
		const std::string levelName = std::to_string(index + 1);
		json["levels"][levelName] = stats.levelAmounts[index];
		json["descriptions"][levelName] = stats.levelDescriptions[index];
		json["textures"][levelName] = stats.levelTextureFilePaths[index];
	}
	return json;
}

PlayerStatusItemStats JsonToPlayerStatusItemStats(const nlohmann::json& json, const PlayerStatusItemStats& fallback) {
	PlayerStatusItemStats stats = fallback;
	if (!json.is_object()) {
		return stats;
	}
	stats.name = json.value("name", stats.name);
	stats.type = PlayerStatusItemTypeFromName(json.value("type", std::string(PlayerStatusItemTypeToName(stats.type))));
	// descriptions/texturesがない旧JSONも、空の既定値で互換性を保って読み込む。
	const nlohmann::json levelsJson = json.value("levels", nlohmann::json::object());
	const nlohmann::json descriptionsJson = json.value("descriptions", nlohmann::json::object());
	const nlohmann::json texturesJson = json.value("textures", nlohmann::json::object());
	for (int index = 0; index < static_cast<int>(stats.levelAmounts.size()); ++index) {
		const std::string levelName = std::to_string(index + 1);
		stats.levelAmounts[index] = levelsJson.value(levelName, stats.levelAmounts[index]);
		stats.levelAmounts[index] = (std::max)(0.0f, stats.levelAmounts[index]);
		stats.levelDescriptions[index] = descriptionsJson.value(levelName, stats.levelDescriptions[index]);
		stats.levelTextureFilePaths[index] = texturesJson.value(levelName, stats.levelTextureFilePaths[index]);
	}
	return stats;
}

nlohmann::json LoadPlayerStatusItemRoot() {
	// ファイルが存在しない場合でも、最低限のAttackUpを返してインスペクターを空にしない。
	std::ifstream ifs(StringUtility::Utf8ToPath(kPlayerStatusItemFilePath));
	if (!ifs) {
		nlohmann::json root;
		root["AttackUp"] = PlayerStatusItemStatsToJson(MakeDefaultPlayerStatusItemStats());
		return root;
	}

	nlohmann::json root;
	ifs >> root;
	if (!root.is_object()) {
		root = nlohmann::json::object();
	}
	if (!root.contains("AttackUp")) {
		root["AttackUp"] = PlayerStatusItemStatsToJson(MakeDefaultPlayerStatusItemStats());
	}
	return root;
}

std::vector<std::string> LoadPlayerStatusItemNames() {
	// コンボボックスで安定して表示するため、JSONキーをソートして返す。
	std::vector<std::string> names;
	const nlohmann::json root = LoadPlayerStatusItemRoot();
	for (auto it = root.begin(); it != root.end(); ++it) {
		names.push_back(it.key());
	}
	std::sort(names.begin(), names.end());
	if (names.empty()) {
		names.push_back("AttackUp");
	}
	return names;
}

PlayerStatusItemStats LoadPlayerStatusItemStats(const std::string& itemName) {
	// 未指定時はDefault相当のAttackUpを読み、存在しない名前でも安全な既定値を返す。
	const std::string name = itemName.empty() ? "AttackUp" : itemName;
	const nlohmann::json root = LoadPlayerStatusItemRoot();
	const PlayerStatusItemStats fallback = MakeDefaultPlayerStatusItemStats();
	if (!root.contains(name)) {
		return fallback;
	}
	PlayerStatusItemStats stats = JsonToPlayerStatusItemStats(root.at(name), fallback);
	stats.name = name;
	return stats;
}

void SavePlayerStatusItemStats(const std::string& itemName, const PlayerStatusItemStats& stats) {
	// 既存JSONを読み込んだうえで対象アイテムだけ更新し、他アイテムを消さない。
	const std::string name = itemName.empty() ? "AttackUp" : itemName;
	nlohmann::json root = LoadPlayerStatusItemRoot();
	PlayerStatusItemStats saveStats = stats;
	saveStats.name = name;
	root[name] = PlayerStatusItemStatsToJson(saveStats);

	std::filesystem::create_directories(StringUtility::Utf8ToPath(kPlayerStatusItemFilePath).parent_path());
	std::ofstream ofs(StringUtility::Utf8ToPath(kPlayerStatusItemFilePath));
	if (ofs) {
		ofs << std::setw(4) << root << std::endl;
	}
}

int PlayerStatusSlotLevelToIndex(const std::string& level) {
	// 文字列レベルを0始まりの配列添字へ変換し、範囲外はLv1～Lv5へ丸める。
	const int parsedLevel = (std::max)(1, std::atoi(level.c_str()));
	return (std::min)(4, parsedLevel - 1);
}

PlayerStats ApplyPlayerStatusItems(const PlayerStats& baseStats) {
	// 基礎能力へ現在保存されている各強化アイテムの効果を順番に合成する。
	PlayerStats effectiveStats = baseStats;
	for (const PlayerStatusSlot& slot : baseStats.statusSlots) {
		if (!slot.enabled || slot.statusName.empty()) {
			continue;
		}
		const PlayerStatusItemStats itemStats = LoadPlayerStatusItemStats(slot.statusName);
		const float amount = itemStats.levelAmounts[PlayerStatusSlotLevelToIndex(slot.level)];
		switch (itemStats.type) {
		case PlayerStatusItemType::Attack:
			effectiveStats.attack += amount;
			break;
		case PlayerStatusItemType::Health:
			effectiveStats.health += amount;
			break;
		case PlayerStatusItemType::AttackSpeed:
			effectiveStats.attackSpeed += amount;
			break;
		case PlayerStatusItemType::Speed:
			effectiveStats.speed += amount;
			break;
		case PlayerStatusItemType::Defense:
			effectiveStats.defense += amount;
			break;
		case PlayerStatusItemType::AttackSize:
			effectiveStats.attackSize += amount;
			break;
		case PlayerStatusItemType::Experience:
			effectiveStats.experienceCorrection += amount;
			break;
		}
	}
	return effectiveStats;
}

PlayerStats MakeDefaultPlayerStats() {
	// 新規プレイヤータイプは球モデルと1枠目のStraight攻撃を持つ状態から始める。
	PlayerStats stats;
	stats.modelFilePath = "sphere.obj";
	stats.attackSlots[0].enabled = true;
	stats.attackSlots[0].attackName = stats.initialAttackName;
	stats.attackSlots[0].attackLevel = stats.initialAttackLevel;
	return stats;
}

std::vector<std::string> GetPlayerAttackLevels() {
	// 通常Lv1～Lv5に加え、条件達成時だけ使うsuperを同じ配列で扱う。
	return {"1", "2", "3", "4", "5", "super"};
}

std::string NormalizePlayerAttackName(const std::string& attackName) {
	// 旧設定で使われていた内部名を、新しい表示名へ読み替えてセーブ互換を保つ。
	return attackName == "ArcHoming" ? "勾玉" : attackName;
}

PlayerAttackLevelStats MakeDefaultPlayerAttackLevelStats(const std::string& level) {
	// 攻撃JSONが存在しない場合でも各Lvが操作可能になるよう、レベルに応じた仮値を作る。
	PlayerAttackLevelStats stats;
	stats.level = level;
	const float levelScale = level == "super" ? 6.0f : static_cast<float>((std::max)(1, std::atoi(level.c_str())));
	stats.attack = 100.0f * levelScale;
	stats.speed = 0.3f + levelScale * 0.02f;
	stats.size = 100.0f + levelScale * 10.0f;
	stats.shotCount = level == "super" ? 5 : 1;
	stats.angles = stats.shotCount > 1 ? std::vector<float>{-30.0f, -15.0f, 0.0f, 15.0f, 30.0f} : std::vector<float>{0.0f};
	stats.spawnOffsets.assign(stats.shotCount, {0.0f, 0.5f, 1.2f});
	stats.modelFilePath = "sphere.obj";
	stats.homing = false;
	stats.homingAccuracy = 1.0f;
	stats.attackInterval = level == "super" ? 0.25f : 0.5f;
	stats.lifeTime = 3.0f;
	stats.travelDistance = 6.0f;
	stats.pierceCount = level == "super" ? 3 : 0;
	stats.infinitePierce = false;
	return stats;
}

PlayerAttackStats MakeDefaultPlayerAttackStats() {
	PlayerAttackStats stats;
	stats.name = "Straight";
	for (const std::string& level : GetPlayerAttackLevels()) {
		stats.levels.push_back(MakeDefaultPlayerAttackLevelStats(level));
	}
	return stats;
}

nlohmann::json PlayerAttackLevelStatsToJson(const PlayerAttackLevelStats& stats) {
	nlohmann::json json;
	json["level"] = stats.level;
	json["description"] = stats.choiceDescription;
	if (stats.level == "super") {
		json["texture"] = stats.choiceTextureFilePath;
	}
	json["attack"] = stats.attack;
	json["speed"] = stats.speed;
	json["size"] = stats.size;
	json["shotCount"] = stats.shotCount;
	json["angles"] = stats.angles;
	// 発射順を保持するため、弾番号と同じ順序のVector3配列として保存する。
	json["spawnOffsets"] = nlohmann::json::array();
	for (const Vector3& spawnOffset : stats.spawnOffsets) {
		json["spawnOffsets"].push_back(SceneJsonUtility::Vector3ToJson(spawnOffset));
	}
	json["model"] = stats.modelFilePath;
	json["homing"] = stats.homing;
	json["homingAccuracy"] = stats.homingAccuracy;
	json["attackInterval"] = stats.attackInterval;
	json["lifeTime"] = stats.lifeTime;
	json["travelDistance"] = stats.travelDistance;
	json["pierceCount"] = stats.pierceCount;
	json["infinitePierce"] = stats.infinitePierce;
	return json;
}

PlayerAttackLevelStats JsonToPlayerAttackLevelStats(const nlohmann::json& json, const PlayerAttackLevelStats& fallback) {
	PlayerAttackLevelStats stats = fallback;
	if (!json.is_object()) {
		return stats;
	}
	stats.level = json.value("level", stats.level);
	stats.choiceDescription = json.value("description", stats.choiceDescription);
	stats.choiceTextureFilePath = json.value("texture", stats.choiceTextureFilePath);
	stats.attack = (std::max)(0.0f, json.value("attack", stats.attack));
	stats.speed = (std::max)(0.0f, json.value("speed", stats.speed));
	stats.size = (std::max)(0.0f, json.value("size", stats.size));
	stats.shotCount = (std::max)(1, json.value("shotCount", stats.shotCount));
	// 現行の複数形配列を優先し、旧データの単一spawnOffsetも移行元として受け付ける。
	if (json.contains("spawnOffsets") && json.at("spawnOffsets").is_array()) {
		stats.spawnOffsets.clear();
		for (const nlohmann::json& spawnOffsetJson : json.at("spawnOffsets")) {
			stats.spawnOffsets.push_back(SceneJsonUtility::JsonToVector3(spawnOffsetJson, {0.0f, 0.5f, 1.2f}));
		}
	} else if (json.contains("spawnOffset")) {
		stats.spawnOffsets = {SceneJsonUtility::JsonToVector3(json.at("spawnOffset"), {0.0f, 0.5f, 1.2f})};
	}
	if (stats.spawnOffsets.empty()) {
		stats.spawnOffsets.push_back({0.0f, 0.5f, 1.2f});
	}
	// shotCountと配列数を必ず一致させ、発射側が弾番号で安全に参照できる状態にする。
	while (static_cast<int>(stats.spawnOffsets.size()) < stats.shotCount) {
		stats.spawnOffsets.push_back(stats.spawnOffsets.back());
	}
	while (static_cast<int>(stats.spawnOffsets.size()) > stats.shotCount) {
		stats.spawnOffsets.pop_back();
	}
	stats.modelFilePath = json.value("model", stats.modelFilePath);
	stats.homing = json.value("homing", stats.homing);
	stats.homingAccuracy = (std::clamp)(json.value("homingAccuracy", stats.homingAccuracy), 0.0f, 1.0f);
	stats.attackInterval = (std::max)(0.01f, json.value("attackInterval", stats.attackInterval));
	stats.lifeTime = (std::max)(0.01f, json.value("lifeTime", stats.lifeTime));
	stats.travelDistance = (std::max)(0.1f, json.value("travelDistance", stats.travelDistance));
	stats.pierceCount = (std::max)(0, json.value("pierceCount", stats.pierceCount));
	stats.infinitePierce = json.value("infinitePierce", stats.infinitePierce);
	if (json.contains("angles") && json.at("angles").is_array()) {
		stats.angles.clear();
		for (const nlohmann::json& angleJson : json.at("angles")) {
			stats.angles.push_back(angleJson.get<float>());
		}
	}
	while (static_cast<int>(stats.angles.size()) < stats.shotCount) {
		stats.angles.push_back(0.0f);
	}
	return stats;
}

nlohmann::json PlayerAttackStatsToJson(const PlayerAttackStats& stats) {
	nlohmann::json json;
	json["name"] = stats.name;
	json["texture"] = stats.choiceTextureFilePath;
	json["superCondition"]["status"] = stats.superConditionStatusName;
	json["superCondition"]["level"] = stats.superConditionStatusLevel;
	for (const PlayerAttackLevelStats& levelStats : stats.levels) {
		json["levels"][levelStats.level] = PlayerAttackLevelStatsToJson(levelStats);
	}
	return json;
}

PlayerAttackStats JsonToPlayerAttackStats(const nlohmann::json& json, const PlayerAttackStats& fallback) {
	PlayerAttackStats stats = fallback;
	if (!json.is_object()) {
		return stats;
	}
	stats.name = json.value("name", stats.name);
	stats.choiceTextureFilePath = json.value("texture", stats.choiceTextureFilePath);
	const nlohmann::json superConditionJson = json.value("superCondition", nlohmann::json::object());
	stats.superConditionStatusName = superConditionJson.value("status", stats.superConditionStatusName);
	stats.superConditionStatusLevel = superConditionJson.value("level", stats.superConditionStatusLevel);
	const std::vector<std::string> statusLevels = GetPlayerStatusItemLevels();
	if (std::find(statusLevels.begin(), statusLevels.end(), stats.superConditionStatusLevel) == statusLevels.end()) {
		stats.superConditionStatusLevel = "1";
	}
	stats.levels.clear();
	const nlohmann::json levelsJson = json.value("levels", nlohmann::json::object());
	for (const std::string& level : GetPlayerAttackLevels()) {
		PlayerAttackLevelStats fallbackLevel = MakeDefaultPlayerAttackLevelStats(level);
		for (const PlayerAttackLevelStats& existingLevel : fallback.levels) {
			if (existingLevel.level == level) {
				fallbackLevel = existingLevel;
				break;
			}
		}
		const nlohmann::json levelJson = levelsJson.value(level, nlohmann::json::object());
		stats.levels.push_back(JsonToPlayerAttackLevelStats(levelJson, fallbackLevel));
		stats.levels.back().level = level;
		// 旧周回弾は1点しか保存していないため、その半径と高さを保ったまま円周上へ等間隔展開する。
		if (stats.name == "Orbit" && !levelJson.contains("spawnOffsets") && !stats.levels.back().spawnOffsets.empty()) {
			const Vector3 baseOffset = stats.levels.back().spawnOffsets.front();
			const int shotCount = stats.levels.back().shotCount;
			stats.levels.back().spawnOffsets.clear();
			for (int index = 0; index < shotCount; ++index) {
				const float angle = MathConstants::kTwoPi * static_cast<float>(index) / static_cast<float>(shotCount);
				const float cosine = std::cos(angle);
				const float sine = std::sin(angle);
				stats.levels.back().spawnOffsets.push_back({
				    baseOffset.x * cosine + baseOffset.z * sine,
				    baseOffset.y,
				    -baseOffset.x * sine + baseOffset.z * cosine
				});
			}
		}
	}
	if (stats.choiceTextureFilePath.empty()) {
		for (const PlayerAttackLevelStats& levelStats : stats.levels) {
			if (levelStats.level != "super" && !levelStats.choiceTextureFilePath.empty()) {
				stats.choiceTextureFilePath = levelStats.choiceTextureFilePath;
				break;
			}
		}
	}
	return stats;
}

nlohmann::json LoadPlayerAttackStatusRoot() {
	// 攻撃設定ファイルがない場合はStraightのみを持つ仮JSONを返す。
	std::ifstream ifs(StringUtility::Utf8ToPath(kPlayerAttackStatusFilePath));
	if (!ifs) {
		nlohmann::json root;
		root["Straight"] = PlayerAttackStatsToJson(MakeDefaultPlayerAttackStats());
		return root;
	}

	nlohmann::json root;
	ifs >> root;
	if (!root.is_object()) {
		root = nlohmann::json::object();
	}
	if (!root.contains("Straight")) {
		root["Straight"] = PlayerAttackStatsToJson(MakeDefaultPlayerAttackStats());
	}
	return root;
}

std::vector<std::string> LoadPlayerAttackNames() {
	// 攻撃タイプの選択UIで順序が毎回変わらないよう、JSONキーをソートする。
	std::vector<std::string> names;
	const nlohmann::json root = LoadPlayerAttackStatusRoot();
	for (auto it = root.begin(); it != root.end(); ++it) {
		names.push_back(it.key());
	}
	std::sort(names.begin(), names.end());
	if (names.empty()) {
		names.push_back("Straight");
	}
	return names;
}

PlayerAttackStats LoadPlayerAttackStats(const std::string& attackName) {
	// 未登録の攻撃名を参照してもクラッシュさせず、Straight相当の既定値で復帰する。
	const std::string name = NormalizePlayerAttackName(attackName.empty() ? "Straight" : attackName);
	const nlohmann::json root = LoadPlayerAttackStatusRoot();
	const PlayerAttackStats fallback = MakeDefaultPlayerAttackStats();
	if (!root.contains(name)) {
		return fallback;
	}
	PlayerAttackStats stats = JsonToPlayerAttackStats(root.at(name), fallback);
	stats.name = name;
	return stats;
}

void ApplyPlayerAttackSlots(PlayerAttackComponent* attack, const PlayerStats& playerStats) {
	// プレイヤー設定のスロット内容を実行時攻撃コンポーネントへ反映する。
	if (!attack) {
		return;
	}
	attack->ClearAttackSlots();
	for (const PlayerAttackSlot& slot : playerStats.attackSlots) {
		if (slot.attackName.empty()) {
			continue;
		}
		attack->AddAttackSlot(LoadPlayerAttackStats(slot.attackName), slot.attackLevel, slot.enabled);
	}
}

void SavePlayerAttackStats(const std::string& attackName, const PlayerAttackStats& stats) {
	// 攻撃タイプ単位でJSONを更新し、他の攻撃タイプ設定は維持する。
	const std::string name = attackName.empty() ? "Straight" : attackName;
	nlohmann::json root = LoadPlayerAttackStatusRoot();
	PlayerAttackStats saveStats = stats;
	saveStats.name = name;
	root[name] = PlayerAttackStatsToJson(saveStats);

	std::filesystem::create_directories(StringUtility::Utf8ToPath(kPlayerAttackStatusFilePath).parent_path());
	std::ofstream ofs(StringUtility::Utf8ToPath(kPlayerAttackStatusFilePath));
	if (ofs) {
		ofs << std::setw(4) << root << std::endl;
	}
}

nlohmann::json PlayerStatsToJson(const PlayerStats& stats) {
	// PlayerStatsの保存対象だけをJSONへ変換する。実効ステータスではなく基礎値を保存する。
	nlohmann::json json;
	json["name"] = stats.name;
	json["baseHealth"] = stats.baseHealth;
	json["health"] = stats.health;
	json["attack"] = stats.attack;
	json["defense"] = stats.defense;
	json["baseSpeed"] = stats.baseSpeed;
	json["speed"] = stats.speed;
	json["attackSpeed"] = stats.attackSpeed;
	json["attackSize"] = stats.attackSize;
	json["sizeScale"] = stats.sizeScale;
	json["damageInvincibilityDuration"] = stats.damageInvincibilityDuration;
	json["level"] = stats.level;
	json["experience"] = stats.experience;
	json["experienceCorrection"] = stats.experienceCorrection;
	json["model"] = stats.modelFilePath;
	json["isAnimationModel"] = stats.isAnimationModel;
	json["initialAttack"] = stats.initialAttackName;
	json["initialAttackLevel"] = stats.initialAttackLevel;
	json["attackSlots"] = nlohmann::json::array();
	for (const PlayerAttackSlot& slot : stats.attackSlots) {
		nlohmann::json slotJson;
		slotJson["enabled"] = slot.enabled;
		slotJson["attack"] = slot.attackName;
		slotJson["level"] = slot.attackLevel;
		json["attackSlots"].push_back(slotJson);
	}
	json["statusSlots"] = nlohmann::json::array();
	for (const PlayerStatusSlot& slot : stats.statusSlots) {
		nlohmann::json slotJson;
		slotJson["enabled"] = slot.enabled;
		slotJson["status"] = slot.statusName;
		slotJson["level"] = slot.level;
		json["statusSlots"].push_back(slotJson);
	}
	return json;
}

PlayerStats JsonToPlayerStats(const nlohmann::json& json, const PlayerStats& fallback) {
	// 欠けている項目はfallbackで補い、古いセーブデータも読み込めるようにする。
	PlayerStats stats = fallback;
	if (!json.is_object()) {
		return stats;
	}

	stats.name = json.value("name", stats.name);
	stats.baseHealth = json.value("baseHealth", stats.baseHealth);
	stats.health = json.value("health", stats.health);
	stats.attack = json.value("attack", stats.attack);
	stats.defense = json.value("defense", stats.defense);
	stats.baseSpeed = json.value("baseSpeed", stats.baseSpeed);
	stats.speed = json.value("speed", stats.speed);
	stats.attackSpeed = json.value("attackSpeed", stats.attackSpeed);
	stats.attackSize = json.value("attackSize", stats.attackSize);
	stats.sizeScale = json.value("sizeScale", stats.sizeScale);
	stats.damageInvincibilityDuration = json.value("damageInvincibilityDuration", stats.damageInvincibilityDuration);
	stats.level = json.value("level", stats.level);
	stats.experience = json.value("experience", stats.experience);
	stats.experienceCorrection = json.value("experienceCorrection", stats.experienceCorrection);
	stats.modelFilePath = json.value("model", stats.modelFilePath);
	stats.isAnimationModel = json.value("isAnimationModel", stats.isAnimationModel);
	stats.initialAttackName = NormalizePlayerAttackName(json.value("initialAttack", stats.initialAttackName));
	stats.initialAttackLevel = json.value("initialAttackLevel", stats.initialAttackLevel);
	for (PlayerAttackSlot& slot : stats.attackSlots) {
		slot.enabled = false;
		slot.attackName.clear();
		slot.attackLevel = "1";
	}
	stats.attackSlots[0].enabled = true;
	stats.attackSlots[0].attackName = stats.initialAttackName;
	stats.attackSlots[0].attackLevel = stats.initialAttackLevel;
	if (json.contains("attackSlots") && json.at("attackSlots").is_array()) {
		const nlohmann::json& slotsJson = json.at("attackSlots");
		for (size_t index = 0; index < stats.attackSlots.size() && index < slotsJson.size(); ++index) {
			const nlohmann::json& slotJson = slotsJson.at(index);
			if (!slotJson.is_object()) {
				continue;
			}
			stats.attackSlots[index].enabled = slotJson.value("enabled", stats.attackSlots[index].enabled);
			stats.attackSlots[index].attackName = NormalizePlayerAttackName(slotJson.value("attack", stats.attackSlots[index].attackName));
			stats.attackSlots[index].attackLevel = slotJson.value("level", stats.attackSlots[index].attackLevel);
		}
	}
	stats.initialAttackName = stats.attackSlots[0].attackName;
	stats.initialAttackLevel = stats.attackSlots[0].attackLevel;
	for (PlayerStatusSlot& slot : stats.statusSlots) {
		slot.enabled = false;
		slot.statusName.clear();
	}
	if (json.contains("statusSlots") && json.at("statusSlots").is_array()) {
		const nlohmann::json& slotsJson = json.at("statusSlots");
		for (size_t index = 0; index < stats.statusSlots.size() && index < slotsJson.size(); ++index) {
			const nlohmann::json& slotJson = slotsJson.at(index);
			if (!slotJson.is_object()) {
				continue;
			}
			stats.statusSlots[index].enabled = slotJson.value("enabled", stats.statusSlots[index].enabled);
			stats.statusSlots[index].statusName = slotJson.value("status", stats.statusSlots[index].statusName);
			stats.statusSlots[index].level = slotJson.value("level", stats.statusSlots[index].level);
			if (PlayerStatusSlotLevelToIndex(stats.statusSlots[index].level) < 0) {
				stats.statusSlots[index].level = "1";
			}
			if (stats.statusSlots[index].statusName.empty()) {
				stats.statusSlots[index].enabled = false;
			}
		}
	}

	stats.baseHealth = (std::max)(0.0f, stats.baseHealth);
	stats.health = (std::max)(0.0f, stats.health);
	stats.attack = (std::max)(0.0f, stats.attack);
	stats.defense = (std::max)(0.0f, stats.defense);
	stats.baseSpeed = (std::max)(0.0f, stats.baseSpeed);
	stats.speed = (std::max)(0.0f, stats.speed);
	stats.attackSpeed = (std::max)(0.0f, stats.attackSpeed);
	stats.attackSize = (std::max)(0.0f, stats.attackSize);
	stats.sizeScale = (std::max)(0.1f, stats.sizeScale);
	stats.damageInvincibilityDuration = (std::max)(0.0f, stats.damageInvincibilityDuration);
	stats.level = (std::max)(1, stats.level);
	stats.experience = (std::max)(0, stats.experience);
	stats.experienceCorrection = (std::max)(0.0f, stats.experienceCorrection);
	return stats;
}

nlohmann::json LoadPlayerStatusRoot() {
	// プレイヤーステータスファイルがない場合はDefaultを含む仮JSONを返す。
	std::ifstream ifs(StringUtility::Utf8ToPath(kPlayerStatusFilePath));
	if (!ifs) {
		nlohmann::json root;
		root["Default"] = PlayerStatsToJson(MakeDefaultPlayerStats());
		return root;
	}

	nlohmann::json root;
	ifs >> root;
	if (!root.is_object()) {
		root = nlohmann::json::object();
	}
	if (!root.contains("Default")) {
		root["Default"] = PlayerStatsToJson(MakeDefaultPlayerStats());
	}
	return root;
}

bool SavePlayerStatusRoot(const nlohmann::json& root) {
	std::filesystem::create_directories(StringUtility::Utf8ToPath(kPlayerStatusFilePath).parent_path());
	std::ofstream ofs(StringUtility::Utf8ToPath(kPlayerStatusFilePath));
	if (!ofs) {
		return false;
	}
	ofs << std::setw(4) << root << std::endl;
	return static_cast<bool>(ofs);
}

std::vector<std::string> LoadPlayerTypeNames() {
	// プレイヤー選択UIで使うタイプ名をJSONキーから収集する。
	std::vector<std::string> names;
	const nlohmann::json root = LoadPlayerStatusRoot();
	for (auto it = root.begin(); it != root.end(); ++it) {
		names.push_back(it.key());
	}
	std::sort(names.begin(), names.end());
	if (names.empty()) {
		names.push_back("Default");
	}
	return names;
}

PlayerStats LoadPlayerStats(const std::string& playerTypeName) {
	// 存在しないタイプ名が指定された場合はDefault相当のプレイヤー設定を返す。
	const std::string typeName = playerTypeName.empty() ? "Default" : playerTypeName;
	const nlohmann::json root = LoadPlayerStatusRoot();
	const PlayerStats fallback = MakeDefaultPlayerStats();
	if (!root.contains(typeName)) {
		return fallback;
	}
	PlayerStats stats = JsonToPlayerStats(root.at(typeName), fallback);
	stats.name = typeName;
	return stats;
}

void SavePlayerStats(const std::string& playerTypeName, const PlayerStats& stats) {
	// プレイヤータイプ単位で上書きし、他タイプの設定は保持する。
	const std::string typeName = playerTypeName.empty() ? "Default" : playerTypeName;
	nlohmann::json root = LoadPlayerStatusRoot();
	PlayerStats saveStats = stats;
	saveStats.name = typeName;
	root[typeName] = PlayerStatsToJson(saveStats);
	SavePlayerStatusRoot(root);
}

bool RenamePlayerStats(const std::string& oldTypeName, const std::string& newTypeName, const PlayerStats& stats) {
	if (oldTypeName.empty() || newTypeName.empty()) {
		return false;
	}
	if (oldTypeName == newTypeName) {
		SavePlayerStats(oldTypeName, stats);
		return true;
	}

	nlohmann::json root = LoadPlayerStatusRoot();
	if (!root.contains(oldTypeName) || root.contains(newTypeName)) {
		return false;
	}

	PlayerStats saveStats = stats;
	saveStats.name = newTypeName;
	root[newTypeName] = PlayerStatsToJson(saveStats);
	root.erase(oldTypeName);
	return SavePlayerStatusRoot(root);
}

bool DeletePlayerStats(const std::string& playerTypeName) {
	// Default は参照切れ時のフォールバックとして常に残す。
	if (playerTypeName.empty() || playerTypeName == "Default") {
		return false;
	}

	nlohmann::json root = LoadPlayerStatusRoot();
	if (!root.contains(playerTypeName)) {
		return false;
	}
	root.erase(playerTypeName);
	return SavePlayerStatusRoot(root);
}
