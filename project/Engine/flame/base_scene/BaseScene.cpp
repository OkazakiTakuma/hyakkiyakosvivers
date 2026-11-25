#include "base_scene/BaseScene.h"
#include "PostEffect.h"
#include "repositories/PlayerStatusRepository.h"

BaseScene::~BaseScene() {}

void BaseScene::Initialize() {}

void BaseScene::Update() {}

bool BaseScene::CanSetObjectParent(const Entity* child, const std::string& parentName) const {
	if (!child) return false;
	// 親なしへの変更は常に有効。親ありの場合は両方が同じSceneWorldに存在することを要求する。
	const auto childEntry = std::find_if(sceneObjects_.begin(), sceneObjects_.end(),
		[child](const std::unique_ptr<Entity>& object) { return object.get() == child; });
	if (childEntry == sceneObjects_.end()) return false;
	if (parentName.empty()) return true;

	const Entity* parent = FindObjectByName(parentName);
	if (!parent || parent == child) return false;

	// 親候補から祖先をたどり、childへ戻る接続と既存の不正な循環を拒否する。
	std::vector<uint64_t> visitedIds;
	while (parent) {
		if (parent == child || std::find(visitedIds.begin(), visitedIds.end(), parent->GetRuntimeId()) != visitedIds.end()) {
			return false;
		}
		visitedIds.push_back(parent->GetRuntimeId());
		if (parent->GetParentName().empty()) break;
		parent = FindObjectByName(parent->GetParentName());
	}
	return true;
}

bool BaseScene::TrySetObjectParent(Entity* child, const std::string& parentName) {
	if (!CanSetObjectParent(child, parentName)) return false;
	child->SetParentName(parentName);
	return true;
}

void BaseScene::ClearEntityReferences(Entity* target) {
	if (!target) return;
	CameraComponent* deletedCamera = target->GetComponent<CameraComponent>();
	// Entityの所有権はWorldに残したまま、削除前に非所有参照だけを一括して無効化する。
	for (const auto& object : sceneObjects_) {
		if (CameraComponent* camera = object->GetComponent<CameraComponent>();
		    camera && camera->GetFollowTarget() == target) camera->SetResolvedFollowTarget(nullptr);
		if (EnemyComponent* enemy = object->GetComponent<EnemyComponent>();
		    enemy && enemy->GetTarget() == target) enemy->SetTarget(nullptr);
		if (EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>();
		    spawnPoint) {
			if (spawnPoint->GetTarget() == target) spawnPoint->SetTarget(nullptr);
			// CameraはEntityではなく内部Cameraへの参照なので、所有Entity削除時に個別に解除する。
			if (deletedCamera && spawnPoint->GetCamera() == deletedCamera->GetCamera()) spawnPoint->SetCamera(nullptr);
		}
		if (ExperienceComponent* experience = object->GetComponent<ExperienceComponent>();
		    experience && experience->GetTarget() == target) experience->SetTarget(nullptr);
		if (ItemDropComponent* itemDrop = object->GetComponent<ItemDropComponent>();
		    itemDrop && itemDrop->GetTarget() == target) itemDrop->SetTarget(nullptr);
		if (PlayerAttackEntityComponent* attack = object->GetComponent<PlayerAttackEntityComponent>()) {
			if (attack->GetHomingTarget() == target) attack->SetHomingTarget(nullptr);
			if (attack->GetMotionAnchor() == target) attack->SetMotionAnchor(nullptr);
		}
		if (EnemyAttackEntityComponent* attack = object->GetComponent<EnemyAttackEntityComponent>();
		    attack && attack->GetHomingTarget() == target) attack->SetHomingTarget(nullptr);
	}
}

void BaseScene::DrawSkyBox() {
	sceneEditorSkyBox_.Draw();
}

void BaseScene::Draw2D() {}

void BaseScene::Draw3D() {}

bool BaseScene::IsPlayerDefeated() const {
	// プレイヤーはシーン内に1体という前提で、最初に見つかったプレイヤーのHPを判定する。
	for (const auto& object : sceneObjects_) {
		const PlayerComponent* player = object->GetComponent<PlayerComponent>();
		if (player) {
			return player->GetCurrentHealth() <= 0.0f;
		}
	}
	return false;
}

