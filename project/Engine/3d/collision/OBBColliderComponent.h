#pragma once
#include "../../collision/CollisionPrimitive.h"
#include "core/Component.h"
#include "core/GameObject.h"
#include "LineDrawer.h"
#include "Matrix.h"
#include <algorithm>

class OBBColliderComponent : public Component {
public:
	/// <summary>
	/// 3D 要素の描画処理を行います。
	/// </summary>
	void Draw3D() override {
#ifndef USE_IMGUI
		return;
#else
		if (!isDrawDebug_) {
			return;
		}

		DrawDebugOBB(GetWorldOBB(), isColliding_ ? Vector4{1.0f, 0.2f, 0.2f, 1.0f} : Vector4{0.2f, 1.0f, 0.2f, 1.0f});
#endif
	}

	/// <summary>
	/// オーナーの Transform を反映したワールド空間の OBB を取得します。
	/// </summary>
	/// <returns>ワールド空間で扱う OBB 形状を返します。</returns>
	OBBColliderShape GetWorldOBB() const {
		OBBColliderShape result{};
		if (!GetOwner()) {
			return result;
		}

		const EulerTransform& transform = GetOwner()->GetTransform();
		const Matrix4x4 rotateMatrix = MakeRotateXYZMatrix(transform.rotate);
		const Vector3 axisX{rotateMatrix.m[0][0], rotateMatrix.m[0][1], rotateMatrix.m[0][2]};
		const Vector3 axisY{rotateMatrix.m[1][0], rotateMatrix.m[1][1], rotateMatrix.m[1][2]};
		const Vector3 axisZ{rotateMatrix.m[2][0], rotateMatrix.m[2][1], rotateMatrix.m[2][2]};
		result.orientation[0] = Normalize(axisX);
		result.orientation[1] = Normalize(axisY);
		result.orientation[2] = Normalize(axisZ);

		const Vector3 scaledOffset{
		    centerOffset_.x * transform.scale.x,
		    centerOffset_.y * transform.scale.y,
		    centerOffset_.z * transform.scale.z
		};
		result.center =
		    transform.translate +
		    scaledOffset.x * result.orientation[0] +
		    scaledOffset.y * result.orientation[1] +
		    scaledOffset.z * result.orientation[2];
		result.halfSize = {
		    halfSize_.x < 0.0f ? 0.0f : halfSize_.x * std::fabs(transform.scale.x),
		    halfSize_.y < 0.0f ? 0.0f : halfSize_.y * std::fabs(transform.scale.y),
		    halfSize_.z < 0.0f ? 0.0f : halfSize_.z * std::fabs(transform.scale.z)
		};
		return result;
	}

	void SetCenterOffset(const Vector3& centerOffset) { centerOffset_ = centerOffset; }
	const Vector3& GetCenterOffset() const { return centerOffset_; }
	void SetHalfSize(const Vector3& halfSize) { halfSize_ = halfSize; }
	const Vector3& GetHalfSize() const { return halfSize_; }
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
	/// OBB コライダーの各辺をデバッグ線として描画します。
	/// </summary>
	/// <param name="obb">描画する OBB 形状を指定します。</param>
	/// <param name="color">色を指定します。</param>
	static void DrawDebugOBB(const OBBColliderShape& obb, const Vector4& color) {
		Vector3 vertices[8]{};
		for (int i = 0; i < 8; ++i) {
			const float xSign = (i & 1) ? 1.0f : -1.0f;
			const float ySign = (i & 2) ? 1.0f : -1.0f;
			const float zSign = (i & 4) ? 1.0f : -1.0f;
			vertices[i] =
			    obb.center +
			    (xSign * obb.halfSize.x) * obb.orientation[0] +
			    (ySign * obb.halfSize.y) * obb.orientation[1] +
			    (zSign * obb.halfSize.z) * obb.orientation[2];
		}

		static constexpr int kEdges[12][2] = {
		    {0, 1}, {1, 3}, {3, 2}, {2, 0},
		    {4, 5}, {5, 7}, {7, 6}, {6, 4},
		    {0, 4}, {1, 5}, {2, 6}, {3, 7}
		};

		for (const auto& edge : kEdges) {
			LineDrawer::GetInstance()->DrawLine(vertices[edge[0]], vertices[edge[1]], color);
		}
	}

	// オーナーの位置から見たコライダー中心のローカルオフセットです。
	Vector3 centerOffset_{0.0f, 0.0f, 0.0f};
	// OBB のローカル半径サイズです。実際の判定ではオーナーの scale を反映します。
	Vector3 halfSize_{0.5f, 0.5f, 0.5f};
	// Debug / Development でコライダー線を描画するかを保持します。
	bool isDrawDebug_ = true;
	// 衝突時にオーナーの位置を押し戻すかを保持します。保存データにも書き出します。
	bool isPushBackEnabled_ = false;
	// 現在フレームで他コライダーと衝突しているかを保持します。
	bool isColliding_ = false;
};
