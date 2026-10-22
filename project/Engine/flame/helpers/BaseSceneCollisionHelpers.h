#pragma once

#include "base_scene/BaseScene.h"
#include "../../math/MathConstants.h"

#include <cmath>

/// <summary>
/// BaseSceneの衝突押し戻し計算を提供します。
/// </summary>
namespace BaseSceneCollisionHelpers {

/// <summary>
/// 値を指定範囲内に収めます。
/// </summary>
/// <param name="value">補正対象の値を指定します。</param>
/// <param name="minValue">許可する最小値を指定します。</param>
/// <param name="maxValue">許可する最大値を指定します。</param>
/// <returns>指定範囲内に収めた値を返します。</returns>
inline float ClampFloat(float value, float minValue, float maxValue) {
	if (value < minValue) {
		return minValue;
	}
	if (value > maxValue) {
		return maxValue;
	}
	return value;
}

/// <summary>
/// 方向ベクトルを正規化し、長さが小さすぎる場合は代替方向を使用します。
/// </summary>
/// <param name="direction">正規化する方向ベクトルを指定します。</param>
/// <param name="fallback">direction が無効な場合に使う代替方向を指定します。</param>
/// <returns>押し戻し計算に使える単位方向ベクトルを返します。</returns>
inline Vector3 NormalizeOrFallback(const Vector3& direction, const Vector3& fallback) {
	const float length = Length(direction);
	if (length <= MathConstants::kNormalizationEpsilon) {
		return Normalize(fallback);
	}
	return Normalize(direction);
}

/// <summary>
/// OBB を指定軸へ射影したときの半径を求めます。
/// </summary>
/// <param name="obb">射影対象の OBB を指定します。</param>
/// <param name="axis">射影先の単位軸を指定します。</param>
/// <returns>指定軸上での OBB の射影半径を返します。</returns>
inline float ProjectOBBRadius(const OBBColliderShape& obb, const Vector3& axis) {
	return
	    obb.halfSize.x * std::fabs(Dot(obb.orientation[0], axis)) +
	    obb.halfSize.y * std::fabs(Dot(obb.orientation[1], axis)) +
	    obb.halfSize.z * std::fabs(Dot(obb.orientation[2], axis));
}

/// <summary>
/// ワールド空間の点に最も近い OBB 上の点を求めます。
/// </summary>
/// <param name="obb">最近接点を求める OBB を指定します。</param>
/// <param name="point">OBB へ近づける対象点を指定します。</param>
/// <returns>OBB 表面または内部の最近接点を返します。</returns>
inline Vector3 ClosestPointOnOBB(const OBBColliderShape& obb, const Vector3& point) {
	const Vector3 diff = point - obb.center;
	Vector3 result = obb.center;
	const float halfSizes[3] = {obb.halfSize.x, obb.halfSize.y, obb.halfSize.z};
	for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
		const float distance = ClampFloat(Dot(diff, obb.orientation[axisIndex]), -halfSizes[axisIndex], halfSizes[axisIndex]);
		result = result + distance * obb.orientation[axisIndex];
	}
	return result;
}

/// <summary>
/// Sphere 同士の押し戻し方向と侵入量を計算します。
/// </summary>
/// <param name="a">押し戻し元の Sphere を指定します。</param>
/// <param name="b">押し戻し先の Sphere を指定します。</param>
/// <param name="directionAToB">a から b へ向かう押し戻し方向を受け取ります。</param>
/// <param name="penetration">重なっている距離を受け取ります。</param>
/// <returns>押し戻しが必要な場合 true を返します。</returns>
inline bool CalculateSphereSpherePushBack(const SphereColliderShape& a, const SphereColliderShape& b, Vector3& directionAToB, float& penetration) {
	const Vector3 diff = b.center - a.center;
	const float distance = Length(diff);
	const float radius = a.radius + b.radius;
	penetration = radius - distance;
	if (penetration <= 0.0f) {
		return false;
	}
	directionAToB = NormalizeOrFallback(diff, {1.0f, 0.0f, 0.0f});
	return true;
}

/// <summary>
/// OBB と Sphere の押し戻し方向と侵入量を計算します。
/// </summary>
/// <param name="obb">押し戻し元の OBB を指定します。</param>
/// <param name="sphere">押し戻し先の Sphere を指定します。</param>
/// <param name="directionOBBToSphere">OBB から Sphere へ向かう押し戻し方向を受け取ります。</param>
/// <param name="penetration">重なっている距離を受け取ります。</param>
/// <returns>押し戻しが必要な場合 true を返します。</returns>
inline bool CalculateOBBSpherePushBack(const OBBColliderShape& obb, const SphereColliderShape& sphere, Vector3& directionOBBToSphere, float& penetration) {
	const Vector3 closestPoint = ClosestPointOnOBB(obb, sphere.center);
	const Vector3 diff = sphere.center - closestPoint;
	const float distance = Length(diff);
	if (distance > MathConstants::kNormalizationEpsilon) {
		penetration = sphere.radius - distance;
		if (penetration <= 0.0f) {
			return false;
		}
		directionOBBToSphere = Normalize(diff);
		return true;
	}

	const Vector3 localDiff = sphere.center - obb.center;
	float localPosition[3] = {
	    Dot(localDiff, obb.orientation[0]),
	    Dot(localDiff, obb.orientation[1]),
	    Dot(localDiff, obb.orientation[2])
	};
	const float halfSizes[3] = {obb.halfSize.x, obb.halfSize.y, obb.halfSize.z};
	int nearestAxis = 0;
	float nearestFaceDistance = halfSizes[0] - std::fabs(localPosition[0]);
	for (int axisIndex = 1; axisIndex < 3; ++axisIndex) {
		const float faceDistance = halfSizes[axisIndex] - std::fabs(localPosition[axisIndex]);
		if (faceDistance < nearestFaceDistance) {
			nearestFaceDistance = faceDistance;
			nearestAxis = axisIndex;
		}
	}

	const float sign = localPosition[nearestAxis] < 0.0f ? -1.0f : 1.0f;
	directionOBBToSphere = sign * obb.orientation[nearestAxis];
	penetration = sphere.radius + nearestFaceDistance;
	return penetration > 0.0f;
}

