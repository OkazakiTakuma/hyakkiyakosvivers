#include "base_scene/BaseScene.h"
#include "helpers/BaseSceneCollisionHelpers.h"
#include "helpers/BaseSceneEditorGeometry.h"
#include "repositories/EnemyStatusRepository.h"
#include "repositories/ParticlePresetRepository.h"
#include "MathConstants.h"
#include "model/ModelManager.h"
#include "repositories/PlayerStatusRepository.h"
#include "SceneManager.h"
#include "StringUtility.h"
#include "particle/TrailRenderer.h"
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <random>
#include <unordered_set>
#include <Xinput.h>
#ifdef USE_IMGUI
#include "../../../../../imgui/ImGuizmo.h"
#endif

void BaseScene::ApplyCamera(Camera* camera) {
	if (!camera) {
		return;
	}

	const float clientWidth = static_cast<float>(Input::GetInstance()->GetClientWidth());
	const float clientHeight = static_cast<float>(Input::GetInstance()->GetClientHeight());
	float aspectRatio = clientWidth > 0.0f && clientHeight > 0.0f ? clientWidth / clientHeight : 1.0f;
#ifdef USE_IMGUI
	const float gameViewAspectRatio = ImGuiManager::GetInstance()->GetGameViewAspectRatio();
	if ((!sceneManager_ || !sceneManager_->IsDemoPlayActive()) && gameViewAspectRatio > 0.0f) {
		aspectRatio = gameViewAspectRatio;
	}
#endif
	camera->SetAspectRatio(aspectRatio);
	camera->Update();
	Object3dCommon::GetInstance()->SetDefaultCamera(camera);
	SkyBoxCommon::GetInstance()->SetDefaultCamera(camera);
	ParticleManager::GetInstance()->SetCamera(camera);
	for (const auto& object : sceneObjects_) {
		if (Object3dComponent* object3dComponent = object->GetComponent<Object3dComponent>()) {
			if (!object3dComponent->IsEnabled()) continue;
			object3dComponent->SetCamera(camera);
			// カメラ変更は描画情報だけを同期し、アニメーション時間を再加算しない。
			object3dComponent->SyncRenderState();
		}
	}
}

/// <summary>
/// 現在選択されているアクティブカメラを反映します。
/// </summary>
void BaseScene::ApplyActiveCamera() {
	if (!sceneEditorState_.HasActiveCamera()) {
		ApplyCamera(fallbackCamera_);
		return;
	}

	Entity* cameraObject = FindObjectByName(sceneEditorState_.GetActiveCameraName());
	if (!cameraObject) {
		sceneEditorState_.ClearActiveCamera();
		ApplyCamera(fallbackCamera_);
		return;
	}

	CameraComponent* cameraComponent = cameraObject->GetComponent<CameraComponent>();
	if (!cameraComponent || !cameraComponent->IsEnabled() || !cameraComponent->GetCamera()) {
		sceneEditorState_.ClearActiveCamera();
		ApplyCamera(fallbackCamera_);
		return;
	}

	cameraComponent->Update();
	ApplyCamera(cameraComponent->GetCamera());
}

/// <summary>
/// カメラの追従対象リンクを名前から解決します。
/// </summary>
void BaseScene::ResolveCameraLinks() {
	for (Entity* object : sceneObjects_.GetEntitiesWith<CameraComponent>()) {
		CameraComponent* cameraComponent = object->GetComponent<CameraComponent>();
		if (!cameraComponent || !cameraComponent->IsEnabled() || cameraComponent->GetFollowTargetName().empty()) {
			continue;
		}
		Entity* resolvedTarget = cameraComponent->GetFollowTarget();
		if (resolvedTarget && resolvedTarget != object &&
		    resolvedTarget->GetName() == cameraComponent->GetFollowTargetName()) {
			// 削除時と設定名変更時に参照が解除されるため、有効なリンクは再検索しない。
			continue;
		}

		Entity* target = FindObjectByName(cameraComponent->GetFollowTargetName());
		// 解決失敗時も古いアドレスを残さず、名前だけを次回の再解決用に維持する。
		cameraComponent->SetResolvedFollowTarget(target != object ? target : nullptr);
	}
}

