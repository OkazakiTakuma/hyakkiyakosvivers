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

void BaseScene::DrawPlayerInspector() {
#ifdef USE_IMGUI
	if (!ImGui::Begin("Player Inspector")) {
		ImGui::End();
		return;
	}
	if (selectedObjectIndex_ < 0 || selectedObjectIndex_ >= static_cast<int>(sceneObjects_.size())) {
		ImGui::Text("No selected player");
		ImGui::End();
		return;
	}

	GameObject* selectedObject = sceneObjects_[selectedObjectIndex_].get();
	Player* player = selectedObject->GetComponent<Player>();
	if (!player) {
		ImGui::Text("Selected object has no Player component");
		ImGui::End();
		return;
	}

	ImGui::Text("%s", selectedObject->GetName().c_str());
	const float currentHealth = player->GetCurrentHealth();
	const float maxHealth = player->GetMaxHealth();
	const float healthRate = maxHealth > 0.0f ? currentHealth / maxHealth : 0.0f;
	ImGui::Text("Current Health: %.1f / %.1f", currentHealth, maxHealth);
	ImGui::ProgressBar(healthRate, ImVec2(-1.0f, 0.0f));
	ImGui::Separator();

	std::vector<std::string> playerTypes = LoadPlayerTypeNames();
	int currentPlayerTypeIndex = 0;
	for (int index = 0; index < static_cast<int>(playerTypes.size()); ++index) {
		if (playerTypes[index] == player->GetPlayerTypeName()) {
			currentPlayerTypeIndex = index;
			break;
		}
	}
	if (!playerTypes.empty()) {
		std::vector<const char*> playerTypeLabels = MakeLabelPointers(playerTypes);
		if (ImGui::Combo("Player Type", &currentPlayerTypeIndex, playerTypeLabels.data(), static_cast<int>(playerTypeLabels.size()))) {
			const std::string& selectedTypeName = playerTypes[currentPlayerTypeIndex];
			player->SetPlayerTypeName(selectedTypeName);
			std::fill(playerTypeNameBuffer_.begin(), playerTypeNameBuffer_.end(), '\0');
			const size_t copyLength = (std::min)(selectedTypeName.size(), playerTypeNameBuffer_.size() - 1);
			std::memcpy(playerTypeNameBuffer_.data(), selectedTypeName.data(), copyLength);
			PlayerStats stats = LoadPlayerStats(selectedTypeName);
			player->ApplyStats(stats, ApplyPlayerStatusItems(stats));
			selectedObject->GetTransform().scale = {stats.sizeScale, stats.sizeScale, stats.sizeScale};
			if (PlayerAttackComponent* attack = selectedObject->GetComponent<PlayerAttackComponent>()) {
				ApplyPlayerAttackSlots(attack, stats);
			}
			if (Object3dComponent* object3dComponent = selectedObject->GetComponent<Object3dComponent>()) {
				if (!stats.modelFilePath.empty() && ModelManager::GetInstance()->FindModel(stats.modelFilePath)) {
					object3dComponent->SetModel(stats.modelFilePath);
					object3dComponent->SetDrawSkeleton(stats.isAnimationModel);
				}
			}
		}
	}

	Vector3 spawnPoint = player->GetSpawnPoint();
	if (ImGui::DragFloat3("Spawn Point", &spawnPoint.x, 0.1f)) {
		player->SetSpawnPoint(spawnPoint);
	}
	if (ImGui::Button("Set Spawn To Current")) {
		player->SetSpawnPoint(selectedObject->GetTransform().translate);
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset To Spawn")) {
		player->ResetToSpawnPoint();
	}
	if (ImGui::Button("Save Spawn Point")) {
		SaveEditorObjects();
	}

	DrawPlayerStatsInspector(selectedObject, player);

	ImGui::End();
#endif
}

