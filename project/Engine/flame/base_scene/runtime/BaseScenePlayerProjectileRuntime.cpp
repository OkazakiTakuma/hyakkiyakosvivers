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
#include "base_scene/details/BaseSceneRuntimeDetails.h"

namespace {
constexpr float kMoneyItemDropChance = 0.20f;
constexpr int kMoneyDropMinimum = 5;
constexpr int kMoneyDropMaximum = 15;

constexpr int kBossRewardRollMinimum = 1;
constexpr int kBossRewardRollMaximum = 100;
constexpr int kBossRewardSingleUpgradeThreshold = 70;
constexpr int kBossRewardTripleUpgradeThreshold = 95;
constexpr int kBossRewardSingleUpgradeCount = 1;
constexpr int kBossRewardTripleUpgradeCount = 3;
constexpr int kBossRewardRareUpgradeCount = 5;

int GetBossRewardUpgradeCount(int rewardRoll) {
	if (rewardRoll <= kBossRewardSingleUpgradeThreshold) {
		return kBossRewardSingleUpgradeCount;
	}
	if (rewardRoll <= kBossRewardTripleUpgradeThreshold) {
		return kBossRewardTripleUpgradeCount;
	}
	return kBossRewardRareUpgradeCount;
}
}

GameObject* BaseScene::FindNearestEnemy(const Vector3& position) const {
	GameObject* nearest = nullptr;
	float nearestDistance = 0.0f;
	for (const auto& object : sceneObjects_) {
		EnemyComponent* enemy = object->GetComponent<EnemyComponent>();
		if (!enemy || !enemy->IsEnabled() || enemy->GetCurrentHealth() <= 0.0f) {
			continue;
		}

		const float distance = Length(object->GetTransform().translate - position);
		if (!nearest || distance < nearestDistance) {
			nearest = object.get();
			nearestDistance = distance;
		}
	}
	return nearest;
}

