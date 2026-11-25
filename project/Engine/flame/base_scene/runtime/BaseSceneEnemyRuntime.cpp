#include "base_scene/BaseScene.h"
#include "data/PlayerAttackLevel.h"
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
constexpr const char* kDropParticleGroup = "RuntimeDropSparkle";
constexpr const char* kDropParticleTexture = "Resources/circle.png";
constexpr const char* kSelfDestructCoreGroup = "RuntimeSelfDestructCore";
constexpr const char* kSelfDestructFlameGroup = "RuntimeSelfDestructFlame";
constexpr const char* kSelfDestructSparkGroup = "RuntimeSelfDestructSpark";
constexpr const char* kSelfDestructShockwaveGroup = "RuntimeSelfDestructShockwave";

/// <summary>自爆の瞬間に、閃光・火球・火花・地面を走る衝撃波を同時に発生させます。</summary>
void EmitSelfDestructExplosion(const Vector3& position, float explosionRadius) {
	ParticleManager* particleManager = ParticleManager::GetInstance();
	constexpr const char* kTexture = "Resources/circle.png";
	if (!particleManager->HasGroup(kSelfDestructCoreGroup)) {
		particleManager->CreateParticleGroup(kSelfDestructCoreGroup, kTexture, kMeshTypeQuad);
		particleManager->SetGroupBlendMode(kSelfDestructCoreGroup, kBlendModeAdd);
	}
	if (!particleManager->HasGroup(kSelfDestructFlameGroup)) {
		particleManager->CreateParticleGroup(kSelfDestructFlameGroup, kTexture, kMeshTypeQuad);
		particleManager->SetGroupBlendMode(kSelfDestructFlameGroup, kBlendModeAdd);
	}
	if (!particleManager->HasGroup(kSelfDestructSparkGroup)) {
		particleManager->CreateParticleGroup(kSelfDestructSparkGroup, kTexture, kMeshTypeQuad);
		particleManager->SetGroupBlendMode(kSelfDestructSparkGroup, kBlendModeAdd);
	}
	if (!particleManager->HasGroup(kSelfDestructShockwaveGroup)) {
		particleManager->CreateParticleGroup(kSelfDestructShockwaveGroup, kTexture, kMeshTypeRing);
		particleManager->SetGroupBlendMode(kSelfDestructShockwaveGroup, kBlendModeAdd);
	}

	const float radius = (std::max)(1.0f, explosionRadius);
	const Vector3 center = position + Vector3{0.0f, 0.42f, 0.0f};

	// 一瞬だけ残る白熱球を最前面に置き、起爆したフレームを明確にする。
	ParticleEmitParam core{};
	core.scale = {radius * 0.24f, radius * 0.24f, 1.0f};
	core.endScale = {radius * 0.92f, radius * 0.92f, 1.0f};
	core.lifeTime = 0.18f;
	core.color = {1.0f, 0.98f, 0.72f, 1.0f};
	core.endColor = {1.0f, 0.30f, 0.02f, 0.0f};
	core.count = 1;
	core.isBillboard = true;
	particleManager->Emit(kSelfDestructCoreGroup, center, core.count, core);

	// 大きさと速度を散らした火球で、単一スプライトに見えない爆炎を作る。
	ParticleEmitParam flame{};
	flame.scale = {radius * 0.16f, radius * 0.16f, 1.0f};
	flame.endScale = {radius * 0.42f, radius * 0.42f, 1.0f};
	flame.randomScaleRange = {radius * 0.07f, radius * 0.07f, 0.0f};
	flame.baseVelocity = {0.0f, 0.018f, 0.0f};
	flame.randomVelocityRange = {0.026f, 0.016f, 0.026f};
	flame.randomPositionRange = {radius * 0.12f, radius * 0.10f, radius * 0.12f};
	flame.lifeTime = 0.52f;
	flame.color = {1.0f, 0.56f, 0.04f, 0.92f};
	flame.endColor = {0.26f, 0.025f, 0.005f, 0.0f};
	flame.count = static_cast<uint32_t>((std::clamp)(radius * 7.0f, 16.0f, 32.0f));
	flame.isBillboard = true;
	particleManager->Emit(kSelfDestructFlameGroup, center, flame.count, flame);

	// 細長い発光片を外向きに飛ばし、爆発の勢いと危険範囲を強調する。
	ParticleEmitParam spark{};
	spark.scale = {radius * 0.12f, radius * 0.022f, 1.0f};
	spark.endScale = {radius * 0.018f, radius * 0.004f, 1.0f};
	spark.randomScaleRange = {radius * 0.07f, radius * 0.010f, 0.0f};
	spark.baseVelocity = {0.0f, 0.024f, 0.0f};
	spark.randomVelocityRange = {0.070f, 0.036f, 0.070f};
	spark.randomPositionRange = {radius * 0.08f, radius * 0.08f, radius * 0.08f};
	spark.randomRotateRange = {0.0f, 0.0f, MathConstants::kPi};
	spark.isRandomRotate = true;
	spark.lifeTime = 0.40f;
	spark.color = {1.0f, 0.90f, 0.28f, 1.0f};
	spark.endColor = {1.0f, 0.08f, 0.01f, 0.0f};
	spark.count = static_cast<uint32_t>((std::clamp)(radius * 9.0f, 20.0f, 40.0f));
	spark.isBillboard = true;
	particleManager->Emit(kSelfDestructSparkGroup, center, spark.count, spark);

	// リングをXZ平面へ寝かせ、実際のダメージ半径まで短時間で広げる。
	ParticleEmitParam shockwave{};
	shockwave.scale = {radius * 0.16f, radius * 0.16f, 1.0f};
	shockwave.endScale = {radius, radius, 1.0f};
	shockwave.baseRotate = {MathConstants::kPi * 0.5f, 0.0f, 0.0f};
	shockwave.lifeTime = 0.34f;
	shockwave.color = {1.0f, 0.76f, 0.18f, 0.88f};
	shockwave.endColor = {1.0f, 0.12f, 0.01f, 0.0f};
	shockwave.count = 1;
	particleManager->Emit(
		kSelfDestructShockwaveGroup, position + Vector3{0.0f, 0.08f, 0.0f},
		shockwave.count, shockwave);
}

/// <summary>特殊ドロップの種類を色で判別できる、低密度の常時発光粒子を追加します。</summary>
void AddDropParticleEmitter(
	Entity* object, const Vector4& color, float radius, float particleScale,
	float frequency, uint32_t count = 1) {
	if (!object) {
		return;
	}

	ParticleManager* particleManager = ParticleManager::GetInstance();
	if (!particleManager->HasGroup(kDropParticleGroup)) {
		particleManager->CreateParticleGroup(
			kDropParticleGroup, kDropParticleTexture, kMeshTypeQuad);
		particleManager->SetGroupBlendMode(kDropParticleGroup, kBlendModeAdd);
	}

	ParticleEmitterComponent* emitter = object->AddComponent<ParticleEmitterComponent>();
	emitter->SetGroupName(kDropParticleGroup);
	emitter->SetTexture(kDropParticleTexture);
	emitter->SetBlendMode(kBlendModeAdd);
	emitter->SetFrequency(frequency);

	ParticleEmitParam param{};
	param.scale = {particleScale, particleScale, 1.0f};
	param.endScale = {particleScale * 0.18f, particleScale * 0.18f, 1.0f};
	param.randomScaleRange = {particleScale * 0.28f, particleScale * 0.28f, 0.0f};
	param.baseVelocity = {0.0f, 0.012f, 0.0f};
	param.randomVelocityRange = {0.006f, 0.004f, 0.006f};
	param.randomPositionRange = {radius, radius * 0.45f, radius};
	param.lifeTime = 0.55f;
	param.color = color;
	param.endColor = {color.x * 0.72f, color.y * 0.72f, color.z * 0.72f, 0.0f};
	param.count = count;
	param.isBillboard = true;
	emitter->SetParam(param);

	// 自動発生の最初の周期を待たず、生成フレームからドロップの位置を示す。
	emitter->SetTranslate(object->GetTransform().translate);
	emitter->Emit();
}
} // namespace

