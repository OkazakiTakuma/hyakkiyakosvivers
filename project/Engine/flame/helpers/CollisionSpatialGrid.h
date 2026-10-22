#pragma once

#include "../../collision/CollisionPrimitive.h"
#include "core/GameObject.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/// <summary>XZ平面上でオブジェクトを管理する、衝突Broad Phase用の一様グリッドです。</summary>
class CollisionSpatialGrid {
public:
	struct Bounds {
		float minX = 0.0f;
		float minZ = 0.0f;
		float maxX = 0.0f;
		float maxZ = 0.0f;
	};

	explicit CollisionSpatialGrid(float cellSize = 10.0f) { SetCellSize(cellSize); }

	void SetCellSize(float cellSize) {
		cellSize_ = cellSize > 0.0f ? cellSize : 1.0f;
	}
	float GetCellSize() const { return cellSize_; }

	void Clear() {
		cells_.clear();
		entryCount_ = 0;
	}

	/// <summary>指定範囲と重なるすべてのセルへオブジェクトを登録します。</summary>
	void Insert(GameObject* object, const Bounds& sourceBounds) {
		if (!object) {
			return;
		}

		const Bounds bounds = NormalizeBounds(sourceBounds);
		const CellCoordinate minimum = ToCell(bounds.minX, bounds.minZ);
		const CellCoordinate maximum = ToCell(bounds.maxX, bounds.maxZ);
		for (int z = minimum.z; z <= maximum.z; ++z) {
			for (int x = minimum.x; x <= maximum.x; ++x) {
				cells_[{x, z}].push_back(object);
				++entryCount_;
			}
		}
	}

	/// <summary>指定範囲が触れるセルから候補を取得します。同じオブジェクトは一度だけ返します。</summary>
	std::vector<GameObject*> Query(const Bounds& sourceBounds) const {
		const Bounds bounds = NormalizeBounds(sourceBounds);
		const CellCoordinate minimum = ToCell(bounds.minX, bounds.minZ);
		const CellCoordinate maximum = ToCell(bounds.maxX, bounds.maxZ);
		std::vector<GameObject*> result;
		std::unordered_set<GameObject*> visited;

		for (int z = minimum.z; z <= maximum.z; ++z) {
			for (int x = minimum.x; x <= maximum.x; ++x) {
				const auto cell = cells_.find({x, z});
				if (cell == cells_.end()) {
					continue;
				}
				for (GameObject* object : cell->second) {
					if (visited.insert(object).second) {
						result.push_back(object);
					}
				}
			}
		}
		return result;
	}

	size_t GetOccupiedCellCount() const { return cells_.size(); }
	size_t GetEntryCount() const { return entryCount_; }

	static Bounds FromCenterRadius(const Vector3& center, float radius) {
		const float safeRadius = (std::max)(0.0f, radius);
		return {
			center.x - safeRadius,
			center.z - safeRadius,
			center.x + safeRadius,
			center.z + safeRadius
		};
	}

	static Bounds FromSphere(const SphereColliderShape& sphere) {
		return FromCenterRadius(sphere.center, sphere.radius);
	}

	static Bounds FromOBB(const OBBColliderShape& obb) {
		const float extentX =
			std::fabs(obb.orientation[0].x) * obb.halfSize.x +
			std::fabs(obb.orientation[1].x) * obb.halfSize.y +
			std::fabs(obb.orientation[2].x) * obb.halfSize.z;
		const float extentZ =
			std::fabs(obb.orientation[0].z) * obb.halfSize.x +
			std::fabs(obb.orientation[1].z) * obb.halfSize.y +
			std::fabs(obb.orientation[2].z) * obb.halfSize.z;
		return {
			obb.center.x - extentX,
			obb.center.z - extentZ,
			obb.center.x + extentX,
			obb.center.z + extentZ
		};
	}

	static Bounds Merge(const Bounds& left, const Bounds& right) {
		return {
			(std::min)(left.minX, right.minX),
			(std::min)(left.minZ, right.minZ),
			(std::max)(left.maxX, right.maxX),
			(std::max)(left.maxZ, right.maxZ)
		};
	}

	static Bounds FromSweptSphere(const Vector3& start, const Vector3& end, float radius) {
		const float safeRadius = (std::max)(0.0f, radius);
		return {
			(std::min)(start.x, end.x) - safeRadius,
			(std::min)(start.z, end.z) - safeRadius,
			(std::max)(start.x, end.x) + safeRadius,
			(std::max)(start.z, end.z) + safeRadius
		};
	}

private:
	struct CellCoordinate {
		int x = 0;
		int z = 0;

		bool operator==(const CellCoordinate& other) const {
			return x == other.x && z == other.z;
		}
	};

	struct CellCoordinateHash {
		size_t operator()(const CellCoordinate& coordinate) const {
			const size_t xHash = std::hash<int>{}(coordinate.x);
			const size_t zHash = std::hash<int>{}(coordinate.z);
			return xHash ^ (zHash + 0x9e3779b9u + (xHash << 6u) + (xHash >> 2u));
		}
	};

	static Bounds NormalizeBounds(const Bounds& bounds) {
		return {
			(std::min)(bounds.minX, bounds.maxX),
			(std::min)(bounds.minZ, bounds.maxZ),
			(std::max)(bounds.minX, bounds.maxX),
			(std::max)(bounds.minZ, bounds.maxZ)
		};
	}

	CellCoordinate ToCell(float x, float z) const {
		return {
			static_cast<int>(std::floor(x / cellSize_)),
			static_cast<int>(std::floor(z / cellSize_))
		};
	}

	float cellSize_ = 10.0f;
	size_t entryCount_ = 0;
	std::unordered_map<CellCoordinate, std::vector<GameObject*>, CellCoordinateHash> cells_;
};
