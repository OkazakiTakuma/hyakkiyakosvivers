#include "components/PlayerAttack/PlayerAttackComponent.h"
#include "data/PlayerAttackLevel.h"
#include "core/Entity.h"
#include "MathConstants.h"
#include "../../../../Player/PlayerComponent.h"
#include "GameTime.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <random>
#include <utility>

namespace {
Vector3 RotateYaw(const Vector3& direction, float degrees) {
	const float radians = degrees * MathConstants::kDegreesToRadians;
	const float c = std::cos(radians);
	const float s = std::sin(radians);
	return Normalize(Vector3{
		direction.x * c + direction.z * s,
		0.0f,
		-direction.x * s + direction.z * c});
}

Vector3 GetShotSpawnOffset(const PlayerAttackLevelStats& levelStats, int shotIndex) {
	// 弾番号に対応する値を優先し、不足時は末尾値を複製したものとして扱う。
	if (shotIndex >= 0 && shotIndex < static_cast<int>(levelStats.spawnOffsets.size())) {
		return levelStats.spawnOffsets[shotIndex];
	}
	if (!levelStats.spawnOffsets.empty()) {
		return levelStats.spawnOffsets.back();
	}
	return {0.0f, 0.5f, 1.2f};
}

PlayerAttackShotRequest CreateShotRequest(
	const PlayerAttackContext& context,
	const PlayerAttackStats& attackStats,
	const PlayerAttackLevelStats& levelStats,
	const std::string& currentLevel,
	float angleDegrees,
	int shotIndex,
	bool alignSpawnToShotAngle = false) {
	const Vector3 spawnOffset = GetShotSpawnOffset(levelStats, shotIndex);
	Vector3 forward = Normalize(Vector3{
		std::sin(context.ownerRotationY), 0.0f, std::cos(context.ownerRotationY)});
	const Vector3 right = {
		std::cos(context.ownerRotationY), 0.0f, -std::sin(context.ownerRotationY)};
	const Vector3 shotDirection = RotateYaw(forward, angleDegrees);
	const Vector3 shotRight = {shotDirection.z, 0.0f, -shotDirection.x};
	const Vector3& spawnForward = alignSpawnToShotAngle ? shotDirection : forward;
	const Vector3& spawnRight = alignSpawnToShotAngle ? shotRight : right;

	PlayerAttackShotRequest request;
	request.attackName = attackStats.name;
	request.visualId = attackStats.visualId;
	request.level = currentLevel;
	request.position = context.ownerPosition +
		spawnOffset.x * spawnRight +
		Vector3{0.0f, spawnOffset.y, 0.0f} +
		spawnOffset.z * spawnForward;
	request.direction = shotDirection;
	request.sourceOwner = context.motionAnchor;
	request.attack = levelStats.attack * context.attackRate;
	request.speed = levelStats.speed;
	request.size = (levelStats.size / 100.0f) * context.attackSizeRate;
	request.lifeTime = levelStats.lifeTime;
	request.repeatHitInterval = levelStats.repeatHitInterval;
	request.pierceCount = levelStats.pierceCount;
	request.knockback = levelStats.knockback;
	request.infinitePierce = levelStats.infinitePierce;
	request.modelFilePath = levelStats.modelFilePath;
	request.homing = levelStats.homing;
	request.homingAccuracy = levelStats.homingAccuracy;
	request.colorIndex = shotIndex % 6;
	return request;
}

std::vector<PlayerAttackShotRequest> CreateSpreadRequests(
	const PlayerAttackContext& context,
	const PlayerAttackStats& attackStats,
	const PlayerAttackLevelStats& levelStats,
	const std::string& currentLevel,
	bool alignSpawnToShotAngle = false) {
	std::vector<PlayerAttackShotRequest> requests;
	const int shotCount = (std::max)(1, levelStats.shotCount);
	requests.reserve(shotCount);
	for (int index = 0; index < shotCount; ++index) {
		const float angle = index < static_cast<int>(levelStats.angles.size())
			? levelStats.angles[index]
			: 0.0f;
		requests.push_back(CreateShotRequest(
			context, attackStats, levelStats, currentLevel, angle, index, alignSpawnToShotAngle));
	}
	return requests;
}

// 通常・拡散・追尾はshotCount、angles、homingのデータだけで要求を生成する。
class DefaultPlayerAttackPattern final : public IPlayerAttackPattern {
public:
	std::vector<PlayerAttackShotRequest> CreateShots(
		const PlayerAttackContext& context,
		const PlayerAttackStats& attackStats,
		const PlayerAttackLevelStats& levelStats,
		const std::string& currentLevel) const override {
		return CreateSpreadRequests(context, attackStats, levelStats, currentLevel);
	}
};

class MagatamaPlayerAttackPattern final : public IPlayerAttackPattern {
public:
	std::vector<PlayerAttackShotRequest> CreateShots(
		const PlayerAttackContext& context,
		const PlayerAttackStats& attackStats,
		const PlayerAttackLevelStats& sourceLevelStats,
		const std::string& currentLevel) const override {
		PlayerAttackLevelStats levelStats = sourceLevelStats;
		levelStats.homing = true;
		std::vector<PlayerAttackShotRequest> requests = CreateSpreadRequests(
			context, attackStats, levelStats, currentLevel, true);
		for (PlayerAttackShotRequest& request : requests) {
			request.motionType = PlayerAttackEntityMotionType::Magatama;
		}
		return requests;
	}
};

class OrbitPlayerAttackPattern final : public IPlayerAttackPattern {
public:
	explicit OrbitPlayerAttackPattern(PlayerAttackEntityMotionType motionType)
		: motionType_(motionType) {}

