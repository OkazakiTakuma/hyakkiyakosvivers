#pragma once

#include "struct.h"
#include <cstdint>
#include <vector>

/// <summary>
/// GPUや描画状態に依存しないプリミティブの頂点・インデックス生成を提供します。
/// </summary>
namespace PrimitiveMeshGenerator {

struct MeshData {
	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
};

MeshData GenerateCylinder(float radius, float height, uint32_t subdivision, bool createTopCap, bool createBottomCap);
/// <summary>XY平面上に、表裏どちらからも見える板ポリゴンを生成します。</summary>
MeshData GeneratePlane(float width, float height);

} // 名前空間 PrimitiveMeshGenerator