void BaseScene::ResolveEnemySpawnPointLinks() {
	Entity* firstPlayer = nullptr;
	bool searchedFirstPlayer = false;
	auto findFirstPlayer = [&]() -> Entity* {
		if (!searchedFirstPlayer) {
			searchedFirstPlayer = true;
			const auto& players = sceneObjects_.GetEntitiesWith<PlayerComponent>();
			firstPlayer = players.empty() ? nullptr : players.front();
		}
		return firstPlayer;
	};

	for (Entity* object : sceneObjects_.GetEntitiesWith<EnemySpawnPointComponent>()) {
		EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>();
		if (!spawnPoint || !spawnPoint->IsEnabled()) {
			continue;
		}

		Entity* target = spawnPoint->GetTarget();
		const bool targetLinkIsValid = target &&
			(spawnPoint->GetTargetName().empty() || target->GetName() == spawnPoint->GetTargetName());
		if (!targetLinkIsValid) {
			target = spawnPoint->GetTargetName().empty()
				? findFirstPlayer()
				: FindObjectByName(spawnPoint->GetTargetName());
		}
		if (target && target != spawnPoint->GetTarget()) {
			spawnPoint->SetTarget(target);
			if (spawnPoint->GetTargetName().empty()) {
				spawnPoint->SetTargetName(target->GetName());
			}
		} else if (!target) {
			spawnPoint->SetTarget(nullptr);
		}

		Camera* camera = spawnPoint->GetCamera();
		if (!camera && !spawnPoint->GetCameraName().empty()) {
			Entity* cameraObject = FindObjectByName(spawnPoint->GetCameraName());
			CameraComponent* cameraComponent = cameraObject ? cameraObject->GetComponent<CameraComponent>() : nullptr;
			if (cameraComponent && cameraComponent->IsEnabled()) {
				camera = cameraComponent->GetCamera();
			}
		}
		if (!camera && Object3dCommon::GetInstance()) {
			camera = Object3dCommon::GetInstance()->GetDefaultCamera();
		}
		spawnPoint->SetCamera(camera);
	}
}

void BaseScene::ResolveEnemyLinks() {
	Entity* firstPlayer = nullptr;
	bool searchedFirstPlayer = false;
	auto findFirstPlayer = [&]() -> Entity* {
		if (!searchedFirstPlayer) {
			searchedFirstPlayer = true;
			const auto& players = sceneObjects_.GetEntitiesWith<PlayerComponent>();
			firstPlayer = players.empty() ? nullptr : players.front();
		}
		return firstPlayer;
	};

	for (Entity* object : sceneObjects_.GetEntitiesWith<EnemyComponent>()) {
		EnemyComponent* enemy = object->GetComponent<EnemyComponent>();
		if (!enemy || !enemy->IsEnabled()) {
			continue;
		}

		Entity* target = enemy->GetTarget();
		const bool targetLinkIsValid = target &&
			(enemy->GetTargetName().empty() || target->GetName() == enemy->GetTargetName());
		if (!targetLinkIsValid) {
			target = enemy->GetTargetName().empty()
				? findFirstPlayer()
				: FindObjectByName(enemy->GetTargetName());
		}
		if (target && target != enemy->GetTarget()) {
			enemy->SetTarget(target);
			if (enemy->GetTargetName().empty()) {
				enemy->SetTargetName(target->GetName());
			}
		} else if (!target) {
			enemy->SetTarget(nullptr);
		}
	}
}

void BaseScene::BeginRuntimeBossEncounter(const Entity& boss) {
	enemySpawnSystem_.BeginBossEncounter(boss.GetName());
}

