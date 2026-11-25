#pragma once

#include "data/PlayerAttackDefinition.h"
#include "data/PlayerStatsData.h"

class PlayerAttackComponent;

/// <summary>
/// プレイヤーの装備スロットとロード済み攻撃定義を、実行時Componentへ反映します。
/// JSONの読込は行わず、RepositoryとComponentの直接依存を仲介します。
/// </summary>
void ApplyPlayerAttackSlots(
	PlayerAttackComponent* attack,
	const PlayerStats& playerStats,
	const PlayerAttackDefinitions& definitions);
