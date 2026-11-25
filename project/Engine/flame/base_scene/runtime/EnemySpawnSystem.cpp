#include "base_scene/runtime/EnemySpawnSystem.h"

#include "base_scene/runtime/SceneWorld.h"
#include "components/Enemy/EnemyComponent.h"
#include "components/Experience/ExperienceComponent.h"
#include "repositories/EnemyStatusRepository.h"

void EnemySpawnSystem::Reset() {
	activeBossObjectName_.clear();
}

void EnemySpawnSystem::BeginBossEncounter(const std::string& bossObjectName) {
	activeBossObjectName_ = bossObjectName;
}

EnemySpawnUpdateResult EnemySpawnSystem::Update(
	SceneWorld& world,
	bool isSelectionActive,
	bool hasPendingBossOffers) {
	EnemySpawnUpdateResult result;

	if (!activeBossObjectName_.empty()) {
		// ボス名から毎フレーム検索し直し、SceneWorld内の所有位置変更に依存しないようにする。
		Entity* bossObject = world.FindByName(activeBossObjectName_);
		EnemyComponent* bossEnemy = bossObject ? bossObject->GetComponent<EnemyComponent>() : nullptr;
		const bool bossIsAlive = bossEnemy && bossEnemy->IsEnabled() &&
			(bossEnemy->GetCurrentHealth() > 0.0f || !bossEnemy->IsDeathEffectFinished());
		// ボスの生存中は、通常のスポーン地点へボス戦中であることを通知する。
		for (const auto& object : world) {
			if (EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>()) {
				spawnPoint->SetBossEncounterActive(bossIsAlive);
			}
		}
		if (bossIsAlive) {
			return result;
		}
		// ボス撃破後も報酬の回収・適用が完了するまではクリア扱いにしない。
		for (const auto& object : world) {
			ExperienceComponent* reward = object->GetComponent<ExperienceComponent>();
			if (reward && reward->IsBossUpgradeReward() &&
				(!reward->IsCollected() || !reward->IsBossUpgradeApplied())) {
				return result;
			}
		}
		// 報酬選択画面または未表示の取得候補が残る間もステージ終了を待機する。
		if (isSelectionActive || hasPendingBossOffers) {
			return result;
		}
		activeBossObjectName_.clear();
		result.stageCleared = true;
		return result;
	}

	for (const auto& object : world) {
		EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>();
		if (!spawnPoint || !spawnPoint->IsEnabled()) {
			continue;
		}

		// ボス要求を最優先し、同じフレームに蓄積した通常敵要求は破棄する。
		if (spawnPoint->ConsumeBossEncounterRequest()) {
			result.hasBossEncounterRequest = true;
			result.bossEncounter.settings = spawnPoint->GetBossEncounterSettings();
			result.bossEncounter.player = spawnPoint->GetTarget();
			result.enemyRequests.clear();
			return result;
		}

		// スケジュール方式の地点では、時刻到達済みの要求をまとめて取り出す。
		if (!spawnPoint->GetSpawnSchedules().empty()) {
			for (const EnemySpawnPointComponent::ScheduledSpawnRequest& request :
				spawnPoint->ConsumeScheduledSpawnRequests()) {
				result.enemyRequests.push_back({
					request.enemyTypeName,
					request.position,
					spawnPoint->GetTarget(),
					request.statMultipliers});
			}
			continue;
		}

		// スケジュール未設定の地点は、従来の毎分生成数による方式で処理する。
		const std::string enemyTypeName = spawnPoint->GetEnemyTypeName();
		const EnemyStats stats = LoadEnemyStats(enemyTypeName);
		Vector3 spawnPosition{};
		if (spawnPoint->ConsumeSpawnRequest(stats.spawnsPerMinute, spawnPosition)) {
			result.enemyRequests.push_back({
				enemyTypeName,
				spawnPosition,
				spawnPoint->GetTarget(),
				spawnPoint->GetCurrentTimeScaling()});
		}
	}
	return result;
}