void BaseScene::UpdateEnemySpawning() {
	EnemySpawnUpdateResult updateResult = enemySpawnSystem_.Update(
		sceneObjects_,
		IsLevelUpSelectionActive(),
		!bossAcquisitionOfferQueue_.empty());
	if (updateResult.stageCleared) {
		gameplaySession_.MarkStageCleared();
		return;
	}

	if (updateResult.hasBossEncounterRequest) {
		const EnemySpawnPointComponent::BossEncounterSettings& bossSettings =
			updateResult.bossEncounter.settings;
		Entity* player = updateResult.bossEncounter.player;
		if (player) {
			// ボス生成より先にプレイヤーを戦闘位置へ移し、落下速度を残さない。
			player->GetTransform().translate = bossSettings.playerWarpPosition;
			if (GravityComponent* gravity = player->GetComponent<GravityComponent>()) {
				gravity->ResetVelocity();
			}
		}

		// ボス戦へ通常敵や途中の中ボスを持ち越さないよう、全EnemyComponentを即時除去する。
		// erase中はunique_ptrの並び替えが発生するため、事前走査で全参照を安全に解除する。
		for (const auto& object : sceneObjects_) {
			if (object->GetComponent<EnemyComponent>()) {
				ClearEntityReferences(object.get());
			}
		}
		sceneObjects_.erase(
		    std::remove_if(sceneObjects_.begin(), sceneObjects_.end(), [](const std::unique_ptr<Entity>& object) {
			    return object->GetComponent<EnemyComponent>() != nullptr;
		    }),
		    sceneObjects_.end()
		);
		sceneEditorState_.ClearSelection();
		for (const auto& object : sceneObjects_) {
			if (EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>()) {
				spawnPoint->SetBossEncounterActive(true);
			}
		}
		CreateBossArenaWalls(bossSettings);

		Entity* bossObject = CreateRuntimeEnemy(bossSettings.enemyTypeName, bossSettings.bossPosition, player);
		if (bossObject) {
			BeginRuntimeBossEncounter(*bossObject);
		}
		return;
	}

	for (const EnemySpawnRequest& request : updateResult.enemyRequests) {
		Vector3 spawnPosition = request.position;
		// ステージ2では画面外の生成候補も上下の壁より内側へ収める。
		if (sceneManager_ && sceneManager_->GetSelectedGameplayStageId() == "stage2") {
			constexpr float kStage2SpawnLimitZ = 8.5f;
			spawnPosition.z = std::clamp(spawnPosition.z, -kStage2SpawnLimitZ, kStage2SpawnLimitZ);
		}
		CreateRuntimeEnemy(request.enemyTypeName, spawnPosition, request.target, request.statMultipliers);
	}
}

namespace {
constexpr const char* kBossArenaWallPrefix = "__BossArenaWall_";
}

void BaseScene::CreateBossArenaWalls(const EnemySpawnPointComponent::BossEncounterSettings& settings) {
	RemoveBossArenaWalls();
	if (!settings.arenaWallsEnabled) {
		return;
	}

	const float extent = (std::max)(2.0f, settings.arenaHalfExtent);
	const float height = (std::max)(0.5f, settings.arenaWallHeight);
	const float thickness = (std::max)(0.1f, settings.arenaWallThickness);
	const Vector3 center = settings.bossPosition;

	struct WallPlacement {
		const char* suffix;
		Vector3 position;
		Vector3 scale;
		Vector3 tangent;
	};
	const std::array<WallPlacement, 4> placements = {{
		{"North", {center.x, center.y + height * 0.5f, center.z + extent + thickness * 0.5f},
		 {extent + thickness, height * 0.5f, thickness * 0.5f}, {1.0f, 0.0f, 0.0f}},
		{"South", {center.x, center.y + height * 0.5f, center.z - extent - thickness * 0.5f},
		 {extent + thickness, height * 0.5f, thickness * 0.5f}, {1.0f, 0.0f, 0.0f}},
		{"East", {center.x + extent + thickness * 0.5f, center.y + height * 0.5f, center.z},
		 {thickness * 0.5f, height * 0.5f, extent + thickness}, {0.0f, 0.0f, 1.0f}},
		{"West", {center.x - extent - thickness * 0.5f, center.y + height * 0.5f, center.z},
		 {thickness * 0.5f, height * 0.5f, extent + thickness}, {0.0f, 0.0f, 1.0f}}
	}};
	const float barrierHalfLength = extent + thickness;

	for (const WallPlacement& placement : placements) {
		auto wall = std::make_unique<Entity>();
		wall->SetName(std::string(kBossArenaWallPrefix) + placement.suffix);
		wall->SetEditorType("RuntimeBossArenaWall");
		wall->GetTransform().translate = placement.position;
		wall->GetTransform().scale = placement.scale;

		OBBColliderComponent* collider = wall->AddComponent<OBBColliderComponent>();
		collider->SetHalfSize({1.0f, 1.0f, 1.0f});
		collider->SetPushBackEnabled(false);
		collider->SetDrawDebug(false);

		wall->Update();
		sceneObjects_.push_back(std::move(wall));

		// 添付の透過グラデーション画像を壁面全体へ貼り、光の筋が連なる結界として表示する。
		auto barrierVisual = std::make_unique<Entity>();
		barrierVisual->SetName(std::string(kBossArenaWallPrefix) + placement.suffix + "_Visual");
		barrierVisual->SetEditorType("RuntimeBossArenaVisual");
		barrierVisual->GetTransform().translate = placement.position;
		barrierVisual->GetTransform().rotate = placement.tangent.x != 0.0f
			? Vector3{}
			: Vector3{0.0f, MathConstants::kPi * 0.5f, 0.0f};
		barrierVisual->GetTransform().scale = {barrierHalfLength, height * 0.5f, 1.0f};

		Object3dComponent* visual = barrierVisual->AddComponent<Object3dComponent>();
		visual->CreatePlane(2.0f, 2.0f);
		visual->SetTexture("Resources/gradationLine.png");
		visual->SetColor({0.24f, 0.72f, 1.0f, 0.82f});
		visual->SetEmission({0.08f, 0.48f, 1.0f}, 2.4f);
		visual->SetLightingEnabled(false);
		visual->SetShadowEnabled(false);

		barrierVisual->Update();
		sceneObjects_.push_back(std::move(barrierVisual));
	}
}

