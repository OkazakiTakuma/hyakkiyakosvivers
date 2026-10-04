#include "PlayerAttackComponent.h"
#include "GameObject.h"
#include "MathConstants.h"
#include "../../Player/Player.h"
#include "../base/GameTime.h"
#include <algorithm>
#include <cmath>

void PlayerAttackComponent::Update() {
		// 各攻撃スロットのクールダウンを進め、発射可能な攻撃を要求へ変換する。
		GameObject* owner = GetOwner();
		if (!owner) {
			return;
		}

		Player* player = owner->GetComponent<Player>();
		if (!player) {
			return;
		}

		const float playerAttackSpeedRate = (std::max)(0.01f, player->GetStats().attackSpeed / 100.0f);
		for (AttackSlotRuntime& slot : slots_) {
			if (!slot.enabled) {
				continue;
			}
			if (slot.attackTimer > 0.0f) {
				slot.attackTimer -= GameTime::GetDeltaTime();
			}
			if (slot.attackTimer > 0.0f) {
				continue;
			}
			const std::string currentLevel = slot.level;
			const PlayerAttackLevelStats levelStats = FindCurrentLevelStats(slot, currentLevel);
			CreateAttackByName(owner, *player, slot, levelStats, currentLevel);
			slot.attackTimer = levelStats.attackInterval / playerAttackSpeedRate;
		}
	}

void PlayerAttackComponent::ApplyAttackStats(const PlayerAttackStats& stats, const std::string& level) {
		ClearAttackSlots();
		AddAttackSlot(stats, level, true);
	}

void PlayerAttackComponent::ClearAttackSlots() {
		slots_.clear();
	}

void PlayerAttackComponent::AddAttackSlot(const PlayerAttackStats& stats, const std::string& level, bool enabled) {
		AttackSlotRuntime slot;
		slot.stats = stats;
		slot.level = NormalizeLevel(level);
		slot.enabled = enabled;
		slots_.push_back(slot);
	}

void PlayerAttackComponent::UpdateAttackStatsByName(const std::string& attackName, const PlayerAttackStats& stats) {
		for (AttackSlotRuntime& slot : slots_) {
			if (slot.stats.name == attackName) {
				slot.stats = stats;
			}
		}
	}

const PlayerAttackStats& PlayerAttackComponent::GetAttackStats() const { return slots_.empty() ? emptyStats_ : slots_.front().stats; }

const std::string& PlayerAttackComponent::GetAttackName() const { return slots_.empty() ? emptyStats_.name : slots_.front().stats.name; }

const std::string& PlayerAttackComponent::GetLevel() const { return slots_.empty() ? defaultLevel_ : slots_.front().level; }

void PlayerAttackComponent::SetLevel(const std::string& level) {
		if (!slots_.empty()) {
			slots_.front().level = NormalizeLevel(level);
		}
	}

std::vector<PlayerAttackShotRequest> PlayerAttackComponent::ConsumeShotRequests() {
		std::vector<PlayerAttackShotRequest> requests = shotRequests_;
		shotRequests_.clear();
		return requests;
	}

std::string PlayerAttackComponent::NormalizeLevel(const std::string& level) {
		if (level == "1" || level == "2" || level == "3" || level == "4" || level == "5" || level == "super") {
			return level;
		}
		return "1";
	}

PlayerAttackLevelStats PlayerAttackComponent::FindCurrentLevelStats(const AttackSlotRuntime& slot, const std::string& level) const {
		for (const PlayerAttackLevelStats& levelStats : slot.stats.levels) {
			if (levelStats.level == level) {
				return levelStats;
			}
		}
		if (!slot.stats.levels.empty()) {
			return slot.stats.levels.front();
		}
		return {};
	}

Vector3 PlayerAttackComponent::RotateYaw(const Vector3& direction, float degrees) {
		const float radians = degrees * MathConstants::kDegreesToRadians;
		const float c = std::cos(radians);
		const float s = std::sin(radians);
		const Vector3 rotated = {direction.x * c + direction.z * s, 0.0f, -direction.x * s + direction.z * c};
		return NormalizeReturnVector(rotated);
	}

Vector3 PlayerAttackComponent::GetShotSpawnOffset(const PlayerAttackLevelStats& levelStats, int shotIndex) {
		// 弾番号に対応する値を優先し、不足時は末尾値を複製したものとして扱う。
		if (shotIndex >= 0 && shotIndex < static_cast<int>(levelStats.spawnOffsets.size())) {
			return levelStats.spawnOffsets[shotIndex];
		}
		if (!levelStats.spawnOffsets.empty()) {
			return levelStats.spawnOffsets.back();
		}
		// データが空でも従来と同じ「少し前方・上方」から発射できる安全値を返す。
		return {0.0f, 0.5f, 1.2f};
	}

