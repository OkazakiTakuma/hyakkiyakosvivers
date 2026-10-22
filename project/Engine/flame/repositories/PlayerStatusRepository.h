#pragma once
#include "components/PlayerAttack/PlayerAttackComponent.h"
#include "../../../Player/Player.h"
#include <array>
#include <json_fwd.hpp>
#include <string>
#include <vector>

// プレイヤー能力、強化アイテム、攻撃設定をJSONへ変換して永続化するリポジトリ。
constexpr const char* kPlayerStatusFilePath = "Resources/Data/player_status.json";
constexpr const char* kPlayerAttackStatusFilePath = "Resources/Data/player_attack_status.json";
constexpr const char* kPlayerStatusItemFilePath = "Resources/Data/player_status_item_status.json";

/// <summary>強化アイテムが変更するプレイヤー能力の種類です。</summary>
enum class PlayerStatusItemType {
	Attack,
	Health,
	AttackSpeed,
	Speed,
	Defense,
	AttackSize,
	Experience
};

/// <summary>強化アイテムのレベル別効果量と選択画面表示情報です。</summary>
struct PlayerStatusItemStats {
	std::string name = "AttackUp";
	PlayerStatusItemType type = PlayerStatusItemType::Attack;
	std::array<float, 5> levelAmounts{10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
	/// レベルアップ選択カードへ表示するLv1～Lv5の説明文です。
	std::array<std::string, 5> levelDescriptions{};
	/// レベルアップ選択カードと右上スロットHUDで使うLv1～Lv5の画像パスです。
	std::array<std::string, 5> levelTextureFilePaths{};
};

std::vector<std::string> GetPlayerStatusItemLevels();

const char* PlayerStatusItemTypeToName(PlayerStatusItemType type);

PlayerStatusItemType PlayerStatusItemTypeFromName(const std::string& typeName);

PlayerStatusItemStats MakeDefaultPlayerStatusItemStats();

nlohmann::json PlayerStatusItemStatsToJson(const PlayerStatusItemStats& stats);

PlayerStatusItemStats JsonToPlayerStatusItemStats(const nlohmann::json& json, const PlayerStatusItemStats& fallback);

nlohmann::json LoadPlayerStatusItemRoot();

std::vector<std::string> LoadPlayerStatusItemNames();

PlayerStatusItemStats LoadPlayerStatusItemStats(const std::string& itemName);

void SavePlayerStatusItemStats(const std::string& itemName, const PlayerStatusItemStats& stats);

int PlayerStatusSlotLevelToIndex(const std::string& level);

PlayerStats ApplyPlayerStatusItems(const PlayerStats& baseStats);

PlayerStats MakeDefaultPlayerStats();

std::vector<std::string> GetPlayerAttackLevels();

std::string NormalizePlayerAttackName(const std::string& attackName);

PlayerAttackLevelStats MakeDefaultPlayerAttackLevelStats(const std::string& level);

PlayerAttackStats MakeDefaultPlayerAttackStats();

nlohmann::json PlayerAttackLevelStatsToJson(const PlayerAttackLevelStats& stats);

PlayerAttackLevelStats JsonToPlayerAttackLevelStats(const nlohmann::json& json, const PlayerAttackLevelStats& fallback);

nlohmann::json PlayerAttackStatsToJson(const PlayerAttackStats& stats);

PlayerAttackStats JsonToPlayerAttackStats(const nlohmann::json& json, const PlayerAttackStats& fallback);

nlohmann::json LoadPlayerAttackStatusRoot();

std::vector<std::string> LoadPlayerAttackNames();

PlayerAttackStats LoadPlayerAttackStats(const std::string& attackName);

void ApplyPlayerAttackSlots(PlayerAttackComponent* attack, const PlayerStats& playerStats);

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
