#include "base_scene/BaseScene.h"
#include "StringUtility.h"
#include "helpers/BaseSceneEditorGeometry.h"
#include "helpers/BaseSceneResourceCatalog.h"
#include "repositories/EnemyStatusRepository.h"
#include "repositories/ParticlePresetRepository.h"
#include "repositories/PlayerStatusRepository.h"

#include <array>

#ifdef USE_IMGUI
#include "../../../../../imgui/ImGuizmo.h"
#include <cctype>
#include <limits>
#endif
#include "base_scene/details/BaseSceneEditorDetails.h"

void BaseScene::DrawEnemySpawnPointInspector(GameObject* selectedObject) {
#ifdef USE_IMGUI
	if (EnemySpawnPointComponent* enemySpawnPoint = selectedObject->GetComponent<EnemySpawnPointComponent>()) {
		ImGui::Separator();
		ImGui::Text("EnemySpawnPointComponent");

		std::vector<std::string> targetLabels;
		targetLabels.push_back("Auto First Player");
		int currentTargetIndex = 0;
		for (const auto& object : sceneObjects_) {
			if (!object->GetComponent<Player>()) {
				continue;
			}
			targetLabels.push_back(object->GetName());
			if (enemySpawnPoint->GetTargetName() == object->GetName()) {
				currentTargetIndex = static_cast<int>(targetLabels.size()) - 1;
			}
		}
		std::vector<const char*> targetLabelPtrs = MakeLabelPointers(targetLabels);
		if (ImGui::Combo("Target Player", &currentTargetIndex, targetLabelPtrs.data(), static_cast<int>(targetLabelPtrs.size()))) {
			enemySpawnPoint->SetTargetName(currentTargetIndex == 0 ? "" : targetLabels[currentTargetIndex]);
			enemySpawnPoint->SetTarget(nullptr);
		}

		std::vector<std::string> cameraLabels;
		cameraLabels.push_back("Active Camera");
		int currentCameraIndex = 0;
		for (const auto& object : sceneObjects_) {
			if (!object->GetComponent<CameraComponent>()) {
				continue;
			}
			cameraLabels.push_back(object->GetName());
			if (enemySpawnPoint->GetCameraName() == object->GetName()) {
				currentCameraIndex = static_cast<int>(cameraLabels.size()) - 1;
			}
		}
		std::vector<const char*> cameraLabelPtrs = MakeLabelPointers(cameraLabels);
		if (ImGui::Combo("Reference Camera", &currentCameraIndex, cameraLabelPtrs.data(), static_cast<int>(cameraLabelPtrs.size()))) {
			enemySpawnPoint->SetCameraName(currentCameraIndex == 0 ? "" : cameraLabels[currentCameraIndex]);
			enemySpawnPoint->SetCamera(nullptr);
		}

		std::vector<std::string> enemyTypes = LoadEnemyTypeNames();
		int currentEnemyTypeIndex = 0;
		for (int index = 0; index < static_cast<int>(enemyTypes.size()); ++index) {
			if (enemyTypes[index] == enemySpawnPoint->GetEnemyTypeName()) {
				currentEnemyTypeIndex = index;
				break;
			}
		}
		if (!enemyTypes.empty()) {
			std::vector<const char*> enemyTypeLabels = MakeLabelPointers(enemyTypes);
			if (ImGui::Combo("Spawn Enemy Type", &currentEnemyTypeIndex, enemyTypeLabels.data(), static_cast<int>(enemyTypeLabels.size()))) {
				enemySpawnPoint->SetEnemyTypeName(enemyTypes[currentEnemyTypeIndex]);
				enemySpawnPoint->ResetSpawnTimer();
			}
		}
		bool spawnEnabled = enemySpawnPoint->GetSpawnEnabled();
		if (ImGui::Checkbox("Spawn Enabled", &spawnEnabled)) {
			enemySpawnPoint->SetSpawnEnabled(spawnEnabled);
			enemySpawnPoint->ResetSpawnTimer();
		}

		int spawnCount = enemySpawnPoint->GetSpawnCount();
		if (ImGui::DragInt("Spawn Count", &spawnCount, 1.0f, 1, 64)) {
			enemySpawnPoint->SetSpawnCount(spawnCount);
		}

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Spawn Schedules", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto schedules = enemySpawnPoint->GetSpawnSchedules();
			bool schedulesChanged = false;
			ImGui::Text("Elapsed Time: %.2f sec", enemySpawnPoint->GetElapsedTimeSeconds());
			if (ImGui::Button("Reset Schedule Time")) {
				enemySpawnPoint->ResetSpawnTimer();
			}
			ImGui::SameLine();
			if (ImGui::Button("Add Schedule")) {
				EnemySpawnPointComponent::SpawnSchedule schedule;
				schedule.enemyTypeName = !enemyTypes.empty() ? enemyTypes.front() : enemySpawnPoint->GetEnemyTypeName();
				schedules.push_back(schedule);
				schedulesChanged = true;
			}

			int removeScheduleIndex = -1;
			for (int scheduleIndex = 0; scheduleIndex < static_cast<int>(schedules.size()); ++scheduleIndex) {
				auto& schedule = schedules[scheduleIndex];
				ImGui::PushID(scheduleIndex);
				ImGui::Separator();
				ImGui::Text("Schedule %d", scheduleIndex + 1);

				if (!enemyTypes.empty()) {
					int scheduleEnemyTypeIndex = 0;
					for (int enemyTypeIndex = 0; enemyTypeIndex < static_cast<int>(enemyTypes.size()); ++enemyTypeIndex) {
						if (enemyTypes[enemyTypeIndex] == schedule.enemyTypeName) {
							scheduleEnemyTypeIndex = enemyTypeIndex;
							break;
						}
					}
					std::vector<const char*> scheduleEnemyTypeLabels = MakeLabelPointers(enemyTypes);
					if (ImGui::Combo("Enemy Type", &scheduleEnemyTypeIndex, scheduleEnemyTypeLabels.data(), static_cast<int>(scheduleEnemyTypeLabels.size()))) {
						schedule.enemyTypeName = enemyTypes[scheduleEnemyTypeIndex];
						schedulesChanged = true;
					}
				}

				if (ImGui::DragFloat("Start Time (sec)", &schedule.startTimeSeconds, 0.1f, 0.0f, 36000.0f)) {
					schedule.startTimeSeconds = (std::max)(0.0f, schedule.startTimeSeconds);
					schedule.endTimeSeconds = (std::max)(schedule.startTimeSeconds, schedule.endTimeSeconds);
					schedulesChanged = true;
				}
				if (ImGui::DragFloat("End Time (sec)", &schedule.endTimeSeconds, 0.1f, schedule.startTimeSeconds, 36000.0f)) {
					schedule.endTimeSeconds = (std::max)(schedule.startTimeSeconds, schedule.endTimeSeconds);
					schedulesChanged = true;
				}
				if (ImGui::DragInt("Interval (frames)", &schedule.spawnIntervalFrames, 1.0f, 1, 360000)) {
					schedule.spawnIntervalFrames = (std::max)(1, schedule.spawnIntervalFrames);
					schedulesChanged = true;
				}
				if (ImGui::DragInt("Amount Per Spawn", &schedule.spawnAmount, 1.0f, 1, 64)) {
					schedule.spawnAmount = std::clamp(schedule.spawnAmount, 1, 64);
					schedulesChanged = true;
				}
				if (ImGui::Checkbox("Apply Time Scaling", &schedule.applyTimeScaling)) {
					schedulesChanged = true;
				}
				// 中ボスのような時刻イベントは、一度だけ生成する設定にできる。
				if (ImGui::Checkbox("Spawn Once", &schedule.spawnOnce)) {
					schedulesChanged = true;
				}
				if (ImGui::Button("Remove Schedule")) {
					removeScheduleIndex = scheduleIndex;
				}
				ImGui::PopID();
			}

			if (removeScheduleIndex >= 0) {
				schedules.erase(schedules.begin() + removeScheduleIndex);
				schedulesChanged = true;
			}
			if (schedulesChanged) {
				enemySpawnPoint->SetSpawnSchedules(schedules);
			}

			if (schedules.empty()) {
				ImGui::TextDisabled("No schedules: legacy enemy Spawns Per Minute is used.");
			} else {
				ImGui::TextDisabled("Schedule mode overrides legacy Spawn Enemy Type / Spawns Per Minute.");
			}
		}

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Enemy Time Scaling", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::TextDisabled("Multipliers are fixed when an enabled schedule spawns an enemy.");
			auto tiers = enemySpawnPoint->GetTimeScalingTiers();
			if (ImGui::Button("Add Scaling Tier")) {
				EnemySpawnPointComponent::TimeScalingTier tier;
				if (!tiers.empty()) {
					tier = tiers.back();
					tier.startTimeSeconds += 60.0f;
				}
				tiers.push_back(tier);
			}

			int removeTierIndex = -1;
			bool tiersChanged = false;
			for (int tierIndex = 0; tierIndex < static_cast<int>(tiers.size()); ++tierIndex) {
				auto& tier = tiers[tierIndex];
				ImGui::PushID(10000 + tierIndex);
				ImGui::Separator();
				ImGui::Text("Tier %d", tierIndex + 1);
				tiersChanged |= ImGui::DragFloat("Start Time (sec)", &tier.startTimeSeconds, 1.0f, 0.0f, 36000.0f);
				tiersChanged |= ImGui::DragFloat("Health Multiplier", &tier.multipliers.healthMultiplier, 0.01f, 0.0f, 100.0f);
				tiersChanged |= ImGui::DragFloat("Speed Multiplier", &tier.multipliers.speedMultiplier, 0.01f, 0.0f, 100.0f);
				tiersChanged |= ImGui::DragFloat("Experience Multiplier", &tier.multipliers.experienceMultiplier, 0.01f, 0.0f, 100.0f);
				if (ImGui::Button("Remove Scaling Tier")) {
					removeTierIndex = tierIndex;
				}
				ImGui::PopID();
			}
			if (removeTierIndex >= 0) {
				tiers.erase(tiers.begin() + removeTierIndex);
				tiersChanged = true;
			}
			if (tiersChanged) {
				enemySpawnPoint->SetTimeScalingTiers(tiers);
			}
		}

		ImGui::Separator();
		// 最終ボス専用イベントの時刻、種類、出現座標、プレイヤーワープ座標を編集する。
		if (ImGui::CollapsingHeader("Boss Encounter", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto bossSettings = enemySpawnPoint->GetBossEncounterSettings();
			bool bossSettingsChanged = false;
			bossSettingsChanged |= ImGui::Checkbox("Enable Boss Encounter", &bossSettings.enabled);
			if (ImGui::DragFloat("Boss Trigger Time (sec)", &bossSettings.triggerTimeSeconds, 0.1f, 0.0f, 36000.0f)) {
				bossSettings.triggerTimeSeconds = (std::max)(0.0f, bossSettings.triggerTimeSeconds);
				bossSettingsChanged = true;
			}

			if (!enemyTypes.empty()) {
				int bossEnemyTypeIndex = 0;
				for (int enemyTypeIndex = 0; enemyTypeIndex < static_cast<int>(enemyTypes.size()); ++enemyTypeIndex) {
					if (enemyTypes[enemyTypeIndex] == bossSettings.enemyTypeName) {
						bossEnemyTypeIndex = enemyTypeIndex;
						break;
					}
				}
				std::vector<const char*> bossEnemyTypeLabels = MakeLabelPointers(enemyTypes);
				if (ImGui::Combo("Boss Enemy Type", &bossEnemyTypeIndex, bossEnemyTypeLabels.data(), static_cast<int>(bossEnemyTypeLabels.size()))) {
					bossSettings.enemyTypeName = enemyTypes[bossEnemyTypeIndex];
					bossSettingsChanged = true;
				}
			}
			bossSettingsChanged |= ImGui::DragFloat3("Boss Position", &bossSettings.bossPosition.x, 0.1f);
			bossSettingsChanged |= ImGui::DragFloat3("Player Warp Position", &bossSettings.playerWarpPosition.x, 0.1f);
			const char* bossState = enemySpawnPoint->IsBossEncounterActive()
			    ? "Active (normal spawn timer paused)"
			    : enemySpawnPoint->IsBossEncounterTriggered() ? "Finished" : "Waiting";
			ImGui::Text("State: %s", bossState);
			ImGui::TextDisabled("The player is warped immediately before the boss appears.");
			if (bossSettingsChanged) {
				enemySpawnPoint->SetBossEncounterSettings(bossSettings);
			}
		}

		float outerMargin = enemySpawnPoint->GetOuterMargin();
		if (ImGui::DragFloat("Outside Margin", &outerMargin, 0.1f, 0.0f, 1000.0f)) {
			enemySpawnPoint->SetOuterMargin(outerMargin);
		}
		float minimumRadius = enemySpawnPoint->GetMinimumRadius();
		if (ImGui::DragFloat("Minimum Radius", &minimumRadius, 0.1f, 0.0f, 1000.0f)) {
			enemySpawnPoint->SetMinimumRadius(minimumRadius);
		}
		float groundY = enemySpawnPoint->GetGroundY();
		if (ImGui::DragFloat("Ground Y", &groundY, 0.1f, -1000.0f, 1000.0f)) {
			enemySpawnPoint->SetGroundY(groundY);
		}
		float pointHeight = enemySpawnPoint->GetPointHeight();
		if (ImGui::DragFloat("Point Height", &pointHeight, 0.05f, -1000.0f, 1000.0f)) {
			enemySpawnPoint->SetPointHeight(pointHeight);
		}
		bool drawDebug = enemySpawnPoint->GetDrawDebug();
		if (ImGui::Checkbox("Draw Spawn Debug", &drawDebug)) {
			enemySpawnPoint->SetDrawDebug(drawDebug);
		}
		float debugPointSize = enemySpawnPoint->GetDebugPointSize();
		if (ImGui::DragFloat("Debug Point Size", &debugPointSize, 0.05f, 0.01f, 100.0f)) {
			enemySpawnPoint->SetDebugPointSize(debugPointSize);
		}
		ImGui::Text("Spawn Points: %d", static_cast<int>(enemySpawnPoint->GetSpawnPoints().size()));
	}