GameObject* BaseScene::CreateRuntimePlayerProjectile(const PlayerAttackShotRequest& request) {
	if (request.motionType == PlayerProjectileMotionType::PoisonCloud) {
		// 五行結界はプレイヤーへ追従する持続型フィールドなので、攻撃間隔ごとに新規生成すると
		// 生存中の五芒星が同じ場所へ重なり続ける。既存の結界が消えるまでは1個だけ維持する。
		for (const auto& sceneObject : sceneObjects_) {
			PlayerProjectileComponent* existingProjectile = sceneObject->GetComponent<PlayerProjectileComponent>();
			if (existingProjectile && existingProjectile->IsEnabled() && !existingProjectile->IsExpired() &&
			    existingProjectile->GetMotionType() == PlayerProjectileMotionType::PoisonCloud) {
				return sceneObject.get();
			}
		}
	}

	auto object = std::make_unique<GameObject>();
	object->SetName(MakeUniqueObjectName(request.attackName.empty() ? "PlayerAttack" : request.attackName));
	object->SetEditorType("PlayerProjectile");
	object->GetTransform().translate = request.position;
	object->GetTransform().scale = {request.size, request.size, request.size};

	PlayerProjectileComponent* projectile = object->AddComponent<PlayerProjectileComponent>();
	projectile->SetAttackName(request.attackName);
	projectile->SetLevel(request.level);
	projectile->SetDirection(request.direction);
	projectile->SetSpeed(request.speed);
	projectile->SetAttack(request.attack);
	projectile->SetSize(request.size);
	projectile->SetLifeTime(request.lifeTime);
	projectile->SetPierceCount(request.pierceCount);
	projectile->SetInfinitePierce(request.infinitePierce);
	projectile->SetRepeatHitInterval(request.repeatHitInterval);
	projectile->SetHomingEnabled(request.homing);
	projectile->SetHomingAccuracy(request.homingAccuracy);
	projectile->SetMotionType(request.motionType);
	projectile->SetMotionAnchor(request.motionAnchor);
	projectile->SetOrbitAngleRadians(request.orbitAngleRadians);
	projectile->SetOrbitRadius(request.orbitRadius);
	projectile->SetOrbitHeight(request.orbitHeight);
	projectile->SetOrbitAngularSpeed(request.orbitAngularSpeed);
	projectile->SetTravelDistance(request.travelDistance);
	projectile->SetTravelOrigin(request.position);
	projectile->SetClawSlashIndex(request.clawSlashIndex);
	projectile->SetClawSlashCount(request.clawSlashCount);
	const PlayerProjectilePresentation presentation = projectile->GetMotionPresentation();
	if (request.repeatHitInterval <= 0.0f && presentation.repeatHitInterval > 0.0f) {
		projectile->SetRepeatHitInterval(presentation.repeatHitInterval);
	}
	if (request.homing) {
		projectile->SetHomingTarget(FindNearestEnemy(request.position));
	}
	if (presentation.aimAtNearestEnemyOnSpawn) {
		if (GameObject* target = FindNearestEnemy(request.position)) {
			Vector3 toTarget = target->GetTransform().translate - request.position;
			toTarget.y = 0.0f;
			if (Length(toTarget) > MathConstants::kDirectionEpsilon) {
				projectile->SetDirection(toTarget);
			}
		}
	}

	if (request.attackName == "Straight" && request.motionType == PlayerProjectileMotionType::Linear) {
		// 共通の弾パラメーター設定後に描画だけを差し替え、他の攻撃のモデル・軌跡には影響させない。
		// 球モデルと直線のトレイルを使わず、初フレームから完成した斬撃を表示する。
		object->AddComponent<StraightSlashVisualComponent>(request.lifeTime);
		object->Update();
		sceneObjects_.push_back(std::move(object));
		++nextObjectId_;
		return sceneObjects_.back().get();
	}

	const std::string modelFilePath = request.modelFilePath.empty() ? "sphere.obj" : request.modelFilePath;
	if (!ModelManager::GetInstance()->FindModel(modelFilePath)) {
		ModelManager::GetInstance()->LoadModel(modelFilePath);
	}
	object->GetTransform().scale = {
		request.size * presentation.scaleMultiplier.x,
		request.size * presentation.scaleMultiplier.y,
		request.size * presentation.scaleMultiplier.z
	};
	Object3dComponent* object3d = object->AddComponent<Object3dComponent>();
	if (ModelManager::GetInstance()->FindModel(modelFilePath)) {
		object3d->SetModel(modelFilePath);
	} else {
		ModelManager::GetInstance()->LoadModel("sphere.obj");
		object3d->SetModel("sphere.obj");
	}
	object3d->SetEnabled(presentation.modelVisible);
	if (request.motionType == PlayerProjectileMotionType::PoisonCloud ||
	    request.motionType == PlayerProjectileMotionType::FlameField) {
		// 持続フィールドは球モデルではなく加算合成の専用演出だけを表示するため、
		// 非表示モデルがシャドウマップへ描かれて円形の黒い影を残さないようにする。
		object3d->SetShadowEnabled(false);
	}
	if (presentation.useMagatamaPalette) {
		// 白い玉部分だけを6色に染め、黒い孔の象嵌は輪郭として残す。
		object3d->SetColor(GetMagatamaProjectileColor(request.colorIndex));
	} else if (presentation.overrideModelColor) {
		object3d->SetColor(presentation.modelColor);
	}
	if (presentation.addSkyLaserVisual) {
		// 球を引き伸ばした旧表示は隠し、加算合成の専用エフェクトでレーザーを描画する。
		object->AddComponent<SkyLaserVisualComponent>(request.lifeTime, request.size);
	}
	if (request.motionType == PlayerProjectileMotionType::PoisonCloud) {
		object->AddComponent<PoisonBarrierVisualComponent>(request.lifeTime, request.orbitHeight);
	}
	if (request.motionType == PlayerProjectileMotionType::FlameField) {
		object->AddComponent<FlameFieldVisualComponent>(request.lifeTime, request.size, request.orbitHeight);
	}
	if (presentation.addTrail) {
		TrailRendererComponent* trail = object->AddComponent<TrailRendererComponent>();
		trail->SetWidth((std::max)(presentation.minimumTrailWidth, request.size * presentation.trailWidthMultiplier));
		trail->SetLifeTime(presentation.trailLifeTime);
		trail->SetMinSegmentLength((std::max)(0.025f, request.size * 0.08f));
		if (presentation.trailUsesMotionAnchor) {
			trail->SetPositionReference(request.motionAnchor);
		}
		if (presentation.useMagatamaPalette) {
			Vector4 headColor = GetMagatamaProjectileColor(request.colorIndex);
			headColor.w = 0.95f;
			trail->SetHeadColor(headColor);
			trail->SetTailColor(GetMagatamaTrailTailColor(request.colorIndex));
		} else if (presentation.overrideTrailColors) {
			trail->SetHeadColor(presentation.trailHeadColor);
			trail->SetTailColor(presentation.trailTailColor);
		}
		if (presentation.addCoreTrail) {
			// 細い白熱線を外光へ重ね、一本ごとの切っ先を読み取りやすくする。
			TrailRendererComponent* coreTrail = object->AddComponent<TrailRendererComponent>();
			coreTrail->SetWidth((std::max)(0.045f, request.size * 0.085f));
			coreTrail->SetLifeTime(0.16f);
			coreTrail->SetMinSegmentLength((std::max)(0.018f, request.size * 0.055f));
			coreTrail->SetPositionReference(request.motionAnchor);
			coreTrail->SetHeadColor({1.0f, 1.0f, 0.86f, 1.0f});
			coreTrail->SetTailColor({1.0f, 0.28f, 0.02f, 0.0f});
		}
	}
	if (presentation.addGlowEmitter) {
		// 環境反射ではなく、加算合成パーティクルを弾から漏れ出す光として連続発生させる。
		constexpr const char* kGlowParticleGroup = "MagatamaGlow";
		constexpr const char* kGlowTexture = "Resources/circle.png";
		if (!ParticleManager::GetInstance()->HasGroup(kGlowParticleGroup)) {
			ParticleManager::GetInstance()->CreateParticleGroup(kGlowParticleGroup, kGlowTexture, kMeshTypeQuad);
		}

		ParticleEmitterComponent* glowEmitter = object->AddComponent<ParticleEmitterComponent>();
		glowEmitter->SetGroupName(kGlowParticleGroup);
		glowEmitter->SetTexture(kGlowTexture);
		glowEmitter->SetBlendMode(kBlendModeAdd);
		glowEmitter->SetFrequency(0.020f);

		Vector4 glowColor = GetMagatamaProjectileColor(request.colorIndex);
		glowColor.x *= 1.35f;
		glowColor.y *= 1.35f;
		glowColor.z *= 1.35f;
		glowColor.w = 0.55f;

		ParticleEmitParam glowParam;
		const float glowScale = (std::max)(0.16f, request.size * 0.82f);
		glowParam.scale = {glowScale, glowScale, glowScale};
		glowParam.endScale = {glowScale * 0.12f, glowScale * 0.12f, glowScale * 0.12f};
		glowParam.baseVelocity = {0.0f, 0.008f, 0.0f};
		glowParam.randomVelocityRange = {0.018f, 0.018f, 0.018f};
		const float leakRadius = request.size * 0.42f;
		glowParam.randomPositionRange = {leakRadius, leakRadius, leakRadius};
		glowParam.lifeTime = 0.36f;
		glowParam.color = glowColor;
		glowParam.endColor = {glowColor.x * 0.55f, glowColor.y * 0.55f, glowColor.z * 0.55f, 0.0f};
		glowParam.randomScaleRange = {glowScale * 0.22f, glowScale * 0.22f, glowScale * 0.22f};
		glowParam.count = 2;
		glowParam.isBillboard = true;
		glowEmitter->SetParam(glowParam);
		// Component::Update より前に即時発生させるため、初回分も弾の生成位置へ同期しておく。
		glowEmitter->SetTranslate(request.position);
		glowEmitter->Emit();
	}

	object->Update();
	sceneObjects_.push_back(std::move(object));
	++nextObjectId_;
	return sceneObjects_.back().get();
}