void BaseScene::UpdatePlayerAttacks() {
	std::vector<PlayerAttackShotRequest> shotRequests;
	for (Entity* object : sceneObjects_.GetEntitiesWith<PlayerAttackComponent>()) {
		PlayerAttackComponent* attack = object->GetComponent<PlayerAttackComponent>();
		if (!attack || !attack->IsEnabled()) {
			continue;
		}

		std::vector<PlayerAttackShotRequest> requests = attack->ConsumeShotRequests();
		shotRequests.insert(shotRequests.end(), requests.begin(), requests.end());
	}

	// SkyLaserは画面外の敵を対象にしない。候補をシャッフルして同一発射内では別々の敵へ割り当てる。
	std::vector<Entity*> laserTargets;
	for (Entity* object : sceneObjects_.GetEntitiesWith<EnemyComponent>()) {
		EnemyComponent* enemy = object->GetComponent<EnemyComponent>();
		if (enemy && enemy->IsEnabled() && enemy->GetCurrentHealth() > 0.0f &&
		    !IsPointOutsideView(object->GetTransform().translate, 0.0f)) {
			laserTargets.push_back(object);
		}
	}
	static std::mt19937 laserRandomEngine(std::random_device{}());
	std::shuffle(laserTargets.begin(), laserTargets.end(), laserRandomEngine);
	size_t nextLaserTargetIndex = 0;

	for (PlayerAttackShotRequest& request : shotRequests) {
		if (request.motionType == PlayerAttackEntityMotionType::SkyLaser) {
			// 画面内に標的が残っていない分のレーザーは生成しない。
			if (nextLaserTargetIndex >= laserTargets.size()) {
				continue;
			}
			const Vector3 targetPosition = laserTargets[nextLaserTargetIndex++]->GetTransform().translate;
			request.position = {targetPosition.x, targetPosition.y + 3.0f, targetPosition.z};
		}
		CreateRuntimePlayerAttackEntity(request);
	}
}

Entity* BaseScene::CreateRuntimeEnemy(
	const std::string& enemyTypeName, const Vector3& position, Entity* target,
	const EnemySpawnPointComponent::EnemyStatMultipliers& statMultipliers) {
	auto object = std::make_unique<Entity>();
	const std::string resolvedTypeName = enemyTypeName.empty() ? "Default" : enemyTypeName;
	EnemyStats stats = LoadEnemyStats(resolvedTypeName);
	// 時間帯倍率は生成時に固定し、戦闘中の最大HPや移動速度が突然変化しないようにする。
	stats.health *= (std::max)(0.0f, statMultipliers.healthMultiplier);
	stats.speed *= (std::max)(0.0f, statMultipliers.speedMultiplier);
	stats.experience = (std::max)(0, static_cast<int>(std::lround(
		static_cast<float>(stats.experience) * (std::max)(0.0f, statMultipliers.experienceMultiplier))));
	object->SetName(MakeUniqueObjectName(resolvedTypeName));
	object->SetEditorType("Enemy");
	object->GetTransform().translate = position;
	if (stats.behavior == EnemyBehaviorType::SelfDestruct && stats.selfDestructSpawnInFront && target) {
		// 特殊な自爆敵はランダムなスポーン地点ではなく、プレイヤーが現在向いている正面へ割り込ませる。
		const float playerYaw = target->GetTransform().rotate.y;
		const Vector3 playerForward{std::sin(playerYaw), 0.0f, std::cos(playerYaw)};
		object->GetTransform().translate =
			target->GetTransform().translate + stats.selfDestructSpawnDistance * playerForward;
	}
	// 0.75を通常敵の基準サイズとし、ボス系の敵はデータ側の倍率で大型化する。
	// OBBはTransform.scaleを参照するため、見た目と接触判定が同じ倍率で拡大される。
	const float scale = 0.75f * stats.sizeScale;
	object->GetTransform().scale = {scale, scale, scale};

	EnemyComponent* enemy = object->AddComponent<EnemyComponent>();
	enemy->SetEnemyTypeName(resolvedTypeName);
	enemy->ApplyStats(stats);
	enemy->SetRuntimeSpawned(true);
	if (target) {
		enemy->SetTarget(target);
		enemy->SetTargetName(target->GetName());
	}

	// 敵定義に専用モデルがなければ、行動Strategyが提供する標準モデルを使用する。
	const EnemyBehaviorPresentation initialPresentation = enemy->GetBehaviorPresentation();
	const std::string enemyModelFilePath = stats.modelFilePath.empty()
		? initialPresentation.modelFilePath : stats.modelFilePath;
	if (!ModelManager::GetInstance()->FindModel(enemyModelFilePath)) {
		if (!stats.modelDirectory.empty()) {
			ModelManager::GetInstance()->LoadModel(
				enemyModelFilePath, stats.usesAnimatedModel, stats.modelDirectory);
		} else {
			ModelManager::GetInstance()->LoadModel(enemyModelFilePath);
		}
	}
	Object3dComponent* object3d = object->AddComponent<Object3dComponent>();
	object3d->SetModel(enemyModelFilePath);

	OBBColliderComponent* collider = object->AddComponent<OBBColliderComponent>();
	collider->SetHalfSize({0.4f, 0.4f, 0.4f});
	collider->SetPushBackEnabled(true);
	if (initialPresentation.addAttackTrail) {
		// 高速移動した経路を紫色の帯として残し、切り返し方向を視覚化する。
		TrailRendererComponent* trail = object->AddComponent<TrailRendererComponent>();
		trail->SetWidth(1.15f * stats.sizeScale);
		trail->SetLifeTime(0.32f);
		trail->SetMinSegmentLength(0.08f);
		trail->SetHeadColor({0.92f, 0.20f, 1.0f, 0.92f});
		trail->SetTailColor({0.20f, 0.02f, 0.40f, 0.0f});
	}

	object->Update();
	sceneObjects_.push_back(std::move(object));
	++nextObjectId_;
	return sceneObjects_.back().get();
}

