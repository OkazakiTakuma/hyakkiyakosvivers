#include "services/PlayerAttackAssembler.h"

#include "components/PlayerAttack/PlayerAttackComponent.h"

void ApplyPlayerAttackSlots(
	PlayerAttackComponent* attack,
	const PlayerStats& playerStats,
	const PlayerAttackDefinitions& definitions) {
	if (!attack) {
		return;
	}
	attack->ClearAttackSlots();
	for (const PlayerAttackSlot& slot : playerStats.attackSlots) {
		if (slot.attackName.empty()) {
			continue;
		}
		auto definition = definitions.find(slot.attackName);
		if (definition == definitions.end()) {
			// 旧データの未登録名はStraightへ戻し、実行時スロットを有効な定義に保つ。
			definition = definitions.find("Straight");
		}
		const PlayerAttackStats fallback;
		const PlayerAttackStats& stats = definition != definitions.end()
			? definition->second
			: fallback;
		attack->AddAttackSlot(stats, slot.attackLevel, slot.enabled);
	}
}