StageResultData BaseScene::GetStageResultData() const {
	// シーン破棄後も表示できるよう、終了時点の値を所有型へコピーして返す。
	StageResultData result = gameplaySession_.MakeResultData();
	for (const auto& object : sceneObjects_) {
		const PlayerComponent* player = object->GetComponent<PlayerComponent>();
		if (!player) {
			continue;
		}
		const PlayerStats& stats = player->GetBaseStats();
		// 空スロットや無効化されたスロットはリザルトへ表示しない。
		for (const PlayerAttackSlot& slot : stats.attackSlots) {
			if (slot.enabled && !slot.attackName.empty()) {
				result.attacks.push_back({
					LoadPlayerAttackDisplayName(slot.attackName, slot.attackLevel == "super"),
					slot.attackLevel});
			}
		}
		for (const PlayerStatusSlot& slot : stats.statusSlots) {
			if (slot.enabled && !slot.statusName.empty()) {
				result.statuses.push_back({LoadPlayerStatusItemDisplayName(slot.statusName), slot.level});
			}
		}
		break;
	}
	return result;
}

/// <summary>
/// シーンが保持しているリソースを解放します。
/// </summary>
void BaseScene::Finalize() {
	// 撃破演出中にシーンが切り替わっても、次のシーンへ時間倍率とブラーを持ち越さない。
	GameTime::SetTimeScale(1.0f);
	PostEffect::GetInstance()->SetCinematicRadialBlur(0.0f);
	sceneObjects_.clear();
	enemySpawnSystem_.Reset();
	gameplaySession_.Reset();
	sceneEditorSkyBox_.Reset();
	gameplayHud_.Reset();
	damageNumberPopups_.clear();
	nextDamageNumberIndex_ = 0;
	SetGameplayMode(GameplayMode::Playing);
	levelUpPlayer_ = nullptr;
	levelUpChoices_.clear();
	levelUpRerollUsed_ = false;
	bossAcquisitionOfferQueue_.clear();
	bossAcquisitionPlayer_ = nullptr;
	levelUpOverlaySprite_.reset();
	levelUpPanelSprite_.reset();
	for (auto& sprite : levelUpPanelAccentSprites_) sprite.reset();
	for (auto& sprite : levelUpChoiceBorderSprites_) sprite.reset();
	for (auto& sprite : levelUpChoiceSprites_) sprite.reset();
	for (auto& sprite : levelUpChoiceAccentSprites_) sprite.reset();
	for (auto& sprite : levelUpChoiceIconSprites_) sprite.reset();
	for (auto& sprites : levelUpChoiceCorrespondingIconSprites_) {
		for (auto& sprite : sprites) sprite.reset();
	}
	levelUpTitleTextObject_.reset();
	levelUpInstructionTextObject_.reset();
	for (auto& object : levelUpChoiceTextObjects_) object.reset();
	bossRewardResults_.clear();
	bossRewardPresentationTimer_ = 0.0f;
	bossRewardPresentationReady_ = false;
	bossRewardPresentationMoney_ = 0;
	bossRewardOverlaySprite_.reset();
	bossRewardFlashSprite_.reset();
	bossRewardChestBackSprite_.reset();
	bossRewardChestFrontSprite_.reset();
	bossRewardChestLidSprite_.reset();
	for (auto& sprite : bossRewardBurstSprites_) sprite.reset();
	for (auto& sprite : bossRewardIconBackSprites_) sprite.reset();
	for (auto& sprite : bossRewardIconSprites_) sprite.reset();
	bossRewardTitleTextObject_.reset();
	bossRewardMoneyTextObject_.reset();
	bossRewardInstructionTextObject_.reset();
	sceneEditorState_.Reset();
	enemyInspectorObjectName_.clear();
}

void BaseScene::SetGameplayMode(GameplayMode mode) {
	gameplaySession_.SetMode(mode);
}

/// <summary>
/// シーン内オブジェクトの更新と当たり判定を行います。
/// </summary>
void BaseScene::UpdateSceneObjects() {
	// GameTimeが一時停止中はDeltaTimeが0になるため、レベルアップ選択時間は含まれない。
	gameplaySession_.AdvanceTime(GameTime::GetDeltaTime());
	ResolveCameraLinks();
	ResolveEnemySpawnPointLinks();
	ResolveEnemyLinks();
	sceneEditorSkyBox_.Update();
	sceneObjects_.Update();
	// Object3dの更新後に上書きし、光源所有者ではなくシーン全体へ同じライトを適用する。
	ApplyScenePointLight();
	UpdateEnemyAttacks();
	combatSystem_.UpdateEnemyProjectileHits(sceneObjects_);
	UpdatePlayerAttacks();
	UpdatePlayerAttackEntityHits();
	UpdateDamageNumbers();
	UpdateBossUpgradeRewards();
	UpdateItemDrops();
	UpdateExperienceCompression();
	UpdateEnemySpawning();
	UpdateBossArenaWalls();
	// 地形のループ判定より先に補正し、壁抜けした座標へ地形が追従しないようにする。
	ConstrainPlayersToBossArena();
	CleanupExpiredPlayerAttackEntities();
	// G取得処理より後で更新し、このフレームに拾った金額を即座にHUDへ表示する。
	UpdateGameplayHud();
	UpdateEditorObjectPicking();
	UpdateEditorCameraControl();
	UpdateLoopingTerrain();
	combatSystem_.UpdateColliderCollisions(sceneObjects_);
	// 薄い壁を1フレームで通過した場合や、他の衝突補正で外側へ押された場合も場内へ戻す。
	ConstrainPlayersToBossArena();
	ApplyActiveCamera();
}