	std::vector<PlayerAttackShotRequest> CreateShots(
		const PlayerAttackContext& context,
		const PlayerAttackStats& attackStats,
		const PlayerAttackLevelStats& levelStats,
		const std::string& currentLevel) const override {
		std::vector<PlayerAttackShotRequest> requests;
		const int shotCount = (std::max)(1, levelStats.shotCount);
		requests.reserve(shotCount);
		for (int index = 0; index < shotCount; ++index) {
			PlayerAttackShotRequest request = CreateShotRequest(
				context, attackStats, levelStats, currentLevel, 0.0f, index);
			const Vector3 spawnOffset = GetShotSpawnOffset(levelStats, index);
			const float horizontalRadius = std::sqrt(
				spawnOffset.x * spawnOffset.x + spawnOffset.z * spawnOffset.z);
			const float localStartAngle = horizontalRadius > MathConstants::kDirectionEpsilon
				? std::atan2(spawnOffset.z, spawnOffset.x)
				: MathConstants::kTwoPi * static_cast<float>(index) / static_cast<float>(shotCount);
			request.motionType = motionType_;
			request.motionAnchor = context.motionAnchor;
			request.orbitAngleRadians = localStartAngle - context.ownerRotationY;
			request.orbitRadius = (std::max)(0.1f, horizontalRadius);
			request.orbitHeight = spawnOffset.y;
			request.orbitAngularSpeed = (std::max)(0.1f, levelStats.speed);
			request.speed = 0.0f;
			if (motionType_ == PlayerAttackEntityMotionType::FlameField) {
				static std::mt19937 randomEngine(std::random_device{}());
				std::uniform_real_distribution<float> angleDistribution(0.0f, MathConstants::kTwoPi);
				std::uniform_real_distribution<float> distanceDistribution(2.5f, 7.0f);
				const float angle = angleDistribution(randomEngine);
				const float distance = distanceDistribution(randomEngine);
				request.position = context.ownerPosition + Vector3{
					std::cos(angle) * distance,
					levelStats.spawnOffsets.empty() ? 0.5f : levelStats.spawnOffsets.front().y,
					std::sin(angle) * distance};
				request.motionAnchor = nullptr;
			}
			requests.push_back(std::move(request));
		}
		return requests;
	}

private:
	PlayerAttackEntityMotionType motionType_;
};

class SkyLaserPlayerAttackPattern final : public IPlayerAttackPattern {
public:
	std::vector<PlayerAttackShotRequest> CreateShots(
		const PlayerAttackContext& context,
		const PlayerAttackStats& attackStats,
		const PlayerAttackLevelStats& levelStats,
		const std::string& currentLevel) const override {
		std::vector<PlayerAttackShotRequest> requests;
		const int targetCount = (std::max)(1, levelStats.shotCount);
		requests.reserve(targetCount);
		for (int index = 0; index < targetCount; ++index) {
			PlayerAttackShotRequest request = CreateShotRequest(
				context, attackStats, levelStats, currentLevel, 0.0f, index);
			request.motionType = PlayerAttackEntityMotionType::SkyLaser;
			request.direction = {0.0f, -1.0f, 0.0f};
			request.speed = 0.0f;
			requests.push_back(std::move(request));
		}
		return requests;
	}
};

class BoomerangPlayerAttackPattern final : public IPlayerAttackPattern {
public:
	std::vector<PlayerAttackShotRequest> CreateShots(
		const PlayerAttackContext& context,
		const PlayerAttackStats& attackStats,
		const PlayerAttackLevelStats& levelStats,
		const std::string& currentLevel) const override {
		PlayerAttackShotRequest request = CreateShotRequest(
			context, attackStats, levelStats, currentLevel, 0.0f, 0);
		request.motionType = PlayerAttackEntityMotionType::Boomerang;
		request.motionAnchor = context.motionAnchor;
		request.travelDistance = levelStats.travelDistance;
		request.homing = false;
		request.modelFilePath = "oonusa_handle.obj";
		return {std::move(request)};
	}
};

class RicochetPlayerAttackPattern final : public IPlayerAttackPattern {
public:
	std::vector<PlayerAttackShotRequest> CreateShots(
		const PlayerAttackContext& context,
		const PlayerAttackStats& attackStats,
		const PlayerAttackLevelStats& levelStats,
		const std::string& currentLevel) const override {
		std::vector<PlayerAttackShotRequest> requests = CreateSpreadRequests(
			context, attackStats, levelStats, currentLevel);
		for (PlayerAttackShotRequest& request : requests) {
			request.motionType = PlayerAttackEntityMotionType::Ricochet;
			request.homing = false;
			request.modelFilePath = "ricochet_ofuda_paper.obj";
		}
		return requests;
	}
};

class ClawSlashPlayerAttackPattern final : public IPlayerAttackPattern {
public:
	std::vector<PlayerAttackShotRequest> CreateShots(
		const PlayerAttackContext& context,
		const PlayerAttackStats& attackStats,
		const PlayerAttackLevelStats& levelStats,
		const std::string& currentLevel) const override {
		std::vector<PlayerAttackShotRequest> requests;
		const int slashCount = (std::max)(3, levelStats.shotCount);
		requests.reserve(slashCount);
		for (int index = 0; index < slashCount; ++index) {
			PlayerAttackShotRequest request = CreateShotRequest(
				context, attackStats, levelStats, currentLevel, 0.0f, index);
			request.motionType = PlayerAttackEntityMotionType::ClawSlash;
			request.motionAnchor = context.motionAnchor;
			request.speed = 0.0f;
			request.homing = false;
			request.attack /= static_cast<float>(slashCount);
			request.clawSlashIndex = index;
			request.clawSlashCount = slashCount;
			requests.push_back(std::move(request));
		}
		return requests;
	}
};
}