Entity* BaseScene::CreateRuntimeExperience(const EnemyStats& enemyStats, const Vector3& position, Entity* target) {
	if (enemyStats.experience <= 0) {
		return nullptr;
	}
	if (!target) {
		for (const auto& object : sceneObjects_) {
			if (object->GetComponent<PlayerComponent>()) {
				target = object.get();
				break;
			}
		}
	}

	const std::string modelFilePath = enemyStats.experienceModelFilePath.empty()
	    ? kEnemyDropModelFilePath
	    : enemyStats.experienceModelFilePath;
	if (!ModelManager::GetInstance()->FindModel(modelFilePath)) {
		ModelManager::GetInstance()->LoadModel(modelFilePath);
	}
	if (!ModelManager::GetInstance()->FindModel(modelFilePath)) {
		ModelManager::GetInstance()->LoadModel(kEnemyDropModelFilePath);
	}
	const std::string resolvedModelFilePath = ModelManager::GetInstance()->FindModel(modelFilePath)
	    ? modelFilePath
	    : kEnemyDropModelFilePath;
	Entity* firstExperienceObject = nullptr;
	int remainingExperience = enemyStats.experience;
	int particleIndex = 0;
	for (auto denominationIt = kExperienceDenominations.rbegin(); denominationIt != kExperienceDenominations.rend(); ++denominationIt) {
		const int denomination = *denominationIt;
		const int particleCount = remainingExperience / denomination;
		remainingExperience %= denomination;
		for (int index = 0; index < particleCount; ++index, ++particleIndex) {
			auto object = std::make_unique<Entity>();
			object->SetName(MakeUniqueObjectName("Experience" + std::to_string(denomination)));
			object->SetEditorType("Experience");
			const float angle = static_cast<float>(particleIndex) * 2.39996323f;
			const float radius = particleIndex == 0 ? 0.0f : 0.18f + 0.07f * std::sqrt(static_cast<float>(particleIndex));
			object->GetTransform().translate = {
				position.x + std::cos(angle) * radius,
				position.y + 0.08f * static_cast<float>(particleIndex % 3),
				position.z + std::sin(angle) * radius
			};
			const float scale = GetExperienceScale(denomination);
			object->GetTransform().scale = {scale, scale, scale};

			ExperienceComponent* experience = object->AddComponent<ExperienceComponent>();
			experience->SetExperience(denomination);
			experience->SetModelFilePath(resolvedModelFilePath);
			experience->SetTarget(target);
			object->AddComponent<DropItemVisualComponent>();
			object->AddComponent<ExperienceGlowVisualComponent>();

			Object3dComponent* object3d = object->AddComponent<Object3dComponent>();
			object3d->SetModel(resolvedModelFilePath);
			const Vector4 experienceColor = GetExperienceColor(denomination);
			object3d->SetColor(experienceColor);
			object3d->SetEmission(
				{experienceColor.x, experienceColor.y, experienceColor.z}, 0.54f);

			object->Update();
			Entity* createdObject = object.get();
			sceneObjects_.push_back(std::move(object));
			if (!firstExperienceObject) {
				firstExperienceObject = createdObject;
			}
			++nextObjectId_;
		}
	}
	return firstExperienceObject;
}

/// <summary>
/// 敵の撃破位置へ回復、経験値回収、または所持金用のドロップオブジェクトを生成します。
/// MoneyタイプではmoneyAmountを保持し、プレイヤーが接触した時に今回の獲得額へ加算します。
/// </summary>
Entity* BaseScene::CreateRuntimeItemDrop(
	ItemDropType type, const Vector3& position, Entity* target, float healAmount, int moneyAmount) {
	// 撃破した敵にターゲットが設定されていない場合も回収できるよう、シーン内のプレイヤーを補完する。
	if (!target) {
		// 全スポーンポイントへ停止状態を伝え、経過秒数とスケジュールカウンターを凍結する。
		for (const auto& object : sceneObjects_) {
			if (object->GetComponent<PlayerComponent>()) {
				target = object.get();
				break;
			}
		}
	}

	ModelManager::GetInstance()->LoadModel(kEnemyDropModelFilePath);
	auto object = std::make_unique<Entity>();
	const bool isHealthItem = type == ItemDropType::Health;
	const bool isMoneyItem = type == ItemDropType::Money;
	// 種類ごとに名前、表示サイズ、色を変え、プレイ中に効果を判別できるようにする。
	object->SetName(MakeUniqueObjectName(isHealthItem ? "HealthItem" : isMoneyItem ? "Money" : "ExperienceCollector"));
	object->SetEditorType(isHealthItem ? "HealthItem" : isMoneyItem ? "Money" : "ExperienceCollector");
	object->GetTransform().translate = position + Vector3{isHealthItem ? -0.35f : 0.35f, 0.35f, 0.0f};
	const float scale = isHealthItem ? 0.42f : isMoneyItem ? 0.30f : 0.50f;
	object->GetTransform().scale = {scale, scale, scale};

	ItemDropComponent* item = object->AddComponent<ItemDropComponent>();
	item->SetType(type);
	item->SetTarget(target);
	item->SetHealAmount(healAmount);
	item->SetMoneyAmount(moneyAmount);
	object->AddComponent<DropItemVisualComponent>();

	Object3dComponent* object3d = object->AddComponent<Object3dComponent>();
	object3d->SetModel(kEnemyDropModelFilePath);
	object3d->SetColor(isHealthItem
		? Vector4{0.15f, 1.0f, 0.25f, 1.0f}
		: isMoneyItem ? Vector4{1.0f, 0.55f, 0.05f, 1.0f}
		: Vector4{1.0f, 0.85f, 0.10f, 1.0f});
	object3d->SetEmission(
		isHealthItem ? Vector3{0.08f, 0.55f, 0.12f}
		             : isMoneyItem ? Vector3{0.65f, 0.28f, 0.03f}
		                           : Vector3{0.65f, 0.48f, 0.05f},
		0.55f);

	// 回復は緑、所持金は橙、経験値全回収は黄で、モデルの発光色と粒子色を統一する。
	AddDropParticleEmitter(
		object.get(),
		isHealthItem
			? Vector4{0.22f, 1.0f, 0.32f, 0.82f}
			: isMoneyItem ? Vector4{1.0f, 0.58f, 0.06f, 0.86f}
			              : Vector4{1.0f, 0.88f, 0.14f, 0.86f},
		isHealthItem ? 0.34f : 0.28f,
		isHealthItem ? 0.105f : 0.085f,
		0.11f);

	object->Update();
	sceneObjects_.push_back(std::move(object));
	++nextObjectId_;
	return sceneObjects_.back().get();
}

/// <summary>
/// ボス撃破位置へ金色の強化報酬を1個生成します。
/// upgradeCountはドロップ数ではなく、回収時に実行する強化抽選の回数です。
/// </summary>
void BaseScene::CreateRuntimeBossUpgradeDrop(const Vector3& position, Entity* target, int upgradeCount) {
	if (!target) {
		for (const auto& object : sceneObjects_) {
			if (object->GetComponent<PlayerComponent>()) {
				target = object.get();
				break;
			}
		}
	}

	ModelManager::GetInstance()->LoadModel(kEnemyDropModelFilePath);
	auto object = std::make_unique<Entity>();
	object->SetName(MakeUniqueObjectName("BossUpgradeReward"));
	object->SetEditorType("BossUpgradeReward");
	object->GetTransform().translate = position + Vector3{0.0f, 0.45f, 0.0f};
	object->GetTransform().scale = {0.65f, 0.65f, 0.65f};

	// 経験値コンポーネントの接触判定を再利用するが、ボス報酬フラグ時は吸着を行わない。
	ExperienceComponent* reward = object->AddComponent<ExperienceComponent>();
	reward->SetBossUpgradeReward(true);
	// 撃破位置がプレイヤーと重なっていても、ボスの倒れ込みが終わる前には回収させない。
	reward->SetCollectionEnabled(false);
	reward->SetBossUpgradeCount(upgradeCount);
	reward->SetTarget(target);
	reward->SetCollectDistance(0.8f);
	object->AddComponent<DropItemVisualComponent>();

	Object3dComponent* object3d = object->AddComponent<Object3dComponent>();
	object3d->SetModel(kEnemyDropModelFilePath);
	object3d->SetColor({1.0f, 0.72f, 0.08f, 1.0f});
	object3d->SetEmission({0.85f, 0.48f, 0.04f}, 0.75f);

	// ボス報酬だけは粒子を少し大きく、密度も高くして通常ドロップとの差を出す。
	AddDropParticleEmitter(
		object.get(), {1.0f, 0.78f, 0.16f, 0.92f},
		0.48f, 0.13f, 0.075f, 2);

	object->Update();
	sceneObjects_.push_back(std::move(object));
	++nextObjectId_;
}

