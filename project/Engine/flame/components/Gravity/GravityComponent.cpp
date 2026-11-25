#include "components/Gravity/GravityComponent.h"

#include "core/Entity.h"
#include "GameTime.h"
#include "MathConstants.h"

void GravityComponent::Update() {
	Entity* owner = GetOwner();
	if (!owner) {
		return;
	}
	velocity_ += strength_ * GameTime::GetDeltaTime();
	owner->GetTransform().translate.y -= velocity_ * GameTime::GetDeltaTime();
}

void GravityComponent::ApplyCollisionResponse(const Vector3& collisionNormal) {
	if (velocity_ <= 0.0f) {
		return;
	}
	const float normalLength = Length(collisionNormal);
	if (normalLength <= MathConstants::kNormalizationEpsilon) {
		return;
	}
	const Vector3 normal = Normalize(collisionNormal);
	const float upwardRate = normal.y < 0.0f ? 0.0f : normal.y;
	velocity_ *= 1.0f - upwardRate;
	if (velocity_ < MathConstants::kDirectionEpsilon) {
		velocity_ = 0.0f;
	}
}
