#pragma once

#include "helpers/CollisionSpatialGrid.h"

#include <cstddef>

class SceneWorld;
class Entity;

/// <summary>プレイヤー弾の精密判定結果と、命中による撃破の有無です。</summary>
struct PlayerProjectileHitResult {
	bool hit = false;
	bool enemyDefeated = false;
	/// <summary>命中によって実際に減少した敵HPです。</summary>
	float damageDealt = 0.0f;
};

/// <summary>衝突Broad Phase、接触応答、命中判定の計測状態を所有します。</summary>
class CombatSystem final {
public:
	/// <summary>空間分割の効果をエディタ上で確認するためのフレーム別計測値です。</summary>
	struct DebugStats {
		std::size_t colliderObjects = 0;
		std::size_t occupiedCells = 0;
		std::size_t gridEntries = 0;
		std::size_t colliderCandidatePairs = 0;
		std::size_t colliderNarrowPhaseTests = 0;
		std::size_t playerProjectileCandidates = 0;
		std::size_t playerProjectileNarrowPhaseTests = 0;
		std::size_t enemyProjectileCandidates = 0;
		std::size_t enemyProjectileNarrowPhaseTests = 0;
	};

	/// <summary>空間グリッドで候補を絞り、一般コライダー間の接触判定と押し戻しを行います。</summary>
	void UpdateColliderCollisions(SceneWorld& world);
	/// <summary>敵弾とプレイヤーの候補抽出、命中処理、弾の無効化を行います。</summary>
	void UpdateEnemyProjectileHits(SceneWorld& world);
	/// <summary>プレイヤー弾1個と敵1体を精密判定し、ダメージとノックバックを適用します。</summary>
	PlayerProjectileHitResult ResolvePlayerProjectileHit(
		Entity* projectileObject,
		Entity* enemyObject);
	CollisionSpatialGrid& GetSpatialGrid() { return collisionSpatialGrid_; }
	DebugStats& GetDebugStats() { return collisionDebugStats_; }
	const DebugStats& GetDebugStats() const { return collisionDebugStats_; }
	float GetCellSize() const { return collisionSpatialGrid_.GetCellSize(); }
	void SetCellSize(float cellSize) { collisionSpatialGrid_.SetCellSize(cellSize); }

private:
	// 各戦闘判定で再利用し、毎フレームの動的確保と全組み合わせ判定を抑える。
	CollisionSpatialGrid collisionSpatialGrid_{10.0f};
	// Update開始時に各項目をリセットし、直近フレームの値だけを保持する。
	DebugStats collisionDebugStats_{};
};
