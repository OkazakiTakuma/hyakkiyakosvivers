#pragma once
#include "../../collision/CollisionPrimitive.h"
#include "../../flame/Component.h"
#include "../../flame/GameObject.h"
#include "../../math/MathConstants.h"
#include "LineDrawer.h"
#include <algorithm>
#include <cmath>

class SphereColliderComponent : public Component {
public:
	/// <summary>
	/// デバッグ表示が有効な場合、球コライダーを線で描画します。
	/// </summary>
	void Draw3D() override {
#ifndef USE_IMGUI
		return;
#else
		if (!isDrawDebug_) {
			return;
		}

		DrawDebugSphere(GetWorldSphere(), isColliding_ ? Vector4{1.0f, 0.2f, 0.2f, 1.0f} : Vector4{0.2f, 0.7f, 1.0f, 1.0f});
#endif
	}

	/// <summary>
	/// オブジェクトの Transform を反映したワールド空間の球を取得します。
	/// </summary>
	/// <returns>ワールド空間の球コライダー形状を返します。</returns>
	SphereColliderShape GetWorldSphere() const {
		SphereColliderShape result{};
		if (!GetOwner()) {
			return result;
		}

		const EulerTransform& transform = GetOwner()->GetTransform();
		float maxScale = std::fabs(transform.scale.x);
		if (std::fabs(transform.scale.y) > maxScale) {
			maxScale = std::fabs(transform.scale.y);
		}
		if (std::fabs(transform.scale.z) > maxScale) {
			maxScale = std::fabs(transform.scale.z);
		}
		result.center = transform.translate + centerOffset_;
		result.radius = radius_ * maxScale;
		return result;
	}

	void SetCenterOffset(const Vector3& centerOffset) { centerOffset_ = centerOffset; }
	const Vector3& GetCenterOffset() const { return centerOffset_; }
	void SetRadius(float radius) { radius_ = radius < 0.0f ? 0.0f : radius; }
	float GetRadius() const { return radius_; }
	void SetDrawDebug(bool isDrawDebug) { isDrawDebug_ = isDrawDebug; }
	bool GetDrawDebug() const { return isDrawDebug_; }
	/// <summary>
	/// 衝突時にこのコライダーを押し戻し対象にするかを設定します。
	/// </summary>
	/// <param name="isPushBackEnabled">true の場合、衝突時に位置補正を受けます。</param>
	void SetPushBackEnabled(bool isPushBackEnabled) { isPushBackEnabled_ = isPushBackEnabled; }
	/// <summary>
	/// 衝突時の押し戻し処理が有効かを取得します。
	/// </summary>
	/// <returns>押し戻し処理が有効な場合 true を返します。</returns>
	bool GetPushBackEnabled() const { return isPushBackEnabled_; }
	void SetColliding(bool isColliding) { isColliding_ = isColliding; }
	bool IsColliding() const { return isColliding_; }

private:
	/// <summary>
	/// 球を3軸の円として線描画します。
	/// </summary>
	/// <param name="sphere">描画する球コライダー形状を指定します。</param>
	/// <param name="color">描画色を指定します。</param>
	static void DrawDebugSphere(const SphereColliderShape& sphere, const Vector4& color) {
		constexpr uint32_t kSegmentCount = 16;

		for (uint32_t index = 0; index < kSegmentCount; ++index) {
			const float currentAngle = MathConstants::kTwoPi * static_cast<float>(index) / static_cast<float>(kSegmentCount);
			const float nextAngle = MathConstants::kTwoPi * static_cast<float>(index + 1) / static_cast<float>(kSegmentCount);

			const float currentCos = std::cos(currentAngle) * sphere.radius;
			const float currentSin = std::sin(currentAngle) * sphere.radius;
			const float nextCos = std::cos(nextAngle) * sphere.radius;
			const float nextSin = std::sin(nextAngle) * sphere.radius;

			LineDrawer::GetInstance()->DrawLine(
			    {sphere.center.x + currentCos, sphere.center.y + currentSin, sphere.center.z},
			    {sphere.center.x + nextCos, sphere.center.y + nextSin, sphere.center.z},
			    color
			);
			LineDrawer::GetInstance()->DrawLine(
			    {sphere.center.x, sphere.center.y + currentCos, sphere.center.z + currentSin},
			    {sphere.center.x, sphere.center.y + nextCos, sphere.center.z + nextSin},
			    color
			);
			LineDrawer::GetInstance()->DrawLine(
			    {sphere.center.x + currentCos, sphere.center.y, sphere.center.z + currentSin},
			    {sphere.center.x + nextCos, sphere.center.y, sphere.center.z + nextSin},
			    color
			);
		}
	}

	// オーナーの位置から見た球中心のローカルオフセットです。
	Vector3 centerOffset_{0.0f, 0.0f, 0.0f};
	// 球コライダーのローカル半径です。実際の判定ではオーナーの最大 scale を反映します。
	float radius_ = 0.5f;
	// Debug / Development でコライダー線を描画するかを保持します。
	bool isDrawDebug_ = true;
	// 衝突時にオーナーの位置を押し戻すかを保持します。保存データにも書き出します。
	bool isPushBackEnabled_ = false;
	// 現在フレームで他コライダーと衝突しているかを保持します。
	bool isColliding_ = false;
};