#else
	(void)selectedObject;
#endif
}

/// <summary>
/// パーティクルエミッターコンポーネントの編集UIを描画します。
/// </summary>
void BaseScene::DrawEnemyInspector() {
#ifdef USE_IMGUI
	if (!ImGui::Begin("Enemy Inspector")) {
		ImGui::End();
		return;
	}
	if (selectedObjectIndex_ < 0 || selectedObjectIndex_ >= static_cast<int>(sceneObjects_.size())) {
		ImGui::Text("No selected enemy");
		ImGui::End();
		return;
	}

	GameObject* selectedObject = sceneObjects_[selectedObjectIndex_].get();
	EnemyComponent* enemy = selectedObject->GetComponent<EnemyComponent>();
	if (!enemy) {
		ImGui::Text("Selected object has no Enemy component");
		ImGui::End();
		return;
	}

	auto updateTypeNameBuffer = [this](const std::string& typeName) {
		std::fill(enemyTypeNameBuffer_.begin(), enemyTypeNameBuffer_.end(), '\0');
		const size_t copyLength = (std::min)(typeName.size(), enemyTypeNameBuffer_.size() - 1);
		std::memcpy(enemyTypeNameBuffer_.data(), typeName.data(), copyLength);
	};
	if (enemyInspectorObjectName_ != selectedObject->GetName()) {
		enemyInspectorObjectName_ = selectedObject->GetName();
		updateTypeNameBuffer(enemy->GetEnemyTypeName());
	}

	ImGui::Text("%s", selectedObject->GetName().c_str());
	bool isEnabled = enemy->IsEnabled();
	if (ImGui::Checkbox("Enabled", &isEnabled)) {
		enemy->SetEnabled(isEnabled);
	}
	const float maxHealth = enemy->GetStats().health;
	const float healthRate = maxHealth > 0.0f ? enemy->GetCurrentHealth() / maxHealth : 0.0f;
	ImGui::Text("Current Health: %.1f / %.1f", enemy->GetCurrentHealth(), maxHealth);
	ImGui::ProgressBar(healthRate, ImVec2(-1.0f, 0.0f));
	ImGui::Separator();

	std::vector<std::string> targetLabels = {"Auto First Player"};
	int currentTargetIndex = 0;
	for (const auto& object : sceneObjects_) {
		if (!object->GetComponent<Player>()) {
			continue;
		}
		targetLabels.push_back(object->GetName());
		if (enemy->GetTargetName() == object->GetName()) {
			currentTargetIndex = static_cast<int>(targetLabels.size()) - 1;
		}
	}
	std::vector<const char*> targetLabelPointers = MakeLabelPointers(targetLabels);
	if (ImGui::Combo("Target Player", &currentTargetIndex, targetLabelPointers.data(), static_cast<int>(targetLabelPointers.size()))) {
		enemy->SetTargetName(currentTargetIndex == 0 ? "" : targetLabels[currentTargetIndex]);
		enemy->SetTarget(nullptr);
	}

	std::vector<std::string> enemyTypes = LoadEnemyTypeNames();
	int currentEnemyTypeIndex = 0;
	for (int index = 0; index < static_cast<int>(enemyTypes.size()); ++index) {
		if (enemyTypes[index] == enemy->GetEnemyTypeName()) {
			currentEnemyTypeIndex = index;
			break;
		}
	}
	if (!enemyTypes.empty()) {
		std::vector<const char*> enemyTypeLabels = MakeLabelPointers(enemyTypes);
		if (ImGui::Combo("Enemy Type", &currentEnemyTypeIndex, enemyTypeLabels.data(), static_cast<int>(enemyTypeLabels.size()))) {
			const std::string& selectedTypeName = enemyTypes[currentEnemyTypeIndex];
			enemy->SetEnemyTypeName(selectedTypeName);
			const EnemyStats selectedStats = LoadEnemyStats(selectedTypeName);
			enemy->ApplyStats(selectedStats);
			const float enemyScale = 0.75f * selectedStats.sizeScale;
			selectedObject->GetTransform().scale = {enemyScale, enemyScale, enemyScale};
			updateTypeNameBuffer(selectedTypeName);
		}
	}

	EnemyStats stats = enemy->GetStats();
	bool statsChanged = false;
	statsChanged |= ImGui::DragFloat("Health", &stats.health, 0.1f, 0.0f, 10000.0f);
	statsChanged |= ImGui::DragFloat("Attack", &stats.attack, 0.1f, 0.0f, 10000.0f);
	statsChanged |= ImGui::DragFloat("Speed", &stats.speed, 0.001f, 0.0f, 100.0f);
	if (ImGui::DragFloat("Size Scale", &stats.sizeScale, 0.05f, 0.1f, 10.0f)) {
		const float enemyScale = 0.75f * stats.sizeScale;
		selectedObject->GetTransform().scale = {enemyScale, enemyScale, enemyScale};
		statsChanged = true;
	}
	const char* behaviorLabels[] = {
		"Chase", "Shooter", "Charger", "Night Slash Boss", "Self Destruct", "Tornado Boss", "Burst Shooter"};
	int behaviorIndex = static_cast<int>(stats.behavior);
	if (ImGui::Combo("Behavior", &behaviorIndex, behaviorLabels, 7)) {
		stats.behavior = static_cast<EnemyBehaviorType>(behaviorIndex);
		stats.shoots = stats.behavior == EnemyBehaviorType::Shooter;
		statsChanged = true;
	}
	if (stats.behavior == EnemyBehaviorType::Shooter || stats.behavior == EnemyBehaviorType::BurstShooter) {
		statsChanged |= ImGui::DragFloat("Shoot Interval", &stats.shootingInterval, 0.01f, 0.05f, 1000.0f);
		statsChanged |= ImGui::DragFloat("Preferred Distance", &stats.preferredDistance, 0.1f, 0.0f, 1000.0f);
		statsChanged |= ImGui::DragFloat("Distance Tolerance", &stats.distanceTolerance, 0.1f, 0.0f, 1000.0f);
		statsChanged |= ImGui::DragFloat("Projectile Speed", &stats.projectileSpeed, 0.005f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Projectile Size", &stats.projectileSize, 0.01f, 0.01f, 100.0f);
		statsChanged |= ImGui::DragFloat("Projectile Life", &stats.projectileLifeTime, 0.1f, 0.0f, 1000.0f);
		if (stats.behavior == EnemyBehaviorType::BurstShooter) {
			statsChanged |= ImGui::DragInt("Burst Shot Count", &stats.burstShotCount, 1.0f, 1, 31);
			statsChanged |= ImGui::SliderAngle("Burst Shot Spacing", &stats.burstSpreadAngle, 0.0f, 45.0f);
		}
	} else if (stats.behavior == EnemyBehaviorType::Charger) {
		statsChanged |= ImGui::DragFloat("Charge Trigger Distance", &stats.chargeTriggerDistance, 0.1f, 0.0f, 1000.0f);
		statsChanged |= ImGui::DragFloat("Charge Warning Time", &stats.chargeDuration, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Dash Speed", &stats.dashSpeed, 0.005f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Dash Duration", &stats.dashDuration, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Dash Recovery", &stats.dashRecovery, 0.05f, 0.0f, 100.0f);
	} else if (stats.behavior == EnemyBehaviorType::SelfDestruct) {
		statsChanged |= ImGui::DragFloat("Explosion Trigger Distance", &stats.selfDestructTriggerDistance, 0.1f, 0.0f, 1000.0f);
		statsChanged |= ImGui::DragFloat("Explosion Fuse Time", &stats.selfDestructFuseDuration, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Explosion Radius", &stats.selfDestructRadius, 0.1f, 0.0f, 1000.0f);
	} else if (stats.behavior == EnemyBehaviorType::TornadoBoss) {
		statsChanged |= ImGui::DragFloat("Tornado Trigger Distance", &stats.bossTornadoTriggerDistance, 0.1f, 0.0f, 1000.0f);
		statsChanged |= ImGui::DragFloat("Tornado Warning Time", &stats.bossTornadoWindup, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Tornado Recovery Time", &stats.bossTornadoRecovery, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragInt("Tornado Count", &stats.bossTornadoCount, 1.0f, 1, 16);
		statsChanged |= ImGui::DragFloat("Tornado Initial Radius", &stats.bossTornadoInitialRadius, 0.1f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Tornado Angular Speed", &stats.bossTornadoAngularSpeed, 0.1f, -20.0f, 20.0f);
		statsChanged |= ImGui::DragFloat("Tornado Radial Speed", &stats.bossTornadoRadialSpeed, 0.005f, 0.0f, 5.0f);
		statsChanged |= ImGui::DragFloat("Tornado Size", &stats.bossTornadoSize, 0.05f, 0.01f, 10.0f);
		statsChanged |= ImGui::DragFloat("Tornado Life Time", &stats.bossTornadoLifeTime, 0.1f, 0.0f, 30.0f);
		statsChanged |= ImGui::DragFloat("Tornado Attack Multiplier", &stats.bossTornadoAttackMultiplier, 0.05f, 0.0f, 10.0f);
		ImGui::SeparatorText("Converging Tornado Pattern");
		statsChanged |= ImGui::DragInt("Converging Tornado Count", &stats.bossConvergingTornadoCount, 1.0f, 1, 24);
		statsChanged |= ImGui::DragFloat("Converging Initial Radius", &stats.bossConvergingTornadoInitialRadius, 0.1f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Converging Angular Speed", &stats.bossConvergingTornadoAngularSpeed, 0.1f, -20.0f, 20.0f);
		statsChanged |= ImGui::DragFloat("Converging Radial Speed", &stats.bossConvergingTornadoRadialSpeed, 0.005f, 0.0f, 5.0f);
		statsChanged |= ImGui::DragFloat("Converging Tornado Size", &stats.bossConvergingTornadoSize, 0.05f, 0.01f, 10.0f);
		statsChanged |= ImGui::DragFloat("Converging Life Time", &stats.bossConvergingTornadoLifeTime, 0.1f, 0.0f, 30.0f);
		statsChanged |= ImGui::DragFloat("Converging Attack Multiplier", &stats.bossConvergingTornadoAttackMultiplier, 0.05f, 0.0f, 10.0f);
		ImGui::SeparatorText("Giant Homing Tornado Pattern");
		statsChanged |= ImGui::DragFloat("Giant Tornado Speed", &stats.bossGiantTornadoSpeed, 0.005f, 0.0f, 5.0f);
		statsChanged |= ImGui::DragFloat("Giant Tornado Size", &stats.bossGiantTornadoSize, 0.1f, 0.01f, 20.0f);
		statsChanged |= ImGui::DragFloat("Giant Tornado Life Time", &stats.bossGiantTornadoLifeTime, 0.1f, 0.0f, 30.0f);
		statsChanged |= ImGui::DragFloat("Giant Tornado Attack Multiplier", &stats.bossGiantTornadoAttackMultiplier, 0.05f, 0.0f, 10.0f);
		statsChanged |= ImGui::DragFloat("Giant Tornado Spawn Offset", &stats.bossGiantTornadoSpawnOffset, 0.1f, 0.0f, 100.0f);
	} else if (stats.behavior == EnemyBehaviorType::NightSlashBoss) {
		// 連続斬りの開始距離、予兆、移動、切り返し、フィニッシュを個別に調整する。
		statsChanged |= ImGui::DragFloat("Combo Trigger Distance", &stats.comboTriggerDistance, 0.1f, 0.0f, 1000.0f);
		statsChanged |= ImGui::DragFloat("Combo Warning Time", &stats.comboWindup, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Combo Dash Speed", &stats.comboDashSpeed, 0.005f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Combo Dash Duration", &stats.comboDashDuration, 0.01f, 0.01f, 100.0f);
		statsChanged |= ImGui::DragFloat("Slash Pause", &stats.comboSlashPause, 0.01f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Combo Recovery", &stats.comboRecovery, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragFloat("Side Offset", &stats.comboSideOffset, 0.05f, 0.0f, 100.0f);
		statsChanged |= ImGui::DragInt("Dash Count", &stats.comboDashCount, 1.0f, 1, 12);
		statsChanged |= ImGui::DragFloat("Finisher Speed Multiplier", &stats.finisherSpeedMultiplier, 0.05f, 1.0f, 5.0f);
		// 連続斬り後に循環する全方位弾幕と扇状弾幕の共通パラメーター。
		ImGui::SeparatorText("Ranged Patterns");
		statsChanged |= ImGui::DragFloat("Ranged Warning Time", &stats.bossRangedWindup, 0.05f, 0.0f, 10.0f);
		statsChanged |= ImGui::DragFloat("Ranged Wave Interval", &stats.bossRangedInterval, 0.01f, 0.01f, 10.0f);
		statsChanged |= ImGui::DragInt("Ranged Wave Count", &stats.bossRangedWaves, 1.0f, 1, 20);
		statsChanged |= ImGui::DragInt("Radial Shot Count", &stats.bossRadialShotCount, 1.0f, 1, 64);
		statsChanged |= ImGui::DragInt("Aimed Fan Shot Count", &stats.bossAimedShotCount, 1.0f, 1, 31);
		statsChanged |= ImGui::SliderAngle("Aimed Shot Spacing", &stats.bossAimedSpreadAngle, 0.0f, 45.0f);
		statsChanged |= ImGui::DragFloat("Projectile Attack Multiplier", &stats.bossProjectileAttackMultiplier, 0.05f, 0.0f, 5.0f);
		statsChanged |= ImGui::DragFloat("Boss Projectile Speed", &stats.projectileSpeed, 0.005f, 0.0f, 5.0f);
		statsChanged |= ImGui::DragFloat("Boss Projectile Size", &stats.projectileSize, 0.01f, 0.01f, 5.0f);
		statsChanged |= ImGui::DragFloat("Boss Projectile Life Time", &stats.projectileLifeTime, 0.1f, 0.0f, 30.0f);
	}
	statsChanged |= ImGui::DragFloat("Spawns Per Minute", &stats.spawnsPerMinute, 0.1f, 0.0f, 10000.0f);
	statsChanged |= ImGui::DragInt("Drop Experience", &stats.experience, 1.0f, 0, 100000);
	statsChanged |= ImGui::SliderFloat("Health Item Drop Chance", &stats.healthItemDropChance, 0.0f, 1.0f, "%.2f");
	statsChanged |= ImGui::SliderFloat("EXP Collector Drop Chance", &stats.collectExperienceItemDropChance, 0.0f, 1.0f, "%.2f");
	statsChanged |= ImGui::DragFloat("Health Item Heal Amount", &stats.healthItemHealAmount, 1.0f, 0.0f, 100000.0f);

	const std::vector<std::string> experienceModels = CollectAllLoadedModelNames();
	if (!experienceModels.empty()) {
		int experienceModelIndex = 0;
		for (int index = 0; index < static_cast<int>(experienceModels.size()); ++index) {
			if (experienceModels[index] == stats.experienceModelFilePath) {
				experienceModelIndex = index;
				break;
			}
		}
		std::vector<std::string> experienceModelLabels;
		experienceModelLabels.reserve(experienceModels.size());
		for (const std::string& modelName : experienceModels) {
			Model* model = ModelManager::GetInstance()->FindModel(modelName);
			experienceModelLabels.push_back(model && model->GetIsAnimation() ? "[Anim] " + modelName : "[Model] " + modelName);
		}
		std::vector<const char*> experienceModelLabelPointers = MakeLabelPointers(experienceModelLabels);
		if (ImGui::Combo("Experience Model", &experienceModelIndex, experienceModelLabelPointers.data(), static_cast<int>(experienceModelLabelPointers.size()))) {
			stats.experienceModelFilePath = experienceModels[experienceModelIndex];
			statsChanged = true;
		}
	}
	if (statsChanged) {
		enemy->ApplyStats(stats);
	}

	float currentHealth = enemy->GetCurrentHealth();
	if (ImGui::DragFloat("Edit Current Health", &currentHealth, 0.1f, 0.0f, stats.health)) {
		enemy->SetCurrentHealth(currentHealth);
	}

	ImGui::Separator();
	ImGui::InputText("Status Name", enemyTypeNameBuffer_.data(), enemyTypeNameBuffer_.size());
	if (ImGui::Button("Save Enemy Type")) {
		const std::string saveTypeName = enemyTypeNameBuffer_.data();
		if (!saveTypeName.empty()) {
			enemy->SetEnemyTypeName(saveTypeName);
		}
		SaveEnemyStats(enemy->GetEnemyTypeName(), enemy->GetStats());
	}

	ImGui::End();
#endif
}


