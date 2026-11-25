#include "services/PlayerStatsCalculator.h"

#include <algorithm>
#include <cstdlib>

namespace {
int StatusLevelToIndex(const std::string& level) {
	const int parsedLevel = (std::max)(1, std::atoi(level.c_str()));
	return (std::min)(4, parsedLevel - 1);
}
}

PlayerStats ApplyPlayerStatusItems(
	const PlayerStats& baseStats,
	const PlayerStatusItemDefinitions& definitions) {
	PlayerStats effectiveStats = baseStats;
	for (const PlayerStatusSlot& slot : baseStats.statusSlots) {
		if (!slot.enabled || slot.statusName.empty()) {
			continue;
		}
		const auto definition = definitions.find(slot.statusName);
		if (definition == definitions.end()) {
			// 未知の名前は能力を変えず、破損データから既定アイテムを誤適用しない。
			continue;
		}
		const PlayerStatusItemStats& itemStats = definition->second;
		const float amount = itemStats.levelAmounts[StatusLevelToIndex(slot.level)];
		switch (itemStats.type) {
		case PlayerStatusItemType::Attack: effectiveStats.attack += amount; break;
		case PlayerStatusItemType::Health: effectiveStats.health += amount; break;
		case PlayerStatusItemType::AttackSpeed: effectiveStats.attackSpeed += amount; break;
		case PlayerStatusItemType::Speed: effectiveStats.speed += amount; break;
		case PlayerStatusItemType::Defense: effectiveStats.defense += amount; break;
		case PlayerStatusItemType::AttackSize: effectiveStats.attackSize += amount; break;
		case PlayerStatusItemType::Experience: effectiveStats.experienceCorrection += amount; break;
		}
	}
	return effectiveStats;
}
