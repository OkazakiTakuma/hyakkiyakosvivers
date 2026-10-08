#include "PlayerProjectileComponent.h"
#include "GameObject.h"
#include "MathConstants.h"
#include "../base/GameTime.h"
#include <algorithm>
#include <cmath>

void PlayerProjectileComponent::Initialize() {}

void PlayerProjectileComponent::Update() {
	GameObject* owner = GetOwner();
	if (!owner) {
		return;
	}

	const float deltaTime = GameTime::GetDeltaTime();
	UpdateHitCooldowns(deltaTime);
	const float frameScale = GameTime::GetFrameScale60();

	// プレイヤー弾も大量生成されるため、弾ごとのStrategy確保を行わず種類で直接振り分ける。
	switch (motionType_) {
	case PlayerProjectileMotionType::Magatama:
		UpdateLinearMotion(owner, deltaTime, frameScale, true);
		break;
	case PlayerProjectileMotionType::Orbit: {
		if (!motionAnchor_) {
			lifeTime_ = 0.0f;
			break;
		}
		orbitAngleRadians_ += orbitAngularSpeed_ * deltaTime;
		const Vector3 anchorPosition = motionAnchor_->GetTransform().translate;
		owner->GetTransform().translate = {
			anchorPosition.x + std::cos(orbitAngleRadians_) * orbitRadius_,
			anchorPosition.y + orbitHeight_,
			anchorPosition.z + std::sin(orbitAngleRadians_) * orbitRadius_};
		owner->GetTransform().rotate.y = -orbitAngleRadians_;
		lifeTime_ -= deltaTime;
		constexpr float kShrinkDurationSeconds = 0.75f;
		visualScaleRate_ = (std::clamp)(lifeTime_ / kShrinkDurationSeconds, 0.0f, 1.0f);
		const float visualSize = size_ * visualScaleRate_;
		owner->GetTransform().scale = {visualSize, visualSize, visualSize};
		break;
	}
	case PlayerProjectileMotionType::SkyLaser:
		lifeTime_ -= deltaTime;
		break;
	case PlayerProjectileMotionType::Boomerang:
		UpdateBoomerang(owner, deltaTime, frameScale);
		break;
	case PlayerProjectileMotionType::Ricochet:
		UpdateLinearMotion(owner, deltaTime, frameScale, false);
		break;
	case PlayerProjectileMotionType::ClawSlash:
		UpdateClawSlash(owner, deltaTime);
		break;
	case PlayerProjectileMotionType::Linear:
	default:
		UpdateLinearMotion(owner, deltaTime, frameScale, false);
		break;
	}
}

void PlayerProjectileComponent::SetAttackName(const std::string& attackName) { attackName_ = attackName; }

const std::string& PlayerProjectileComponent::GetAttackName() const { return attackName_; }

void PlayerProjectileComponent::SetLevel(const std::string& level) { level_ = level; }

const std::string& PlayerProjectileComponent::GetLevel() const { return level_; }

void PlayerProjectileComponent::SetDirection(const Vector3& direction) { direction_ = Length(direction) > MathConstants::kDirectionEpsilon ? NormalizeReturnVector(direction) : Vector3{0.0f, 0.0f, 1.0f}; }

const Vector3& PlayerProjectileComponent::GetDirection() const { return direction_; }

void PlayerProjectileComponent::SetSpeed(float speed) { speed_ = speed < 0.0f ? 0.0f : speed; }

float PlayerProjectileComponent::GetSpeed() const { return speed_; }

void PlayerProjectileComponent::SetAttack(float attack) { attack_ = attack < 0.0f ? 0.0f : attack; }

float PlayerProjectileComponent::GetAttack() const { return attack_; }

void PlayerProjectileComponent::SetSize(float size) { size_ = size < 0.01f ? 0.01f : size; }

float PlayerProjectileComponent::GetSize() const { return size_ * visualScaleRate_; }

void PlayerProjectileComponent::SetHomingEnabled(bool homingEnabled) { homingEnabled_ = homingEnabled; }

bool PlayerProjectileComponent::IsHomingEnabled() const { return homingEnabled_; }

