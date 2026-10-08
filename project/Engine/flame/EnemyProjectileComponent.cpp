#include "EnemyProjectileComponent.h"

void EnemyProjectileComponent::Initialize() {}

void EnemyProjectileComponent::Update() {
	if (!GetOwner()) {
		return;
	}
	EulerTransform& transform = GetOwner()->GetTransform();
	previousPosition_ = transform.translate;
	const float frameScale = GameTime::GetFrameScale60();
	const float deltaTime = GameTime::GetDeltaTime();

	// 敵弾は大量生成される短命オブジェクトなので、個別のheap確保と仮想呼び出しを避ける。
	switch (motionType_) {
	case EnemyProjectileMotionType::Homing: {
		if (homingTarget_) {
			Vector3 toTarget = homingTarget_->GetTransform().translate - transform.translate;
			toTarget.y = 0.0f;
			if (Length(toTarget) > MathConstants::kDirectionEpsilon) {
				direction_ = Normalize(toTarget);
			}
		}
		transform.translate = transform.translate + (speed_ * frameScale) * direction_;
		transform.rotate.y = std::atan2(direction_.x, direction_.z);
		break;
	}
	case EnemyProjectileMotionType::ExpandingOrbit:
	case EnemyProjectileMotionType::ContractingOrbit: {
		const float radialDirection = motionType_ == EnemyProjectileMotionType::ExpandingOrbit ? 1.0f : -1.0f;
		orbitRadius_ = (std::max)(0.15f, orbitRadius_ + radialDirection * orbitRadialSpeed_ * frameScale);
		orbitAngle_ += orbitAngularSpeed_ * deltaTime;
		transform.translate = orbitCenter_ + Vector3{
			std::sin(orbitAngle_) * orbitRadius_, orbitHeight_, std::cos(orbitAngle_) * orbitRadius_};
		const Vector3 movement = transform.translate - previousPosition_;
		if (Length(movement) > MathConstants::kDirectionEpsilon) {
			direction_ = Normalize(movement);
			transform.rotate.y = std::atan2(direction_.x, direction_.z);
		}
		break;
	}
	case EnemyProjectileMotionType::Linear:
	default:
		transform.translate = transform.translate + (speed_ * frameScale) * direction_;
		break;
	}
	elapsedTime_ += deltaTime;
}

void EnemyProjectileComponent::SetDirection(const Vector3& direction) {
	direction_ = Length(direction) > MathConstants::kDirectionEpsilon ? Normalize(direction) : Vector3{0.0f, 0.0f, 1.0f};
}

const Vector3& EnemyProjectileComponent::GetDirection() const { return direction_; }

const Vector3& EnemyProjectileComponent::GetPreviousPosition() const { return previousPosition_; }

void EnemyProjectileComponent::SetMotionType(EnemyProjectileMotionType motionType) {
	motionType_ = motionType;
}

EnemyProjectileMotionType EnemyProjectileComponent::GetMotionType() const { return motionType_; }

void EnemyProjectileComponent::SetExpandingOrbit(
	const Vector3& center, float angle, float initialRadius, float angularSpeed, float radialSpeed, float height) {
	orbitCenter_ = center;
	orbitAngle_ = angle;
	orbitRadius_ = (std::max)(0.0f, initialRadius);
	orbitAngularSpeed_ = angularSpeed;
	orbitRadialSpeed_ = (std::max)(0.0f, radialSpeed);
	orbitHeight_ = height;
	SetMotionType(EnemyProjectileMotionType::ExpandingOrbit);
}

void EnemyProjectileComponent::SetContractingOrbit(
	const Vector3& center, float angle, float initialRadius, float angularSpeed, float radialSpeed, float height) {
	orbitCenter_ = center;
	orbitAngle_ = angle;
	orbitRadius_ = (std::max)(0.0f, initialRadius);
	orbitAngularSpeed_ = angularSpeed;
	orbitRadialSpeed_ = (std::max)(0.0f, radialSpeed);
	orbitHeight_ = height;
	SetMotionType(EnemyProjectileMotionType::ContractingOrbit);
}

void EnemyProjectileComponent::SetHomingTarget(GameObject* target) {
	homingTarget_ = target;
	SetMotionType(EnemyProjectileMotionType::Homing);
}

void EnemyProjectileComponent::SetSpeed(float speed) { speed_ = (std::max)(0.0f, speed); }

void EnemyProjectileComponent::SetAttack(float attack) { attack_ = (std::max)(0.0f, attack); }

float EnemyProjectileComponent::GetAttack() const { return attack_; }

void EnemyProjectileComponent::SetSize(float size) { size_ = (std::max)(0.01f, size); }

float EnemyProjectileComponent::GetSize() const { return size_; }

void EnemyProjectileComponent::SetLifeTime(float lifeTime) { lifeTime_ = (std::max)(0.0f, lifeTime); }

bool EnemyProjectileComponent::IsExpired() const { return hit_ || elapsedTime_ >= lifeTime_; }

void EnemyProjectileComponent::MarkHit() { hit_ = true; }
