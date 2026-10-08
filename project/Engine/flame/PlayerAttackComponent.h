#pragma once
#include "Component.h"
#include "Vector.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Player;

/// <summary>攻撃1種類の特定レベルにおける発射・威力設定です。</summary>
struct PlayerAttackLevelStats {
	/// <summary>JSON内で識別するレベル名です。通常は1～5またはsuperを指定します。</summary>
	std::string level = "1";
	/// <summary>レベルアップ選択画面に表示する説明文です。</summary>
	std::string choiceDescription;
	/// <summary>選択肢カードに表示する画像パスです。</summary>
	std::string choiceTextureFilePath;
	/// <summary>このレベルの基礎攻撃力です。プレイヤーの攻撃倍率を掛けて弾へ渡します。</summary>
	float attack = 100.0f;
	/// <summary>60FPS時の1フレームあたりの移動量、または特殊攻撃の速度パラメーターです。</summary>
	float speed = 0.3f;
	/// <summary>100を基準とした弾の表示・判定サイズです。</summary>
	float size = 100.0f;
	/// <summary>1回の発射で生成する弾数です。</summary>
	int shotCount = 1;
	/// <summary>弾番号と同じ添字に対応する、プレイヤー前方基準の発射角度です。</summary>
	std::vector<float> angles = {0.0f};
	/// <summary>弾ごとの発射位置です。X=右、Y=上、Z=前のプレイヤーローカル座標で指定します。</summary>
	std::vector<Vector3> spawnOffsets = {{0.0f, 0.5f, 1.2f}};
	/// <summary>弾の表示に使用するモデルファイル名です。</summary>
	std::string modelFilePath = "sphere.obj";
	/// <summary>trueの場合、生成後に最寄りの敵へ追尾対象を設定します。</summary>
	bool homing = false;
	/// <summary>0～1で表す追尾の曲がりやすさです。</summary>
	float homingAccuracy = 1.0f;
	/// <summary>次にこの攻撃を発射できるまでの基本秒数です。</summary>
	float attackInterval = 0.5f;
	/// <summary>弾が自動消滅するまでの秒数です。</summary>
	float lifeTime = 3.0f;
	/// <summary>ブーメランなど、距離で挙動を切り替える攻撃に使用する移動距離です。</summary>
	float travelDistance = 6.0f;
	/// <summary>何体まで貫通できるかを指定します。0なら命中時に消滅します。</summary>
	int pierceCount = 0;
	/// <summary>trueの場合、命中しても貫通回数を消費せず寿命まで残ります。</summary>
	bool infinitePierce = false;
};

/// <summary>攻撃名と、選択可能なレベル設定のまとまりです。</summary>
struct PlayerAttackStats {
	std::string name = "Straight";
	std::string choiceTextureFilePath;
	std::string superConditionStatusName;
	std::string superConditionStatusLevel = "1";
	std::vector<PlayerAttackLevelStats> levels;
};

/// <summary>プレイヤー弾の移動パターンです。</summary>
enum class PlayerProjectileMotionType {
	Linear,
	// 発射方向へ一度膨らんでから、時間経過で追尾力を強めて敵へ収束する勾玉。
	Magatama,
	Orbit,
	SkyLaser,
	Boomerang,
	Ricochet,
	ClawSlash
};

/// <summary>シーン側で実体の弾へ変換する発射要求です。</summary>
struct PlayerAttackShotRequest {
	/// <summary>生成元の攻撃名です。見た目や特殊処理の分岐に使用します。</summary>
	std::string attackName;
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
	bool infinitePierce = false;
	std::string modelFilePath = "sphere.obj";
	bool homing = false;
	float homingAccuracy = 1.0f;
	PlayerProjectileMotionType motionType = PlayerProjectileMotionType::Linear;
	/// <summary>周回弾や爪攻撃の基準にするGameObjectです。所有権は持ちません。</summary>
	GameObject* motionAnchor = nullptr;
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

class PlayerAttackComponent;

/// <summary>
/// 攻撃名に対応する発射要求の組み立て方を差し替えるStrategyです。
/// 攻撃値などのデータはJSON側に残し、特殊な弾配置や軌道指定だけを実装クラスへ分離します。
/// </summary>
class IPlayerAttackPattern {
public:
	virtual ~IPlayerAttackPattern() = default;

	virtual void CreateShots(
		PlayerAttackComponent& component,
		GameObject* owner,
		const Player& player,
		const PlayerAttackStats& attackStats,
		const PlayerAttackLevelStats& levelStats,
		const std::string& currentLevel) const = 0;
};

class DefaultPlayerAttackPattern;
class MagatamaPlayerAttackPattern;
class OrbitPlayerAttackPattern;
class SkyLaserPlayerAttackPattern;
class BoomerangPlayerAttackPattern;
class RicochetPlayerAttackPattern;
class ClawSlashPlayerAttackPattern;

/// <summary>装備中の攻撃スロットを更新し、発射タイミングごとに弾生成要求を作ります。</summary>
class PlayerAttackComponent : public Component {
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
	friend class DefaultPlayerAttackPattern;
	friend class MagatamaPlayerAttackPattern;
	friend class OrbitPlayerAttackPattern;
	friend class SkyLaserPlayerAttackPattern;
	friend class BoomerangPlayerAttackPattern;
	friend class RicochetPlayerAttackPattern;
	friend class ClawSlashPlayerAttackPattern;

	/// <summary>装備スロットごとのレベル、クールダウン、利用可否を保持します。</summary>
	struct AttackSlotRuntime {
		PlayerAttackStats stats;
		std::string level = "1";
		float attackTimer = 0.0f;
		bool enabled = true;
	};

	static std::string NormalizeLevel(const std::string& level);

	PlayerAttackLevelStats FindCurrentLevelStats(const AttackSlotRuntime& slot, const std::string& level) const;

	static Vector3 RotateYaw(const Vector3& direction, float degrees);

	static Vector3 GetShotSpawnOffset(const PlayerAttackLevelStats& levelStats, int shotIndex);

	void QueueShot(GameObject* owner, const Player& player, const PlayerAttackStats& attackStats, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel, float angleDegrees, int shotIndex, bool alignSpawnToShotAngle = false);

	void CreateStraightAttack(GameObject* owner, const Player& player, const PlayerAttackStats& attackStats, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel);

	void CreateSpreadAttack(GameObject* owner, const Player& player, const PlayerAttackStats& attackStats, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel);

	void CreateHomingAttack(GameObject* owner, const Player& player, const PlayerAttackStats& attackStats, PlayerAttackLevelStats levelStats, const std::string& currentLevel);

	void CreateMagatamaAttack(GameObject* owner, const Player& player, const PlayerAttackStats& attackStats, PlayerAttackLevelStats levelStats, const std::string& currentLevel);

	void CreateOrbitAttack(GameObject* owner, const Player& player, const PlayerAttackStats& attackStats, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel);

	void CreateSkyLaserAttack(GameObject* owner, const Player& player, const PlayerAttackStats& attackStats, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel);

	void CreateBoomerangAttack(GameObject* owner, const Player& player, const PlayerAttackStats& attackStats, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel);

	void CreateRicochetAttack(GameObject* owner, const Player& player, const PlayerAttackStats& attackStats, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel);

	void CreateClawSlashAttack(GameObject* owner, const Player& player, const PlayerAttackStats& attackStats, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel);

	void CreateAttack(GameObject* owner, const Player& player, const AttackSlotRuntime& slot, const PlayerAttackLevelStats& levelStats, const std::string& currentLevel);
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
