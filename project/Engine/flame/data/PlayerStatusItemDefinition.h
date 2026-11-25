#pragma once

#include <array>
#include <string>
#include <unordered_map>

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
	std::string displayName = "龍の加護";
	/// <summary>選択肢やリザルトへ表示する名称です。未設定時は内部名へフォールバックします。</summary>
	const std::string& GetDisplayName() const {
		return displayName.empty() ? name : displayName;
	}
	PlayerStatusItemType type = PlayerStatusItemType::Attack;
	std::array<float, 5> levelAmounts{10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
	std::array<std::string, 5> levelDescriptions{};
	std::array<std::string, 5> levelTextureFilePaths{};
};

/// <summary>アイテム名からロード済み定義を検索するためのコレクションです。</summary>
using PlayerStatusItemDefinitions = std::unordered_map<std::string, PlayerStatusItemStats>;
