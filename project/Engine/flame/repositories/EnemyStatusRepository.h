#pragma once

#include "components/Enemy/EnemyComponent.h"
#include "../../base/StringUtility.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#pragma warning(push)
#pragma warning(disable: 26495)
#include <json.hpp>
#pragma warning(pop)
#include <string>
#include <vector>

/// <summary>
/// 敵ステータスのJSON変換・読み込み・保存を担当します。
/// </summary>
namespace EnemyStatusCacheDetail {
inline nlohmann::json root;
inline bool initialized = false;
}

namespace {

inline constexpr const char* kFilePath = "Resources/Data/enemy_status.json";

inline EnemyStats MakeDefaultStats() {
	return {};
}

inline const char* BehaviorToString(EnemyBehaviorType behavior) {
	switch (behavior) {
	case EnemyBehaviorType::Shooter: return "Shooter";
	case EnemyBehaviorType::Charger: return "Charger";
	case EnemyBehaviorType::NightSlashBoss: return "NightSlashBoss";
	case EnemyBehaviorType::SelfDestruct: return "SelfDestruct";
	case EnemyBehaviorType::TornadoBoss: return "TornadoBoss";
	case EnemyBehaviorType::BurstShooter: return "BurstShooter";
	default: return "Chase";
	}
}

inline EnemyBehaviorType BehaviorFromString(const std::string& behavior) {
	if (behavior == "Shooter") return EnemyBehaviorType::Shooter;
	if (behavior == "Charger") return EnemyBehaviorType::Charger;
	if (behavior == "NightSlashBoss") return EnemyBehaviorType::NightSlashBoss;
	if (behavior == "SelfDestruct") return EnemyBehaviorType::SelfDestruct;
	if (behavior == "TornadoBoss") return EnemyBehaviorType::TornadoBoss;
	if (behavior == "BurstShooter") return EnemyBehaviorType::BurstShooter;
	return EnemyBehaviorType::Chase;
}

inline nlohmann::json ToJson(const EnemyStats& stats) {
	nlohmann::json json;
	json["health"] = stats.health;
	json["attack"] = stats.attack;
	json["speed"] = stats.speed;
	json["shoots"] = stats.shoots;
	json["shootingInterval"] = stats.shootingInterval;
	json["spawnsPerMinute"] = stats.spawnsPerMinute;
	json["experience"] = stats.experience;
	json["experienceModel"] = stats.experienceModelFilePath;
	// アイテム調整値も敵タイプ別に保存し、エディタで設定した値を次回起動時に復元する。
	json["healthItemDropChance"] = stats.healthItemDropChance;
	json["collectExperienceItemDropChance"] = stats.collectExperienceItemDropChance;
	json["healthItemHealAmount"] = stats.healthItemHealAmount;
	json["behavior"] = BehaviorToString(stats.behavior);
	json["preferredDistance"] = stats.preferredDistance;
	json["distanceTolerance"] = stats.distanceTolerance;
	json["projectileSpeed"] = stats.projectileSpeed;
	json["projectileSize"] = stats.projectileSize;
	json["projectileLifeTime"] = stats.projectileLifeTime;
	json["burstShotCount"] = stats.burstShotCount;
	json["burstSpreadAngle"] = stats.burstSpreadAngle;
	json["chargeTriggerDistance"] = stats.chargeTriggerDistance;
	json["chargeDuration"] = stats.chargeDuration;
	json["dashSpeed"] = stats.dashSpeed;
	json["dashDuration"] = stats.dashDuration;
	json["dashRecovery"] = stats.dashRecovery;
	// つじぎりボスの連続斬り設定。JSON側の値をエディター調整後も保持する。
	json["comboTriggerDistance"] = stats.comboTriggerDistance;
	json["comboWindup"] = stats.comboWindup;
	json["comboDashSpeed"] = stats.comboDashSpeed;
	json["comboDashDuration"] = stats.comboDashDuration;
	json["comboSlashPause"] = stats.comboSlashPause;
	json["comboRecovery"] = stats.comboRecovery;
	json["comboSideOffset"] = stats.comboSideOffset;
	json["comboDashCount"] = stats.comboDashCount;
	json["finisherSpeedMultiplier"] = stats.finisherSpeedMultiplier;
	// 同じボスが交互に使用する全方位弾幕・扇状弾幕の設定。
	json["bossRangedWindup"] = stats.bossRangedWindup;
	json["bossRangedInterval"] = stats.bossRangedInterval;
	json["bossRangedWaves"] = stats.bossRangedWaves;
	json["bossRadialShotCount"] = stats.bossRadialShotCount;
	json["bossAimedShotCount"] = stats.bossAimedShotCount;
	json["bossAimedSpreadAngle"] = stats.bossAimedSpreadAngle;
	json["bossProjectileAttackMultiplier"] = stats.bossProjectileAttackMultiplier;
	json["bossTornadoCount"] = stats.bossTornadoCount;
	json["bossTornadoInitialRadius"] = stats.bossTornadoInitialRadius;
	json["bossTornadoAngularSpeed"] = stats.bossTornadoAngularSpeed;
	json["bossTornadoRadialSpeed"] = stats.bossTornadoRadialSpeed;
	json["bossTornadoSize"] = stats.bossTornadoSize;
	json["bossTornadoLifeTime"] = stats.bossTornadoLifeTime;
	json["bossTornadoAttackMultiplier"] = stats.bossTornadoAttackMultiplier;
	json["bossTornadoTriggerDistance"] = stats.bossTornadoTriggerDistance;
	json["bossTornadoWindup"] = stats.bossTornadoWindup;
	json["bossTornadoRecovery"] = stats.bossTornadoRecovery;
	json["bossConvergingTornadoCount"] = stats.bossConvergingTornadoCount;
	json["bossConvergingTornadoInitialRadius"] = stats.bossConvergingTornadoInitialRadius;
	json["bossConvergingTornadoAngularSpeed"] = stats.bossConvergingTornadoAngularSpeed;
	json["bossConvergingTornadoRadialSpeed"] = stats.bossConvergingTornadoRadialSpeed;
	json["bossConvergingTornadoSize"] = stats.bossConvergingTornadoSize;
	json["bossConvergingTornadoLifeTime"] = stats.bossConvergingTornadoLifeTime;
	json["bossConvergingTornadoAttackMultiplier"] = stats.bossConvergingTornadoAttackMultiplier;
	json["bossGiantTornadoSpeed"] = stats.bossGiantTornadoSpeed;
	json["bossGiantTornadoSize"] = stats.bossGiantTornadoSize;
	json["bossGiantTornadoLifeTime"] = stats.bossGiantTornadoLifeTime;
	json["bossGiantTornadoAttackMultiplier"] = stats.bossGiantTornadoAttackMultiplier;
	json["bossGiantTornadoSpawnOffset"] = stats.bossGiantTornadoSpawnOffset;
	json["selfDestructTriggerDistance"] = stats.selfDestructTriggerDistance;
	json["selfDestructFuseDuration"] = stats.selfDestructFuseDuration;
	json["selfDestructRadius"] = stats.selfDestructRadius;
	json["sizeScale"] = stats.sizeScale;
	return json;
}

inline EnemyStats FromJson(const nlohmann::json& json, const EnemyStats& fallback) {
	EnemyStats stats = fallback;
	if (!json.is_object()) {
		return stats;
	}

	stats.health = (std::max)(0.0f, json.value("health", stats.health));
	stats.attack = (std::max)(0.0f, json.value("attack", stats.attack));
	stats.speed = (std::max)(0.0f, json.value("speed", stats.speed));
	stats.shoots = json.value("shoots", stats.shoots);
	stats.shootingInterval = (std::max)(0.0f, json.value("shootingInterval", stats.shootingInterval));
	stats.spawnsPerMinute = (std::max)(0.0f, json.value("spawnsPerMinute", stats.spawnsPerMinute));
	stats.experience = (std::max)(0, json.value("experience", stats.experience));
	stats.experienceModelFilePath = json.value("experienceModel", stats.experienceModelFilePath);
	// 不正なJSON値で抽選確率や回復量が範囲外にならないよう、読み込み時に補正する。
	stats.healthItemDropChance = (std::clamp)(json.value("healthItemDropChance", stats.healthItemDropChance), 0.0f, 1.0f);
	stats.collectExperienceItemDropChance = (std::clamp)(json.value("collectExperienceItemDropChance", stats.collectExperienceItemDropChance), 0.0f, 1.0f);
	stats.healthItemHealAmount = (std::max)(0.0f, json.value("healthItemHealAmount", stats.healthItemHealAmount));
	stats.behavior = BehaviorFromString(json.value("behavior", std::string(BehaviorToString(stats.behavior))));
	// behavior導入前のデータではshootsだけが保存されていたため、旧データをShooterとして引き継ぐ。
	if (stats.shoots && !json.contains("behavior")) stats.behavior = EnemyBehaviorType::Shooter;
	stats.preferredDistance = (std::max)(0.0f, json.value("preferredDistance", stats.preferredDistance));
	stats.distanceTolerance = (std::max)(0.0f, json.value("distanceTolerance", stats.distanceTolerance));
	stats.projectileSpeed = (std::max)(0.0f, json.value("projectileSpeed", stats.projectileSpeed));
	stats.projectileSize = (std::max)(0.01f, json.value("projectileSize", stats.projectileSize));
	stats.projectileLifeTime = (std::max)(0.0f, json.value("projectileLifeTime", stats.projectileLifeTime));
	stats.burstShotCount = std::clamp(json.value("burstShotCount", stats.burstShotCount), 1, 31);
	stats.burstSpreadAngle = (std::max)(0.0f, json.value("burstSpreadAngle", stats.burstSpreadAngle));
	stats.chargeTriggerDistance = (std::max)(0.0f, json.value("chargeTriggerDistance", stats.chargeTriggerDistance));
	stats.chargeDuration = (std::max)(0.0f, json.value("chargeDuration", stats.chargeDuration));
	stats.dashSpeed = (std::max)(0.0f, json.value("dashSpeed", stats.dashSpeed));
	stats.dashDuration = (std::max)(0.0f, json.value("dashDuration", stats.dashDuration));
	stats.dashRecovery = (std::max)(0.0f, json.value("dashRecovery", stats.dashRecovery));
	// 時間・距離・速度は負数を禁止し、回数は最低1回、最終速度倍率は最低1倍へ補正する。
	stats.comboTriggerDistance = (std::max)(0.0f, json.value("comboTriggerDistance", stats.comboTriggerDistance));
	stats.comboWindup = (std::max)(0.0f, json.value("comboWindup", stats.comboWindup));
	stats.comboDashSpeed = (std::max)(0.0f, json.value("comboDashSpeed", stats.comboDashSpeed));
	stats.comboDashDuration = (std::max)(0.0f, json.value("comboDashDuration", stats.comboDashDuration));
	stats.comboSlashPause = (std::max)(0.0f, json.value("comboSlashPause", stats.comboSlashPause));
	stats.comboRecovery = (std::max)(0.0f, json.value("comboRecovery", stats.comboRecovery));
	stats.comboSideOffset = (std::max)(0.0f, json.value("comboSideOffset", stats.comboSideOffset));
	stats.comboDashCount = (std::max)(1, json.value("comboDashCount", stats.comboDashCount));
	stats.finisherSpeedMultiplier = (std::max)(1.0f, json.value("finisherSpeedMultiplier", stats.finisherSpeedMultiplier));
	stats.bossRangedWindup = (std::max)(0.0f, json.value("bossRangedWindup", stats.bossRangedWindup));
	stats.bossRangedInterval = (std::max)(0.01f, json.value("bossRangedInterval", stats.bossRangedInterval));
	stats.bossRangedWaves = std::clamp(json.value("bossRangedWaves", stats.bossRangedWaves), 1, 20);
	stats.bossRadialShotCount = std::clamp(json.value("bossRadialShotCount", stats.bossRadialShotCount), 1, 64);
	stats.bossAimedShotCount = std::clamp(json.value("bossAimedShotCount", stats.bossAimedShotCount), 1, 31);
	stats.bossAimedSpreadAngle = (std::max)(0.0f, json.value("bossAimedSpreadAngle", stats.bossAimedSpreadAngle));
	stats.bossProjectileAttackMultiplier =
	    (std::max)(0.0f, json.value("bossProjectileAttackMultiplier", stats.bossProjectileAttackMultiplier));
	stats.bossTornadoCount = std::clamp(json.value("bossTornadoCount", stats.bossTornadoCount), 1, 16);
	stats.bossTornadoInitialRadius =
	    (std::max)(0.0f, json.value("bossTornadoInitialRadius", stats.bossTornadoInitialRadius));
	stats.bossTornadoAngularSpeed = json.value("bossTornadoAngularSpeed", stats.bossTornadoAngularSpeed);
	stats.bossTornadoRadialSpeed =
	    (std::max)(0.0f, json.value("bossTornadoRadialSpeed", stats.bossTornadoRadialSpeed));
	stats.bossTornadoSize = (std::max)(0.01f, json.value("bossTornadoSize", stats.bossTornadoSize));
	stats.bossTornadoLifeTime = (std::max)(0.0f, json.value("bossTornadoLifeTime", stats.bossTornadoLifeTime));
	stats.bossTornadoAttackMultiplier =
	    (std::max)(0.0f, json.value("bossTornadoAttackMultiplier", stats.bossTornadoAttackMultiplier));
	stats.bossTornadoTriggerDistance =
	    (std::max)(0.0f, json.value("bossTornadoTriggerDistance", stats.bossTornadoTriggerDistance));
	stats.bossTornadoWindup = (std::max)(0.0f, json.value("bossTornadoWindup", stats.bossTornadoWindup));
	stats.bossTornadoRecovery = (std::max)(0.0f, json.value("bossTornadoRecovery", stats.bossTornadoRecovery));
	stats.bossConvergingTornadoCount =
	    std::clamp(json.value("bossConvergingTornadoCount", stats.bossConvergingTornadoCount), 1, 24);
	stats.bossConvergingTornadoInitialRadius = (std::max)(
	    0.0f, json.value("bossConvergingTornadoInitialRadius", stats.bossConvergingTornadoInitialRadius));
	stats.bossConvergingTornadoAngularSpeed =
	    json.value("bossConvergingTornadoAngularSpeed", stats.bossConvergingTornadoAngularSpeed);
	stats.bossConvergingTornadoRadialSpeed = (std::max)(
	    0.0f, json.value("bossConvergingTornadoRadialSpeed", stats.bossConvergingTornadoRadialSpeed));
	stats.bossConvergingTornadoSize =
	    (std::max)(0.01f, json.value("bossConvergingTornadoSize", stats.bossConvergingTornadoSize));
	stats.bossConvergingTornadoLifeTime =
	    (std::max)(0.0f, json.value("bossConvergingTornadoLifeTime", stats.bossConvergingTornadoLifeTime));
	stats.bossConvergingTornadoAttackMultiplier = (std::max)(
	    0.0f, json.value("bossConvergingTornadoAttackMultiplier", stats.bossConvergingTornadoAttackMultiplier));
	stats.bossGiantTornadoSpeed =
	    (std::max)(0.0f, json.value("bossGiantTornadoSpeed", stats.bossGiantTornadoSpeed));
	stats.bossGiantTornadoSize =
	    (std::max)(0.01f, json.value("bossGiantTornadoSize", stats.bossGiantTornadoSize));
	stats.bossGiantTornadoLifeTime =
	    (std::max)(0.0f, json.value("bossGiantTornadoLifeTime", stats.bossGiantTornadoLifeTime));
	stats.bossGiantTornadoAttackMultiplier =
	    (std::max)(0.0f, json.value("bossGiantTornadoAttackMultiplier", stats.bossGiantTornadoAttackMultiplier));
	stats.bossGiantTornadoSpawnOffset =
	    (std::max)(0.0f, json.value("bossGiantTornadoSpawnOffset", stats.bossGiantTornadoSpawnOffset));
	stats.selfDestructTriggerDistance =
	    (std::max)(0.0f, json.value("selfDestructTriggerDistance", stats.selfDestructTriggerDistance));
	stats.selfDestructFuseDuration =
	    (std::max)(0.0f, json.value("selfDestructFuseDuration", stats.selfDestructFuseDuration));
	stats.selfDestructRadius = (std::max)(0.0f, json.value("selfDestructRadius", stats.selfDestructRadius));
	// 0以下の倍率でモデルやコライダーが消失しないよう、最小サイズを保証する。
	stats.sizeScale = (std::max)(0.1f, json.value("sizeScale", stats.sizeScale));
	return stats;
}

inline nlohmann::json& CachedRoot() {
	return EnemyStatusCacheDetail::root;
}

inline bool& IsCacheInitialized() {
	return EnemyStatusCacheDetail::initialized;
}

inline void InvalidateEnemyStatsCache() {
	CachedRoot() = nlohmann::json();
	IsCacheInitialized() = false;
}

inline const nlohmann::json& LoadRoot() {
	// 敵生成のたびにファイルを開いてJSON全体を解析すると周期的なフレーム落ちになるため、
	// 明示的に保存・無効化されるまではメモリ上のスナップショットを返す。
	if (IsCacheInitialized()) {
		return CachedRoot();
	}

	std::ifstream ifs(StringUtility::Utf8ToPath(kFilePath));
	if (!ifs) {
		nlohmann::json root;
		root["Default"] = ToJson(MakeDefaultStats());
		CachedRoot() = root;
		IsCacheInitialized() = true;
		return CachedRoot();
	}

	nlohmann::json root;
	ifs >> root;
	if (!root.is_object()) {
		root = nlohmann::json::object();
	}
	if (!root.contains("Default")) {
		root["Default"] = ToJson(MakeDefaultStats());
	}
	CachedRoot() = root;
	IsCacheInitialized() = true;
	return CachedRoot();
}

inline std::vector<std::string> LoadEnemyTypeNames() {
	std::vector<std::string> names;
	const nlohmann::json& root = LoadRoot();
	for (auto it = root.begin(); it != root.end(); ++it) {
		names.push_back(it.key());
	}
	std::sort(names.begin(), names.end());
	if (names.empty()) {
		names.push_back("Default");
	}
	return names;
}

inline EnemyStats LoadEnemyStats(const std::string& enemyTypeName) {
	const std::string typeName = enemyTypeName.empty() ? "Default" : enemyTypeName;
	const nlohmann::json& root = LoadRoot();
	const EnemyStats fallback = MakeDefaultStats();
	return root.contains(typeName) ? FromJson(root.at(typeName), fallback) : fallback;
}

inline void SaveEnemyStats(const std::string& enemyTypeName, const EnemyStats& stats) {
	const std::string typeName = enemyTypeName.empty() ? "Default" : enemyTypeName;
	nlohmann::json root = LoadRoot();
	root[typeName] = ToJson(stats);

	std::filesystem::create_directories(StringUtility::Utf8ToPath(kFilePath).parent_path());
	std::ofstream ofs(StringUtility::Utf8ToPath(kFilePath));
	if (ofs) {
		ofs << std::setw(4) << root << std::endl;
		// エディタ保存直後から、次回の敵生成も同じ内容を参照する。
		CachedRoot() = root;
		IsCacheInitialized() = true;
	}
}

}
