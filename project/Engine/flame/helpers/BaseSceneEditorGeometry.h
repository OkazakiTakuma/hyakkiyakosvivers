#pragma once

#include "base_scene/BaseScene.h"
#include "../../math/MathConstants.h"

#include <algorithm>
#include <cmath>

/// <summary>
/// BaseSceneのエディター操作に必要な幾何計算を提供します。
/// </summary>
namespace BaseSceneEditorGeometry {

inline const char* EditorCreateTypeName(BaseScene::EditorCreateType type) {
	switch (type) {
	case BaseScene::EditorCreateType::Object3dSphere:
		return "Object3dSphere";
	case BaseScene::EditorCreateType::Object3dCylinder:
		return "Object3dCylinder";
	case BaseScene::EditorCreateType::Object3dCylinderOpen:
		return "Object3dCylinderOpen";
	case BaseScene::EditorCreateType::Sprite:
		return "Sprite";
	case BaseScene::EditorCreateType::Text:
		return "Text";
	case BaseScene::EditorCreateType::LoadedModel:
		return "LoadedModel";
	case BaseScene::EditorCreateType::AnimatedModel:
		return "AnimatedModel";
	case BaseScene::EditorCreateType::Camera:
		return "Camera";
	case BaseScene::EditorCreateType::PointLight:
		return "PointLight";
	case BaseScene::EditorCreateType::ParticleEmitter:
		return "ParticleEmitter";
	case BaseScene::EditorCreateType::Player:
		return "Player";
	case BaseScene::EditorCreateType::EnemySpawnPoint:
		return "EnemySpawnPoint";
	case BaseScene::EditorCreateType::Enemy:
		return "Enemy";
	case BaseScene::EditorCreateType::Empty:
	default:
		return "Empty";
	}
}

inline float ToDegrees(float radians) {
	return radians * 180.0f / MathConstants::kPi;
}

inline float ToRadians(float degrees) {
	return degrees * MathConstants::kPi / 180.0f;
}

inline Vector3 RotateAroundAxis(const Vector3& value, const Vector3& axis, float radians) {
	const Vector3 normalizedAxis = Normalize(axis);
	if (Length(normalizedAxis) <= MathConstants::kNormalizationEpsilon) {
		return value;
	}

	const float cosValue = std::cos(radians);
	const float sinValue = std::sin(radians);
	return
	    cosValue * value +
	    sinValue * Cross(normalizedAxis, value) +
	    Dot(normalizedAxis, value) * (1.0f - cosValue) * normalizedAxis;
}

inline bool TryGetCameraTransform(Camera* camera, Vector3& translate, Vector3& rotate) {
	if (!camera) {
		return false;
	}

	translate = camera->GetTranslate();
	rotate = camera->GetRotate();
	return true;
}

inline Vector3 TransformCoord(const Vector3& vector, const Matrix4x4& matrix) {
	return Transformation(vector, matrix);
}

inline bool IntersectRayToOBB(const Vector3& rayOrigin, const Vector3& rayDirection, const OBBColliderShape& obb, float& distance) {
	float tMin = 0.0f;
	float tMax = 100000.0f;
	const Vector3 centerToRay = rayOrigin - obb.center;

	for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
		const float origin = Dot(centerToRay, obb.orientation[axisIndex]);
		const float direction = Dot(rayDirection, obb.orientation[axisIndex]);
		const float minValue = -((&obb.halfSize.x)[axisIndex]);
		const float maxValue = (&obb.halfSize.x)[axisIndex];

		if (std::fabs(direction) < MathConstants::kNormalizationEpsilon) {
			if (origin < minValue || origin > maxValue) {
				return false;
			}
			continue;
		}

		float t1 = (minValue - origin) / direction;
		float t2 = (maxValue - origin) / direction;
		if (t1 > t2) {
			std::swap(t1, t2);
		}
		if (t1 > tMin) {
			tMin = t1;
		}
		if (t2 < tMax) {
			tMax = t2;
		}
		if (tMin > tMax) {
			return false;
		}
	}

	distance = tMin;
	return true;
}

inline OBBColliderShape MakePickOBB(GameObject* object) {
	if (OBBColliderComponent* collider = object->GetComponent<OBBColliderComponent>()) {
		return collider->GetWorldOBB();
	}

	const EulerTransform& transform = object->GetTransform();
	const Matrix4x4 rotateMatrix = MakeRotateXYZMatrix(transform.rotate);
	const Vector3 axisX{rotateMatrix.m[0][0], rotateMatrix.m[0][1], rotateMatrix.m[0][2]};
	const Vector3 axisY{rotateMatrix.m[1][0], rotateMatrix.m[1][1], rotateMatrix.m[1][2]};
	const Vector3 axisZ{rotateMatrix.m[2][0], rotateMatrix.m[2][1], rotateMatrix.m[2][2]};

	OBBColliderShape obb{};
	obb.center = transform.translate;
	obb.orientation[0] = Normalize(axisX);
	obb.orientation[1] = Normalize(axisY);
	obb.orientation[2] = Normalize(axisZ);
	const float halfX = std::fabs(transform.scale.x) * 0.5f;
	const float halfY = std::fabs(transform.scale.y) * 0.5f;
	const float halfZ = std::fabs(transform.scale.z) * 0.5f;
	obb.halfSize = {
	    halfX < 0.25f ? 0.25f : halfX,
	    halfY < 0.25f ? 0.25f : halfY,
	    halfZ < 0.25f ? 0.25f : halfZ
	};
	return obb;
}

} // 名前空間 BaseSceneEditorGeometry