void BaseScene::UpdatePlayerProjectileHits() {
	struct ExperienceDropRequest {
		EnemyStats stats;
		Vector3 position;
		GameObject* target = nullptr;
	};
	struct BossUpgradeDropRequest {
		// 生成する報酬は常に1個で、upgradeCountだけが抽選結果に応じて変化する。
		Vector3 position;
		GameObject* target = nullptr;
		int upgradeCount = 1;
	};
	struct ItemDropRequest {
		ItemDropType type = ItemDropType::Health;
		Vector3 position;
		GameObject* target = nullptr;
		float healAmount = 0.0f;
		int moneyAmount = 0;
	};
	std::vector<ExperienceDropRequest> experienceDropRequests;
	std::vector<BossUpgradeDropRequest> bossUpgradeDropRequests;
	// sceneObjects_の走査中に要素を追加すると反復子が無効になるため、生成要求を一旦蓄積する。
	std::vector<ItemDropRequest> itemDropRequests;
	Camera* camera = Object3dCommon::GetInstance() ? Object3dCommon::GetInstance()->GetDefaultCamera() : nullptr;
	for (const auto& projectileObject : sceneObjects_) {
		PlayerProjectileComponent* projectile = projectileObject->GetComponent<PlayerProjectileComponent>();
		if (!projectile || projectile->IsExpired() ||
		    !projectile->GetMotionPresentation().ricochets) {
			continue;
		}

		Vector3& position = projectileObject->GetTransform().translate;
		Vector3 currentNdc{};
		Vector3 aheadNdc{};
		// ワールド座標を画面座標へ投影し、左右端・上下端に応じて画面上の進行成分を反転する。
		if (camera && ProjectToNdc(camera, position, currentNdc) &&
		    ProjectToNdc(camera, position + projectile->GetDirection(), aheadNdc)) {
			const float edgeInset = (std::clamp)(0.025f + projectile->GetSize() * 0.02f, 0.025f, 0.20f);
			const float boundary = 1.0f - edgeInset;
			const bool hitHorizontalEdge = currentNdc.x < -boundary || currentNdc.x > boundary;
			const bool hitVerticalEdge = currentNdc.y < -boundary || currentNdc.y > boundary;
			if (hitHorizontalEdge || hitVerticalEdge) {
				Vector2 screenDirection{aheadNdc.x - currentNdc.x, aheadNdc.y - currentNdc.y};
				if (hitHorizontalEdge) {
					screenDirection.x = -screenDirection.x;
				}
				if (hitVerticalEdge) {
					screenDirection.y = -screenDirection.y;
				}
				const Vector2 clampedNdc{
				    (std::clamp)(currentNdc.x, -boundary, boundary),
				    (std::clamp)(currentNdc.y, -boundary, boundary)
				};
				const Vector2 reflectedNdc{clampedNdc.x + screenDirection.x, clampedNdc.y + screenDirection.y};
				Vector3 boundaryPoint{};
				Vector3 reflectedPoint{};
				if (IntersectScreenRayToHeight(camera, clampedNdc, position.y, boundaryPoint) &&
				    IntersectScreenRayToHeight(camera, reflectedNdc, position.y, reflectedPoint)) {
					position = boundaryPoint;
					projectile->SetDirection(reflectedPoint - boundaryPoint);
				}
			}
		}

		// 障害物との反射は弾を球として扱い、衝突面の法線から反射ベクトルを求める。
		SphereColliderShape projectileSphere{position, projectile->GetSize()};
		for (const auto& obstacleObject : sceneObjects_) {
			if (obstacleObject.get() == projectileObject.get() ||
			    obstacleObject->GetComponent<Player>() ||
			    obstacleObject->GetComponent<EnemyComponent>() ||
			    obstacleObject->GetComponent<PlayerProjectileComponent>()) {
				continue;
			}

			Vector3 surfaceNormal{};
			float penetration = 0.0f;
			bool collided = false;
			if (OBBColliderComponent* obstacle = obstacleObject->GetComponent<OBBColliderComponent>();
			    obstacle && obstacle->IsEnabled() && obstacle->GetPushBackEnabled()) {
				collided = BaseSceneCollisionHelpers::CalculateOBBSpherePushBack(obstacle->GetWorldOBB(), projectileSphere, surfaceNormal, penetration);
			}
			if (!collided) {
				if (SphereColliderComponent* obstacle = obstacleObject->GetComponent<SphereColliderComponent>();
				    obstacle && obstacle->IsEnabled() && obstacle->GetPushBackEnabled()) {
					Vector3 projectileToObstacle{};
					collided = BaseSceneCollisionHelpers::CalculateSphereSpherePushBack(projectileSphere, obstacle->GetWorldSphere(), projectileToObstacle, penetration);
					surfaceNormal = -1.0f * projectileToObstacle;
				}
			}
			if (!collided) {
				continue;
			}

			surfaceNormal.y = 0.0f;
			if (Length(surfaceNormal) <= MathConstants::kDirectionEpsilon) {
				continue;
			}
			surfaceNormal = NormalizeReturnVector(surfaceNormal);
			const float incomingAmount = Dot(projectile->GetDirection(), surfaceNormal);
			if (incomingAmount >= 0.0f) {
				continue;
			}
			// めり込みを解消してから反射させ、同じ面へ連続衝突することを防ぐ。
			position = position + (penetration + 0.01f) * surfaceNormal;
			projectile->SetDirection(projectile->GetDirection() - (2.0f * incomingAmount) * surfaceNormal);
			projectileObject->GetTransform().rotate.y = std::atan2(projectile->GetDirection().x, projectile->GetDirection().z);
			break;
		}
	}
	// 生存中の敵だけを空間グリッドへ登録し、各弾が近傍の敵だけを精密判定する。
	collisionSpatialGrid_.Clear();
	collisionDebugStats_.playerProjectileCandidates = 0;
	collisionDebugStats_.playerProjectileNarrowPhaseTests = 0;
	for (const auto& enemyObject : sceneObjects_) {
		EnemyComponent* enemy = enemyObject->GetComponent<EnemyComponent>();
		if (!enemy || !enemy->IsEnabled() || enemy->GetCurrentHealth() <= 0.0f) {
			continue;
		}
		const float enemyRadius = 0.5f * enemy->GetStats().sizeScale;
		collisionSpatialGrid_.Insert(
			enemyObject.get(),
			CollisionSpatialGrid::FromCenterRadius(enemyObject->GetTransform().translate, enemyRadius));
	}

	for (const auto& projectileObject : sceneObjects_) {
		PlayerProjectileComponent* projectile = projectileObject->GetComponent<PlayerProjectileComponent>();
		if (!projectile || !projectile->IsEnabled() || projectile->IsExpired()) {
			continue;
		}

		const PlayerProjectilePresentation presentation = projectile->GetMotionPresentation();
		const float projectileQueryRadius = presentation.usesVerticalHitArea
			? projectile->GetSize() * 0.35f
			: projectile->GetSize();
		const std::vector<GameObject*> enemyCandidates = collisionSpatialGrid_.Query(
			CollisionSpatialGrid::FromCenterRadius(
				projectileObject->GetTransform().translate, projectileQueryRadius));
		collisionDebugStats_.playerProjectileCandidates += enemyCandidates.size();

		for (GameObject* enemyObject : enemyCandidates) {
			EnemyComponent* enemy = enemyObject->GetComponent<EnemyComponent>();
			if (!enemy || !enemy->IsEnabled() || enemy->GetCurrentHealth() <= 0.0f) {
				continue;
			}
			if (projectile->HasHitObject(enemyObject)) {
				continue;
			}
			++collisionDebugStats_.playerProjectileNarrowPhaseTests;

			float distance = Length(enemyObject->GetTransform().translate - projectileObject->GetTransform().translate);
			float hitRadius = projectile->GetSize() + 0.5f * enemy->GetStats().sizeScale;
			if (presentation.usesVerticalHitArea) {
				Vector3 horizontalDifference = enemyObject->GetTransform().translate - projectileObject->GetTransform().translate;
				horizontalDifference.y = 0.0f;
				distance = Length(horizontalDifference);
				hitRadius = projectile->GetSize() * 0.35f + 0.5f * enemy->GetStats().sizeScale;
			}
			if (distance <= hitRadius) {
				enemy->TakeDamage(projectile->GetAttack());
				if (enemy->GetCurrentHealth() <= 0.0f) {
					// 生存中の敵だけがこの分岐へ入るため、1体につき一度だけ加算される。
					++defeatedEnemyCount_;
					experienceDropRequests.push_back({enemy->GetStats(), enemyObject->GetTransform().translate, enemy->GetTarget()});
					static std::mt19937 itemDropRandomEngine(std::random_device{}());
					std::uniform_real_distribution<float> itemDropDistribution(0.0f, 1.0f);
					const EnemyStats& enemyStats = enemy->GetStats();
					// 各アイテムを独立して抽選するため、同じ敵から複数種類が同時に落ちる場合がある。
					if (itemDropDistribution(itemDropRandomEngine) < enemyStats.healthItemDropChance) {
						itemDropRequests.push_back({
							ItemDropType::Health, enemyObject->GetTransform().translate,
							enemy->GetTarget(), enemyStats.healthItemHealAmount
						});
					}
					if (itemDropDistribution(itemDropRandomEngine) < enemyStats.collectExperienceItemDropChance) {
						itemDropRequests.push_back({
							ItemDropType::CollectAllExperience, enemyObject->GetTransform().translate,
							enemy->GetTarget(), 0.0f
						});
					}
					if (itemDropDistribution(itemDropRandomEngine) < kMoneyItemDropChance) {
						std::uniform_int_distribution<int> moneyDistribution(kMoneyDropMinimum, kMoneyDropMaximum);
						itemDropRequests.push_back({
							ItemDropType::Money, enemyObject->GetTransform().translate,
							enemy->GetTarget(), 0.0f, moneyDistribution(itemDropRandomEngine)
						});
					}
					const std::string& enemyTypeName = enemy->GetEnemyTypeName();
					// 通常敵を除外し、途中の中ボスと各ステージの最終ボスだけを強化報酬の対象にする。
					const bool dropsBossUpgradeReward =
					    enemyTypeName == "MidBoss" ||
					    enemyTypeName == "ChaserMidBoss" ||
					    enemyTypeName == "ShooterMidBoss" ||
					    enemyTypeName == "ChargerMidBoss" ||
					    enemyTypeName == "Stage2GaleMidBoss" ||
					    enemyTypeName == "Stage2Boss";
					if (dropsBossUpgradeReward) {
						static std::mt19937 bossRewardRandomEngine(std::random_device{}());
						std::uniform_int_distribution<int> rewardRollDistribution(
							kBossRewardRollMinimum, kBossRewardRollMaximum);
						const int rewardRoll = rewardRollDistribution(bossRewardRandomEngine);
						const int upgradeCount = GetBossRewardUpgradeCount(rewardRoll);
						bossUpgradeDropRequests.push_back({
							enemyObject->GetTransform().translate,
							enemy->GetTarget(),
							upgradeCount
						});
					}
				}
				projectile->RegisterHitObject(enemyObject);
				break;
			}
		}
	}

	for (const ExperienceDropRequest& request : experienceDropRequests) {
		CreateRuntimeExperience(request.stats, request.position, request.target);
	}
	for (const BossUpgradeDropRequest& request : bossUpgradeDropRequests) {
		CreateRuntimeBossUpgradeDrop(request.position, request.target, request.upgradeCount);
	}
	for (const ItemDropRequest& request : itemDropRequests) {
		CreateRuntimeItemDrop(request.type, request.position, request.target, request.healAmount, request.moneyAmount);
	}
}

