#pragma once

#include "components/EnemySpawnPoint/EnemySpawnPointComponent.h"

#include <string>
#include <vector>

class Entity;
class SceneWorld;

/// <summary>BaseSceneへ通常敵1体の生成を依頼するための値オブジェクトです。</summary>
struct EnemySpawnRequest {
	std::string enemyTypeName;
	Vector3 position{};
	Entity* target = nullptr;
	EnemySpawnPointComponent::EnemyStatMultipliers statMultipliers{};
};

/// <summary>ボス戦開始時に必要な設定と対象プレイヤーをまとめた要求です。</summary>
struct BossEncounterRequest {
	EnemySpawnPointComponent::BossEncounterSettings settings{};
	Entity* player = nullptr;
};

/// <summary>1フレームの生成判定で発生した要求と進行状態の変化です。</summary>
struct EnemySpawnUpdateResult {
	// SceneWorldの走査中にEntityを追加しないよう、生成はBaseSceneへ遅延する。
	std::vector<EnemySpawnRequest> enemyRequests;
	bool hasBossEncounterRequest = false;
	BossEncounterRequest bossEncounter{};
	bool stageCleared = false;
};

/// <summary>通常敵生成要求とボス遭遇の進行状態を管理します。</summary>
class EnemySpawnSystem final {
public:
	/// <summary>追跡中のボスを解除して初期状態へ戻します。</summary>
	void Reset();
	/// <summary>生成済みボスの名前を記録し、撃破後の報酬完了まで追跡します。</summary>
	void BeginBossEncounter(const std::string& bossObjectName);
	/// <summary>各スポーン地点を更新し、生成要求またはステージクリアを返します。</summary>
	EnemySpawnUpdateResult Update(
		SceneWorld& world,
		bool isSelectionActive,
		bool hasPendingBossOffers);

private:
	// Entityの再配置やコンテナ更新後も検索し直せるよう、非所有ポインタではなく名前を保持する。
	std::string activeBossObjectName_;
};