void PlayerProjectileComponent::SetHomingAccuracy(float homingAccuracy) { homingAccuracy_ = (std::clamp)(homingAccuracy, 0.0f, 1.0f); }

float PlayerProjectileComponent::GetHomingAccuracy() const { return homingAccuracy_; }

void PlayerProjectileComponent::SetMotionType(PlayerProjectileMotionType motionType) {
	motionType_ = motionType;
}

PlayerProjectileMotionType PlayerProjectileComponent::GetMotionType() const { return motionType_; }

PlayerProjectilePresentation PlayerProjectileComponent::GetMotionPresentation() const {
	PlayerProjectilePresentation result;
	switch (motionType_) {
	case PlayerProjectileMotionType::Magatama:
		result.useMagatamaPalette = true;
		result.addGlowEmitter = true;
		break;
	case PlayerProjectileMotionType::Orbit:
		result.overrideModelColor = true;
		result.modelColor = {0.35f, 0.75f, 1.0f, 1.0f};
		result.overrideTrailColors = true;
		result.trailHeadColor = {0.45f, 0.90f, 1.0f, 0.9f};
		result.trailTailColor = {0.10f, 0.30f, 1.0f, 0.0f};
		result.trailLifeTime = 0.45f;
		result.trailUsesMotionAnchor = true;
		result.repeatHitInterval = 0.75f;
		result.expiresOutsideView = false;
		break;
	case PlayerProjectileMotionType::SkyLaser:
		result.scaleMultiplier = {0.30f, 5.0f, 0.30f};
		result.modelVisible = false;
		result.addSkyLaserVisual = true;
		result.addTrail = false;
		result.usesVerticalHitArea = true;
		result.expiresOutsideView = false;
		break;
	case PlayerProjectileMotionType::Boomerang:
		result.overrideModelColor = true;
		result.modelColor = {1.0f, 0.65f, 0.20f, 1.0f};
		result.overrideTrailColors = true;
		result.trailHeadColor = {1.0f, 0.72f, 0.20f, 0.95f};
		result.trailTailColor = {1.0f, 0.12f, 0.02f, 0.0f};
		result.aimAtNearestEnemyOnSpawn = true;
		result.expiresOutsideView = false;
		break;
	case PlayerProjectileMotionType::Ricochet:
		result.overrideModelColor = true;
		result.modelColor = {0.45f, 1.0f, 0.30f, 1.0f};
		result.overrideTrailColors = true;
		result.trailHeadColor = {0.65f, 1.0f, 0.30f, 0.95f};
		result.trailTailColor = {0.05f, 0.80f, 0.15f, 0.0f};
		result.ricochets = true;
		result.expiresOutsideView = false;
		break;
	case PlayerProjectileMotionType::ClawSlash:
		result.scaleMultiplier = {0.18f, 0.18f, 0.18f};
		result.modelVisible = false;
		result.overrideModelColor = true;
		result.modelColor = {1.0f, 0.22f, 0.10f, 1.0f};
		result.trailWidthMultiplier = 0.30f;
		result.minimumTrailWidth = 0.14f;
		result.trailLifeTime = 0.24f;
		result.trailUsesMotionAnchor = true;
		result.overrideTrailColors = true;
		result.trailHeadColor = {1.0f, 0.30f, 0.04f, 0.92f};
		result.trailTailColor = {0.70f, 0.01f, 0.00f, 0.0f};
		result.addCoreTrail = true;
		result.expiresOutsideView = false;
		break;
	case PlayerProjectileMotionType::Linear:
	default:
		break;
	}
	return result;
}

void PlayerProjectileComponent::SetMotionAnchor(GameObject* motionAnchor) { motionAnchor_ = motionAnchor; }

void PlayerProjectileComponent::SetOrbitAngleRadians(float angle) { orbitAngleRadians_ = angle; }

void PlayerProjectileComponent::SetOrbitRadius(float radius) { orbitRadius_ = (std::max)(0.1f, radius); }

void PlayerProjectileComponent::SetOrbitHeight(float height) { orbitHeight_ = height; }