/// <summary>各報酬枠で有効な3テーブルを重み抽選し、結果を演出用配列へ積みます。</summary>
void BaseScene::RollBossRewards(PlayerComponent* player, int rewardCount) {
	bossRewardResults_.clear();
	bossAcquisitionOfferQueue_.clear();
	bossAcquisitionPlayer_ = player;
	if (!player || rewardCount <= 0) return;

	static std::mt19937 randomEngine(std::random_device{}());
	PlayerStats stats = player->GetBaseStats();
	const BossRewardSettings& settings = lootDropService_.GetBossRewardSettings();
	// 現在のプレイヤーの初期武器は所持済み判定、他プレイヤーの初期武器はこの除外集合で弾く。
	const auto otherPlayerInitialAttacks = GetOtherPlayerInitialAttacks(*player);
	std::unordered_set<std::string> reservedAttacks;
	std::unordered_set<std::string> reservedStatuses;
	bool statsChanged = false;

	auto attackTexture = [](const std::string& name, const std::string& level) {
		const PlayerAttackStats definition = LoadPlayerAttackStats(name);
		if (!PlayerAttackLevel::IsSuper(level)) return definition.choiceTextureFilePath;
		for (const auto& levelStats : definition.levels) {
			if (PlayerAttackLevel::IsSuper(levelStats.level)) return levelStats.choiceTextureFilePath;
		}
		return std::string{};
	};
	auto attackDescription = [](const std::string& name, const std::string& level, const std::string& fallback) {
		for (const auto& levelStats : LoadPlayerAttackStats(name).levels) {
			if (levelStats.level == level && !levelStats.choiceDescription.empty()) return levelStats.choiceDescription;
		}
		return fallback;
	};

	for (int rewardIndex = 0; rewardIndex < rewardCount; ++rewardIndex) {
		std::vector<LevelUpChoice> owned;
		std::vector<LevelUpChoice> unownedWeapons;
		std::vector<LevelUpChoice> unownedItems;
		std::unordered_set<std::string> ownedAttackNames;
		std::unordered_set<std::string> ownedStatusNames;
		int emptyAttackSlot = -1;
		int emptyStatusSlot = -1;

		for (int index = 0; index < static_cast<int>(stats.attackSlots.size()); ++index) {
			const PlayerAttackSlot& slot = stats.attackSlots[index];
			if (slot.attackName.empty()) {
				if (emptyAttackSlot < 0) emptyAttackSlot = index;
				continue;
			}
			ownedAttackNames.insert(slot.attackName);
			if (!slot.enabled || PlayerAttackLevel::IsSuper(slot.attackLevel)) continue;
			const int level = std::atoi(slot.attackLevel.c_str());
			if (level >= 1 && level < 5) {
				const std::string nextLevel = std::to_string(level + 1);
				const PlayerAttackStats definition = LoadPlayerAttackStats(slot.attackName);
				owned.push_back({LevelUpChoiceType::AttackLevelUp, slot.attackName,
					definition.GetDisplayName() + " Lv." + nextLevel,
					attackDescription(slot.attackName, nextLevel, "Weapon upgraded"), index,
					attackTexture(slot.attackName, nextLevel)});
			} else if (slot.attackLevel == "5") {
				const PlayerAttackStats definition = LoadPlayerAttackStats(slot.attackName);
				const int requiredLevel = (std::max)(1, std::atoi(definition.superConditionStatusLevel.c_str()));
				const bool conditionMet = !definition.superConditionStatusName.empty() &&
					std::any_of(stats.statusSlots.begin(), stats.statusSlots.end(), [&](const PlayerStatusSlot& status) {
						return status.enabled && status.statusName == definition.superConditionStatusName &&
							std::atoi(status.level.c_str()) >= requiredLevel;
					});
				if (conditionMet) {
					owned.push_back({LevelUpChoiceType::AttackSuper, slot.attackName,
						definition.GetDisplayName(true), attackDescription(slot.attackName, "super", "Weapon evolved"),
						index, attackTexture(slot.attackName, "super")});
				}
			}
		}

		for (int index = 0; index < static_cast<int>(stats.statusSlots.size()); ++index) {
			const PlayerStatusSlot& slot = stats.statusSlots[index];
			if (slot.statusName.empty()) {
				if (emptyStatusSlot < 0) emptyStatusSlot = index;
				continue;
			}
			ownedStatusNames.insert(slot.statusName);
			const int level = std::atoi(slot.level.c_str());
			if (slot.enabled && level >= 1 && level < 5) {
				const PlayerStatusItemStats definition = LoadPlayerStatusItemStats(slot.statusName);
				const int nextLevelIndex = level;
				const std::string description = definition.levelDescriptions[nextLevelIndex].empty()
					? "Item upgraded" : definition.levelDescriptions[nextLevelIndex];
				owned.push_back({LevelUpChoiceType::StatusLevelUp, slot.statusName,
					definition.GetDisplayName() + " Lv." + std::to_string(level + 1), description, index,
					definition.levelTextureFilePaths[nextLevelIndex]});
			}
		}

		if (emptyAttackSlot >= 0) {
			for (const std::string& name : LoadPlayerAttackNames()) {
				if (ownedAttackNames.contains(name) || reservedAttacks.contains(name) ||
				    otherPlayerInitialAttacks.contains(name)) continue;
				const PlayerAttackStats definition = LoadPlayerAttackStats(name);
				unownedWeapons.push_back({LevelUpChoiceType::NewAttack, name, "Acquire " + definition.GetDisplayName(),
					attackDescription(name, "1", "Acquire this weapon?"), -1, attackTexture(name, "1")});
			}
		}
		if (emptyStatusSlot >= 0) {
			for (const std::string& name : LoadPlayerStatusItemNames()) {
				if (ownedStatusNames.contains(name) || reservedStatuses.contains(name)) continue;
				const PlayerStatusItemStats definition = LoadPlayerStatusItemStats(name);
				unownedItems.push_back({LevelUpChoiceType::NewStatus, name, "取得 " + definition.GetDisplayName(),
					definition.levelDescriptions[0].empty() ? "Acquire this item?" : definition.levelDescriptions[0],
					-1, definition.levelTextureFilePaths[0]});
			}
		}

		struct Table { std::vector<LevelUpChoice>* choices; float weight; };
		std::vector<Table> tables;
		if (!owned.empty()) tables.push_back({&owned, settings.ownedUpgradeTableWeight});
		if (!unownedWeapons.empty()) tables.push_back({&unownedWeapons, settings.unownedWeaponTableWeight});
		if (!unownedItems.empty()) tables.push_back({&unownedItems, settings.unownedItemTableWeight});
		if (tables.empty()) break;
		float totalWeight = 0.0f;
		for (const Table& table : tables) totalWeight += (std::max)(0.0f, table.weight);
		size_t tableIndex = 0;
		if (totalWeight > 0.0f) {
			std::uniform_real_distribution<float> tableRoll(0.0f, totalWeight);
			float roll = tableRoll(randomEngine);
			for (; tableIndex + 1 < tables.size(); ++tableIndex) {
				roll -= (std::max)(0.0f, tables[tableIndex].weight);
				if (roll < 0.0f) break;
			}
		} else {
			std::uniform_int_distribution<size_t> tableRoll(0, tables.size() - 1);
			tableIndex = tableRoll(randomEngine);
		}
		std::vector<LevelUpChoice>& choices = *tables[tableIndex].choices;
		std::uniform_int_distribution<size_t> choiceRoll(0, choices.size() - 1);
		const LevelUpChoice selected = choices[choiceRoll(randomEngine)];
		bossRewardResults_.push_back(selected);

		switch (selected.type) {
		case LevelUpChoiceType::AttackLevelUp:
			stats.attackSlots[selected.slotIndex].attackLevel = std::to_string(
				(std::min)(5, std::atoi(stats.attackSlots[selected.slotIndex].attackLevel.c_str()) + 1));
			statsChanged = true;
			break;
		case LevelUpChoiceType::AttackSuper:
			stats.attackSlots[selected.slotIndex].attackLevel = "super";
			statsChanged = true;
			break;
		case LevelUpChoiceType::StatusLevelUp:
			stats.statusSlots[selected.slotIndex].level = std::to_string(
				(std::min)(5, std::atoi(stats.statusSlots[selected.slotIndex].level.c_str()) + 1));
			statsChanged = true;
			break;
		case LevelUpChoiceType::NewAttack:
			reservedAttacks.insert(selected.name);
			bossAcquisitionOfferQueue_.push_back(selected);
			break;
		case LevelUpChoiceType::NewStatus:
			reservedStatuses.insert(selected.name);
			bossAcquisitionOfferQueue_.push_back(selected);
			break;
		case LevelUpChoiceType::Decline:
			break;
		}
	}

	if (statsChanged) {
		player->ApplyStats(stats, ApplyPlayerStatusItems(stats, LoadPlayerStatusItemDefinitions()));
		if (Entity* owner = player->GetOwner()) {
			ApplyPlayerAttackSlots(owner->GetComponent<PlayerAttackComponent>(), stats, LoadPlayerAttackDefinitions());
		}
	}
}