void BaseScene::UpdateBossArenaWalls() {
	bool encounterActive = false;
	for (const auto& object : sceneObjects_) {
		const EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>();
		if (spawnPoint && spawnPoint->IsBossEncounterActive()) {
			encounterActive = true;
			break;
		}
	}
	if (!encounterActive) {
		RemoveBossArenaWalls();
	}
}

void BaseScene::ConstrainPlayersToBossArena() {
	const EnemySpawnPointComponent::BossEncounterSettings* activeSettings = nullptr;
	for (const auto& object : sceneObjects_) {
		const EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>();
		if (spawnPoint && spawnPoint->IsBossEncounterActive() &&
		    spawnPoint->IsBossEncounterTriggered()) {
			activeSettings = &spawnPoint->GetBossEncounterSettings();
			break;
		}
	}
	if (!activeSettings || !activeSettings->arenaWallsEnabled) {
		return;
	}

	const float extent = (std::max)(2.0f, activeSettings->arenaHalfExtent);
	const float arenaMinX = activeSettings->bossPosition.x - extent;
	const float arenaMaxX = activeSettings->bossPosition.x + extent;
	const float arenaMinZ = activeSettings->bossPosition.z - extent;
	const float arenaMaxZ = activeSettings->bossPosition.z + extent;

	for (Entity* player : sceneObjects_.GetEntitiesWith<PlayerComponent>()) {
		const PlayerComponent* playerComponent = player ? player->GetComponent<PlayerComponent>() : nullptr;
		if (!playerComponent || !playerComponent->IsEnabled()) {
			continue;
		}

		// プレイヤーが持つすべての有効コライダーを囲むXZ範囲を求める。
		CollisionSpatialGrid::Bounds playerBounds{};
		bool hasBounds = false;
		if (const OBBColliderComponent* obb = player->GetComponent<OBBColliderComponent>();
		    obb && obb->IsEnabled()) {
			playerBounds = CollisionSpatialGrid::FromOBB(obb->GetWorldOBB());
			hasBounds = true;
		}
		if (const SphereColliderComponent* sphere = player->GetComponent<SphereColliderComponent>();
		    sphere && sphere->IsEnabled()) {
			const CollisionSpatialGrid::Bounds sphereBounds =
				CollisionSpatialGrid::FromSphere(sphere->GetWorldSphere());
			playerBounds = hasBounds
				? CollisionSpatialGrid::Merge(playerBounds, sphereBounds)
				: sphereBounds;
			hasBounds = true;
		}
		if (!hasBounds) {
			const Vector3 position = player->GetTransform().translate;
			playerBounds = CollisionSpatialGrid::FromCenterRadius(position, 0.0f);
		}

		Vector3 correction{};
		const float boundsWidth = playerBounds.maxX - playerBounds.minX;
		const float boundsDepth = playerBounds.maxZ - playerBounds.minZ;
		if (boundsWidth > extent * 2.0f) {
			correction.x = activeSettings->bossPosition.x -
				(playerBounds.minX + playerBounds.maxX) * 0.5f;
		} else if (playerBounds.minX < arenaMinX) {
			correction.x = arenaMinX - playerBounds.minX;
		} else if (playerBounds.maxX > arenaMaxX) {
			correction.x = arenaMaxX - playerBounds.maxX;
		}
		if (boundsDepth > extent * 2.0f) {
			correction.z = activeSettings->bossPosition.z -
				(playerBounds.minZ + playerBounds.maxZ) * 0.5f;
		} else if (playerBounds.minZ < arenaMinZ) {
			correction.z = arenaMinZ - playerBounds.minZ;
		} else if (playerBounds.maxZ > arenaMaxZ) {
			correction.z = arenaMaxZ - playerBounds.maxZ;
		}

		player->GetTransform().translate = player->GetTransform().translate + correction;
	}
}