void PlayerProjectileComponent::SetOrbitAngularSpeed(float speed) { orbitAngularSpeed_ = speed; }

void PlayerProjectileComponent::SetTravelDistance(float distance) { travelDistance_ = (std::max)(0.1f, distance); }

void PlayerProjectileComponent::SetTravelOrigin(const Vector3& origin) { travelOrigin_ = origin; }

void PlayerProjectileComponent::SetClawSlashIndex(int index) { clawSlashIndex_ = (std::max)(0, index); }

void PlayerProjectileComponent::SetClawSlashCount(int count) { clawSlashCount_ = (std::max)(1, count); }

void PlayerProjectileComponent::SetHomingTarget(GameObject* target) { homingTarget_ = target; }

GameObject* PlayerProjectileComponent::GetHomingTarget() const { return homingTarget_; }

void PlayerProjectileComponent::SetLifeTime(float lifeTime) { lifeTime_ = lifeTime; initialLifeTime_ = lifeTime; }

void PlayerProjectileComponent::Expire() { lifeTime_ = 0.0f; }

bool PlayerProjectileComponent::IsExpired() const { return lifeTime_ <= 0.0f; }

void PlayerProjectileComponent::SetPierceCount(int pierceCount) { pierceCount_ = pierceCount < 0 ? 0 : pierceCount; }

int PlayerProjectileComponent::GetPierceCount() const { return pierceCount_; }

void PlayerProjectileComponent::SetInfinitePierce(bool infinitePierce) { infinitePierce_ = infinitePierce; }

bool PlayerProjectileComponent::IsInfinitePierce() const { return infinitePierce_; }

void PlayerProjectileComponent::SetRepeatHitInterval(float intervalSeconds) { repeatHitIntervalSeconds_ = (std::max)(0.0f, intervalSeconds); }

float PlayerProjectileComponent::GetRepeatHitInterval() const { return repeatHitIntervalSeconds_; }

bool PlayerProjectileComponent::HasHitObject(GameObject* object) const {
		for (const HitRecord& hitRecord : hitRecords_) {
			if (hitRecord.object == object) {
				return true;
			}
		}
		return false;
	}

void PlayerProjectileComponent::RegisterHitObject(GameObject* object) {
		if (!object || HasHitObject(object)) {
			return;
		}
		// 再ヒット間隔0では履歴を寿命まで保持し、周回弾などは指定秒数後に履歴を破棄する。
		hitRecords_.push_back({object, repeatHitIntervalSeconds_ > 0.0f ? repeatHitIntervalSeconds_ : -1.0f});
		if (infinitePierce_) {
			return;
		}
		if (pierceCount_ <= 0) {
			Expire();
			return;
		}
		--pierceCount_;
	}

void PlayerProjectileComponent::UpdateHitCooldowns(float deltaTime) {
		// 再ヒット対応弾だけ履歴の残り時間を進め、期限切れの敵を再び命中可能にする。
		if (repeatHitIntervalSeconds_ <= 0.0f) {
			return;
		}
		for (HitRecord& hitRecord : hitRecords_) {
			hitRecord.cooldownSeconds -= deltaTime;
		}
		hitRecords_.erase(
			std::remove_if(hitRecords_.begin(), hitRecords_.end(), [](const HitRecord& hitRecord) {
				return hitRecord.cooldownSeconds <= 0.0f;
			}),
			hitRecords_.end());
	}

void PlayerProjectileComponent::UpdateLinearMotion(GameObject* owner, float deltaTime, float frameScale, bool rampHoming) {
	if (homingEnabled_ && homingTarget_) {
		Vector3 toTarget = homingTarget_->GetTransform().translate - owner->GetTransform().translate;
		toTarget.y = 0.0f;
		if (Length(toTarget) > MathConstants::kDirectionEpsilon) {
			const Vector3 targetDirection = NormalizeReturnVector(toTarget);
			float homingRate = (std::clamp)(homingAccuracy_, 0.0f, 1.0f);
			if (rampHoming) {
				magatamaElapsedSeconds_ += deltaTime;
				// 勾玉は短時間で追尾力を最大まで上げ、大きな発射角から敵へ収束させる。
				const float turnRamp = (std::clamp)(magatamaElapsedSeconds_ / 0.45f, 0.32f, 1.0f);
				homingRate *= turnRamp;
			}
			const float accuracy = 1.0f - std::pow(1.0f - homingRate, frameScale);
			direction_ = NormalizeReturnVector(Leap(direction_, targetDirection, accuracy));
		}
	}

	owner->GetTransform().translate = owner->GetTransform().translate + (speed_ * frameScale) * direction_;
	owner->GetTransform().rotate.y = std::atan2(direction_.x, direction_.z);
	lifeTime_ -= deltaTime;
}