/// <summary>
/// 回収済みの金色ボス報酬へ強化効果を適用し、強化アイテム数に比例したお金を付与します。
/// </summary>
void BaseScene::UpdateBossUpgradeRewards() {
	bool defeatAnimationFinished = true;
	for (Entity* enemyObject : sceneObjects_.GetEntitiesWith<EnemyComponent>()) {
		const EnemyComponent* enemy = enemyObject->GetComponent<EnemyComponent>();
		if (enemy && enemy->GetCurrentHealth() <= 0.0f && !enemy->IsDeathEffectFinished()) {
			defeatAnimationFinished = false;
			break;
		}
	}
	for (const auto& object : sceneObjects_) {
		ExperienceComponent* reward = object->GetComponent<ExperienceComponent>();
		if (reward && reward->IsBossUpgradeReward()) {
			reward->SetCollectionEnabled(defeatAnimationFinished);
		}
		if (!reward || !reward->IsBossUpgradeReward() || !reward->IsCollected() || reward->IsBossUpgradeApplied()) {
			continue;
		}
		PlayerComponent* player = reward->GetTarget() ? reward->GetTarget()->GetComponent<PlayerComponent>() : nullptr;
		// 金色報酬を拾った瞬間に全体G強化を適用し、所持金と今回獲得額の両方へ反映する。
		const int baseMoney = lootDropService_.GetBossRewardMoney(reward->GetBossUpgradeCount());
		const int gainedMoney = sceneManager_ ? sceneManager_->ApplyGlobalGoldBonus(baseMoney) : baseMoney;
		// challengeMoneyEarned_はリザルト表示用、AddMoneyは所持金更新とJSON即時保存を担当する。
		gameplaySession_.AddChallengeMoney(gainedMoney);
		if (sceneManager_) sceneManager_->AddMoney(gainedMoney);
		RollBossRewards(player, reward->GetBossUpgradeCount());
		bossRewardPresentationMoney_ = gainedMoney;
		bossRewardPresentationTimer_ = 0.0f;
		bossRewardPresentationReady_ = false;
		SetGameplayMode(GameplayMode::BossRewardPresentation);
		reward->MarkBossUpgradeApplied();
	}
}

/// <summary>予約候補を再検証し、「取得する／見送る」の2択としてレベルアップUIへ表示します。</summary>
bool BaseScene::ShowNextBossAcquisitionOffer() {
	// 予約後にプレイヤー設定が変更されても、表示時点の新規取得条件で再検証する。
	const auto otherPlayerInitialAttacks = bossAcquisitionPlayer_
	    ? GetOtherPlayerInitialAttacks(*bossAcquisitionPlayer_)
	    : std::unordered_set<std::string>{};
	while (bossAcquisitionPlayer_ && !bossAcquisitionOfferQueue_.empty()) {
		LevelUpChoice offer = bossAcquisitionOfferQueue_.front();
		bossAcquisitionOfferQueue_.erase(bossAcquisitionOfferQueue_.begin());
		const PlayerStats& stats = bossAcquisitionPlayer_->GetBaseStats();
		// 前の確認中にスロット状態が変わる可能性があるため、表示直前に所有状態と空き位置を再確認する。
		if (offer.type == LevelUpChoiceType::NewAttack) {
			const auto owned = std::find_if(stats.attackSlots.begin(), stats.attackSlots.end(), [&offer](const PlayerAttackSlot& slot) {
				return slot.attackName == offer.name;
			});
			const auto empty = std::find_if(stats.attackSlots.begin(), stats.attackSlots.end(), [](const PlayerAttackSlot& slot) {
				return slot.attackName.empty();
			});
			if (owned != stats.attackSlots.end() || empty == stats.attackSlots.end() ||
			    otherPlayerInitialAttacks.contains(offer.name)) {
				continue;
			}
			offer.slotIndex = static_cast<int>(std::distance(stats.attackSlots.begin(), empty));
		} else if (offer.type == LevelUpChoiceType::NewStatus) {
			const auto owned = std::find_if(stats.statusSlots.begin(), stats.statusSlots.end(), [&offer](const PlayerStatusSlot& slot) {
				return slot.statusName == offer.name;
			});
			const auto empty = std::find_if(stats.statusSlots.begin(), stats.statusSlots.end(), [](const PlayerStatusSlot& slot) {
				return slot.statusName.empty();
			});
			if (owned != stats.statusSlots.end() || empty == stats.statusSlots.end()) {
				continue;
			}
			offer.slotIndex = static_cast<int>(std::distance(stats.statusSlots.begin(), empty));
		} else {
			continue;
		}

		// 既存のレベルアップUIを再利用し、候補カードと見送りカードだけを表示する。
		levelUpPlayer_ = bossAcquisitionPlayer_;
		PopulateCorrespondingOwnedIcons(offer, stats);
		levelUpChoices_.clear();
		levelUpChoices_.push_back(offer);
		levelUpChoices_.push_back({
			LevelUpChoiceType::Decline,
			"",
			"Do not acquire",
			"Skip this reward.",
			-1,
			""
		});
		selectedLevelUpChoiceIndex_ = 0;
		SetGameplayMode(GameplayMode::BossReward);
		return true;
	}
	bossAcquisitionPlayer_ = nullptr;
	return false;
}

