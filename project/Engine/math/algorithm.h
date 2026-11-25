#pragma once
#include "Vector.h"
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>
#include <random>


/// <summary>
/// 指定条件に基づくランダムな値を生成します。
/// </summary>
inline float RandomFloatRange(float min, float max) {
    float r = (float)rand() / (float)RAND_MAX; // 0.0 ~ 1.0
    return min + r * (max - min);
}

inline void GenerateLightningRecursive(const Vector3& start, const Vector3& end, float displacement, int generation, std::vector<Vector3>& outPath) {
    if (generation <= 0) {
        return;
    }

    Vector3 mid = {
        (start.x + end.x) * 0.5f,
        (start.y + end.y) * 0.5f,
        (start.z + end.z) * 0.5f
    };

    /// <summary>
    /// 指定条件に基づくランダムな値を生成します。
    /// </summary>
    mid.x += RandomFloatRange(-displacement, displacement);
    /// <summary>
    /// 指定条件に基づくランダムな値を生成します。
    /// </summary>
    mid.y += RandomFloatRange(-displacement, displacement);
    /// <summary>
    /// 指定条件に基づくランダムな値を生成します。
    /// </summary>
    mid.z += RandomFloatRange(-displacement, displacement);

    GenerateLightningRecursive(start, mid, displacement * 0.5f, generation - 1, outPath);

    outPath.push_back(mid);

    GenerateLightningRecursive(mid, end, displacement * 0.5f, generation - 1, outPath);
}

/// <summary>
/// LightningPath を作成し、利用できる状態にします。
/// </summary>
inline std::vector<Vector3> CreateLightningPath(const Vector3& start, const Vector3& end, float initialDisplacement, int generations) {
    std::vector<Vector3> path;

    path.push_back(start); // 蟋狗せ
    GenerateLightningRecursive(start, end, initialDisplacement, generations, path);
    path.push_back(end);

    return path;
}

inline std::vector<VertexData> GenerateRingVertices(uint32_t segments, float outerRadius, float innerRadius) {
	std::vector<VertexData> vertices;
	vertices.reserve(segments * 6);

	for (uint32_t i = 0; i < segments; ++i) {
		float ratio1 = static_cast<float>(i) / segments;
		float ratio2 = static_cast<float>(i + 1) / segments;

		float angle1 = ratio1 * 2.0f * std::numbers::pi_v<float>;
		float angle2 = ratio2 * 2.0f * std::numbers::pi_v<float>;

		Vector4 outer1 = { outerRadius * std::cos(angle1), outerRadius * std::sin(angle1), 0.0f, 1.0f };
		Vector4 inner1 = { innerRadius * std::cos(angle1), innerRadius * std::sin(angle1), 0.0f, 1.0f };
		Vector4 outer2 = { outerRadius * std::cos(angle2), outerRadius * std::sin(angle2), 0.0f, 1.0f };
		Vector4 inner2 = { innerRadius * std::cos(angle2), innerRadius * std::sin(angle2), 0.0f, 1.0f };

		Vector2 uvOuter1 = { ratio1, 0.0f };
		Vector2 uvInner1 = { ratio1, 1.0f };
		Vector2 uvOuter2 = { ratio2, 0.0f };
		Vector2 uvInner2 = { ratio2, 1.0f };

		Vector3 normal = { 0.0f, 0.0f, -1.0f };

		vertices.push_back({ outer1, uvOuter1, normal });
		vertices.push_back({ inner1, uvInner1, normal });
		vertices.push_back({ outer2, uvOuter2, normal });

		vertices.push_back({ inner1, uvInner1, normal });
		vertices.push_back({ inner2, uvInner2, normal });
		vertices.push_back({ outer2, uvOuter2, normal });
	}

	return vertices;
}

/// <param name="radius">半径を指定します。</param>
/// <param name="height">高さを指定します。</param>
inline std::vector<VertexData> GenerateCylinderSideVertices(uint32_t segments, float radius, float height) {
    std::vector<VertexData> vertices;
    vertices.reserve(segments * 6);

    for (uint32_t i = 0; i < segments; ++i) {
        float ratio1 = static_cast<float>(i) / segments;
        float ratio2 = static_cast<float>(i + 1) / segments;

        float angle1 = ratio1 * 2.0f * std::numbers::pi_v<float>;
        float angle2 = ratio2 * 2.0f * std::numbers::pi_v<float>;

        Vector4 p1_b = { radius * std::cos(angle1), 0.0f, radius * std::sin(angle1), 1.0f };
        Vector4 p1_t = { radius * std::cos(angle1), height, radius * std::sin(angle1), 1.0f };
        Vector4 p2_b = { radius * std::cos(angle2), 0.0f, radius * std::sin(angle2), 1.0f };
        Vector4 p2_t = { radius * std::cos(angle2), height, radius * std::sin(angle2), 1.0f };

        Vector3 n1 = { std::cos(angle1), 0.0f, std::sin(angle1) };
        Vector3 n2 = { std::cos(angle2), 0.0f, std::sin(angle2) };

        vertices.push_back({ p1_b, {ratio1, 1.0f}, n1 });
        vertices.push_back({ p1_t, {ratio1, 0.0f}, n1 });
        vertices.push_back({ p2_t, {ratio2, 0.0f}, n2 });
        vertices.push_back({ p1_b, {ratio1, 1.0f}, n1 });
        vertices.push_back({ p2_t, {ratio2, 0.0f}, n2 });
        vertices.push_back({ p2_b, {ratio2, 1.0f}, n2 });
    }
    return vertices;
}