void PlayerProjectileComponent::UpdateBoomerang(GameObject* owner, float deltaTime, float frameScale) {
		if (!motionAnchor_) {
			lifeTime_ = 0.0f;
			return;
		}

		Vector3& position = owner->GetTransform().translate;
		if (!returning_) {
			// 発射位置から水平距離がTravel Distanceへ達した瞬間に、帰還方向を一度だけ確定する。
			Vector3 traveled = position - travelOrigin_;
			traveled.y = 0.0f;
			if (Length(traveled) >= travelDistance_) {
				returnTarget_ = motionAnchor_->GetTransform().translate;
				returnTarget_.y = position.y;
				const Vector3 toPlayer = returnTarget_ - position;
				if (Length(toPlayer) <= MathConstants::kDirectionEpsilon) {
					lifeTime_ = 0.0f;
					return;
				}
				direction_ = NormalizeReturnVector(toPlayer);
				returning_ = true;
			}
		}

		const float movement = speed_ * frameScale;
		if (returning_) {
			// 帰還開始時に保存した位置へ直進し、1フレームの移動量以内まで近づいたら消滅する。
			const Vector3 toReturnTarget = returnTarget_ - position;
			if (Length(toReturnTarget) <= (std::max)(movement, 0.05f)) {
				position = returnTarget_;
				lifeTime_ = 0.0f;
				return;
			}
		}

		position = position + movement * direction_;
		owner->GetTransform().rotate.y = std::atan2(direction_.x, direction_.z);
		lifeTime_ -= deltaTime;
	}

void PlayerProjectileComponent::UpdateClawSlash(GameObject* owner, float deltaTime) {
		if (!motionAnchor_) {
			lifeTime_ = 0.0f;
			return;
		}

		lifeTime_ -= deltaTime;
		const float duration = (std::max)(0.01f, initialLifeTime_);
		const float progress = (std::clamp)(1.0f - lifeTime_ / duration, 0.0f, 1.0f);
		// 発動途中でプレイヤーが旋回しても爪同士の配置が回転しないよう、初回の向きを固定する。
		if (!isClawSlashYawInitialized_) {
			clawSlashYaw_ = motionAnchor_->GetTransform().rotate.y;
			isClawSlashYawInitialized_ = true;
		}
		const Vector3 forward{std::sin(clawSlashYaw_), 0.0f, std::cos(clawSlashYaw_)};
		const Vector3 right{std::cos(clawSlashYaw_), 0.0f, -std::sin(clawSlashYaw_)};
		// 爪番号を中央基準（3本なら-1, 0, 1）へ変換し、各爪を平行にずらす。
		const float centeredIndex = static_cast<float>(clawSlashIndex_) - (static_cast<float>(clawSlashCount_) - 1.0f) * 0.5f;
		const float localX = -0.9f + 1.8f * progress + centeredIndex * 0.34f;
		const float localY = 1.15f - 0.70f * progress + centeredIndex * 0.18f;
		const float localZ = 1.55f + std::abs(centeredIndex) * 0.03f;

		// 向きは固定する一方、基準位置は毎フレーム取得してプレイヤーの移動には追従させる。
		owner->GetTransform().translate = motionAnchor_->GetTransform().translate +
		    localX * right + Vector3{0.0f, localY, 0.0f} + localZ * forward;
		owner->GetTransform().rotate.y = clawSlashYaw_;
		owner->GetTransform().rotate.z = -0.65f;
	}
