#include "services/PlayerAssembler.h"

#include "components/PlayerAttack/PlayerAttackComponent.h"
#include "services/PlayerAttackAssembler.h"
#include "../../../Player/PlayerComponent.h"

void ApplyPlayerDefinition(
	PlayerComponent* player,
	PlayerAttackComponent* attack,
	const std::string& playerTypeName,
	const PlayerStats& baseStats,
	const PlayerStats& effectiveStats,
	const PlayerAttackDefinitions& attackDefinitions) {
	if (!player || !attack) return;

	// タイプ、基礎能力、強化後能力、初期装備を同じ境界で更新し、部分適用を防ぐ。
	player->SetPlayerTypeName(playerTypeName);
	player->ApplyStats(baseStats, effectiveStats);
	ApplyPlayerAttackSlots(attack, baseStats, attackDefinitions);
}
