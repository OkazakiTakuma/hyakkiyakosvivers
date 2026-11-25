#pragma once
#include "data/PlayerAttackDefinition.h"
#include "data/PlayerStatsData.h"
#include "data/PlayerStatusItemDefinition.h"
#include <json_fwd.hpp>
#include <string>
#include <vector>

// プレイヤー能力、強化アイテム、攻撃設定をJSONへ変換して永続化するリポジトリ。
constexpr const char* kPlayerStatusFilePath = "Resources/Data/player_status.json";
constexpr const char* kPlayerAttackStatusFilePath = "Resources/Data/player_attack_status.json";
constexpr const char* kPlayerStatusItemFilePath = "Resources/Data/player_status_item_status.json";

std::vector<std::string> GetPlayerStatusItemLevels();

const char* PlayerStatusItemTypeToName(PlayerStatusItemType type);

PlayerStatusItemType PlayerStatusItemTypeFromName(const std::string& typeName);

PlayerStatusItemStats MakeDefaultPlayerStatusItemStats();

nlohmann::json PlayerStatusItemStatsToJson(const PlayerStatusItemStats& stats);

PlayerStatusItemStats JsonToPlayerStatusItemStats(const nlohmann::json& json, const PlayerStatusItemStats& fallback);

nlohmann::json LoadPlayerStatusItemRoot();

std::vector<std::string> LoadPlayerStatusItemNames();

PlayerStatusItemStats LoadPlayerStatusItemStats(const std::string& itemName);

/// <summary>内部名から、全画面で共通利用する加護の表示名を読み込みます。</summary>
std::string LoadPlayerStatusItemDisplayName(const std::string& itemName);

/// <summary>全強化アイテム定義を一度のファイル読込で名前付きコレクションへ変換します。</summary>
PlayerStatusItemDefinitions LoadPlayerStatusItemDefinitions();

void SavePlayerStatusItemStats(const std::string& itemName, const PlayerStatusItemStats& stats);

int PlayerStatusSlotLevelToIndex(const std::string& level);

PlayerStats MakeDefaultPlayerStats();

std::vector<std::string> GetPlayerAttackLevels();

PlayerAttackLevelStats MakeDefaultPlayerAttackLevelStats(const std::string& level);

PlayerAttackStats MakeDefaultPlayerAttackStats();

nlohmann::json PlayerAttackLevelStatsToJson(const PlayerAttackLevelStats& stats);

PlayerAttackLevelStats JsonToPlayerAttackLevelStats(const nlohmann::json& json, const PlayerAttackLevelStats& fallback);

nlohmann::json PlayerAttackStatsToJson(const PlayerAttackStats& stats);

PlayerAttackStats JsonToPlayerAttackStats(const nlohmann::json& json, const PlayerAttackStats& fallback);

nlohmann::json LoadPlayerAttackStatusRoot();

std::vector<std::string> LoadPlayerAttackNames();

PlayerAttackStats LoadPlayerAttackStats(const std::string& attackName);

/// <summary>内部名から、全画面で共通利用する武器の通常またはSuper表示名を読み込みます。</summary>
std::string LoadPlayerAttackDisplayName(const std::string& attackName, bool isSuper = false);

/// <summary>全攻撃定義を一度のファイル読込で名前付きコレクションへ変換します。</summary>
PlayerAttackDefinitions LoadPlayerAttackDefinitions();

void SavePlayerAttackStats(const std::string& attackName, const PlayerAttackStats& stats);

nlohmann::json PlayerStatsToJson(const PlayerStats& stats);

PlayerStats JsonToPlayerStats(const nlohmann::json& json, const PlayerStats& fallback);

nlohmann::json LoadPlayerStatusRoot();

/// <summary>
/// プレイヤータイプ一覧をJSONファイルへ書き込みます。
/// 名前変更と削除でも同じ保存処理を利用し、書き込み結果を呼び出し元へ返します。
/// </summary>
bool SavePlayerStatusRoot(const nlohmann::json& root);

std::vector<std::string> LoadPlayerTypeNames();

PlayerStats LoadPlayerStats(const std::string& playerTypeName);

void SavePlayerStats(const std::string& playerTypeName, const PlayerStats& stats);

/// <summary>
/// プレイヤータイプのキーと内部の名前を同時に変更します。
/// 変更先が既に存在する場合は、既存タイプを上書きしないため失敗とします。
/// </summary>
bool RenamePlayerStats(const std::string& oldTypeName, const std::string& newTypeName, const PlayerStats& stats);

/// <summary>
/// 指定したプレイヤータイプをJSONから削除します。
/// </summary>
bool DeletePlayerStats(const std::string& playerTypeName);
