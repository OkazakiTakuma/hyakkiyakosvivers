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
#include <unordered_map>
#include <unordered_set>
#include <Xinput.h>
#ifdef USE_IMGUI
#include "../../../../../imgui/ImGuizmo.h"
#endif
#include "base_scene/details/BaseSceneRuntimeDetails.h"

void BaseScene::UpdateColliderCollisions() {
	std::vector<EnemyPlayerContact> enemyPlayerContacts;
	struct ColliderRecord {
		GameObject* owner = nullptr;
		OBBColliderComponent* obb = nullptr;
		SphereColliderComponent* sphere = nullptr;
		CollisionSpatialGrid::Bounds bounds{};
	};
	std::vector<ColliderRecord> colliderRecords;
	colliderRecords.reserve(sceneObjects_.size());
	collisionSpatialGrid_.Clear();
	collisionDebugStats_.colliderCandidatePairs = 0;
	collisionDebugStats_.colliderNarrowPhaseTests = 0;

	for (const auto& object : sceneObjects_) {
		OBBColliderComponent* collider = object->GetComponent<OBBColliderComponent>();
		const bool hasOBB = collider && collider->IsEnabled();
		if (collider) {
			collider->SetColliding(false);
		}
		SphereColliderComponent* sphereCollider = object->GetComponent<SphereColliderComponent>();
		const bool hasSphere = sphereCollider && sphereCollider->IsEnabled();
		if (sphereCollider) {
			sphereCollider->SetColliding(false);
		}

		if (!hasOBB && !hasSphere) {
			continue;
		}

		CollisionSpatialGrid::Bounds bounds{};
		if (hasOBB) {
			bounds = CollisionSpatialGrid::FromOBB(collider->GetWorldOBB());
		}
		if (hasSphere) {
			const CollisionSpatialGrid::Bounds sphereBounds =
				CollisionSpatialGrid::FromSphere(sphereCollider->GetWorldSphere());
			bounds = hasOBB ? CollisionSpatialGrid::Merge(bounds, sphereBounds) : sphereBounds;
		}
		colliderRecords.push_back({object.get(), hasOBB ? collider : nullptr, hasSphere ? sphereCollider : nullptr, bounds});
		collisionSpatialGrid_.Insert(object.get(), bounds);
	}

	collisionDebugStats_.colliderObjects = colliderRecords.size();
	collisionDebugStats_.occupiedCells = collisionSpatialGrid_.GetOccupiedCellCount();
	collisionDebugStats_.gridEntries = collisionSpatialGrid_.GetEntryCount();

	std::unordered_map<GameObject*, size_t> recordIndices;
	recordIndices.reserve(colliderRecords.size());
	for (size_t index = 0; index < colliderRecords.size(); ++index) {
		recordIndices.emplace(colliderRecords[index].owner, index);
	}

	for (ColliderRecord& recordA : colliderRecords) {
		for (GameObject* candidateOwner : collisionSpatialGrid_.Query(recordA.bounds)) {
			if (recordA.owner->GetRuntimeId() >= candidateOwner->GetRuntimeId()) {
				continue;
			}
			const auto recordIndex = recordIndices.find(candidateOwner);
			if (recordIndex == recordIndices.end()) {
				continue;
			}
			ColliderRecord* recordB = &colliderRecords[recordIndex->second];
			if (ShouldSkipColliderPair(recordA.owner, recordB->owner)) {
				continue;
			}
			++collisionDebugStats_.colliderCandidatePairs;

			auto registerContact = [&]() {
				return RegisterEnemyPlayerContact(recordA.owner, recordB->owner, enemyPlayerContacts);
			};

			if (recordA.obb && recordB->obb) {
				++collisionDebugStats_.colliderNarrowPhaseTests;
				const OBBColliderShape shapeA = recordA.obb->GetWorldOBB();
				const OBBColliderShape shapeB = recordB->obb->GetWorldOBB();
				if (IsCollisionOBBToOBB(shapeA, shapeB)) {
					recordA.obb->SetColliding(true);
					recordB->obb->SetColliding(true);
					if (!registerContact()) {
						Vector3 direction{};
						float penetration = 0.0f;
						if (BaseSceneCollisionHelpers::CalculateOBBOBBPushBack(shapeA, shapeB, direction, penetration)) {
							BaseSceneCollisionHelpers::ApplyColliderPushBack(
								recordA.owner, recordA.obb->GetPushBackEnabled(),
								recordB->owner, recordB->obb->GetPushBackEnabled(), direction, penetration);
						}
					}
				}
			}

			if (recordA.sphere && recordB->sphere) {
				++collisionDebugStats_.colliderNarrowPhaseTests;
				const SphereColliderShape shapeA = recordA.sphere->GetWorldSphere();
				const SphereColliderShape shapeB = recordB->sphere->GetWorldSphere();
				if (IsCollisionSphereToSphere(shapeA, shapeB)) {
					recordA.sphere->SetColliding(true);
					recordB->sphere->SetColliding(true);
					if (!registerContact()) {
						Vector3 direction{};
						float penetration = 0.0f;
						if (BaseSceneCollisionHelpers::CalculateSphereSpherePushBack(shapeA, shapeB, direction, penetration)) {
							BaseSceneCollisionHelpers::ApplyColliderPushBack(
								recordA.owner, recordA.sphere->GetPushBackEnabled(),
								recordB->owner, recordB->sphere->GetPushBackEnabled(), direction, penetration);
						}
					}
				}
			}

			auto testOBBSphere = [&](ColliderRecord& obbRecord, OBBColliderComponent* obb,
				ColliderRecord& sphereRecord, SphereColliderComponent* sphere) {
				++collisionDebugStats_.colliderNarrowPhaseTests;
				const OBBColliderShape obbShape = obb->GetWorldOBB();
				const SphereColliderShape sphereShape = sphere->GetWorldSphere();
				if (!IsCollisionOBBToSphere(obbShape, sphereShape)) {
					return;
				}
				obb->SetColliding(true);
				sphere->SetColliding(true);
				if (registerContact()) {
					return;
				}
				Vector3 direction{};
				float penetration = 0.0f;
				if (BaseSceneCollisionHelpers::CalculateOBBSpherePushBack(obbShape, sphereShape, direction, penetration)) {
					BaseSceneCollisionHelpers::ApplyColliderPushBack(
						obbRecord.owner, obb->GetPushBackEnabled(), sphereRecord.owner,
						sphere->GetPushBackEnabled(), direction, penetration);
				}
			};
			if (recordA.obb && recordB->sphere) {
				testOBBSphere(recordA, recordA.obb, *recordB, recordB->sphere);
			}
			if (recordB->obb && recordA.sphere) {
				testOBBSphere(*recordB, recordB->obb, recordA, recordA.sphere);
			}
		}
	}

	std::vector<std::pair<Player*, float>> playerDamageTotals;
	// 同じフレームで複数の敵へ触れた場合はプレイヤー単位で合算し、TakeDamageを一度だけ呼ぶ。
	for (const EnemyPlayerContact& contact : enemyPlayerContacts) {
		auto total = std::find_if(playerDamageTotals.begin(), playerDamageTotals.end(), [&contact](const std::pair<Player*, float>& entry) {
			return entry.first == contact.player;
		});
		if (total == playerDamageTotals.end()) {
			playerDamageTotals.push_back({contact.player, contact.enemy->GetContactAttackDamage()});
		} else {
			total->second += contact.enemy->GetContactAttackDamage();
		}
	}
	for (const auto& [player, totalDamage] : playerDamageTotals) {
		player->TakeDamage(totalDamage);
	}
}

/// <summary>
/// マウスクリックによるエディタオブジェクト選択を更新します。
/// </summary>