/// <summary>
/// エディタ用のカメラ操作とオブジェクト選択を更新します。
/// </summary>
void BaseScene::UpdateEditorTools() {
	ResolveCameraLinks();
	ResolveEnemySpawnPointLinks();
	ResolveEnemyLinks();
	sceneEditorSkyBox_.Update();
	UpdateGameplayHud();
	UpdateEditorObjectPicking();
	UpdateEditorCameraControl();
	ApplyScenePointLight();
	ApplyActiveCamera();
}

void BaseScene::ApplyScenePointLight() {
	PointLightComponent* activeLight = nullptr;
	Entity* lightOwner = nullptr;
	for (const auto& object : sceneObjects_) {
		PointLightComponent* candidate = object->GetComponent<PointLightComponent>();
		if (candidate && candidate->IsEnabled()) {
			activeLight = candidate;
			lightOwner = object.get();
			break;
		}
	}
	if (!activeLight || !lightOwner) {
		return;
	}

	Vector4 lightColor = activeLight->GetColor();
	if (activeLight->GetUseMaterialEmissionColor()) {
		if (Object3dComponent* sourceObject3d = lightOwner->GetComponent<Object3dComponent>()) {
			const Vector3 emissionColor = sourceObject3d->GetEmissionColor();
			lightColor = {emissionColor.x, emissionColor.y, emissionColor.z, 1.0f};
		}
	}
	const Vector3 lightPosition = lightOwner->GetTransform().translate + activeLight->GetPositionOffset();

	for (const auto& object : sceneObjects_) {
		if (Object3dComponent* object3d = object->GetComponent<Object3dComponent>()) {
			object3d->SetPointLight(
			    lightColor,
			    lightPosition,
			    activeLight->GetIntensity(),
			    activeLight->GetRadius(),
			    activeLight->GetDecay()
			);
		}
	}
}

/// <summary>
/// シーン内オブジェクトの2D描画を行います。
/// </summary>
void BaseScene::DrawSceneObjects2D() {
	sceneObjects_.Draw2D();
	// ワールド上の敵位置へ追従させつつ、3Dオブジェクトより手前へ数値を重ねる。
	DrawDamageNumbers();
	// 開始演出中は戦闘HUDを隠し、霧が晴れて操作可能になる瞬間にまとめて表示する。
	if (gameplaySession_.GetMode() == GameplayMode::Intro) {
		return;
	}
	// シーンオブジェクトより後に描画し、3Dステージや配置物に隠れないHUDとして重ねる。
	gameplayHud_.Draw();
	DrawBossRewardPresentation2D();
	DrawLevelUpSelection2D();
}

/// <summary>
/// シーン内オブジェクトの3D描画を行います。
/// </summary>
void BaseScene::DrawSceneObjects3D() {
	sceneObjects_.Draw3D();
}

void BaseScene::DrawSceneObjectShadowMaps() {
	for (const auto& object : sceneObjects_) {
		// 通常描画で無効なコンポーネントは影だけを残さないよう、影生成からも除外する。
		// SkyLaserやClawSlashなど、専用エフェクトへ表示を差し替えた攻撃の非表示モデルも
		// これによりシャドウマップへ混入しない。
		if (auto* component = object->GetComponent<Object3dComponent>();
		    component && component->IsEnabled() && component->GetShadowEnabled()) {
			component->DrawShadowMap();
		}
	}
}

