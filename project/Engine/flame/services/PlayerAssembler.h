#pragma once

#include "data/PlayerAttackDefinition.h"
#include "data/PlayerStatsData.h"

#include <string>

class PlayerAttackComponent;
class PlayerComponent;

/// <summary>
/// ロード済みのプレイヤー定義を、Playerと初期攻撃Componentへ一貫して反映します。
/// モデルやTransformなど生成・復元ごとの差分は呼び出し側が担当します。
/// </summary>
void ApplyPlayerDefinition(
	PlayerComponent* player,
	PlayerAttackComponent* attack,
	const std::string& playerTypeName,
	const PlayerStats& baseStats,
	const PlayerStats& effectiveStats,
	const PlayerAttackDefinitions& attackDefinitions);