void BaseScene::UpdateEnemyAttacks() {
	std::vector<EnemyShotRequest> shotRequests;
	std::vector<BossUpgradeDropRequest> selfDestructBossRewards;
	for (Entity* object : sceneObjects_.GetEntitiesWith<EnemyComponent>()) {
		EnemyComponent* enemy = object->GetComponent<EnemyComponent>();
		if (!enemy || !enemy->IsEnabled()) {
			continue;
		}
		if (enemy->GetCurrentHealth() <= 0.0f) {
			if (TrailRendererComponent* trail = object->GetComponent<TrailRendererComponent>()) {
				trail->SetEmitting(false);
			}
			continue;
		}
		if (enemy->ConsumeSelfDestructRequest()) {
			// 爆発要求はこのフレームで一度だけ処理し、EnemyComponent側で体力0にして後続の掃除対象にする。
			EmitSelfDestructExplosion(
				object->GetTransform().translate, enemy->GetStats().selfDestructRadius);
			Entity* target = enemy->GetTarget();
			PlayerComponent* player = target ? target->GetComponent<PlayerComponent>() : nullptr;
			if (player && player->IsEnabled() && player->GetCurrentHealth() > 0.0f) {
				Vector3 toPlayer = target->GetTransform().translate - object->GetTransform().translate;
				toPlayer.y = 0.0f;
				if (Length(toPlayer) <= enemy->GetStats().selfDestructRadius) {
					player->TakeDamage(enemy->GetStats().attack);
				}
			}
			// 自爆で消滅した中ボスも報酬対象にする。通常自爆敵はdropsBossUpgrade=falseなので何も生成しない。
			if (enemy->GetStats().dropsBossUpgrade) {
				const LootDropResult loot = lootDropService_.Roll(*enemy, object->GetTransform().translate);
				if (loot.hasBossUpgrade) {
					selfDestructBossRewards.push_back(loot.bossUpgrade);
				}
			}
			continue;
		}
		std::vector<EnemyShotRequest> requests = enemy->ConsumeShotRequests();
		shotRequests.insert(shotRequests.end(), requests.begin(), requests.end());
		const EnemyBehaviorPresentation presentation = enemy->GetBehaviorPresentation();

		if (TrailRendererComponent* trail = object->GetComponent<TrailRendererComponent>()) {
			// 待機中の不要な軌跡を防ぎ、斬撃判定が有効な期間だけ新しい点を記録する。
			trail->SetEmitting(presentation.attackTrailEmitting);
		}

		if (Object3dComponent* object3d = object->GetComponent<Object3dComponent>()) {
			if (enemy->IsDamageFlashActive()) {
				// 行動中の予兆色よりも被弾フィードバックを優先し、短時間だけ赤くする。
				object3d->SetColor({1.0f, 0.28f, 0.28f, 1.0f});
			} else if (presentation.overrideColor) {
				object3d->SetColor(presentation.color);
			} else if (enemy->GetStats().overridesBaseColor) {
				// 敵名ではなく定義データから、通常時の固有色を適用する。
				object3d->SetColor(enemy->GetStats().baseColor);
			} else {
				// 被弾色が終了した通常の追跡敵を標準色へ戻す。
				object3d->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
			}
		}
	}
	for (const EnemyShotRequest& request : shotRequests) {
		CreateRuntimeEnemyAttackEntity(request);
	}
	for (const BossUpgradeDropRequest& request : selfDestructBossRewards) {
		CreateRuntimeBossUpgradeDrop(request.position, request.target, request.upgradeCount);
	}
}

/// <summary>敵の発射要求から、移動・描画・当たり判定を持つ敵弾を生成します。</summary>
Entity* BaseScene::CreateRuntimeEnemyAttackEntity(const EnemyShotRequest& request) {
	auto object = std::make_unique<Entity>();
	// 軌道タイプは移動だけでなく、竜巻ごとの名前・色・軌跡表現の選択にも使用する。
	const bool isContractingTornado = request.motionType == EnemyAttackEntityMotionType::ContractingOrbit;
	const bool isGiantTornado = request.motionType == EnemyAttackEntityMotionType::Homing;
	const bool isTornado = request.motionType == EnemyAttackEntityMotionType::ExpandingOrbit ||
	    request.motionType == EnemyAttackEntityMotionType::ContractingOrbit || isGiantTornado;
	object->SetName(MakeUniqueObjectName(
	    isGiantTornado ? "BossGiantTornado"
	    : isContractingTornado ? "BossConvergingTornado" : isTornado ? "BossTornado" : "EnemyAttackEntity"));
	object->SetEditorType("EnemyAttackEntity");
	object->GetTransform().translate = request.position;
	object->GetTransform().scale = {request.size, request.size, request.size};
	object->GetTransform().rotate.y = std::atan2(request.direction.x, request.direction.z);

	EnemyAttackEntityComponent* projectile = object->AddComponent<EnemyAttackEntityComponent>();
	projectile->SetDirection(request.direction);
	if (request.motionType == EnemyAttackEntityMotionType::ExpandingOrbit) {
		projectile->SetExpandingOrbit(
		    request.orbitCenter,
		    request.orbitAngle,
		    request.orbitInitialRadius,
		    request.orbitAngularSpeed,
		    request.orbitRadialSpeed,
		    request.orbitHeight);
	} else if (request.motionType == EnemyAttackEntityMotionType::ContractingOrbit) {
		projectile->SetContractingOrbit(
		    request.orbitCenter,
		    request.orbitAngle,
		    request.orbitInitialRadius,
		    request.orbitAngularSpeed,
		    request.orbitRadialSpeed,
		    request.orbitHeight);
	} else if (isGiantTornado) {
		projectile->SetHomingTarget(request.homingTarget);
	}
	projectile->SetSpeed(request.speed);
	projectile->SetAttack(request.attack);
	projectile->SetSize(request.size);
	projectile->SetLifeTime(request.lifeTime);

	// 敵弾の見た目と同じ範囲を命中判定に使用する。
	SphereColliderComponent* collider = object->AddComponent<SphereColliderComponent>();
	// Transform の scale が弾サイズなので、ローカル半径 1.0 でワールド半径が request.size になる。
	collider->SetRadius(1.0f);
	collider->SetPushBackEnabled(false);

	ModelManager::GetInstance()->LoadModel("sphere.obj");
	Object3dComponent* object3d = object->AddComponent<Object3dComponent>();
	object3d->SetModel("sphere.obj");
	object3d->SetColor(isGiantTornado
	    ? Vector4{0.20f, 1.0f, 0.48f, 1.0f}
	    : isContractingTornado
	    ? Vector4{0.78f, 0.35f, 1.0f, 1.0f}
	    : isTornado ? Vector4{0.50f, 0.92f, 1.0f, 1.0f} : Vector4{1.0f, 0.12f, 0.04f, 1.0f});
	// 竜巻弾は球モデルではなく、上方へ広がる渦パーティクルを本体表示として使う。
	object3d->SetEnabled(!isTornado);
	if (isTornado) {
		// 非表示の当たり判定用球が、竜巻の位置へ球形の影だけを残さないようにする。
		object3d->SetShadowEnabled(false);
		// パーティクルだけでは途切れて見える円錐輪郭を、回転する螺旋帯と接地リングで補完する。
		object->AddComponent<TornadoVisualComponent>(isContractingTornado, isGiantTornado);
		// 全竜巻で同じ描画グループを共有し、竜巻ごとのGPUバッファ生成を避ける。
		constexpr const char* kTornadoParticleGroup = "RuntimeTornadoWispParticle";
		if (!ParticleManager::GetInstance()->HasGroup(kTornadoParticleGroup)) {
			ParticleManager::GetInstance()->CreateParticleGroup(
			    kTornadoParticleGroup, "Resources/tornado_wisp.png", kMeshTypeQuad);
		}

		ParticleEmitterComponent* tornadoEmitter = object->AddComponent<ParticleEmitterComponent>();
		tornadoEmitter->SetGroupName(kTornadoParticleGroup);
		// 色や生成周期などの共通値をプリセットから読み、攻撃サイズ依存値だけ下で上書きする。
		ParticlePresetData tornadoPreset;
		// 省略項目には生成直後の値を使い、旧形式のプリセットとも互換性を保つ。
		CaptureParticlePreset(tornadoEmitter, tornadoPreset);
		if (ParticlePresetRepository::Load("Tornado", tornadoPreset)) {
			ApplyParticlePreset(tornadoEmitter, tornadoPreset);
		}
		tornadoEmitter->SetFrequency(isGiantTornado ? 0.040f : 0.055f);

		ParticleEmitParam tornadoParam = tornadoEmitter->GetParam();
		// 当たり判定サイズに比例させ、通常・収束・巨大竜巻でシルエットの比率を統一する。
		tornadoParam.vortexBaseRadius = (std::max)(0.10f, request.size * 0.16f);
		tornadoParam.vortexTopRadius = (std::max)(0.65f, request.size * 1.45f);
		tornadoParam.vortexHeight = (std::max)(1.8f, request.size * 3.6f);
		// 三日月状の風テクスチャを少し横長にし、短い煙粒ではなく旋回する帯として重ねる。
		tornadoParam.scale = {request.size * 0.62f, request.size * 0.44f, request.size * 0.44f};
		tornadoParam.endScale = {request.size * 0.78f, request.size * 0.58f, request.size * 0.58f};
		tornadoParam.randomScaleRange = {
		    request.size * 0.14f, request.size * 0.10f, 0.0f};
		tornadoParam.count = isGiantTornado ? 7u : 4u;
		tornadoParam.lifeTime = isGiantTornado ? 1.65f : 1.35f;
		tornadoParam.baseVelocity.y = tornadoParam.vortexHeight / (tornadoParam.lifeTime * 60.0f) * 0.62f;
		tornadoParam.randomVelocityRange.y = tornadoParam.baseVelocity.y * 0.30f;
		tornadoParam.acceleration.y = 0.00035f;
		tornadoParam.vortexAngularSpeed = isContractingTornado ? -12.5f : isGiantTornado ? 9.5f : 12.5f;
		// 攻撃パターンを見分けられるよう、巨大は緑、収束は紫、通常は水色にする。
		tornadoParam.color = isGiantTornado
		    ? Vector4{0.38f, 1.0f, 0.58f, 0.78f}
		    : isContractingTornado
		    ? Vector4{0.82f, 0.48f, 1.0f, 0.78f}
		    : Vector4{0.58f, 0.92f, 1.0f, 0.78f};
		tornadoParam.endColor = {tornadoParam.color.x, tornadoParam.color.y, tornadoParam.color.z, 0.0f};
		tornadoEmitter->SetParam(tornadoParam);
	}
	TrailRendererComponent* trail = object->AddComponent<TrailRendererComponent>();
	trail->SetWidth((std::max)(0.10f, request.size * (isTornado ? 0.72f : 0.8f)));
	trail->SetLifeTime(isTornado ? 0.62f : 0.28f);
	trail->SetHeadColor(isGiantTornado
	    ? Vector4{0.72f, 1.0f, 0.82f, 0.98f}
	    : isContractingTornado
	    ? Vector4{0.95f, 0.75f, 1.0f, 0.95f}
	    : isTornado ? Vector4{0.80f, 1.0f, 1.0f, 0.95f} : Vector4{1.0f, 0.35f, 0.05f, 0.95f});
	trail->SetTailColor(isGiantTornado
	    ? Vector4{0.02f, 0.42f, 0.16f, 0.0f}
	    : isContractingTornado
	    ? Vector4{0.35f, 0.02f, 0.60f, 0.0f}
	    : isTornado ? Vector4{0.05f, 0.30f, 0.55f, 0.0f} : Vector4{0.70f, 0.02f, 0.01f, 0.0f});

	object->Update();
	sceneObjects_.push_back(std::move(object));
	++nextObjectId_;
	return sceneObjects_.back().get();
}