void BaseScene::RemoveBossArenaWalls() {
	sceneObjects_.erase(
		std::remove_if(sceneObjects_.begin(), sceneObjects_.end(), [](const std::unique_ptr<Entity>& object) {
			return object && object->GetName().rfind(kBossArenaWallPrefix, 0) == 0;
		}),
		sceneObjects_.end());
}

void BaseScene::UpdateLoopingTerrain() {
	if (!sceneManager_) {
		return;
	}

	const std::string& stageId = sceneManager_->GetSelectedGameplayStageId();
	// ステージ1はX/Z両方向、ステージ2は壁に沿ったX方向だけを20単位で循環させる。
	const bool wrapsBothAxes = stageId == "default";
	const bool wrapsHorizontally = stageId == "stage2";
	const bool isStage2 = stageId == "stage2";
	if (!wrapsBothAxes && !wrapsHorizontally) {
		return;
	}

	Entity* playerObject = nullptr;
	Entity* centerTerrain = nullptr;
	for (const auto& object : sceneObjects_) {
		if (object->GetComponent<PlayerComponent>()) {
			playerObject = object.get();
		}
		if (object->GetName() == "terrain_7") {
			centerTerrain = object.get();
		}
	}
	if (!playerObject || !centerTerrain) {
		return;
	}

	// プレイヤーが中央タイルの端を越えた分だけ、地形側をタイル1枚分ずつ送る。
	// プレイヤー・敵・弾・ドロップなどのワールド座標は一切変更しない。
	constexpr float halfWidth = 10.0f;
	const float width = halfWidth * 2.0f;
	const Vector3 playerPosition = playerObject->GetTransform().translate;
	const Vector3 terrainPosition = centerTerrain->GetTransform().translate;
	Vector3 shift{};
	// 1フレームで複数タイル分移動しても、プレイヤーを囲む位置まで追い付かせる。
	while (playerPosition.x > terrainPosition.x + shift.x + halfWidth) shift.x += width;
	while (playerPosition.x < terrainPosition.x + shift.x - halfWidth) shift.x -= width;
	if (wrapsBothAxes) {
		constexpr float kHalfDepth = 10.0f;
		constexpr float kDepth = kHalfDepth * 2.0f;
		while (playerPosition.z > terrainPosition.z + shift.z + kHalfDepth) shift.z += kDepth;
		while (playerPosition.z < terrainPosition.z + shift.z - kHalfDepth) shift.z -= kDepth;
	}

	if (shift.x == 0.0f && shift.z == 0.0f) {
		return;
	}

	// 床の9タイルに加え、ステージ2では壁と和風背景も同じ量だけ送り、
	// ループ境界を越えても街並みの相対配置が崩れないようにする。
	auto isLoopingTerrain = [isStage2](const Entity* target) {
		if (!target) return false;
		const std::string& name = target->GetName();
		return name == "terrain_7" ||
		    name.rfind("terrain_loop_", 0) == 0 ||
		    (isStage2 && name.rfind("stage2_", 0) == 0);
	};

	for (const auto& object : sceneObjects_) {
		Entity* target = object.get();
		if (isLoopingTerrain(target)) {
			target->GetTransform().translate = target->GetTransform().translate + shift;
			// 通常のObject3d更新後に再配置するため、描画行列だけは同フレーム中に同期する。
			if (Object3dComponent* object3d = target->GetComponent<Object3dComponent>()) {
				object3d->SyncRenderState();
			}
		}
	}
}