void PlayerAttackComponent::Update() {
		// 各攻撃スロットのクールダウンを進め、発射可能な攻撃を要求へ変換する。
		Entity* owner = GetOwner();
		if (!owner) {
			return;
		}

		PlayerComponent* player = owner->GetComponent<PlayerComponent>();
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
			CreateAttack(owner, *player, slot, levelStats, currentLevel);
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
		// Repositoryと同じ値定義を使い、Component側で有効レベルを重複列挙しない。
		return PlayerAttackLevel::Normalize(level);
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

void PlayerAttackComponent::CreateAttack(Entity* owner, const PlayerComponent& player, const AttackSlotRuntime& slot, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel) {
	// Componentは発射時点の値をContextへ写し、具体的な要求計算をStrategyへ委譲する。
	if (!defaultAttackPattern_) {
		RegisterAttackPatterns();
	}
	const auto pattern = attackPatterns_.find(slot.stats.patternId);
	const IPlayerAttackPattern* selectedPattern =
		pattern != attackPatterns_.end() ? pattern->second.get() : defaultAttackPattern_.get();
	const PlayerAttackContext context{
		owner->GetTransform().translate,
		owner->GetTransform().rotate.y,
		player.GetStats().attack / 100.0f,
		player.GetStats().attackSize / 100.0f,
		owner};
	std::vector<PlayerAttackShotRequest> requests = selectedPattern->CreateShots(
		context, slot.stats, levelStats, currentLevel);
	shotRequests_.insert(
		shotRequests_.end(),
		std::make_move_iterator(requests.begin()),
		std::make_move_iterator(requests.end()));
}

void PlayerAttackComponent::RegisterAttackPatterns() {
	defaultAttackPattern_ = std::make_unique<DefaultPlayerAttackPattern>();
	attackPatterns_.clear();
	attackPatterns_.emplace("magatama", std::make_unique<MagatamaPlayerAttackPattern>());
	attackPatterns_.emplace(
		"orbit", std::make_unique<OrbitPlayerAttackPattern>(PlayerAttackEntityMotionType::Orbit));
	attackPatterns_.emplace(
		"poison_cloud", std::make_unique<OrbitPlayerAttackPattern>(PlayerAttackEntityMotionType::PoisonCloud));
	attackPatterns_.emplace(
		"flame_field", std::make_unique<OrbitPlayerAttackPattern>(PlayerAttackEntityMotionType::FlameField));
	attackPatterns_.emplace("sky_laser", std::make_unique<SkyLaserPlayerAttackPattern>());
	attackPatterns_.emplace("boomerang", std::make_unique<BoomerangPlayerAttackPattern>());
	attackPatterns_.emplace("ricochet", std::make_unique<RicochetPlayerAttackPattern>());
	attackPatterns_.emplace("claw_slash", std::make_unique<ClawSlashPlayerAttackPattern>());
}

