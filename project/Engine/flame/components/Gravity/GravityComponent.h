#pragma once

#include "core/EntityComponent.h"
#include "Vector.h"

/// <summary>Entity単位の重力移動と接地時の落下速度リセットを担当します。</summary>
class GravityComponent final : public EntityComponent {
public:
	void Update() override;

	void SetStrength(float strength) { strength_ = strength < 0.0f ? 0.0f : strength; }
	float GetStrength() const { return strength_; }
	void ResetVelocity() { velocity_ = 0.0f; }
	float GetVelocity() const { return velocity_; }
	void ApplyCollisionResponse(const Vector3& collisionNormal);

private:
	float strength_ = 9.8f;
	float velocity_ = 0.0f;
};