void BaseScene::DrawPlayerStatsInspector(GameObject* selectedObject, Player* player) {
#ifdef USE_IMGUI
	PlayerStats stats = player->GetBaseStats();
	bool statsChanged = false;
	bool statusSlotsChanged = false;
	if (playerTypeNameBuffer_[0] == '\0') {
		const std::string& currentTypeName = player->GetPlayerTypeName();
		const size_t copyLength = (std::min)(currentTypeName.size(), playerTypeNameBuffer_.size() - 1);
		std::memcpy(playerTypeNameBuffer_.data(), currentTypeName.data(), copyLength);
		playerTypeNameBuffer_[copyLength] = '\0';
	}
	ImGui::InputText("Player Type Name", playerTypeNameBuffer_.data(), playerTypeNameBuffer_.size());
	statsChanged |= ImGui::DragFloat("Base Health", &stats.baseHealth, 1.0f, 0.0f, 100000.0f);
	statsChanged |= ImGui::DragFloat("Health %", &stats.health, 1.0f, 0.0f, 100000.0f);
	statsChanged |= ImGui::DragFloat("Attack", &stats.attack, 1.0f, 0.0f, 100000.0f);
	statsChanged |= ImGui::DragFloat("Defense", &stats.defense, 1.0f, 0.0f, 10000.0f);
	statsChanged |= ImGui::DragFloat("Base Speed", &stats.baseSpeed, 0.001f, 0.0f, 100.0f);
	statsChanged |= ImGui::DragFloat("Speed %", &stats.speed, 1.0f, 0.0f, 100000.0f);
	statsChanged |= ImGui::DragFloat("Attack Speed", &stats.attackSpeed, 1.0f, 0.0f, 100000.0f);
	statsChanged |= ImGui::DragFloat("Attack Size", &stats.attackSize, 1.0f, 0.0f, 100000.0f);
	if (ImGui::DragFloat("Size Scale", &stats.sizeScale, 0.05f, 0.1f, 10.0f)) {
		selectedObject->GetTransform().scale = {stats.sizeScale, stats.sizeScale, stats.sizeScale};
		statsChanged = true;
	}
	statsChanged |= ImGui::DragFloat("Damage Invincibility (sec)", &stats.damageInvincibilityDuration, 0.05f, 0.0f, 60.0f);
	statsChanged |= ImGui::DragInt("Level", &stats.level, 1.0f, 1, 100000);
	statsChanged |= ImGui::DragInt("Experience", &stats.experience, 1.0f, 0, 100000000);
	const int nextLevelExperience = Player::GetRequiredExperienceForNextLevel(stats.level);
	if (nextLevelExperience == (std::numeric_limits<int>::max)()) {
		ImGui::Text("Next Level EXP: Max");
	} else {
		ImGui::Text("Next Level EXP: %d / %d", stats.experience, nextLevelExperience);
	}
	statsChanged |= ImGui::DragFloat("Experience Correction %", &stats.experienceCorrection, 1.0f, 0.0f, 100000.0f);
	ImGui::Text("Max Health: %.1f", stats.baseHealth * (stats.health / 100.0f));
	ImGui::Text("Effective Speed: %.3f", stats.baseSpeed * (stats.speed / 100.0f));
	float editedCurrentHealth = player->GetCurrentHealth();
	if (ImGui::DragFloat("Edit Current Health", &editedCurrentHealth, 1.0f, 0.0f, player->GetMaxHealth())) {
		player->SetCurrentHealth(editedCurrentHealth);
	}
	if (!isPlayerAttackCacheLoaded_) {
		ReloadPlayerAttackInspectorCache();
	}
	std::vector<std::string> attackLevels = GetPlayerAttackLevels();
	const std::vector<std::string>& attackNames = cachedPlayerAttackNames_;
	if (!attackNames.empty()) {
		std::vector<std::string> attackSlotLabels;
		attackSlotLabels.reserve(attackNames.size() + 1);
		attackSlotLabels.push_back("None");
		attackSlotLabels.insert(attackSlotLabels.end(), attackNames.begin(), attackNames.end());
		std::vector<const char*> attackLabels = MakeLabelPointers(attackSlotLabels);
		std::vector<const char*> attackLevelLabels = MakeLabelPointers(attackLevels);
		if (ImGui::CollapsingHeader("Attack Slots", ImGuiTreeNodeFlags_DefaultOpen)) {
			for (int slotIndex = 0; slotIndex < static_cast<int>(stats.attackSlots.size()); ++slotIndex) {
				ImGui::PushID(slotIndex);
				PlayerAttackSlot& slot = stats.attackSlots[slotIndex];
				ImGui::Separator();
				ImGui::Text("Slot %d", slotIndex + 1);
				if (ImGui::Checkbox("Enabled", &slot.enabled)) {
					statsChanged = true;
				}
				int attackIndex = 0;
				for (int index = 0; index < static_cast<int>(attackNames.size()); ++index) {
					if (attackNames[index] == slot.attackName) {
						attackIndex = index + 1;
						break;
					}
				}
				if (ImGui::Combo("Attack", &attackIndex, attackLabels.data(), static_cast<int>(attackLabels.size()))) {
					slot.attackName = attackIndex <= 0 ? "" : attackNames[attackIndex - 1];
					if (slot.attackName.empty()) {
						slot.enabled = false;
					}
					statsChanged = true;
				}
				int attackLevelIndex = 0;
				for (int index = 0; index < static_cast<int>(attackLevels.size()); ++index) {
					if (attackLevels[index] == slot.attackLevel) {
						attackLevelIndex = index;
						break;
					}
				}
				if (ImGui::Combo("Level", &attackLevelIndex, attackLevelLabels.data(), static_cast<int>(attackLevelLabels.size()))) {
					slot.attackLevel = attackLevels[attackLevelIndex];
					statsChanged = true;
				}
				ImGui::PopID();
			}
		}
	}

	if (ImGui::CollapsingHeader("Status Slots", ImGuiTreeNodeFlags_DefaultOpen)) {
		std::vector<std::string> statusItemNames = LoadPlayerStatusItemNames();
		std::vector<std::string> statusSlotLabels;
		statusSlotLabels.reserve(statusItemNames.size() + 1);
		statusSlotLabels.push_back("None");
		statusSlotLabels.insert(statusSlotLabels.end(), statusItemNames.begin(), statusItemNames.end());
		std::vector<const char*> statusLabels = MakeLabelPointers(statusSlotLabels);
		std::vector<std::string> statusLevels = GetPlayerStatusItemLevels();
		std::vector<const char*> statusLevelLabels = MakeLabelPointers(statusLevels);
		for (int slotIndex = 0; slotIndex < static_cast<int>(stats.statusSlots.size()); ++slotIndex) {
			ImGui::PushID(1000 + slotIndex);
			PlayerStatusSlot& slot = stats.statusSlots[slotIndex];
			ImGui::Separator();
			ImGui::Text("Status Slot %d", slotIndex + 1);
			if (ImGui::Checkbox("Enabled", &slot.enabled)) {
				if (slot.statusName.empty()) {
					slot.enabled = false;
				}
				statsChanged = true;
				statusSlotsChanged = true;
			}

			int statusIndex = 0;
			for (int index = 0; index < static_cast<int>(statusItemNames.size()); ++index) {
				if (statusItemNames[index] == slot.statusName) {
					statusIndex = index + 1;
					break;
				}
			}
			if (ImGui::Combo("Status Item", &statusIndex, statusLabels.data(), static_cast<int>(statusLabels.size()))) {
				slot.statusName = statusIndex <= 0 ? "" : statusItemNames[statusIndex - 1];
				if (slot.statusName.empty()) {
					slot.enabled = false;
				} else {
					slot.enabled = true;
				}
				statsChanged = true;
				statusSlotsChanged = true;
			}
			int statusLevelIndex = 0;
			for (int index = 0; index < static_cast<int>(statusLevels.size()); ++index) {
				if (statusLevels[index] == slot.level) {
					statusLevelIndex = index;
					break;
				}
			}
			if (ImGui::Combo("Level", &statusLevelIndex, statusLevelLabels.data(), static_cast<int>(statusLevelLabels.size()))) {
				slot.level = statusLevels[statusLevelIndex];
				if (!slot.statusName.empty()) {
					slot.enabled = true;
				}
				statsChanged = true;
				statusSlotsChanged = true;
			}
			ImGui::PopID();
		}
	}
	const PlayerStats appliedStatusStats = ApplyPlayerStatusItems(stats);
	if (ImGui::CollapsingHeader("Applied Status", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Base Health: %.1f", appliedStatusStats.baseHealth);
		ImGui::Text("Health %%: %.1f", appliedStatusStats.health);
		ImGui::Text("Max Health: %.1f", appliedStatusStats.baseHealth * (appliedStatusStats.health / 100.0f));
		ImGui::Text("Attack: %.1f", appliedStatusStats.attack);
		ImGui::Text("Defense: %.1f", appliedStatusStats.defense);
		ImGui::Text("Base Speed: %.3f", appliedStatusStats.baseSpeed);
		ImGui::Text("Speed %%: %.1f", appliedStatusStats.speed);
		ImGui::Text("Effective Speed: %.3f", appliedStatusStats.baseSpeed * (appliedStatusStats.speed / 100.0f));
		ImGui::Text("Attack Speed: %.1f", appliedStatusStats.attackSpeed);
		ImGui::Text("Attack Size: %.1f", appliedStatusStats.attackSize);
		ImGui::Text("Size Scale: %.2f", appliedStatusStats.sizeScale);
		ImGui::Text("Damage Invincibility: %.2f sec", appliedStatusStats.damageInvincibilityDuration);
		ImGui::Text("Experience Correction %%: %.1f", appliedStatusStats.experienceCorrection);
	}

	DrawPlayerPersistenceInspector(selectedObject, player, stats, statsChanged, statusSlotsChanged);
#else
	(void)selectedObject;
	(void)player;
#endif
}

void BaseScene::DrawPlayerPersistenceInspector(GameObject* selectedObject, Player* player, PlayerStats& stats, bool& statsChanged, bool statusSlotsChanged) {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("Status Item Settings")) {
		static int selectedStatusItemIndex = 0;
		static std::array<char, 128> statusItemNameBuffer{};
		static PlayerStatusItemStats editingStatusItemStats{};
		static std::string editingStatusItemName;
		static bool isEditingStatusItemLoaded = false;
		std::vector<std::string> statusItemNames = LoadPlayerStatusItemNames();
		if (selectedStatusItemIndex >= static_cast<int>(statusItemNames.size())) {
			selectedStatusItemIndex = 0;
		}
		std::vector<const char*> statusItemLabels = MakeLabelPointers(statusItemNames);
		if (!statusItemLabels.empty() && ImGui::Combo("Item", &selectedStatusItemIndex, statusItemLabels.data(), static_cast<int>(statusItemLabels.size()))) {
			std::fill(statusItemNameBuffer.begin(), statusItemNameBuffer.end(), '\0');
			isEditingStatusItemLoaded = false;
		}

		const std::string selectedItemName = statusItemNames.empty() ? "AttackUp" : statusItemNames[selectedStatusItemIndex];
		if (!isEditingStatusItemLoaded || editingStatusItemName != selectedItemName) {
			editingStatusItemStats = LoadPlayerStatusItemStats(selectedItemName);
			editingStatusItemName = selectedItemName;
			isEditingStatusItemLoaded = true;
		}
		if (statusItemNameBuffer[0] == '\0') {
			const size_t copyLength = (std::min)(editingStatusItemStats.name.size(), statusItemNameBuffer.size() - 1);
			std::memcpy(statusItemNameBuffer.data(), editingStatusItemStats.name.data(), copyLength);
			statusItemNameBuffer[copyLength] = '\0';
		}
		ImGui::InputText("Item Name", statusItemNameBuffer.data(), statusItemNameBuffer.size());

		const char* typeLabels[] = {"Attack", "HP", "AttackSpeed", "Speed", "Defense", "AttackSize", "Experience"};
		int typeIndex = static_cast<int>(editingStatusItemStats.type);
		if (ImGui::Combo("Type", &typeIndex, typeLabels, _countof(typeLabels))) {
			editingStatusItemStats.type = static_cast<PlayerStatusItemType>(typeIndex);
		}
		for (int levelIndex = 0; levelIndex < static_cast<int>(editingStatusItemStats.levelAmounts.size()); ++levelIndex) {
			// 効果量と同じレベル単位で、選択カードの文章・画像も編集する。
			ImGui::PushID(2000 + levelIndex);
			ImGui::DragFloat(("Lv" + std::to_string(levelIndex + 1) + " Amount").c_str(), &editingStatusItemStats.levelAmounts[levelIndex], 1.0f, 0.0f, 100000.0f);
			InputTextMultilineString(("Lv" + std::to_string(levelIndex + 1) + " Selection Text").c_str(), editingStatusItemStats.levelDescriptions[levelIndex]);
			SelectionTextureCombo(("Lv" + std::to_string(levelIndex + 1) + " Selection Texture").c_str(), editingStatusItemStats.levelTextureFilePaths[levelIndex]);
			ImGui::PopID();
		}
		if (ImGui::Button("Save Status Item")) {
			const std::string saveItemName = statusItemNameBuffer.data();
			editingStatusItemStats.name = saveItemName.empty() ? selectedItemName : saveItemName;
			SavePlayerStatusItemStats(editingStatusItemStats.name, editingStatusItemStats);
			// 保存直後の画像を右上HUDへ反映するため、ステータス画像キャッシュを破棄する。
			playerStatusSlotTextureKeys_.fill({});
			playerStatusSlotTexturePaths_.fill({});
			player->ApplyStats(stats, ApplyPlayerStatusItems(stats));
			std::fill(statusItemNameBuffer.begin(), statusItemNameBuffer.end(), '\0');
			isEditingStatusItemLoaded = false;
		}
	}

	const std::vector<std::string> loadedModels = CollectAllLoadedModelNames();
	if (!loadedModels.empty()) {
		// 登録済みモデルから選択し、設定値とシーン上の表示モデルを同時に更新する。
		int modelIndex = 0;
		for (int index = 0; index < static_cast<int>(loadedModels.size()); index++) {
			if (loadedModels[index] == stats.modelFilePath) {
				modelIndex = index;
				break;
			}
		}

		std::vector<std::string> modelLabels;
		modelLabels.reserve(loadedModels.size());
		for (const std::string& modelName : loadedModels) {
			Model* model = ModelManager::GetInstance()->FindModel(modelName);
			modelLabels.push_back(model && model->GetIsAnimation() ? "[Anim] " + modelName : "[Model] " + modelName);
		}
		std::vector<const char*> loadedModelLabels = MakeLabelPointers(modelLabels);
		if (ImGui::Combo("Player Model", &modelIndex, loadedModelLabels.data(), static_cast<int>(loadedModelLabels.size()))) {
			const std::string& modelFilePath = loadedModels[modelIndex];
			Model* model = ModelManager::GetInstance()->FindModel(modelFilePath);
			const bool isAnimationModel = model && model->GetIsAnimation();
			stats.modelFilePath = modelFilePath;
			stats.isAnimationModel = isAnimationModel;
			statsChanged = true;
			if (Object3dComponent* object3dComponent = selectedObject->GetComponent<Object3dComponent>()) {
				object3dComponent->SetModel(modelFilePath);
				object3dComponent->SetDrawSkeleton(isAnimationModel);
			}
		}
		ImGui::Text("Current Player Model: %s", stats.modelFilePath.empty() ? "None" : stats.modelFilePath.c_str());
	} else {
		ImGui::Text("No loaded models");
	}

	if (statsChanged) {
		// ステータススロットはJSONへ即時保存せず、現在シーン上のプレイヤーにだけ反映する。
		player->ApplyStats(stats, ApplyPlayerStatusItems(stats));
		if (PlayerAttackComponent* attack = selectedObject->GetComponent<PlayerAttackComponent>()) {
			ApplyPlayerAttackSlots(attack, stats);
		}
	}
	if (statusSlotsChanged) {
		ImGui::Text("Status slots applied to current player only.");
	}
	const std::string currentTypeName = player->GetPlayerTypeName();
	const std::string editedTypeName = playerTypeNameBuffer_.data();
	if (ImGui::Button("Save / Rename Player Type")) {
		if (editedTypeName.empty()) {
			playerTypeEditMessage_ = "Player type name cannot be empty.";
		} else {
			PlayerStats saveStats = player->GetBaseStats();
			saveStats.name = editedTypeName;
			player->ApplyStats(saveStats, ApplyPlayerStatusItems(saveStats));
			if (editedTypeName == currentTypeName) {
				// 名前が同じ場合は能力値や使用モデルなどの編集内容だけを保存する。
				SavePlayerStats(currentTypeName, saveStats);
				playerTypeEditMessage_ = "Saved player type: " + currentTypeName;
			} else if (RenamePlayerStats(currentTypeName, editedTypeName, saveStats)) {
				// シーン内にある同タイプのプレイヤー参照も新しい名前へ揃える。
				for (const auto& object : sceneObjects_) {
					if (Player* scenePlayer = object->GetComponent<Player>();
					    scenePlayer && scenePlayer->GetPlayerTypeName() == currentTypeName) {
						scenePlayer->SetPlayerTypeName(editedTypeName);
					}
				}
				playerTypeEditMessage_ = "Renamed player type to: " + editedTypeName;
			} else {
				playerTypeEditMessage_ = "Rename failed. The new name may already exist.";
			}
		}
	}
	ImGui::SameLine();
	// Default は設定ファイル欠損時にも使う基準タイプなので削除させない。
	const bool canDeletePlayerType = currentTypeName != "Default";
	if (!canDeletePlayerType) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Delete Player Type")) {
		ImGui::OpenPopup("Delete Player Type?");
	}
	if (!canDeletePlayerType) {
		ImGui::EndDisabled();
	}
	if (ImGui::BeginPopupModal("Delete Player Type?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Delete \"%s\"?", currentTypeName.c_str());
		ImGui::TextDisabled("Players using it will be changed to Default.");
		if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f))) {
			if (DeletePlayerStats(currentTypeName)) {
				// 削除したタイプへの参照を残さないよう、対象プレイヤーへDefault設定を適用する。
				const PlayerStats defaultStats = LoadPlayerStats("Default");
				for (const auto& object : sceneObjects_) {
					Player* scenePlayer = object->GetComponent<Player>();
					if (!scenePlayer || scenePlayer->GetPlayerTypeName() != currentTypeName) {
						continue;
					}
					scenePlayer->SetPlayerTypeName("Default");
					scenePlayer->ApplyStats(defaultStats, ApplyPlayerStatusItems(defaultStats));
					if (PlayerAttackComponent* attack = object->GetComponent<PlayerAttackComponent>()) {
						ApplyPlayerAttackSlots(attack, defaultStats);
					}
					if (Object3dComponent* object3d = object->GetComponent<Object3dComponent>()) {
						if (!defaultStats.modelFilePath.empty() &&
						    !ModelManager::GetInstance()->FindModel(defaultStats.modelFilePath)) {
							ModelManager::GetInstance()->LoadModel(defaultStats.modelFilePath);
						}
						if (Model* model = ModelManager::GetInstance()->FindModel(defaultStats.modelFilePath)) {
							object3d->SetModel(defaultStats.modelFilePath);
							object3d->SetDrawSkeleton(model->GetIsAnimation());
						}
					}
				}
				std::fill(playerTypeNameBuffer_.begin(), playerTypeNameBuffer_.end(), '\0');
				const std::string defaultTypeName = "Default";
				std::memcpy(playerTypeNameBuffer_.data(), defaultTypeName.data(), defaultTypeName.size());
				selectedPlayerTypeIndex_ = 0;
				playerTypeEditMessage_ = "Deleted player type: " + currentTypeName;
			} else {
				playerTypeEditMessage_ = "Delete failed.";
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	if (!playerTypeEditMessage_.empty()) {
		ImGui::TextWrapped("%s", playerTypeEditMessage_.c_str());
	}

#else
	(void)selectedObject;
	(void)player;
	(void)stats;
	(void)statsChanged;
	(void)statusSlotsChanged;
#endif
}

void BaseScene::ReloadPlayerAttackInspectorCache() {
	cachedPlayerAttackNames_ = LoadPlayerAttackNames();
	if (cachedPlayerAttackNames_.empty()) {
		cachedPlayerAttackNames_.push_back("Straight");
	}
	cachedPlayerAttackStats_.clear();
	cachedPlayerAttackStats_.reserve(cachedPlayerAttackNames_.size());
	for (const std::string& attackName : cachedPlayerAttackNames_) {
		cachedPlayerAttackStats_.push_back(LoadPlayerAttackStats(attackName));
	}
	if (selectedPlayerAttackTypeIndex_ >= static_cast<int>(cachedPlayerAttackNames_.size())) {
		selectedPlayerAttackTypeIndex_ = 0;
	}
	std::fill(playerAttackNameBuffer_.begin(), playerAttackNameBuffer_.end(), '\0');
	isPlayerAttackCacheLoaded_ = true;
}

PlayerAttackStats* BaseScene::FindCachedPlayerAttackStats(const std::string& attackName) {
	if (!isPlayerAttackCacheLoaded_) {
		ReloadPlayerAttackInspectorCache();
	}
	const std::string targetName = attackName.empty() ? "Straight" : attackName;
	for (PlayerAttackStats& stats : cachedPlayerAttackStats_) {
		if (stats.name == targetName) {
			return &stats;
		}
	}
	return nullptr;
}

void BaseScene::DrawPlayerAttackInspector() {
#ifdef USE_IMGUI
	if (!ImGui::Begin("Player Attack Inspector")) {
		ImGui::End();
		return;
	}

	if (!isPlayerAttackCacheLoaded_) {
		ReloadPlayerAttackInspectorCache();
	}
	if (ImGui::Button("Load")) {
		ReloadPlayerAttackInspectorCache();
	}
	ImGui::SameLine();
	ImGui::Text("Source: %s", kPlayerAttackStatusFilePath);

	if (selectedPlayerAttackTypeIndex_ >= static_cast<int>(cachedPlayerAttackNames_.size())) {
		selectedPlayerAttackTypeIndex_ = 0;
	}

	std::vector<const char*> attackLabels = MakeLabelPointers(cachedPlayerAttackNames_);
	if (ImGui::Combo("Attack Type", &selectedPlayerAttackTypeIndex_, attackLabels.data(), static_cast<int>(attackLabels.size()))) {
		std::fill(playerAttackNameBuffer_.begin(), playerAttackNameBuffer_.end(), '\0');
	}
	PlayerAttackStats* attackStats = selectedPlayerAttackTypeIndex_ < static_cast<int>(cachedPlayerAttackStats_.size()) ? &cachedPlayerAttackStats_[selectedPlayerAttackTypeIndex_] : nullptr;
	if (!attackStats) {
		ImGui::Text("No attack status");
		ImGui::End();
		return;
	}

	if (playerAttackNameBuffer_[0] == '\0') {
		const size_t copyLength = (std::min)(attackStats->name.size(), playerAttackNameBuffer_.size() - 1);
		std::memcpy(playerAttackNameBuffer_.data(), attackStats->name.data(), copyLength);
		playerAttackNameBuffer_[copyLength] = '\0';
	}
	ImGui::InputText("Attack Name", playerAttackNameBuffer_.data(), playerAttackNameBuffer_.size());

	std::vector<std::string> levelNames = GetPlayerAttackLevels();
	if (selectedPlayerAttackLevelIndex_ >= static_cast<int>(levelNames.size())) {
		selectedPlayerAttackLevelIndex_ = 0;
	}
	std::vector<const char*> levelLabels = MakeLabelPointers(levelNames);
	ImGui::Combo("Level", &selectedPlayerAttackLevelIndex_, levelLabels.data(), static_cast<int>(levelLabels.size()));

	PlayerAttackLevelStats* selectedLevel = nullptr;
	for (PlayerAttackLevelStats& levelStats : attackStats->levels) {
		if (levelStats.level == levelNames[selectedPlayerAttackLevelIndex_]) {
			selectedLevel = &levelStats;
			break;
		}
	}
	if (!selectedLevel) {
		attackStats->levels.push_back(MakeDefaultPlayerAttackLevelStats(levelNames[selectedPlayerAttackLevelIndex_]));
		selectedLevel = &attackStats->levels.back();
	}

	bool changed = false;
	if (ImGui::CollapsingHeader("Super Condition", ImGuiTreeNodeFlags_DefaultOpen)) {
		std::vector<std::string> statusItemNames = LoadPlayerStatusItemNames();
		std::vector<std::string> statusItemLabels;
		statusItemLabels.reserve(statusItemNames.size() + 1);
		statusItemLabels.push_back("None");
		statusItemLabels.insert(statusItemLabels.end(), statusItemNames.begin(), statusItemNames.end());
		int statusItemIndex = 0;
		for (int index = 0; index < static_cast<int>(statusItemNames.size()); ++index) {
			if (statusItemNames[index] == attackStats->superConditionStatusName) {
				statusItemIndex = index + 1;
				break;
			}
		}
		std::vector<const char*> statusItemLabelPointers = MakeLabelPointers(statusItemLabels);
		if (ImGui::Combo("Required Status Item", &statusItemIndex, statusItemLabelPointers.data(), static_cast<int>(statusItemLabelPointers.size()))) {
			attackStats->superConditionStatusName = statusItemIndex == 0 ? "" : statusItemNames[statusItemIndex - 1];
			changed = true;
		}

		std::vector<std::string> statusLevels = GetPlayerStatusItemLevels();
		int statusLevelIndex = 0;
		for (int index = 0; index < static_cast<int>(statusLevels.size()); ++index) {
			if (statusLevels[index] == attackStats->superConditionStatusLevel) {
				statusLevelIndex = index;
				break;
			}
		}
		std::vector<const char*> statusLevelLabels = MakeLabelPointers(statusLevels);
		if (ImGui::Combo("Required Status Level", &statusLevelIndex, statusLevelLabels.data(), static_cast<int>(statusLevelLabels.size()))) {
			attackStats->superConditionStatusLevel = statusLevels[statusLevelIndex];
			changed = true;
		}
		if (attackStats->superConditionStatusName.empty()) {
			ImGui::TextDisabled("Super promotion is disabled.");
		} else {
			ImGui::TextWrapped("Uses the super level when the player equips %s at level %s or higher.", attackStats->superConditionStatusName.c_str(), attackStats->superConditionStatusLevel.c_str());
		}
	}
	ImGui::Separator();
	changed |= InputTextMultilineString("Selection Text", selectedLevel->choiceDescription);
	if (selectedLevel->level == "super") {
		// Superは見た目を区別できるよう、通常レベルとは別の画像を保持する。
		changed |= SelectionTextureCombo("Super Selection Texture", selectedLevel->choiceTextureFilePath);
	} else {
		// Lv1～5は攻撃ごとの共通画像を編集し、レベル変更による画像差を作らない。
		changed |= SelectionTextureCombo("Selection Texture (Lv1-5)", attackStats->choiceTextureFilePath);
	}
	ImGui::TextDisabled("Shown when this level is offered. Empty text uses the default description.");
	changed |= ImGui::DragFloat("Attack", &selectedLevel->attack, 1.0f, 0.0f, 100000.0f);
	changed |= ImGui::DragFloat("Speed", &selectedLevel->speed, 0.01f, 0.0f, 100.0f);
	changed |= ImGui::DragFloat("Size %", &selectedLevel->size, 1.0f, 0.0f, 100000.0f);
	changed |= ImGui::DragInt("Shot Count", &selectedLevel->shotCount, 1.0f, 1, 32);
	// 弾数を変更した直後も、角度と発射位置を同じ弾番号で編集できるよう配列数を同期する。
	while (static_cast<int>(selectedLevel->angles.size()) < selectedLevel->shotCount) {
		selectedLevel->angles.push_back(0.0f);
	}
	while (static_cast<int>(selectedLevel->angles.size()) > selectedLevel->shotCount) {
		selectedLevel->angles.pop_back();
	}
	while (static_cast<int>(selectedLevel->spawnOffsets.size()) < selectedLevel->shotCount) {
		selectedLevel->spawnOffsets.push_back(
		    selectedLevel->spawnOffsets.empty() ? Vector3{0.0f, 0.5f, 1.2f} : selectedLevel->spawnOffsets.back());
	}
	while (static_cast<int>(selectedLevel->spawnOffsets.size()) > selectedLevel->shotCount) {
		selectedLevel->spawnOffsets.pop_back();
	}
	for (int index = 0; index < selectedLevel->shotCount; ++index) {
		// 角度と位置を弾ごとに隣接表示し、同じ番号の設定であることを分かりやすくする。
		std::string label = "Shot Angle " + std::to_string(index + 1);
		changed |= ImGui::DragFloat(label.c_str(), &selectedLevel->angles[index], 1.0f, -180.0f, 180.0f);
		label = "Shot " + std::to_string(index + 1) + " Spawn Offset";
		changed |= ImGui::DragFloat3(label.c_str(), &selectedLevel->spawnOffsets[index].x, 0.05f, -100.0f, 100.0f);
	}
	ImGui::TextDisabled("Local position: X = right, Y = up, Z = forward");

	const std::vector<std::string> loadedModels = CollectAllLoadedModelNames();
	if (!loadedModels.empty()) {
		int modelIndex = 0;
		for (int index = 0; index < static_cast<int>(loadedModels.size()); ++index) {
			if (loadedModels[index] == selectedLevel->modelFilePath) {
				modelIndex = index;
				break;
			}
		}
		std::vector<const char*> modelLabels = MakeLabelPointers(loadedModels);
		if (ImGui::Combo("Attack Model", &modelIndex, modelLabels.data(), static_cast<int>(modelLabels.size()))) {
			selectedLevel->modelFilePath = loadedModels[modelIndex];
			changed = true;
		}
	} else {
		ImGui::Text("No loaded models");
	}
	changed |= ImGui::Checkbox("Homing", &selectedLevel->homing);
	changed |= ImGui::SliderFloat("Homing Accuracy", &selectedLevel->homingAccuracy, 0.0f, 1.0f);
	changed |= ImGui::DragFloat("Attack Interval", &selectedLevel->attackInterval, 0.01f, 0.01f, 100.0f);
	changed |= ImGui::DragFloat("Life Time", &selectedLevel->lifeTime, 0.01f, 0.01f, 100.0f);
	changed |= ImGui::DragFloat("Travel Distance", &selectedLevel->travelDistance, 0.1f, 0.1f, 1000.0f);
	changed |= ImGui::DragInt("Pierce Count", &selectedLevel->pierceCount, 1.0f, 0, 1000);
	changed |= ImGui::Checkbox("Infinite Pierce", &selectedLevel->infinitePierce);

	const std::string currentAttackName = cachedPlayerAttackNames_[selectedPlayerAttackTypeIndex_];
	if (changed) {
		attackStats->name = currentAttackName;
		for (const auto& object : sceneObjects_) {
			PlayerAttackComponent* attack = object->GetComponent<PlayerAttackComponent>();
			if (attack) {
				attack->UpdateAttackStatsByName(currentAttackName, *attackStats);
			}
		}
	}
	if (ImGui::Button("Save Attack Type")) {
		const std::string saveName = playerAttackNameBuffer_.data();
		const std::string finalName = saveName.empty() ? currentAttackName : saveName;
		attackStats->name = finalName;
		SavePlayerAttackStats(finalName, *attackStats);
		playerAttackSlotTextureKeys_.fill({});
		playerAttackSlotTexturePaths_.fill({});
		cachedPlayerAttackNames_[selectedPlayerAttackTypeIndex_] = finalName;
		std::fill(playerAttackNameBuffer_.begin(), playerAttackNameBuffer_.end(), '\0');
	}

	ImGui::End();
#endif
}


