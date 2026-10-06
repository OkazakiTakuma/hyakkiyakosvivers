#include "BaseScene.h"

BaseScene::~BaseScene() {}

void BaseScene::Initialize() {}

void BaseScene::Update() {}

void BaseScene::DrawSkyBox() {
	if (isEditorSkyBoxEnabled_ && editorSkyBox_) {
		editorSkyBox_->Draw();
	}
}

void BaseScene::Draw2D() {}

void BaseScene::Draw3D() {}

bool BaseScene::IsPlayerDefeated() const {
	// プレイヤーはシーン内に1体という前提で、最初に見つかったプレイヤーのHPを判定する。
	for (const auto& object : sceneObjects_) {
		const Player* player = object->GetComponent<Player>();
		if (player) {
			return player->GetCurrentHealth() <= 0.0f;
		}
	}
	return false;
}

StageResultData BaseScene::GetStageResultData() const {
	// シーン破棄後も表示できるよう、終了時点の値を所有型へコピーして返す。
	StageResultData result;
	result.defeatedEnemyCount = defeatedEnemyCount_;
	// 挑戦中に回収した金額をリザルト表示と全体所持金への加算処理へ引き渡す。
	result.moneyEarned = challengeMoneyEarned_;
	result.survivalTimeSeconds = survivalTimeSeconds_;
	result.stageCleared = isStageCleared_;
	for (const auto& object : sceneObjects_) {
		const Player* player = object->GetComponent<Player>();
		if (!player) {
			continue;
		}
		const PlayerStats& stats = player->GetBaseStats();
		// 空スロットや無効化されたスロットはリザルトへ表示しない。
		for (const PlayerAttackSlot& slot : stats.attackSlots) {
			if (slot.enabled && !slot.attackName.empty()) {
				result.attacks.push_back({slot.attackName, slot.attackLevel});
			}
		}
		for (const PlayerStatusSlot& slot : stats.statusSlots) {
			if (slot.enabled && !slot.statusName.empty()) {
				result.statuses.push_back({slot.statusName, slot.level});
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
	sceneObjects_.clear();
	activeBossEncounterObjectName_.clear();
	isStageCleared_ = false;
	defeatedEnemyCount_ = 0;
	survivalTimeSeconds_ = 0.0f;
	editorSkyBox_.reset();
	playerHealthBarBackground_.reset();
	playerHealthBarFill_.reset();
	isPlayerHealthHudVisible_ = false;
	// TextComponentが内部生成した文字テクスチャもGameObjectの破棄と同時に解放する。
	challengeMoneyTextObject_.reset();
	isChallengeMoneyHudVisible_ = false;
	playerExperienceBarBackground_.reset();
	playerExperienceBarFill_.reset();
	playerExperienceTextObject_.reset();
	playerExperienceRate_ = 0.0f;
	isPlayerExperienceHudVisible_ = false;
	for (auto& sprite : playerAttackSlotBackgroundSprites_) sprite.reset();
	for (auto& sprite : playerAttackSlotIconSprites_) sprite.reset();
	for (auto& sprite : playerStatusSlotBackgroundSprites_) sprite.reset();
	for (auto& sprite : playerStatusSlotIconSprites_) sprite.reset();
	playerAttackSlotIconVisible_.fill(false);
	playerStatusSlotIconVisible_.fill(false);
	playerAttackSlotTextureKeys_.fill({});
	playerAttackSlotTexturePaths_.fill({});
	playerStatusSlotTextureKeys_.fill({});
	playerStatusSlotTexturePaths_.fill({});
	playerAttackSlotLabelObject_.reset();
	playerStatusSlotLabelObject_.reset();
	isPlayerSlotHudVisible_ = false;
	isLevelUpSelectionActive_ = false;
	levelUpPlayer_ = nullptr;
	levelUpChoices_.clear();
	bossAcquisitionOfferQueue_.clear();
	bossAcquisitionPlayer_ = nullptr;
	isBossAcquisitionOfferActive_ = false;
	levelUpOverlaySprite_.reset();
	levelUpPanelSprite_.reset();
	for (auto& sprite : levelUpChoiceBorderSprites_) sprite.reset();
	for (auto& sprite : levelUpChoiceSprites_) sprite.reset();
	for (auto& sprite : levelUpChoiceIconSprites_) sprite.reset();
	levelUpTitleTextObject_.reset();
	levelUpInstructionTextObject_.reset();
	for (auto& object : levelUpChoiceTextObjects_) object.reset();
	GameTime::SetPaused(false);
	selectedObjectIndex_ = -1;
	activeCameraObjectName_.clear();
	enemyInspectorObjectName_.clear();
}

/// <summary>
/// シーン内オブジェクトの更新と当たり判定を行います。
/// </summary>
void BaseScene::UpdateSceneObjects() {
	// GameTimeが一時停止中はDeltaTimeが0になるため、レベルアップ選択時間は含まれない。
	survivalTimeSeconds_ += GameTime::GetDeltaTime();
	ResolveCameraLinks();
	ResolveEnemySpawnPointLinks();
	ResolveEnemyLinks();
	if (editorSkyBox_) {
		editorSkyBox_->Update();
	}
	for (const auto& object : sceneObjects_) {
		object->Update();
	}
	// Object3dの更新後に上書きし、光源所有者ではなくシーン全体へ同じライトを適用する。
	ApplyScenePointLight();
	UpdateEnemyAttacks();
	UpdateEnemyProjectileHits();
	UpdatePlayerAttacks();
	UpdatePlayerProjectileHits();
	UpdateBossUpgradeRewards();
	UpdateItemDrops();
	UpdateExperienceCompression();
	UpdateEnemySpawning();
	CleanupExpiredPlayerProjectiles();
	UpdatePlayerHealthHud();
	// G取得処理より後で更新し、このフレームに拾った金額を即座にHUDへ表示する。
	UpdateChallengeMoneyHud();
	UpdatePlayerExperienceHud();
	UpdatePlayerSlotHud();
	UpdateEditorObjectPicking();
	UpdateEditorCameraControl();
	UpdateStageBoundaryWrapping();
	UpdateColliderCollisions();
	ApplyActiveCamera();
}

/// <summary>
/// エディタ用のカメラ操作とオブジェクト選択を更新します。
/// </summary>
void BaseScene::UpdateEditorTools() {
	ResolveCameraLinks();
	ResolveEnemySpawnPointLinks();
	ResolveEnemyLinks();
	if (editorSkyBox_) {
		editorSkyBox_->Update();
	}
	UpdatePlayerHealthHud();
	UpdateChallengeMoneyHud();
	UpdatePlayerExperienceHud();
	UpdatePlayerSlotHud();
	UpdateEditorObjectPicking();
	UpdateEditorCameraControl();
	ApplyScenePointLight();
	ApplyActiveCamera();
}

void BaseScene::ApplyScenePointLight() {
	PointLightComponent* activeLight = nullptr;
	GameObject* lightOwner = nullptr;
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
	for (const auto& object : sceneObjects_) {
		object->Draw2D();
	}
	if (isPlayerHealthHudVisible_ && playerHealthBarBackground_ && playerHealthBarFill_) {
		playerHealthBarBackground_->Draw();
		playerHealthBarFill_->Draw();
	}
	// シーンオブジェクトより後に描画し、3Dステージや配置物に隠れないHUDとして重ねる。
	DrawChallengeMoneyHud();
	DrawPlayerExperienceHud();
	DrawPlayerSlotHud();
	DrawLevelUpSelection2D();
}

/// <summary>
/// シーン内オブジェクトの3D描画を行います。
/// </summary>
void BaseScene::DrawSceneObjects3D() {
	for (const auto& object : sceneObjects_) {
		object->Draw3D();
	}
}

void BaseScene::DrawSceneObjectShadowMaps() {
	for (const auto& object : sceneObjects_) {
		// castShadow相当の既存設定を利用し、床など影を落とさない物体を除外できます。
		if (auto* component = object->GetComponent<Object3dComponent>();
		    component && component->GetShadowEnabled()) {
			component->DrawShadowMap();
		}
	}
}

/// <summary>
/// エディタ用ImGuiウィンドウを描画します。
/// </summary>
void BaseScene::DrawEditorImGui() {
	UpdateLevelUpSelection();
#ifdef USE_IMGUI
	DrawEditorHierarchy();
	DrawEditorProjectAssets();
	DrawEditorInspector();
	DrawPlayerInspector();
	DrawEnemyInspector();
	DrawPlayerAttackInspector();
	DrawEditorGizmo();
	HandleGameViewAssetDrop();
#endif
}