void PlayerAttackComponent::QueueShot(GameObject* owner, const Player& player, const AttackSlotRuntime& slot, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel, float angleDegrees, int shotIndex, bool alignSpawnToShotAngle) {
		const float playerAttackRate = player.GetStats().attack / 100.0f;
		const float playerAttackSizeRate = player.GetStats().attackSize / 100.0f;
		const Vector3 spawnOffset = GetShotSpawnOffset(levelStats, shotIndex);
		Vector3 forward = {
		    std::sin(owner->GetTransform().rotate.y),
		    0.0f,
		    std::cos(owner->GetTransform().rotate.y)
		};
		forward = NormalizeReturnVector(forward);
		const Vector3 right = {
		    std::cos(owner->GetTransform().rotate.y),
		    0.0f,
		    -std::sin(owner->GetTransform().rotate.y)
		};
		const Vector3 shotDirection = RotateYaw(forward, angleDegrees);
		const Vector3 shotRight = {shotDirection.z, 0.0f, -shotDirection.x};
		// 勾玉では発射角ごとに銃口位置も回し、複数弾が同一点から重ならないようにする。
		const Vector3& spawnForward = alignSpawnToShotAngle ? shotDirection : forward;
		const Vector3& spawnRight = alignSpawnToShotAngle ? shotRight : right;

		PlayerAttackShotRequest request;
		request.attackName = slot.stats.name;
		request.level = currentLevel;
		// ローカルオフセットをプレイヤーの右・上・前ベクトルへ分解してワールド座標へ変換する。
		request.position = owner->GetTransform().translate +
		    spawnOffset.x * spawnRight +
		    Vector3{0.0f, spawnOffset.y, 0.0f} +
		    spawnOffset.z * spawnForward;
		request.direction = shotDirection;
		request.attack = levelStats.attack * playerAttackRate;
		request.speed = levelStats.speed;
		request.size = (levelStats.size / 100.0f) * playerAttackSizeRate;
		request.lifeTime = levelStats.lifeTime;
		request.pierceCount = levelStats.pierceCount;
		request.infinitePierce = levelStats.infinitePierce;
		request.modelFilePath = levelStats.modelFilePath;
		request.homing = levelStats.homing;
		request.homingAccuracy = levelStats.homingAccuracy;
		request.colorIndex = shotIndex % 6;
		shotRequests_.push_back(request);
	}

void PlayerAttackComponent::CreateStraightAttack(GameObject* owner, const Player& player, const AttackSlotRuntime& slot, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel) {
		QueueShot(owner, player, slot, levelStats, currentLevel, 0.0f, 0);
	}

void PlayerAttackComponent::CreateSpreadAttack(GameObject* owner, const Player& player, const AttackSlotRuntime& slot, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel) {
		const int shotCount = (std::max)(1, levelStats.shotCount);
		for (int index = 0; index < shotCount; ++index) {
			const float angle = index < static_cast<int>(levelStats.angles.size()) ? levelStats.angles[index] : 0.0f;
			QueueShot(owner, player, slot, levelStats, currentLevel, angle, index);
		}
	}

void PlayerAttackComponent::CreateHomingAttack(GameObject* owner, const Player& player, const AttackSlotRuntime& slot, PlayerAttackLevelStats levelStats, const std::string& currentLevel) {
		levelStats.homing = true;
		CreateSpreadAttack(owner, player, slot, levelStats, currentLevel);
	}

void PlayerAttackComponent::CreateMagatamaAttack(GameObject* owner, const Player& player, const AttackSlotRuntime& slot, PlayerAttackLevelStats levelStats, const std::string& currentLevel) {
		levelStats.homing = true;
		const int shotCount = (std::max)(1, levelStats.shotCount);
		for (int index = 0; index < shotCount; ++index) {
			// JSONの角度配列により、3発=120度、4発=90度、6発=60度間隔で全周へ発射する。
			const float angle = index < static_cast<int>(levelStats.angles.size()) ? levelStats.angles[index] : 0.0f;
			QueueShot(owner, player, slot, levelStats, currentLevel, angle, index, true);
			PlayerAttackShotRequest& request = shotRequests_.back();
			request.motionType = PlayerProjectileMotionType::Magatama;
		}
	}

void PlayerAttackComponent::CreateOrbitAttack(GameObject* owner, const Player& player, const AttackSlotRuntime& slot, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel) {
		// 弾数分を円周上へ等間隔に配置し、以後の位置更新に使う中心・角度・半径を要求へ記録する。
		const int shotCount = (std::max)(1, levelStats.shotCount);
		for (int index = 0; index < shotCount; ++index) {
			QueueShot(owner, player, slot, levelStats, currentLevel, 0.0f, index);
			PlayerAttackShotRequest& request = shotRequests_.back();
			const Vector3 spawnOffset = GetShotSpawnOffset(levelStats, index);
			const float horizontalRadius = std::sqrt(
			    spawnOffset.x * spawnOffset.x + spawnOffset.z * spawnOffset.z);
			// JSONに個別オフセットがあればその角度を優先し、未設定時は自動的に等間隔へ並べる。
			const float localStartAngle = horizontalRadius > MathConstants::kDirectionEpsilon
			    ? std::atan2(spawnOffset.z, spawnOffset.x)
			    : MathConstants::kTwoPi * static_cast<float>(index) / static_cast<float>(shotCount);
			request.motionType = PlayerProjectileMotionType::Orbit;
			request.motionAnchor = owner;
			// プレイヤーの現在回転を除き、保存したローカル開始角度をワールド周回角へ変換する。
			request.orbitAngleRadians = localStartAngle - owner->GetTransform().rotate.y;
			request.orbitRadius = (std::max)(0.1f, horizontalRadius);
			request.orbitHeight = spawnOffset.y;
			request.orbitAngularSpeed = (std::max)(0.1f, levelStats.speed);
			request.speed = 0.0f;
		}
	}