/// <summary>有効な敵弾とプレイヤーのコライダーを判定し、命中時にダメージと弾の消滅を処理します。</summary>
void CombatSystem::UpdateEnemyProjectileHits(SceneWorld& sceneObjects) {
	// プレイヤーの実コライダー範囲を登録し、高速弾は移動線分を包む範囲で候補検索する。
	collisionSpatialGrid_.Clear();
	collisionDebugStats_.enemyProjectileCandidates = 0;
	collisionDebugStats_.enemyProjectileNarrowPhaseTests = 0;
	for (const auto& playerObject : sceneObjects) {
		PlayerComponent* player = playerObject->GetComponent<PlayerComponent>();
		if (!player || !player->IsEnabled() || player->GetCurrentHealth() <= 0.0f) {
			continue;
		}

		const OBBColliderComponent* obb = playerObject->GetComponent<OBBColliderComponent>();
		const SphereColliderComponent* sphere = playerObject->GetComponent<SphereColliderComponent>();
		const bool hasOBB = obb && obb->IsEnabled();
		const bool hasSphere = sphere && sphere->IsEnabled();
		CollisionSpatialGrid::Bounds bounds = CollisionSpatialGrid::FromCenterRadius(
			playerObject->GetTransform().translate, 0.5f);
		if (hasOBB) {
			bounds = CollisionSpatialGrid::FromOBB(obb->GetWorldOBB());
		}
		if (hasSphere) {
			const CollisionSpatialGrid::Bounds sphereBounds =
				CollisionSpatialGrid::FromSphere(sphere->GetWorldSphere());
			bounds = hasOBB ? CollisionSpatialGrid::Merge(bounds, sphereBounds) : sphereBounds;
		}
		collisionSpatialGrid_.Insert(playerObject.get(), bounds);
	}

	for (const auto& projectileObject : sceneObjects) {
		EnemyAttackEntityComponent* projectile = projectileObject->GetComponent<EnemyAttackEntityComponent>();
		if (!projectile || !projectile->IsEnabled() || projectile->IsExpired()) {
			continue;
		}

		// 通常は生成時に追加した球コライダーを使い、旧データなど未設定の場合は弾サイズから補完する。
		const SphereColliderComponent* projectileCollider =
			projectileObject->GetComponent<SphereColliderComponent>();
		// 明示的に無効化したコライダーは当たり判定にも参加させない。
		if (projectileCollider && !projectileCollider->IsEnabled()) {
			continue;
		}
		const SphereColliderShape projectileSphere = projectileCollider
			? projectileCollider->GetWorldSphere()
			: SphereColliderShape{projectileObject->GetTransform().translate, projectile->GetSize()};
		const Vector3 projectileStart = projectile->GetPreviousPosition();
		const Vector3 projectileEnd = projectileSphere.center;
		const std::vector<Entity*> playerCandidates = collisionSpatialGrid_.Query(
			CollisionSpatialGrid::FromSweptSphere(projectileStart, projectileEnd, projectileSphere.radius));
		collisionDebugStats_.enemyProjectileCandidates += playerCandidates.size();

		for (Entity* playerObject : playerCandidates) {
			PlayerComponent* player = playerObject->GetComponent<PlayerComponent>();
			if (!player || !player->IsEnabled() || player->GetCurrentHealth() <= 0.0f) {
				continue;
			}
			++collisionDebugStats_.enemyProjectileNarrowPhaseTests;

			bool isHit = false;
			// 敵弾は XZ 平面上を移動するトップダウン攻撃として扱う。
			// ボスとプレイヤーは接地処理やモデル原点の違いで Y 座標がずれるため、
			// 3D のまま判定すると画面上では重なった弾が頭上をすり抜けてしまう。
			// プレイヤーの実形状を優先し、OBB と球のどちらの構成にも対応する。
			if (const OBBColliderComponent* playerCollider = playerObject->GetComponent<OBBColliderComponent>();
			    playerCollider && playerCollider->IsEnabled()) {
				const OBBColliderShape playerOBB = playerCollider->GetWorldOBB();
				Vector3 horizontalStart = projectileStart;
				Vector3 horizontalEnd = projectileEnd;
				horizontalStart.y = playerOBB.center.y;
				horizontalEnd.y = playerOBB.center.y;
				isHit = IsSweptSphereToOBB(
				    horizontalStart, horizontalEnd, projectileSphere.radius, playerOBB);
			}
			if (!isHit) {
				if (const SphereColliderComponent* playerCollider = playerObject->GetComponent<SphereColliderComponent>();
				    playerCollider && playerCollider->IsEnabled()) {
					const SphereColliderShape playerSphere = playerCollider->GetWorldSphere();
					Vector3 horizontalStart = projectileStart;
					Vector3 horizontalEnd = projectileEnd;
					horizontalStart.y = playerSphere.center.y;
					horizontalEnd.y = playerSphere.center.y;
					isHit = IsSweptSphereToSphere(
					    horizontalStart, horizontalEnd, projectileSphere.radius, playerSphere);
				}
			}
			// コライダーを持たないプレイヤーでも従来どおり最低限の命中判定を行う。
			if (!isHit &&
			    !playerObject->GetComponent<OBBColliderComponent>() &&
			    !playerObject->GetComponent<SphereColliderComponent>()) {
				const SphereColliderShape fallbackPlayerSphere{playerObject->GetTransform().translate, 0.5f};
				Vector3 horizontalStart = projectileStart;
				Vector3 horizontalEnd = projectileEnd;
				horizontalStart.y = fallbackPlayerSphere.center.y;
				horizontalEnd.y = fallbackPlayerSphere.center.y;
				isHit = IsSweptSphereToSphere(
				    horizontalStart, horizontalEnd, projectileSphere.radius, fallbackPlayerSphere);
			}

			if (isHit) {
				// 1発で複数回ダメージを与えないよう、命中済みにして後段の削除処理へ渡す。
				player->TakeDamage(projectile->GetAttack());
				projectile->MarkHit();
				break;
			}
		}
	}
}

