#pragma once
#include "Component.h"
#include "Vector.h"
#include "PlayerAttackComponent.h"
#include <string>
#include <vector>

/// <summary>軌道Strategyが生成側へ公開する、弾の見た目と衝突上の特性です。</summary>
struct PlayerProjectilePresentation {
	Vector3 scaleMultiplier{1.0f, 1.0f, 1.0f};
	bool modelVisible = true;
	bool addSkyLaserVisual = false;
	bool addTrail = true;
	float trailWidthMultiplier = 0.8f;
	float minimumTrailWidth = 0.12f;
	float trailLifeTime = 0.32f;
	bool trailUsesMotionAnchor = false;
	bool overrideModelColor = false;
	Vector4 modelColor{1.0f, 1.0f, 1.0f, 1.0f};
	bool overrideTrailColors = false;
	Vector4 trailHeadColor{1.0f, 1.0f, 1.0f, 1.0f};
	Vector4 trailTailColor{1.0f, 1.0f, 1.0f, 0.0f};
	bool useMagatamaPalette = false;
	bool addCoreTrail = false;
	bool addGlowEmitter = false;
	float repeatHitInterval = 0.0f;
	bool aimAtNearestEnemyOnSpawn = false;
	bool ricochets = false;
	bool usesVerticalHitArea = false;
	bool expiresOutsideView = true;
};

/// <summary>プレイヤー弾の移動方式、追尾、寿命、貫通・再ヒット制御を管理します。</summary>
class PlayerProjectileComponent : public Component {
public:
	void Initialize() override;
	void Update() override;

	void SetAttackName(const std::string& attackName);
	const std::string& GetAttackName() const;
	void SetLevel(const std::string& level);
	const std::string& GetLevel() const;
	void SetDirection(const Vector3& direction);
	const Vector3& GetDirection() const;
	void SetSpeed(float speed);
	float GetSpeed() const;
	void SetAttack(float attack);
	float GetAttack() const;
	void SetSize(float size);
	float GetSize() const;
	void SetHomingEnabled(bool homingEnabled);
	bool IsHomingEnabled() const;
	void SetHomingAccuracy(float homingAccuracy);
	float GetHomingAccuracy() const;
	void SetMotionType(PlayerProjectileMotionType motionType);
	PlayerProjectileMotionType GetMotionType() const;
	PlayerProjectilePresentation GetMotionPresentation() const;
	void SetMotionAnchor(GameObject* motionAnchor);
	void SetOrbitAngleRadians(float angle);
	void SetOrbitRadius(float radius);
	void SetOrbitHeight(float height);
	void SetOrbitAngularSpeed(float speed);
	void SetTravelDistance(float distance);
	void SetTravelOrigin(const Vector3& origin);
	void SetClawSlashIndex(int index);
	void SetClawSlashCount(int count);
	void SetHomingTarget(GameObject* target);
	GameObject* GetHomingTarget() const;
	void SetLifeTime(float lifeTime);
	void Expire();
	bool IsExpired() const;
	void SetPierceCount(int pierceCount);
	int GetPierceCount() const;
	void SetInfinitePierce(bool infinitePierce);
	bool IsInfinitePierce() const;
	void SetRepeatHitInterval(float intervalSeconds);
	float GetRepeatHitInterval() const;
	bool HasHitObject(GameObject* object) const;
	void RegisterHitObject(GameObject* object);

private:
	/// <summary>同じ対象への連続ヒットを抑制する対象別クールダウンです。</summary>
	struct HitRecord {
		GameObject* object = nullptr;
		float cooldownSeconds = -1.0f;
	};

	void UpdateHitCooldowns(float deltaTime);
	void UpdateLinearMotion(GameObject* owner, float deltaTime, float frameScale, bool rampHoming);

	void UpdateBoomerang(GameObject* owner, float deltaTime, float frameScale);

	void UpdateClawSlash(GameObject* owner, float deltaTime);

	std::string attackName_;
	/// <summary>生成元攻撃のレベルです。デバッグ表示や将来の分岐用に保持します。</summary>
	std::string level_ = "1";
	/// <summary>正規化済みの現在進行方向です。</summary>
	Vector3 direction_{0.0f, 0.0f, 1.0f};
	/// <summary>60FPS時の1フレームあたりの移動量です。</summary>
	float speed_ = 0.3f;
	/// <summary>敵へ命中したときに与えるダメージ量です。</summary>
	float attack_ = 100.0f;
	/// <summary>表示と当たり判定に使う基本サイズです。</summary>
	float size_ = 1.0f;
	/// <summary>消滅演出などで一時的に弾の見た目と判定を縮小する倍率です。</summary>
	float visualScaleRate_ = 1.0f;
	/// <summary>残り寿命です。0以下になると削除対象になります。</summary>
	float lifeTime_ = 3.0f;
	/// <summary>生成時の寿命です。爪攻撃などの進行率計算に使用します。</summary>
	float initialLifeTime_ = 3.0f;
	/// <summary>残り貫通回数です。</summary>
	int pierceCount_ = 0;
	/// <summary>trueの場合、貫通回数を消費せず同一対象の再ヒット制御だけ行います。</summary>
	bool infinitePierce_ = false;
	/// <summary>追尾補正を有効にするかを保持します。</summary>
	bool homingEnabled_ = false;
	/// <summary>1フレームごとに目標方向へ寄せる強さです。</summary>
	float homingAccuracy_ = 1.0f;
	/// <summary>勾玉の追尾力を時間で強めるための経過秒数です。</summary>
	float magatamaElapsedSeconds_ = 0.0f;
	/// <summary>直進、周回、ブーメランなどの移動方式です。</summary>
	PlayerProjectileMotionType motionType_ = PlayerProjectileMotionType::Linear;
	/// <summary>周回弾などの中心として使用する非所有参照です。</summary>
	GameObject* motionAnchor_ = nullptr;
	float orbitAngleRadians_ = 0.0f;
	float orbitRadius_ = 2.2f;
	float orbitHeight_ = 0.65f;
	float orbitAngularSpeed_ = 2.2f;
	/// <summary>ブーメランが帰還へ切り替わるまでの直進距離です。</summary>
	float travelDistance_ = 6.0f;
	/// <summary>ブーメランが直進距離を測るための発射位置です。</summary>
	Vector3 travelOrigin_{};
	/// <summary>ブーメランが戻る目標位置です。帰還開始時に固定します。</summary>
	Vector3 returnTarget_{};
	/// <summary>ブーメランが帰還フェーズへ入ったかを表します。</summary>
	bool returning_ = false;
	/// <summary>複数の爪を中央基準で並べるための番号と総数です。</summary>
	int clawSlashIndex_ = 0;
	int clawSlashCount_ = 3;
	/// <summary>爪の位置関係を維持するため、発動時に固定したY軸方向です。</summary>
	float clawSlashYaw_ = 0.0f;
	/// <summary>爪攻撃の基準向きを初回更新で固定済みかを表します。</summary>
	bool isClawSlashYawInitialized_ = false;
	/// <summary>追尾対象となるGameObjectへの非所有参照です。</summary>
	GameObject* homingTarget_ = nullptr;
	/// <summary>同一対象へ再度命中可能になるまでの秒数です。</summary>
	float repeatHitIntervalSeconds_ = 0.0f;
	/// <summary>命中済み対象と残りクールダウンの一覧です。</summary>
	std::vector<HitRecord> hitRecords_;
};