/// <summary>
/// エディタ用ImGuiウィンドウを描画します。
/// </summary>
void BaseScene::DrawEditorImGui() {
	// 選択UIを先に更新し、演出を閉じた決定入力が同じフレームの取得確認へ流用されるのを防ぐ。
	UpdateLevelUpSelection();
	UpdateBossRewardPresentation();
#ifdef USE_IMGUI
	DrawEditorHierarchy();
	DrawEditorProjectAssets();
	DrawEditorInspector();
	DrawPlayerInspector();
	DrawEnemyInspector();
	DrawPlayerAttackInspector();
	if (ImGui::Begin("Boss Reward Settings")) {
		BossRewardSettings settings = lootDropService_.GetBossRewardSettings();
		bool changed = false;
		ImGui::SeparatorText("Reward Count Weights");
		changed |= ImGui::SliderFloat("1 reward", &settings.singleRewardWeight, 0.0f, 100.0f, "%.1f");
		changed |= ImGui::SliderFloat("3 rewards", &settings.tripleRewardWeight, 0.0f, 100.0f, "%.1f");
		changed |= ImGui::SliderFloat("5 rewards", &settings.quintupleRewardWeight, 0.0f, 100.0f, "%.1f");
		const float countWeightTotal = settings.singleRewardWeight + settings.tripleRewardWeight + settings.quintupleRewardWeight;
		if (countWeightTotal > 0.0f) ImGui::TextDisabled("Effective: 1=%.1f%%  3=%.1f%%  5=%.1f%%",
			100.0f * settings.singleRewardWeight / countWeightTotal,
			100.0f * settings.tripleRewardWeight / countWeightTotal,
			100.0f * settings.quintupleRewardWeight / countWeightTotal);
		ImGui::SeparatorText("Reward Table Weights");
		changed |= ImGui::SliderFloat("Owned upgrade", &settings.ownedUpgradeTableWeight, 0.0f, 100.0f, "%.1f");
		changed |= ImGui::SliderFloat("Unowned weapon", &settings.unownedWeaponTableWeight, 0.0f, 100.0f, "%.1f");
		changed |= ImGui::SliderFloat("Unowned item", &settings.unownedItemTableWeight, 0.0f, 100.0f, "%.1f");
		const float tableWeightTotal = settings.ownedUpgradeTableWeight + settings.unownedWeaponTableWeight + settings.unownedItemTableWeight;
		if (tableWeightTotal > 0.0f) ImGui::TextDisabled("Base: owned=%.1f%%  weapon=%.1f%%  item=%.1f%%",
			100.0f * settings.ownedUpgradeTableWeight / tableWeightTotal,
			100.0f * settings.unownedWeaponTableWeight / tableWeightTotal,
			100.0f * settings.unownedItemTableWeight / tableWeightTotal);
		ImGui::TextDisabled("Available tables are normalized automatically.");
		ImGui::SeparatorText("Gold Reward");
		changed |= ImGui::DragInt("G for 1 reward", &settings.singleRewardMoney, 1.0f, 0, 1000000);
		changed |= ImGui::DragInt("G for 3 rewards", &settings.tripleRewardMoney, 1.0f, 0, 1000000);
		changed |= ImGui::DragInt("G for 5 rewards", &settings.quintupleRewardMoney, 1.0f, 0, 1000000);
		if (changed) lootDropService_.SetBossRewardSettings(settings);
		if (ImGui::Button("Save Boss Reward Settings")) lootDropService_.SaveBossRewardSettings();
	}
	ImGui::End();
	if (ImGui::Begin("Collision Performance")) {
		float cellSize = combatSystem_.GetCellSize();
		if (ImGui::SliderFloat("Cell Size", &cellSize, 1.0f, 50.0f, "%.1f")) {
			combatSystem_.SetCellSize(cellSize);
		}
		const CombatSystem::DebugStats& collisionDebugStats = combatSystem_.GetDebugStats();
		const size_t colliderCount = collisionDebugStats.colliderObjects;
		const size_t bruteForcePairs = colliderCount > 1 ? colliderCount * (colliderCount - 1) / 2 : 0;
		ImGui::SeparatorText("General Colliders");
		ImGui::Text("Objects: %zu", colliderCount);
		ImGui::Text("Occupied cells: %zu", collisionDebugStats.occupiedCells);
		ImGui::Text("Cell entries: %zu", collisionDebugStats.gridEntries);
		ImGui::Text("Candidate pairs: %zu / %zu brute-force pairs",
			collisionDebugStats.colliderCandidatePairs, bruteForcePairs);
		ImGui::Text("Narrow phase tests: %zu", collisionDebugStats.colliderNarrowPhaseTests);
		ImGui::SeparatorText("Projectile Hits");
		ImGui::Text("Player projectile candidates: %zu", collisionDebugStats.playerProjectileCandidates);
		ImGui::Text("Player projectile tests: %zu", collisionDebugStats.playerProjectileNarrowPhaseTests);
		ImGui::Text("Enemy projectile candidates: %zu", collisionDebugStats.enemyProjectileCandidates);
		ImGui::Text("Enemy projectile tests: %zu", collisionDebugStats.enemyProjectileNarrowPhaseTests);
	}
	ImGui::End();
	DrawEditorGizmo();
	HandleGameViewAssetDrop();
#endif
}