/// <summary>
/// OBB 同士の押し戻し方向と侵入量を SAT によって計算します。
/// </summary>
/// <param name="a">押し戻し元の OBB を指定します。</param>
/// <param name="b">押し戻し先の OBB を指定します。</param>
/// <param name="directionAToB">a から b へ向かう押し戻し方向を受け取ります。</param>
/// <param name="penetration">最小侵入量を受け取ります。</param>
/// <returns>押し戻しが必要な場合 true を返します。</returns>
inline bool CalculateOBBOBBPushBack(const OBBColliderShape& a, const OBBColliderShape& b, Vector3& directionAToB, float& penetration) {
	Vector3 axes[15] = {
	    a.orientation[0], a.orientation[1], a.orientation[2],
	    b.orientation[0], b.orientation[1], b.orientation[2],
	    Cross(a.orientation[0], b.orientation[0]), Cross(a.orientation[0], b.orientation[1]), Cross(a.orientation[0], b.orientation[2]),
	    Cross(a.orientation[1], b.orientation[0]), Cross(a.orientation[1], b.orientation[1]), Cross(a.orientation[1], b.orientation[2]),
	    Cross(a.orientation[2], b.orientation[0]), Cross(a.orientation[2], b.orientation[1]), Cross(a.orientation[2], b.orientation[2])
	};

	const Vector3 centerDiff = b.center - a.center;
	bool foundAxis = false;
	float minimumOverlap = 0.0f;
	Vector3 minimumAxis{1.0f, 0.0f, 0.0f};
	for (const Vector3& rawAxis : axes) {
		if (Length(rawAxis) <= MathConstants::kNormalizationEpsilon) {
			continue;
		}
		Vector3 axis = Normalize(rawAxis);
		const float signedDistance = Dot(centerDiff, axis);
		const float overlap = ProjectOBBRadius(a, axis) + ProjectOBBRadius(b, axis) - std::fabs(signedDistance);
		if (overlap <= 0.0f) {
			return false;
		}
		if (!foundAxis || overlap < minimumOverlap) {
			foundAxis = true;
			minimumOverlap = overlap;
			minimumAxis = signedDistance < 0.0f ? -1.0f * axis : axis;
		}
	}

	if (!foundAxis) {
		return false;
	}
	directionAToB = NormalizeOrFallback(minimumAxis, centerDiff);
	penetration = minimumOverlap;
	return true;
}

/// <summary>
/// 押し戻しフラグが有効なオブジェクトへ位置補正を適用します。
/// </summary>
/// <param name="objectA">補正対象 A を指定します。</param>
/// <param name="isPushBackA">A 側の押し戻し有効状態を指定します。</param>
/// <param name="objectB">補正対象 B を指定します。</param>
/// <param name="isPushBackB">B 側の押し戻し有効状態を指定します。</param>
/// <param name="directionAToB">A から B へ向かう押し戻し方向を指定します。</param>
/// <param name="penetration">重なっている距離を指定します。</param>
inline void ApplyColliderPushBack(GameObject* objectA, bool isPushBackA, GameObject* objectB, bool isPushBackB, const Vector3& directionAToB, float penetration) {
	if (!objectA || !objectB || objectA == objectB || penetration <= 0.0f || (!isPushBackA && !isPushBackB)) {
		return;
	}

	if (isPushBackA && isPushBackB) {
		objectA->GetTransform().translate = objectA->GetTransform().translate + (-0.5f * penetration) * directionAToB;
		objectB->GetTransform().translate = objectB->GetTransform().translate + (0.5f * penetration) * directionAToB;
		objectA->ApplyCollisionResponse(-1.0f * directionAToB);
		objectB->ApplyCollisionResponse(directionAToB);
		return;
	}
	if (isPushBackA) {
		objectA->GetTransform().translate = objectA->GetTransform().translate + (-penetration) * directionAToB;
		objectA->ApplyCollisionResponse(-1.0f * directionAToB);
		return;
	}
	objectB->GetTransform().translate = objectB->GetTransform().translate + penetration * directionAToB;
	objectB->ApplyCollisionResponse(directionAToB);
}

} // 名前空間 BaseSceneCollisionHelpers