/// <summary>
/// 近くにある同じ単位の通常経験値を、合計値を維持したまま次の単位へ圧縮します。
/// </summary>
void BaseScene::UpdateExperienceCompression() {
	// Worldは一度だけ走査し、単位別の候補索引を作る。下位から昇格した経験値は次の配列へ追加する。
	std::array<std::vector<Entity*>, kExperienceDenominations.size()> denominationObjects;
	for (Entity* object : sceneObjects_.GetEntitiesWith<ExperienceComponent>()) {
		ExperienceComponent* experience = object->GetComponent<ExperienceComponent>();
		if (!experience || experience->IsBossUpgradeReward() || experience->IsCollected() ||
		    experience->IsConsumedByCompression()) {
			continue;
		}
		for (size_t index = 0; index < kExperienceDenominations.size(); ++index) {
			if (experience->GetExperience() == kExperienceDenominations[index]) {
				denominationObjects[index].push_back(object);
				break;
			}
		}
	}

	// 小さい単位から順に処理することで、1→10→50→100の連鎖圧縮を同じフレーム内で成立させる。
	for (size_t denominationIndex = 0; denominationIndex + 1 < kExperienceDenominations.size(); ++denominationIndex) {
		const int denomination = kExperienceDenominations[denominationIndex];
		const int nextDenomination = kExperienceDenominations[denominationIndex + 1];
		// 例: 1→10は10個、10→50は5個、50→100は2個必要になる。
		const int requiredCount = nextDenomination / denomination;

		// 同じ単位だけを空間グリッドへ登録し、各基準点の近傍候補を全World走査せず取得する。
		CollisionSpatialGrid experienceGrid(kExperienceCompressionDistance);
		for (Entity* object : denominationObjects[denominationIndex]) {
			experienceGrid.Insert(object, CollisionSpatialGrid::FromCenterRadius(object->GetTransform().translate, 0.0f));
		}

		for (Entity* anchorObject : denominationObjects[denominationIndex]) {
			ExperienceComponent* anchor = anchorObject->GetComponent<ExperienceComponent>();
			if (!anchor || anchor->IsCollected() || anchor->IsConsumedByCompression() ||
			    anchor->GetExperience() != denomination) {
				continue;
			}

			std::vector<Entity*> nearbyObjects;
			nearbyObjects.reserve(requiredCount);
			nearbyObjects.push_back(anchorObject);
			const Vector3 anchorPosition = anchorObject->GetTransform().translate;
			const std::vector<Entity*> candidates = experienceGrid.Query(
				CollisionSpatialGrid::FromCenterRadius(anchorPosition, kExperienceCompressionDistance));
			for (Entity* candidateObject : candidates) {
				if (candidateObject == anchorObject) continue;
				ExperienceComponent* candidate = candidateObject->GetComponent<ExperienceComponent>();
				if (!candidate || candidate->IsCollected() || candidate->IsConsumedByCompression() ||
				    candidate->GetExperience() != denomination) {
					continue;
				}
				if (Length(candidateObject->GetTransform().translate - anchorPosition) <= kExperienceCompressionDistance) {
					nearbyObjects.push_back(candidateObject);
					if (static_cast<int>(nearbyObjects.size()) == requiredCount) break;
				}
			}
			if (static_cast<int>(nearbyObjects.size()) < requiredCount) continue;

			// まとめた経験値の中心へ残存オブジェクトを移し、次の単位の見た目と値へ更新する。
			Vector3 mergedPosition{};
			for (Entity* object : nearbyObjects) {
				mergedPosition = mergedPosition + object->GetTransform().translate;
			}
			mergedPosition = (1.0f / static_cast<float>(requiredCount)) * mergedPosition;
			anchorObject->GetTransform().translate = mergedPosition;
			const float scale = GetExperienceScale(nextDenomination);
			anchorObject->GetTransform().scale = {scale, scale, scale};
			anchor->SetExperience(nextDenomination);
			if (Object3dComponent* object3d = anchorObject->GetComponent<Object3dComponent>()) {
				object3d->SetColor(GetExperienceColor(nextDenomination));
			}
			// 基準以外は削除処理へ渡し、昇格した基準だけを次段階の候補へ追加する。
			for (size_t index = 1; index < nearbyObjects.size(); ++index) {
				nearbyObjects[index]->GetComponent<ExperienceComponent>()->MarkConsumedByCompression();
			}
			denominationObjects[denominationIndex + 1].push_back(anchorObject);
		}
	}
}

void BaseScene::UpdateItemDrops() {
	// 接触判定はItemDropComponentが行い、シーン内の他オブジェクトが必要な効果だけをここで適用する。
	for (const auto& itemObject : sceneObjects_) {
		ItemDropComponent* item = itemObject->GetComponent<ItemDropComponent>();
		if (!item || !item->IsCollected() || item->IsEffectApplied()) {
			continue;
		}

		if (item->GetType() == ItemDropType::CollectAllExperience) {
			// ボス強化報酬や取得・圧縮済みの経験値は通常経験値ではないため対象外とする。
			for (const auto& experienceObject : sceneObjects_) {
				ExperienceComponent* experience = experienceObject->GetComponent<ExperienceComponent>();
				if (!experience || experience->IsBossUpgradeReward() || experience->IsCollected() ||
				    experience->IsConsumedByCompression()) {
					continue;
				}
				// 距離制限を実質解除し、フィールド上の経験値を一斉にプレイヤーへ向かわせる。
				experience->SetAttractDistance((std::numeric_limits<float>::max)());
				experience->SetAttractSpeed(0.22f);
			}
		} else if (item->GetType() == ItemDropType::Money) {
			// Gドロップを拾った瞬間に倍率を掛け、ゲームを途中終了しても獲得分が残るよう即時保存する。
			const int baseMoney = item->GetMoneyAmount();
			const int gainedMoney = sceneManager_ ? sceneManager_->ApplyGlobalGoldBonus(baseMoney) : baseMoney;
			// リザルト集計と永続所持金へ同じ補正後金額を渡し、画面表示との差を作らない。
			gameplaySession_.AddChallengeMoney(gainedMoney);
			if (sceneManager_) sceneManager_->AddMoney(gainedMoney);
		}
		item->MarkEffectApplied();
	}
}