void PlayerAttackComponent::CreateSkyLaserAttack(GameObject* owner, const Player& player, const AttackSlotRuntime& slot, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel) {
		// 実際の敵座標への置き換えは、画面内の敵を列挙できるBaseScene側で行う。
		const int targetCount = (std::max)(1, levelStats.shotCount);
		for (int index = 0; index < targetCount; ++index) {
			QueueShot(owner, player, slot, levelStats, currentLevel, 0.0f, index);
			PlayerAttackShotRequest& request = shotRequests_.back();
			request.motionType = PlayerProjectileMotionType::SkyLaser;
			request.direction = {0.0f, -1.0f, 0.0f};
			request.speed = 0.0f;
		}
	}

void PlayerAttackComponent::CreateBoomerangAttack(GameObject* owner, const Player& player, const AttackSlotRuntime& slot, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel) {
		// 発射時の進行方向はBaseSceneで最寄りの敵へ向けるが、飛行中は追尾しない。
		QueueShot(owner, player, slot, levelStats, currentLevel, 0.0f, 0);
		PlayerAttackShotRequest& request = shotRequests_.back();
		request.motionType = PlayerProjectileMotionType::Boomerang;
		request.motionAnchor = owner;
		request.travelDistance = levelStats.travelDistance;
		request.homing = false;
	}

void PlayerAttackComponent::CreateRicochetAttack(GameObject* owner, const Player& player, const AttackSlotRuntime& slot, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel) {
		// 画面端・障害物での反射判定は、シーン内のカメラとコライダーを参照できるBaseScene側で行う。
		const int shotCount = (std::max)(1, levelStats.shotCount);
		for (int index = 0; index < shotCount; ++index) {
			const float angle = index < static_cast<int>(levelStats.angles.size()) ? levelStats.angles[index] : 0.0f;
			QueueShot(owner, player, slot, levelStats, currentLevel, angle, index);
			PlayerAttackShotRequest& request = shotRequests_.back();
			request.motionType = PlayerProjectileMotionType::Ricochet;
			request.homing = false;
		}
	}

void PlayerAttackComponent::CreateClawSlashAttack(GameObject* owner, const Player& player, const AttackSlotRuntime& slot, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel) {
		// 通常は3本、JSONでそれ以上の本数が指定された場合（Superなど）はその本数で生成する。
		const int slashCount = (std::max)(3, levelStats.shotCount);
		for (int index = 0; index < slashCount; ++index) {
			QueueShot(owner, player, slot, levelStats, currentLevel, 0.0f, index);
			PlayerAttackShotRequest& request = shotRequests_.back();
			request.motionType = PlayerProjectileMotionType::ClawSlash;
			request.motionAnchor = owner;
			request.speed = 0.0f;
			request.homing = false;
			// 全ての爪が同じ敵へ命中しても、JSONのAttack値が合計ダメージになるよう均等に分割する。
			request.attack /= static_cast<float>(slashCount);
			request.clawSlashIndex = index;
			request.clawSlashCount = slashCount;
		}
	}

void PlayerAttackComponent::CreateAttackByName(GameObject* owner, const Player& player, const AttackSlotRuntime& slot, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel) {
		// 専用挙動を持つ攻撃は名前で生成方式を振り分け、その他は設定値から通常・拡散・追尾を選ぶ。
		if (slot.stats.name == "勾玉") {
			CreateMagatamaAttack(owner, player, slot, levelStats, currentLevel);
			return;
		}
		if (slot.stats.name == "ClawSlash") {
			CreateClawSlashAttack(owner, player, slot, levelStats, currentLevel);
			return;
		}
		if (slot.stats.name == "Ricochet") {
			CreateRicochetAttack(owner, player, slot, levelStats, currentLevel);
			return;
		}
		if (slot.stats.name == "Boomerang") {
			CreateBoomerangAttack(owner, player, slot, levelStats, currentLevel);
			return;
		}
		if (slot.stats.name == "Orbit") {
			CreateOrbitAttack(owner, player, slot, levelStats, currentLevel);
			return;
		}
		if (slot.stats.name == "SkyLaser") {
			CreateSkyLaserAttack(owner, player, slot, levelStats, currentLevel);
			return;
		}
		if (levelStats.homing) {
			CreateHomingAttack(owner, player, slot, levelStats, currentLevel);
			return;
		}
		if (levelStats.shotCount > 1) {
			CreateSpreadAttack(owner, player, slot, levelStats, currentLevel);
			return;
		}
		CreateStraightAttack(owner, player, slot, levelStats, currentLevel);
	}
