#pragma once
#include "core/EntityComponent.h"
#include "data/PlayerAttackDefinition.h"
#include "Vector.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class PlayerComponent;

/// <summary>プレイヤー弾の移動パターンです。</summary>
enum class PlayerAttackEntityMotionType {
	Linear,
	Magatama,
	Orbit,
	PoisonCloud,
	FlameField,
	SkyLaser,
	Boomerang,
	Ricochet,
	ClawSlash
};

/// <summary>シーン側で実体の弾へ変換する発射要求です。</summary>
struct PlayerAttackShotRequest {
	/// <summary>生成元の攻撃名です。保存・デバッグ表示に使用します。</summary>
	std::string attackName;
	/// <summary>表示名から独立した、弾の表示方式を選択する安定IDです。</summary>
	std::string visualId = "model";
	/// <summary>生成元の攻撃レベルです。</summary>
	std::string level;
	/// <summary>弾を生成するワールド座標です。</summary>
	Vector3 position{};
	/// <summary>弾の初期進行方向です。</summary>
	Vector3 direction{0.0f, 0.0f, 1.0f};
	float attack = 100.0f;
	float speed = 0.3f;
	float size = 1.0f;
	float lifeTime = 3.0f;
	int pierceCount = 0;
	int knockback = 0;
	bool infinitePierce = false;
	float repeatHitInterval = 0.0f;
	std::string modelFilePath = "sphere.obj";
	bool homing = false;
	float homingAccuracy = 1.0f;
	PlayerAttackEntityMotionType motionType = PlayerAttackEntityMotionType::Linear;
	/// <summary>周回弾や爪攻撃の基準にするGameObjectです。所有権は持ちません。</summary>
	Entity* motionAnchor = nullptr;
	/// <summary>キャラクター特性の命中時判定に使う攻撃元です。所有権は持ちません。</summary>
	Entity* sourceOwner = nullptr;
	/// <summary>周回弾の現在角度です。</summary>
	float orbitAngleRadians = 0.0f;
	/// <summary>周回弾が基準点から離れる水平半径です。</summary>
	float orbitRadius = 2.2f;
	/// <summary>周回弾の基準点からの高さです。</summary>
	float orbitHeight = 0.65f;
	/// <summary>周回弾の角速度です。</summary>
	float orbitAngularSpeed = 2.2f;
	float travelDistance = 6.0f;
	int clawSlashIndex = 0;
	int clawSlashCount = 3;
	// 勾玉の本体・トレイル・発光へ同じ6色を割り当てる番号。
	int colorIndex = 0;
};

/// <summary>
/// Strategyが発射要求を計算するために必要な、発射時点の読み取り専用情報です。
/// Component本体を渡さず、攻撃パターンが実行時状態を直接変更できないようにします。
/// </summary>
struct PlayerAttackContext {
	Vector3 ownerPosition{};
	float ownerRotationY = 0.0f;
	float attackRate = 1.0f;
	float attackSizeRate = 1.0f;
	Entity* motionAnchor = nullptr;
};

/// <summary>
/// 攻撃名に対応する発射要求の組み立て方を差し替えるStrategyです。
/// 攻撃値などのデータはJSON側に残し、特殊な弾配置や軌道指定だけを実装クラスへ分離します。
/// </summary>
class IPlayerAttackPattern {
public:
	virtual ~IPlayerAttackPattern() = default;

	/// <summary>Contextと定義データだけから、この発射で生成する要求を返します。</summary>
	virtual std::vector<PlayerAttackShotRequest> CreateShots(
		const PlayerAttackContext& context,
		const PlayerAttackStats& attackStats,
		const PlayerAttackLevelStats& levelStats,
		const std::string& currentLevel) const = 0;
};

/// <summary>装備中の攻撃スロットを更新し、発射タイミングごとに弾生成要求を作ります。</summary>
class PlayerAttackComponent : public EntityComponent {
public:
	void Update() override;

	void ApplyAttackStats(const PlayerAttackStats& stats, const std::string& level);

	void ClearAttackSlots();

	void AddAttackSlot(const PlayerAttackStats& stats, const std::string& level, bool enabled);
	void UpdateAttackStatsByName(const std::string& attackName, const PlayerAttackStats& stats);

	const PlayerAttackStats& GetAttackStats() const;
	const std::string& GetAttackName() const;
	const std::string& GetLevel() const;
	void SetLevel(const std::string& level);

	std::vector<PlayerAttackShotRequest> ConsumeShotRequests();

private:
	/// <summary>装備スロットごとのレベル、クールダウン、利用可否を保持します。</summary>
	struct AttackSlotRuntime {
		PlayerAttackStats stats;
		std::string level = "1";
		float attackTimer = 0.0f;
		bool enabled = true;
	};

	static std::string NormalizeLevel(const std::string& level);

	PlayerAttackLevelStats FindCurrentLevelStats(const AttackSlotRuntime& slot, const std::string& level) const;

	void CreateAttack(Entity* owner, const PlayerComponent& player, const AttackSlotRuntime& slot, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel);
	void RegisterAttackPatterns();

	PlayerAttackStats emptyStats_;
	std::string defaultLevel_ = "1";
	/// <summary>同時に更新する攻撃スロットの実行時状態です。</summary>
	std::vector<AttackSlotRuntime> slots_;
	/// <summary>JSONの攻撃名から特殊な発射パターンを取得する登録表です。</summary>
	std::unordered_map<std::string, std::unique_ptr<IPlayerAttackPattern>> attackPatterns_;
	/// <summary>専用登録のない通常・拡散・追尾攻撃をデータ駆動で生成する既定Strategyです。</summary>
	std::unique_ptr<IPlayerAttackPattern> defaultAttackPattern_;
	/// <summary>次にシーンが回収する未処理の発射要求です。</summary>
	std::vector<PlayerAttackShotRequest> shotRequests_;
};