void BaseScene::UpdateEditorObjectPicking() {
#ifdef USE_IMGUI
	constexpr int kLeftMouseButton = 0;
	Input* input = Input::GetInstance();
	if (!input->TriggerMouseButton(kLeftMouseButton)) {
		return;
	}
	if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) {
		return;
	}

	Camera* camera = Object3dCommon::GetInstance()->GetDefaultCamera();
	if (!camera) {
		return;
	}

	ImGuiViewport* mainViewport = ImGui::GetMainViewport();
	const ImVec2 gameViewPosition = ImGuiManager::GetInstance()->GetGameViewContentPosition();
	const ImVec2 gameViewSize = ImGuiManager::GetInstance()->GetGameViewContentSize();
	const float viewportOriginX = mainViewport ? mainViewport->Pos.x : 0.0f;
	const float viewportOriginY = mainViewport ? mainViewport->Pos.y : 0.0f;
	const float viewLeft = gameViewSize.x > 1.0f ? gameViewPosition.x - viewportOriginX : 0.0f;
	const float viewTop = gameViewSize.y > 1.0f ? gameViewPosition.y - viewportOriginY : 0.0f;
	const float width = gameViewSize.x > 1.0f ? gameViewSize.x : static_cast<float>(input->GetClientWidth());
	const float height = gameViewSize.y > 1.0f ? gameViewSize.y : static_cast<float>(input->GetClientHeight());
	const float mouseX = static_cast<float>(input->GetMouseClientX());
	const float mouseY = static_cast<float>(input->GetMouseClientY());
	if (mouseX < viewLeft || mouseX > viewLeft + width || mouseY < viewTop || mouseY > viewTop + height) {
		return;
	}

	const float ndcX = ((mouseX - viewLeft) / width) * 2.0f - 1.0f;
	const float ndcY = 1.0f - ((mouseY - viewTop) / height) * 2.0f;
	const Matrix4x4 inverseViewProjection = Inverse(camera->GetViewProjectionMatrix());
	const Vector3 nearPoint = BaseSceneEditorGeometry::TransformCoord({ndcX, ndcY, 0.0f}, inverseViewProjection);
	const Vector3 farPoint = BaseSceneEditorGeometry::TransformCoord({ndcX, ndcY, 1.0f}, inverseViewProjection);
	const Vector3 rayDirection = Normalize(farPoint - nearPoint);

	int hitIndex = -1;
	float nearestDistance = 100000.0f;
	for (int index = 0; index < static_cast<int>(sceneObjects_.size()); ++index) {
		float distance = 0.0f;
		if (BaseSceneEditorGeometry::IntersectRayToOBB(nearPoint, rayDirection, BaseSceneEditorGeometry::MakePickOBB(sceneObjects_[index].get()), distance) && distance < nearestDistance) {
			nearestDistance = distance;
			hitIndex = index;
		}
	}

	if (hitIndex >= 0) {
		sceneEditorState_.SelectObject(hitIndex);
	}
#endif
}

