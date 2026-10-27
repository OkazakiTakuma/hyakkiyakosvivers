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

void BaseScene::UpdatePlayerAttacks() {
	std::vector<PlayerAttackShotRequest> shotRequests;
	for (const auto& object : sceneObjects_) {
		PlayerAttackComponent* attack = object->GetComponent<PlayerAttackComponent>();
		if (!attack || !attack->IsEnabled()) {
			continue;
		}

		std::vector<PlayerAttackShotRequest> requests = attack->ConsumeShotRequests();
		shotRequests.insert(shotRequests.end(), requests.begin(), requests.end());
	}

	// SkyLaserは画面外の敵を対象にしない。候補をシャッフルして同一発射内では別々の敵へ割り当てる。
	std::vector<GameObject*> laserTargets;
	for (const auto& object : sceneObjects_) {
		EnemyComponent* enemy = object->GetComponent<EnemyComponent>();
		if (enemy && enemy->IsEnabled() && enemy->GetCurrentHealth() > 0.0f &&
		    !IsPointOutsideView(object->GetTransform().translate, 0.0f)) {
			laserTargets.push_back(object.get());
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

GameObject* BaseScene::CreateRuntimeEnemy(
	const std::string& enemyTypeName, const Vector3& position, GameObject* target,
	const EnemySpawnPointComponent::EnemyStatMultipliers& statMultipliers) {
	auto object = std::make_unique<GameObject>();
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
	// 0.75を通常敵の基準サイズとし、MidBossなどはデータ側の倍率で大型化する。
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

	// 行動に対応する標準モデルはStrategyから受け取り、ステージ固有キャラクターだけ名前で上書きする。
	const EnemyBehaviorPresentation initialPresentation = enemy->GetBehaviorPresentation();
	std::string enemyModelFilePath = initialPresentation.modelFilePath;
	if (resolvedTypeName == "Stage2Boss") {
		enemyModelFilePath = "doppelganger.gltf";
		if (!ModelManager::GetInstance()->FindModel(enemyModelFilePath)) {
			ModelManager::GetInstance()->LoadModel(enemyModelFilePath, true, "/doppelganger");
		}
	} else if (resolvedTypeName == "MidBoss") {
		enemyModelFilePath = "neko.gltf";
		if (!ModelManager::GetInstance()->FindModel(enemyModelFilePath)) {
			ModelManager::GetInstance()->LoadModel(enemyModelFilePath, true, "/cat");
		}
	} else {
		ModelManager::GetInstance()->LoadModel(enemyModelFilePath);
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

GameObject* BaseScene::CreateRuntimeExperience(const EnemyStats& enemyStats, const Vector3& position, GameObject* target) {
	if (enemyStats.experience <= 0) {
		return nullptr;
	}
	if (!target) {
		for (const auto& object : sceneObjects_) {
			if (object->GetComponent<Player>()) {
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
	GameObject* firstExperienceObject = nullptr;
	int remainingExperience = enemyStats.experience;
	int particleIndex = 0;
	for (auto denominationIt = kExperienceDenominations.rbegin(); denominationIt != kExperienceDenominations.rend(); ++denominationIt) {
		const int denomination = *denominationIt;
		const int particleCount = remainingExperience / denomination;
		remainingExperience %= denomination;
		for (int index = 0; index < particleCount; ++index, ++particleIndex) {
			auto object = std::make_unique<GameObject>();
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
			GameObject* createdObject = object.get();
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
GameObject* BaseScene::CreateRuntimeItemDrop(
	ItemDropType type, const Vector3& position, GameObject* target, float healAmount, int moneyAmount) {
	// 撃破した敵にターゲットが設定されていない場合も回収できるよう、シーン内のプレイヤーを補完する。
	if (!target) {
		// 全スポーンポイントへ停止状態を伝え、経過秒数とスケジュールカウンターを凍結する。
		for (const auto& object : sceneObjects_) {
			if (object->GetComponent<Player>()) {
				target = object.get();
				break;
			}
		}
	}

	ModelManager::GetInstance()->LoadModel(kEnemyDropModelFilePath);
	auto object = std::make_unique<GameObject>();
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

	object->Update();
	sceneObjects_.push_back(std::move(object));
	++nextObjectId_;
	return sceneObjects_.back().get();
}

/// <summary>
/// ボス撃破位置へ金色の強化報酬を1個生成します。
/// upgradeCountはドロップ数ではなく、回収時に実行する強化抽選の回数です。
/// </summary>
void BaseScene::CreateRuntimeBossUpgradeDrop(const Vector3& position, GameObject* target, int upgradeCount) {
	if (!target) {
		for (const auto& object : sceneObjects_) {
			if (object->GetComponent<Player>()) {
				target = object.get();
				break;
			}
		}
	}

	ModelManager::GetInstance()->LoadModel(kEnemyDropModelFilePath);
	auto object = std::make_unique<GameObject>();
	object->SetName(MakeUniqueObjectName("BossUpgradeReward"));
	object->SetEditorType("BossUpgradeReward");
	object->GetTransform().translate = position + Vector3{0.0f, 0.45f, 0.0f};
	object->GetTransform().scale = {0.65f, 0.65f, 0.65f};

	// 経験値と同じ吸着挙動を再利用し、識別フラグによって経験値加算だけを抑止する。
	ExperienceComponent* reward = object->AddComponent<ExperienceComponent>();
	reward->SetBossUpgradeReward(true);
	reward->SetBossUpgradeCount(upgradeCount);
	reward->SetTarget(target);
	reward->SetAttractDistance(12.0f);
	reward->SetAttractSpeed(0.12f);
	object->AddComponent<DropItemVisualComponent>();

	Object3dComponent* object3d = object->AddComponent<Object3dComponent>();
	object3d->SetModel(kEnemyDropModelFilePath);
	object3d->SetColor({1.0f, 0.72f, 0.08f, 1.0f});
	object3d->SetEmission({0.85f, 0.48f, 0.04f}, 0.75f);

	object->Update();
	sceneObjects_.push_back(std::move(object));
	++nextObjectId_;
}

/// <summary>
/// 装備中の強化可能な攻撃・アイテムを、指定回数だけ独立に抽選して1レベルずつ上げます。
/// 同じ装備の再選出を許可し、途中で上限へ達した装備は次の抽選から除外します。
/// </summary>
int BaseScene::ApplyRandomBossUpgrades(Player* player, int upgradeCount) {
	if (!player || upgradeCount <= 0) {
		return 0;
	}

	struct UpgradeCandidate {
		bool isAttack = false;
		int slotIndex = -1;
		bool promoteToSuper = false;
	};
	static std::mt19937 rewardRandomEngine(std::random_device{}());
	PlayerStats upgradedStats = player->GetBaseStats();
	int appliedCount = 0;
	for (; appliedCount < upgradeCount; ++appliedCount) {
		// 直前の強化結果を反映した状態から候補を作り直すことで、重複選出と上限除外を両立する。
		std::vector<UpgradeCandidate> candidates;
		for (int index = 0; index < static_cast<int>(upgradedStats.attackSlots.size()); ++index) {
			const PlayerAttackSlot& slot = upgradedStats.attackSlots[index];
			if (!slot.enabled || slot.attackName.empty() || slot.attackLevel == "super") {
				continue;
			}
			const int level = std::atoi(slot.attackLevel.c_str());
			if (level >= 1 && level < 5) {
				candidates.push_back({true, index, false});
				continue;
			}
			if (slot.attackLevel != "5") {
				continue;
			}
			// レベル5攻撃は、対応アイテムと必要レベルを満たす場合だけスーパー化候補へ加える。
			const PlayerAttackStats attackStats = LoadPlayerAttackStats(slot.attackName);
			bool conditionMet = !attackStats.superConditionStatusName.empty();
			const int requiredLevel = (std::max)(1, std::atoi(attackStats.superConditionStatusLevel.c_str()));
			for (const PlayerStatusSlot& statusSlot : upgradedStats.statusSlots) {
				if (conditionMet && statusSlot.enabled &&
				    statusSlot.statusName == attackStats.superConditionStatusName &&
				    std::atoi(statusSlot.level.c_str()) >= requiredLevel) {
					candidates.push_back({true, index, true});
					break;
				}
			}
		}
		for (int index = 0; index < static_cast<int>(upgradedStats.statusSlots.size()); ++index) {
			const PlayerStatusSlot& slot = upgradedStats.statusSlots[index];
			const int level = std::atoi(slot.level.c_str());
			if (slot.enabled && !slot.statusName.empty() && level >= 1 && level < 5) {
				candidates.push_back({false, index, false});
			}
		}
		if (candidates.empty()) {
			// 適用できなかった残り回数は、呼び出し側で未所持装備の取得確認へ変換する。
			break;
		}
		std::uniform_int_distribution<size_t> candidateDistribution(0, candidates.size() - 1);
		const UpgradeCandidate& selected = candidates[candidateDistribution(rewardRandomEngine)];
		if (selected.isAttack) {
			PlayerAttackSlot& slot = upgradedStats.attackSlots[selected.slotIndex];
			slot.attackLevel = selected.promoteToSuper
			    ? "super"
			    : std::to_string((std::min)(5, std::atoi(slot.attackLevel.c_str()) + 1));
		} else {
			PlayerStatusSlot& slot = upgradedStats.statusSlots[selected.slotIndex];
			slot.level = std::to_string((std::min)(5, std::atoi(slot.level.c_str()) + 1));
		}
	}

	if (appliedCount > 0) {
		player->ApplyStats(upgradedStats, ApplyPlayerStatusItems(upgradedStats));
		if (GameObject* owner = player->GetOwner()) {
			ApplyPlayerAttackSlots(owner->GetComponent<PlayerAttackComponent>(), upgradedStats);
		}
	}
	return appliedCount;
}

/// <summary>
/// 回収済みの金色ボス報酬へ強化効果を適用し、強化アイテム数に比例したお金を付与します。
/// </summary>
void BaseScene::UpdateBossUpgradeRewards() {
	for (const auto& object : sceneObjects_) {
		ExperienceComponent* reward = object->GetComponent<ExperienceComponent>();
		if (!reward || !reward->IsBossUpgradeReward() || !reward->IsCollected() || reward->IsBossUpgradeApplied()) {
			continue;
		}
		Player* player = reward->GetTarget() ? reward->GetTarget()->GetComponent<Player>() : nullptr;
		// 金色報酬を拾った瞬間に全体G強化を適用し、所持金と今回獲得額の両方へ反映する。
		const int baseMoney = reward->GetBossUpgradeCount() * 100;
		const int gainedMoney = sceneManager ? sceneManager->ApplyGlobalGoldBonus(baseMoney) : baseMoney;
		// challengeMoneyEarned_はリザルト表示用、AddMoneyは所持金更新とJSON即時保存を担当する。
		challengeMoneyEarned_ += gainedMoney;
		if (sceneManager) sceneManager->AddMoney(gainedMoney);
		const int appliedCount = ApplyRandomBossUpgrades(player, reward->GetBossUpgradeCount());
		QueueBossAcquisitionOffers(player, reward->GetBossUpgradeCount() - appliedCount);
		reward->MarkBossUpgradeApplied();
	}
}

/// <summary>
/// 強化対象が存在しなかった回数分、空きスロットへ追加できる未所持装備をランダムに予約します。
/// 同じ候補は1つの予約列へ重複登録せず、候補数が不足する場合は存在する分だけ提示します。
/// </summary>
void BaseScene::QueueBossAcquisitionOffers(Player* player, int offerCount) {
	if (!player || offerCount <= 0) {
		return;
	}
	const PlayerStats& stats = player->GetBaseStats();
	const bool hasEmptyAttackSlot = std::any_of(stats.attackSlots.begin(), stats.attackSlots.end(), [](const PlayerAttackSlot& slot) {
		return slot.attackName.empty();
	});
	const bool hasEmptyStatusSlot = std::any_of(stats.statusSlots.begin(), stats.statusSlots.end(), [](const PlayerStatusSlot& slot) {
		return slot.statusName.empty();
	});

	const auto otherPlayerInitialAttacks = GetOtherPlayerInitialAttacks(*player);
	std::unordered_set<std::string> ownedAttacks;
	std::unordered_set<std::string> ownedStatuses;
	for (const PlayerAttackSlot& slot : stats.attackSlots) {
		if (!slot.attackName.empty()) {
			ownedAttacks.insert(slot.attackName);
		}
	}
	for (const PlayerStatusSlot& slot : stats.statusSlots) {
		if (!slot.statusName.empty()) {
			ownedStatuses.insert(slot.statusName);
		}
	}

	// 攻撃とステータスアイテムを同じ候補配列へ集約し、種類をまたいで公平にシャッフルする。
	std::vector<LevelUpChoice> offers;
	if (hasEmptyAttackSlot) {
		for (const std::string& attackName : LoadPlayerAttackNames()) {
			// 通常レベルアップで抽選されない他タイプの初期武器は、ボス報酬でも除外する。
			if (ownedAttacks.find(attackName) != ownedAttacks.end() ||
			    otherPlayerInitialAttacks.contains(attackName)) {
				continue;
			}
			const PlayerAttackStats attackStats = LoadPlayerAttackStats(attackName);
			std::string description = "Acquire this attack at level 1?";
			std::string texture = attackStats.choiceTextureFilePath;
			for (const PlayerAttackLevelStats& levelStats : attackStats.levels) {
				if (levelStats.level == "1") {
					if (!levelStats.choiceDescription.empty()) {
						description = levelStats.choiceDescription;
					}
					if (texture.empty()) {
						texture = levelStats.choiceTextureFilePath;
					}
					break;
				}
			}
			offers.push_back({LevelUpChoiceType::NewAttack, attackName, "Acquire " + attackName, description, -1, texture});
		}
	}
	if (hasEmptyStatusSlot) {
		for (const std::string& statusName : LoadPlayerStatusItemNames()) {
			if (ownedStatuses.find(statusName) != ownedStatuses.end()) {
				continue;
			}
			const PlayerStatusItemStats statusStats = LoadPlayerStatusItemStats(statusName);
			const std::string description = statusStats.levelDescriptions[0].empty()
			    ? "Acquire this item at level 1?"
			    : statusStats.levelDescriptions[0];
			offers.push_back({
				LevelUpChoiceType::NewStatus,
				statusName,
				"Acquire " + statusName,
				description,
				-1,
				statusStats.levelTextureFilePaths[0]
			});
		}
	}
	if (offers.empty()) {
		return;
	}

	static std::mt19937 offerRandomEngine(std::random_device{}());
	std::shuffle(offers.begin(), offers.end(), offerRandomEngine);
	const int queuedCount = (std::min)(offerCount, static_cast<int>(offers.size()));
	bossAcquisitionOfferQueue_.insert(
	    bossAcquisitionOfferQueue_.end(),
	    offers.begin(),
	    offers.begin() + queuedCount
	);
	bossAcquisitionPlayer_ = player;
	if (!IsLevelUpSelectionActive()) {
		ShowNextBossAcquisitionOffer();
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
	for (const auto& object : sceneObjects_) {
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
			GameObject* target = enemy->GetTarget();
			Player* player = target ? target->GetComponent<Player>() : nullptr;
			if (player && player->IsEnabled() && player->GetCurrentHealth() > 0.0f) {
				Vector3 toPlayer = target->GetTransform().translate - object->GetTransform().translate;
				toPlayer.y = 0.0f;
				if (Length(toPlayer) <= enemy->GetStats().selfDestructRadius) {
					player->TakeDamage(enemy->GetStats().attack);
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
			} else if (enemy->GetEnemyTypeName() == "MidBoss") {
				// 行動は通常追跡のまま、紫色と大型シルエットで中ボスだと識別できるようにする。
				object3d->SetColor({0.62f, 0.16f, 0.85f, 1.0f});
			} else {
				// 被弾色が終了した通常の追跡敵を標準色へ戻す。
				object3d->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
			}
		}
	}
	for (const EnemyShotRequest& request : shotRequests) {
		CreateRuntimeEnemyAttackEntity(request);
	}
}

/// <summary>敵の発射要求から、移動・描画・当たり判定を持つ敵弾を生成します。</summary>
GameObject* BaseScene::CreateRuntimeEnemyAttackEntity(const EnemyShotRequest& request) {
	auto object = std::make_unique<GameObject>();
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
		ParticlePresetRepository::Apply("Tornado", tornadoEmitter);
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
void BaseScene::UpdateEnemyAttackEntityHits() {
	// プレイヤーの実コライダー範囲を登録し、高速弾は移動線分を包む範囲で候補検索する。
	collisionSpatialGrid_.Clear();
	collisionDebugStats_.enemyProjectileCandidates = 0;
	collisionDebugStats_.enemyProjectileNarrowPhaseTests = 0;
	for (const auto& playerObject : sceneObjects_) {
		Player* player = playerObject->GetComponent<Player>();
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

	for (const auto& projectileObject : sceneObjects_) {
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
		const std::vector<GameObject*> playerCandidates = collisionSpatialGrid_.Query(
			CollisionSpatialGrid::FromSweptSphere(projectileStart, projectileEnd, projectileSphere.radius));
		collisionDebugStats_.enemyProjectileCandidates += playerCandidates.size();

		for (GameObject* playerObject : playerCandidates) {
			Player* player = playerObject->GetComponent<Player>();
			if (!player || !player->IsEnabled() || player->GetCurrentHealth() <= 0.0f) {
				continue;
			}
			++collisionDebugStats_.enemyProjectileNarrowPhaseTests;

			bool isHit = false;
			// プレイヤーの実形状を優先し、OBB と球のどちらの構成にも対応する。
			if (const OBBColliderComponent* playerCollider = playerObject->GetComponent<OBBColliderComponent>();
			    playerCollider && playerCollider->IsEnabled()) {
				isHit = IsSweptSphereToOBB(
				    projectileStart, projectileEnd, projectileSphere.radius, playerCollider->GetWorldOBB());
			}
			if (!isHit) {
				if (const SphereColliderComponent* playerCollider = playerObject->GetComponent<SphereColliderComponent>();
				    playerCollider && playerCollider->IsEnabled()) {
					isHit = IsSweptSphereToSphere(
					    projectileStart, projectileEnd, projectileSphere.radius, playerCollider->GetWorldSphere());
				}
			}
			// コライダーを持たないプレイヤーでも従来どおり最低限の命中判定を行う。
			if (!isHit &&
			    !playerObject->GetComponent<OBBColliderComponent>() &&
			    !playerObject->GetComponent<SphereColliderComponent>()) {
				const SphereColliderShape fallbackPlayerSphere{playerObject->GetTransform().translate, 0.5f};
				isHit = IsSweptSphereToSphere(
				    projectileStart, projectileEnd, projectileSphere.radius, fallbackPlayerSphere);
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
	// 小さい単位から順に処理することで、1→10→50→100の連鎖圧縮を同じフレーム内で成立させる。
	for (size_t denominationIndex = 0; denominationIndex + 1 < kExperienceDenominations.size(); ++denominationIndex) {
		const int denomination = kExperienceDenominations[denominationIndex];
		const int nextDenomination = kExperienceDenominations[denominationIndex + 1];
		// 例: 1→10は10個、10→50は5個、50→100は2個必要になる。
		const int requiredCount = nextDenomination / denomination;

		// 同じ段階で複数組を圧縮できるよう、候補がなくなるまで検索を繰り返す。
		bool compressed = true;
		while (compressed) {
			compressed = false;
			for (const auto& anchorObject : sceneObjects_) {
				ExperienceComponent* anchor = anchorObject->GetComponent<ExperienceComponent>();
				// ボス強化報酬や、すでに回収・圧縮されたオブジェクトは通常経験値としてまとめない。
				if (!anchor || anchor->IsBossUpgradeReward() || anchor->IsCollected() || anchor->IsConsumedByCompression() ||
				    anchor->GetExperience() != denomination) {
					continue;
				}

				// 基準位置の周囲から、同じ単位の経験値を必要数だけ集める。
				std::vector<GameObject*> nearbyObjects;
				nearbyObjects.reserve(requiredCount);
				nearbyObjects.push_back(anchorObject.get());
				const Vector3 anchorPosition = anchorObject->GetTransform().translate;
				for (const auto& candidateObject : sceneObjects_) {
					if (candidateObject.get() == anchorObject.get()) {
						continue;
					}
					ExperienceComponent* candidate = candidateObject->GetComponent<ExperienceComponent>();
					if (!candidate || candidate->IsBossUpgradeReward() || candidate->IsCollected() || candidate->IsConsumedByCompression() ||
					    candidate->GetExperience() != denomination) {
						continue;
					}
					if (Length(candidateObject->GetTransform().translate - anchorPosition) <= kExperienceCompressionDistance) {
						nearbyObjects.push_back(candidateObject.get());
						if (static_cast<int>(nearbyObjects.size()) == requiredCount) {
							break;
						}
					}
				}
				if (static_cast<int>(nearbyObjects.size()) < requiredCount) {
					continue;
				}

				// まとめた経験値の中心へ残存オブジェクトを移し、次の単位の見た目と値へ更新する。
				Vector3 mergedPosition{};
				for (GameObject* object : nearbyObjects) {
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
				// 基準以外は削除処理へ渡す。経験値は基準側へ集約済みなので、プレイヤーには加算しない。
				for (size_t index = 1; index < nearbyObjects.size(); ++index) {
					nearbyObjects[index]->GetComponent<ExperienceComponent>()->MarkConsumedByCompression();
				}
				compressed = true;
				break;
			}
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
			const int gainedMoney = sceneManager ? sceneManager->ApplyGlobalGoldBonus(baseMoney) : baseMoney;
			// リザルト集計と永続所持金へ同じ補正後金額を渡し、画面表示との差を作らない。
			challengeMoneyEarned_ += gainedMoney;
			if (sceneManager) sceneManager->AddMoney(gainedMoney);
		}
		item->MarkEffectApplied();
	}
}


