#pragma once
#include "../base/struct.h"
#include "MathConstants.h"

class GameObject;

/// <summary>
/// GameObjectへ追加できる振る舞いの共通インターフェースです。
/// 有効状態、所有者、共通の重力応答を管理し、更新・描画処理を派生クラスへ委譲します。
/// </summary>
class Component {
public:
	virtual ~Component() = default;

	virtual void Initialize() {}
	virtual void Update() {}
	virtual void Draw() {}
	virtual void Draw2D() {}
	virtual void Draw3D() {}
	virtual void Finalize() {}

	GameObject* GetOwner() { return owner_; }
	const GameObject* GetOwner() const { return owner_; }

	void SetEnabled(bool enabled) { isEnabled_ = enabled; }
	bool IsEnabled() const { return isEnabled_; }

	/// <summary>
	/// このコンポーネントに共通重力を適用するかを設定します。
	/// </summary>
	/// <param name="enabled">true の場合、ApplyGravity でY方向へ落下速度を加算します。</param>
	void SetGravityEnabled(bool enabled) {
		isGravityEnabled_ = enabled;
		if (!isGravityEnabled_) {
			gravityVelocity_ = 0.0f;
		}
	}
	/// <summary>
	/// 共通重力が有効かを取得します。
	/// </summary>
	/// <returns>重力を適用する場合 true を返します。</returns>
	bool IsGravityEnabled() const { return isGravityEnabled_; }
	/// <summary>
	/// 1秒あたりに増加する落下速度を設定します。
	/// </summary>
	/// <param name="strength">重力加速度として扱う非負の値を指定します。</param>
	void SetGravityStrength(float strength) { gravityStrength_ = strength < 0.0f ? 0.0f : strength; }
	/// <summary>
	/// 現在設定されている重力加速度を取得します。
	/// </summary>
	/// <returns>重力加速度を返します。</returns>
	float GetGravityStrength() const { return gravityStrength_; }
	/// <summary>
	/// 蓄積した落下速度を0へ戻します。
	/// </summary>
	void ResetGravityVelocity() { gravityVelocity_ = 0.0f; }
	/// <summary>
	/// Transformへ重力による落下移動を反映します。
	/// </summary>
	/// <param name="transform">移動させるTransformを指定します。</param>
	/// <param name="deltaTime">前フレームからの経過秒数を指定します。</param>
	void ApplyGravity(EulerTransform& transform, float deltaTime) {
		if (!isGravityEnabled_) {
			return;
		}
		gravityVelocity_ += gravityStrength_ * deltaTime;
		transform.translate.y -= gravityVelocity_ * deltaTime;
	}
	/// <summary>
	/// コライダーの押し戻し方向を受け取り、上向き面に接地した場合は落下速度を抑制します。
	/// </summary>
	/// <param name="collisionNormal">押し戻し方向または接触面の法線方向を指定します。</param>
	void ApplyCollisionResponse(const Vector3& collisionNormal) {
		if (!isGravityEnabled_ || gravityVelocity_ <= 0.0f) {
			return;
		}

		const float normalLength = Length(collisionNormal);
		if (normalLength <= MathConstants::kNormalizationEpsilon) {
			return;
		}

		const Vector3 normal = Normalize(collisionNormal);
		const float upwardRate = normal.y < 0.0f ? 0.0f : normal.y;
		gravityVelocity_ *= 1.0f - upwardRate;
		if (gravityVelocity_ < MathConstants::kDirectionEpsilon) {
			gravityVelocity_ = 0.0f;
		}
	}

private:
	friend class GameObject;
	void SetOwner(GameObject* owner) { owner_ = owner; }

	// このコンポーネントを所有するGameObjectです。所有権はGameObject側が持ちます。
	GameObject* owner_ = nullptr;
	// falseの場合、GameObjectのUpdate/Draw系呼び出しからこのコンポーネントを除外します。
	bool isEnabled_ = true;
	// BaseSceneの共通重力UIから切り替える、簡易重力の有効状態です。
	bool isGravityEnabled_ = false;
	// 1秒ごとにgravityVelocity_へ加算する落下加速度です。
	float gravityStrength_ = 9.8f;
	// 現在蓄積している下向き速度です。上向き接触で減衰します。
	float gravityVelocity_ = 0.0f;
};
