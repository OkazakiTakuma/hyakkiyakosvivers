#include "PrimitiveMeshGenerator.h"

#include <cassert>
#include <cmath>
#include <numbers>

namespace PrimitiveMeshGenerator {

MeshData GenerateCylinder(float radius, float height, uint32_t subdivision, bool createTopCap, bool createBottomCap) {
	// 側面を構成できない分割数は受け付けない。
	assert(subdivision >= 3);
	if (subdivision < 3) {
		return {};
	}

	MeshData mesh;
	const float halfHeight = height * 0.5f;
	const float fullTurn = 2.0f * std::numbers::pi_v<float>;

	// 継ぎ目のUVを分離するため、始点と終点を重複させて側面頂点を生成する。
	for (uint32_t index = 0; index <= subdivision; ++index) {
		const float ratio = static_cast<float>(index) / static_cast<float>(subdivision);
		const float theta = ratio * fullTurn;
		const float cosTheta = std::cos(theta);
		const float sinTheta = std::sin(theta);
		const Vector3 normal = {cosTheta, 0.0f, sinTheta};

		mesh.vertices.push_back({{cosTheta * radius, halfHeight, sinTheta * radius, 1.0f}, {ratio, 0.0f}, normal});
		mesh.vertices.push_back({{cosTheta * radius, -halfHeight, sinTheta * radius, 1.0f}, {ratio, 1.0f}, normal});
	}

	// 各区間を2三角形へ分割し、円柱の側面を構成する。
	for (uint32_t index = 0; index < subdivision; ++index) {
		const uint32_t top1 = index * 2;
		const uint32_t bottom1 = top1 + 1;
		const uint32_t top2 = (index + 1) * 2;
		const uint32_t bottom2 = top2 + 1;
		mesh.indices.insert(mesh.indices.end(), {top1, top2, bottom1, bottom1, top2, bottom2});
	}

	// 蓋は中心点を共有する三角形ファンとして生成する。
	if (createTopCap) {
		const uint32_t centerIndex = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back({{0.0f, halfHeight, 0.0f, 1.0f}, {0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}});
		for (uint32_t index = 0; index <= subdivision; ++index) {
			const float theta = static_cast<float>(index) / static_cast<float>(subdivision) * fullTurn;
			const float cosTheta = std::cos(theta);
			const float sinTheta = std::sin(theta);
			mesh.vertices.push_back({
			    {cosTheta * radius, halfHeight, sinTheta * radius, 1.0f},
			    {cosTheta * 0.5f + 0.5f, -sinTheta * 0.5f + 0.5f},
			    {0.0f, 1.0f, 0.0f}});
		}
		for (uint32_t index = 0; index < subdivision; ++index) {
			mesh.indices.insert(mesh.indices.end(), {centerIndex, centerIndex + 1 + index, centerIndex + 2 + index});
		}
	}

	if (createBottomCap) {
		const uint32_t centerIndex = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back({{0.0f, -halfHeight, 0.0f, 1.0f}, {0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}});
		for (uint32_t index = 0; index <= subdivision; ++index) {
			const float theta = static_cast<float>(index) / static_cast<float>(subdivision) * fullTurn;
			const float cosTheta = std::cos(theta);
			const float sinTheta = std::sin(theta);
			mesh.vertices.push_back({
			    {cosTheta * radius, -halfHeight, sinTheta * radius, 1.0f},
			    {cosTheta * 0.5f + 0.5f, sinTheta * 0.5f + 0.5f},
			    {0.0f, -1.0f, 0.0f}});
		}
		for (uint32_t index = 0; index < subdivision; ++index) {
			mesh.indices.insert(mesh.indices.end(), {centerIndex, centerIndex + 2 + index, centerIndex + 1 + index});
		}
	}

	return mesh;
}

MeshData GeneratePlane(float width, float height) {
	const float halfWidth = width * 0.5f;
	const float halfHeight = height * 0.5f;
	MeshData mesh;
	mesh.vertices = {
		{{-halfWidth, halfHeight, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
		{{halfWidth, halfHeight, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
		{{-halfWidth, -halfHeight, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
		{{halfWidth, -halfHeight, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}}
	};
	// 同じ面を逆巻きでも登録し、アリーナの内外どちらから見ても表示されるようにする。
	mesh.indices = {0, 1, 2, 2, 1, 3, 2, 1, 0, 3, 1, 2};
	return mesh;
}

} // 名前空間 PrimitiveMeshGenerator
