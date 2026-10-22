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
	if (gameViewAspectRatio > 0.0f) {
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
			object3dComponent->SetCamera(camera);
			if (object3dComponent->GetObject3d()) {
				object3dComponent->GetObject3d()->Update();
			}
		}
	}
}

/// <summary>
/// 現在選択されているアクティブカメラを反映します。
/// </summary>
void BaseScene::ApplyActiveCamera() {
	if (activeCameraObjectName_.empty()) {
		ApplyCamera(fallbackCamera_);
		return;
	}

	GameObject* cameraObject = FindObjectByName(activeCameraObjectName_);
	if (!cameraObject) {
		activeCameraObjectName_.clear();
		ApplyCamera(fallbackCamera_);
		return;
	}

	CameraComponent* cameraComponent = cameraObject->GetComponent<CameraComponent>();
	if (!cameraComponent || !cameraComponent->IsEnabled() || !cameraComponent->GetCamera()) {
		activeCameraObjectName_.clear();
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
	for (const auto& object : sceneObjects_) {
		CameraComponent* cameraComponent = object->GetComponent<CameraComponent>();
		if (!cameraComponent || !cameraComponent->IsEnabled() || cameraComponent->GetFollowTargetName().empty()) {
			continue;
		}

		GameObject* target = FindObjectByName(cameraComponent->GetFollowTargetName());
		if (target && target != object.get() && target != cameraComponent->GetFollowTarget()) {
			cameraComponent->SetFollowTarget(target);
		}
	}
}

void BaseScene::ResolveEnemySpawnPointLinks() {
	GameObject* firstPlayer = nullptr;
	for (const auto& object : sceneObjects_) {
		if (object->GetComponent<Player>()) {
			firstPlayer = object.get();
			break;
		}
	}

	for (const auto& object : sceneObjects_) {
		EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>();
		if (!spawnPoint || !spawnPoint->IsEnabled()) {
			continue;
		}

		GameObject* target = spawnPoint->GetTargetName().empty() ? firstPlayer : FindObjectByName(spawnPoint->GetTargetName());
		if (target && target != spawnPoint->GetTarget()) {
			spawnPoint->SetTarget(target);
			if (spawnPoint->GetTargetName().empty()) {
				spawnPoint->SetTargetName(target->GetName());
			}
		}

		Camera* camera = nullptr;
		if (!spawnPoint->GetCameraName().empty()) {
			GameObject* cameraObject = FindObjectByName(spawnPoint->GetCameraName());
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
	GameObject* firstPlayer = nullptr;
	for (const auto& object : sceneObjects_) {
		if (object->GetComponent<Player>()) {
			firstPlayer = object.get();
			break;
		}
	}

	for (const auto& object : sceneObjects_) {
		EnemyComponent* enemy = object->GetComponent<EnemyComponent>();
		if (!enemy || !enemy->IsEnabled()) {
			continue;
		}

		GameObject* target = enemy->GetTargetName().empty() ? firstPlayer : FindObjectByName(enemy->GetTargetName());
		if (target && target != enemy->GetTarget()) {
			enemy->SetTarget(target);
			if (enemy->GetTargetName().empty()) {
				enemy->SetTargetName(target->GetName());
			}
		}
	}
}

void BaseScene::UpdateEnemySpawning() {
	// ボス名を保持している間は通常スポーンを完全停止し、撃破後も報酬処理が終わるまでクリアを確定しない。
	if (!activeBossEncounterObjectName_.empty()) {
		GameObject* bossObject = FindObjectByName(activeBossEncounterObjectName_);
		EnemyComponent* bossEnemy = bossObject ? bossObject->GetComponent<EnemyComponent>() : nullptr;
		const bool bossIsAlive = bossEnemy && bossEnemy->IsEnabled() && bossEnemy->GetCurrentHealth() > 0.0f;
		for (const auto& object : sceneObjects_) {
			if (EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>()) {
				spawnPoint->SetBossEncounterActive(bossIsAlive);
			}
		}
		if (bossIsAlive) {
			return;
		}
		// 最終ボスの金色報酬を拾い、強化選択を終えるまではリザルトへ移動しない。
		for (const auto& object : sceneObjects_) {
			ExperienceComponent* reward = object->GetComponent<ExperienceComponent>();
			if (reward && reward->IsBossUpgradeReward() &&
			    (!reward->IsCollected() || !reward->IsBossUpgradeApplied())) {
				return;
			}
		}
		if (IsLevelUpSelectionActive() || !bossAcquisitionOfferQueue_.empty()) {
			return;
		}
		// Boss Encounter はステージの最後に一度だけ発生するため、撃破をクリア条件とする。
		isStageCleared_ = true;
		activeBossEncounterObjectName_.clear();
		return;
	}

	struct SpawnRequest {
		std::string enemyTypeName;
		Vector3 position;
		GameObject* target = nullptr;
		EnemySpawnPointComponent::EnemyStatMultipliers statMultipliers{};
	};
	std::vector<SpawnRequest> spawnRequests;
	EnemySpawnPointComponent* triggeredBossSpawnPoint = nullptr;
	for (const auto& object : sceneObjects_) {
		EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>();
		if (!spawnPoint || !spawnPoint->IsEnabled()) {
			continue;
		}

		if (spawnPoint->ConsumeBossEncounterRequest()) {
			triggeredBossSpawnPoint = spawnPoint;
			break;
		}

		if (!spawnPoint->GetSpawnSchedules().empty()) {
			const std::vector<EnemySpawnPointComponent::ScheduledSpawnRequest> scheduledRequests = spawnPoint->ConsumeScheduledSpawnRequests();
			for (const EnemySpawnPointComponent::ScheduledSpawnRequest& request : scheduledRequests) {
				spawnRequests.push_back({request.enemyTypeName, request.position, spawnPoint->GetTarget(), request.statMultipliers});
			}
		} else {
			const std::string enemyTypeName = spawnPoint->GetEnemyTypeName();
			const EnemyStats stats = LoadEnemyStats(enemyTypeName);
			Vector3 spawnPosition{};
			if (spawnPoint->ConsumeSpawnRequest(stats.spawnsPerMinute, spawnPosition)) {
				spawnRequests.push_back({enemyTypeName, spawnPosition, spawnPoint->GetTarget(), spawnPoint->GetCurrentTimeScaling()});
			}
		}
	}

	if (triggeredBossSpawnPoint) {
		const EnemySpawnPointComponent::BossEncounterSettings& bossSettings =
		    triggeredBossSpawnPoint->GetBossEncounterSettings();
		GameObject* player = triggeredBossSpawnPoint->GetTarget();
		if (player) {
			// ボス生成より先にプレイヤーを戦闘位置へ移し、落下速度を残さない。
			player->GetTransform().translate = bossSettings.playerWarpPosition;
			if (Player* playerComponent = player->GetComponent<Player>()) {
				playerComponent->ResetGravityVelocity();
			}
		}

		// ボス戦へ通常敵や途中の中ボスを持ち越さないよう、全EnemyComponentを即時除去する。
		sceneObjects_.erase(
		    std::remove_if(sceneObjects_.begin(), sceneObjects_.end(), [](const std::unique_ptr<GameObject>& object) {
			    return object->GetComponent<EnemyComponent>() != nullptr;
		    }),
		    sceneObjects_.end()
		);
		selectedObjectIndex_ = -1;
		for (const auto& object : sceneObjects_) {
			if (EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>()) {
				spawnPoint->SetBossEncounterActive(true);
			}
		}

		GameObject* bossObject = CreateRuntimeEnemy(bossSettings.enemyTypeName, bossSettings.bossPosition, player);
		if (bossObject) {
			activeBossEncounterObjectName_ = bossObject->GetName();
		}
		return;
	}

	for (const SpawnRequest& request : spawnRequests) {
		Vector3 spawnPosition = request.position;
		// ステージ2では画面外の生成候補も上下の壁より内側へ収める。
		if (sceneManager && sceneManager->GetSelectedGameplayStageId() == "stage2") {
			constexpr float kStage2SpawnLimitZ = 8.5f;
			spawnPosition.z = std::clamp(spawnPosition.z, -kStage2SpawnLimitZ, kStage2SpawnLimitZ);
		}
		CreateRuntimeEnemy(request.enemyTypeName, spawnPosition, request.target, request.statMultipliers);
	}
}

void BaseScene::UpdateLoopingTerrain() {
	if (!sceneManager) {
		return;
	}

	const std::string& stageId = sceneManager->GetSelectedGameplayStageId();
	// 平原はX/Zの両方向、ステージ2は壁と平行なX方向だけをループ対象にする。
	const bool wrapsBothAxes = stageId == "default";
	const bool wrapsHorizontally = stageId == "stage2";
	if (!wrapsBothAxes && !wrapsHorizontally) {
		return;
	}

	GameObject* playerObject = nullptr;
	GameObject* centerTerrain = nullptr;
	for (const auto& object : sceneObjects_) {
		if (object->GetComponent<Player>()) {
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
	const float halfWidth = wrapsBothAxes ? 10.0f : 20.0f;
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

	// 床の9タイルに加え、ステージ2の上下壁もX方向へ同期させて回廊を途切れさせない。
	auto isLoopingTerrain = [wrapsHorizontally](const GameObject* target) {
		if (!target) return false;
		const std::string& name = target->GetName();
		return name == "terrain_7" ||
		    name.rfind("terrain_loop_", 0) == 0 ||
		    (wrapsHorizontally && name.rfind("stage2_wall_", 0) == 0);
	};

	for (const auto& object : sceneObjects_) {
		GameObject* target = object.get();
		if (isLoopingTerrain(target)) {
			target->GetTransform().translate = target->GetTransform().translate + shift;
			// 通常のObject3d更新後に再配置するため、描画行列だけは同フレーム中に同期する。
			if (Object3dComponent* object3d = target->GetComponent<Object3dComponent>()) {
				object3d->Update();
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
		selectedObjectIndex_ = hitIndex;
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

	GameObject* selectedObject =
	    selectedObjectIndex_ >= 0 && selectedObjectIndex_ < static_cast<int>(sceneObjects_.size())
	        ? sceneObjects_[selectedObjectIndex_].get()
	        : nullptr;

	GameObject* activeCameraObject = FindObjectByName(activeCameraObjectName_);
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

void BaseScene::SetActiveCameraObject(GameObject* object) {
	CameraComponent* cameraComponent = object ? object->GetComponent<CameraComponent>() : nullptr;
	if (!cameraComponent || !cameraComponent->IsEnabled()) {
		return;
	}

	activeCameraObjectName_ = object->GetName();
	ApplyActiveCamera();
}

/// <summary>
/// 最初に見つかった有効なカメラオブジェクトを返します。
/// </summary>
GameObject* BaseScene::FindFirstCameraObject() {
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
GameObject* BaseScene::FindObjectByName(const std::string& name) const {
	if (name.empty()) {
		return nullptr;
	}

	for (const auto& object : sceneObjects_) {
		if (object->GetName() == name) {
			return object.get();
		}
	}
	return nullptr;
}