/// <summary>
/// エディタ用カメラのマウス操作を更新します。
/// </summary>
void BaseScene::UpdateEditorCameraControl() {
#ifdef USE_IMGUI
	constexpr int kLeftMouseButton = 0;
	constexpr int kMiddleMouseButton = 2;
	constexpr float kRotateSpeed = 0.004f;
	constexpr float kPanSpeed = 0.0015f;

	Input* input = Input::GetInstance();
	const bool isLeftDragging = input->PushMouseButton(kLeftMouseButton);
	const bool isMiddleDragging = input->PushMouseButton(kMiddleMouseButton);
	if (ImGui::GetIO().WantCaptureMouse || (!isLeftDragging && !isMiddleDragging)) {
		return;
	}
	if (isLeftDragging && (ImGuizmo::IsOver() || ImGuizmo::IsUsing())) {
		return;
	}

	const float moveX = static_cast<float>(input->GetMouseMoveX());
	const float moveY = static_cast<float>(input->GetMouseMoveY());
	if (moveX == 0.0f && moveY == 0.0f) {
		return;
	}

	const int selectedObjectIndex = sceneEditorState_.GetSelectedObjectIndex();
	Entity* selectedObject =
	    sceneEditorState_.HasValidSelection(sceneObjects_.size())
	        ? sceneObjects_[selectedObjectIndex].get()
	        : nullptr;

	Entity* activeCameraObject = FindObjectByName(sceneEditorState_.GetActiveCameraName());
	EulerTransform* cameraTransform = nullptr;
	Camera* fallbackCamera = nullptr;
	Vector3 cameraPosition{};
	Vector3 cameraRotate{};

	CameraComponent* activeCameraComponent =
	    activeCameraObject ? activeCameraObject->GetComponent<CameraComponent>() : nullptr;
	if (activeCameraComponent && activeCameraComponent->IsEnabled()) {
		cameraTransform = &activeCameraObject->GetTransform();
		cameraPosition = cameraTransform->translate;
		cameraRotate = cameraTransform->rotate;
	} else if (BaseSceneEditorGeometry::TryGetCameraTransform(fallbackCamera_, cameraPosition, cameraRotate)) {
		fallbackCamera = fallbackCamera_;
	} else {
		return;
	}

	if (isMiddleDragging) {
		const float pitchDelta = moveY * kRotateSpeed;
		const float yawDelta = moveX * kRotateSpeed;
		if (selectedObject) {
			const Vector3 target = selectedObject->GetTransform().translate;
			Vector3 offset = cameraPosition - target;
			if (Length(offset) > MathConstants::kDirectionEpsilon) {
				const Vector3 worldUp{0.0f, 1.0f, 0.0f};
				offset = BaseSceneEditorGeometry::RotateAroundAxis(offset, worldUp, yawDelta);

				Vector3 right = Cross(worldUp, Normalize(offset));
				if (Length(right) > MathConstants::kDirectionEpsilon) {
					offset = BaseSceneEditorGeometry::RotateAroundAxis(offset, right, pitchDelta);
				}
				cameraPosition = target + offset;
			}
		}

		cameraRotate.x += pitchDelta;
		cameraRotate.y += yawDelta;
	} else if (isLeftDragging) {
		const Matrix4x4 rotateMatrix = MakeRotateXYZMatrix(cameraRotate);
		const Vector3 rightAxis{rotateMatrix.m[0][0], rotateMatrix.m[0][1], rotateMatrix.m[0][2]};
		const Vector3 upAxis{rotateMatrix.m[1][0], rotateMatrix.m[1][1], rotateMatrix.m[1][2]};
		const Vector3 right = Normalize(rightAxis);
		const Vector3 up = Normalize(upAxis);
		float distanceScale = 10.0f;
		if (selectedObject) {
			distanceScale = Length(cameraPosition - selectedObject->GetTransform().translate);
			if (distanceScale < 1.0f) {
				distanceScale = 1.0f;
			}
		}
		cameraPosition = cameraPosition + (moveX * distanceScale * kPanSpeed) * right - (moveY * distanceScale * kPanSpeed) * up;
	}

	if (cameraTransform) {
		cameraTransform->translate = cameraPosition;
		cameraTransform->rotate = cameraRotate;
	} else if (fallbackCamera) {
		fallbackCamera->SetTranslate(cameraPosition);
		fallbackCamera->SetRotate(cameraRotate);
	}
#endif
}

/// <summary>
/// 指定したオブジェクトのカメラをアクティブカメラに設定します。
/// </summary>

void BaseScene::SetActiveCameraObject(Entity* object) {
	CameraComponent* cameraComponent = object ? object->GetComponent<CameraComponent>() : nullptr;
	if (!cameraComponent || !cameraComponent->IsEnabled()) {
		return;
	}

	sceneEditorState_.SetActiveCameraName(object->GetName());
	ApplyActiveCamera();
}

/// <summary>
/// 最初に見つかった有効なカメラオブジェクトを返します。
/// </summary>
Entity* BaseScene::FindFirstCameraObject() {
	for (const auto& object : sceneObjects_) {
		CameraComponent* cameraComponent = object->GetComponent<CameraComponent>();
		if (cameraComponent && cameraComponent->IsEnabled()) {
			return object.get();
		}
	}
	return nullptr;
}

/// <summary>
/// 名前に一致するシーンオブジェクトを検索します。
/// </summary>
Entity* BaseScene::FindObjectByName(const std::string& name) const {
	return sceneObjects_.FindByName(name);
}