void BaseScene::CleanupExpiredPlayerProjectiles() {
	sceneObjects_.erase(
	    std::remove_if(sceneObjects_.begin(), sceneObjects_.end(), [](const std::unique_ptr<GameObject>& object) {
		    PlayerProjectileComponent* projectile = object->GetComponent<PlayerProjectileComponent>();
		    if (projectile) {
			    // 弾全体が画面端を抜けてから消えるよう、表示サイズを余白へ反映する。
			    const float viewMargin = 0.05f + projectile->GetSize() * 0.02f;
			    // 周回弾や反射弾は専用挙動を維持し、画面外へ飛び去る弾だけを破棄する。
			    const bool shouldExpireOutsideView = projectile->GetMotionPresentation().expiresOutsideView;
			    if (projectile->IsExpired() ||
				    (shouldExpireOutsideView && IsPointOutsideView(object->GetTransform().translate, viewMargin))) {
				    return true;
			    }
		    }
		    EnemyProjectileComponent* enemyProjectile = object->GetComponent<EnemyProjectileComponent>();
		    if (enemyProjectile && enemyProjectile->IsExpired()) {
			    return true;
		    }
		    EnemyComponent* enemy = object->GetComponent<EnemyComponent>();
		    if (enemy && enemy->GetCurrentHealth() <= 0.0f && enemy->IsDeathEffectFinished()) {
			    return true;
		    }
		    ExperienceComponent* experience = object->GetComponent<ExperienceComponent>();
		    if (experience && (experience->IsCollected() || experience->IsConsumedByCompression())) {
			    return true;
		    }
		    ItemDropComponent* item = object->GetComponent<ItemDropComponent>();
		    return item && item->IsCollected() && item->IsEffectApplied();
	    }),
	    sceneObjects_.end()
	);
	if (selectedObjectIndex_ >= static_cast<int>(sceneObjects_.size())) {
		selectedObjectIndex_ = static_cast<int>(sceneObjects_.size()) - 1;
	}
}


