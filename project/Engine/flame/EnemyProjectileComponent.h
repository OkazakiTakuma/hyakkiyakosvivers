#pragma once

#include "Component.h"
#include "GameObject.h"
#include "MathConstants.h"
#include "../base/GameTime.h"
#include <algorithm>
#include <cmath>

/// <summary>敵弾が使用する移動軌道です。</summary>
enum class EnemyProjectileMotionType {
	/// <summary>生成時に決めた方向へ直進します。</summary>
	Linear,
	/// <summary>固定中心の周囲を回りながら半径を広げます。</summary>
	ExpandingOrbit,
	/// <summary>固定中心の周囲を回りながら半径を狭めます。</summary>
	ContractingOrbit,
	/// <summary>対象の現在位置へ向きを更新しながら追跡します。</summary>
	Homing
};

/// <summary>敵弾の移動、攻撃力、寿命、命中状態を管理します。</summary>
class EnemyProjectileComponent : public Component {
public:
	void Initialize() override;
	void Update() override;

	void SetDirection(const Vector3& direction);
	const Vector3& GetDirection() const;
	/// <summary>このフレームの移動前に弾がいたワールド座標を返します。</summary>
	const Vector3& GetPreviousPosition() const;
	void SetMotionType(EnemyProjectileMotionType motionType);
	EnemyProjectileMotionType GetMotionType() const;
	void SetExpandingOrbit(
		const Vector3& center, float angle, float initialRadius, float angularSpeed, float radialSpeed, float height);
	/// <summary>指定した中心へ収束する螺旋軌道を設定します。</summary>
	void SetContractingOrbit(
		const Vector3& center, float angle, float initialRadius, float angularSpeed, float radialSpeed, float height);
	/// <summary>巨大竜巻が追跡する対象を設定します。対象の所有権は保持しません。</summary>
	void SetHomingTarget(GameObject* target);
	void SetSpeed(float speed);
	void SetAttack(float attack);
	float GetAttack() const;
	void SetSize(float size);
	float GetSize() const;
	void SetLifeTime(float lifeTime);
	bool IsExpired() const;
	void MarkHit();

private:
	/// <summary>正規化された弾の進行方向です。</summary>
	Vector3 direction_{0.0f, 0.0f, 1.0f};
	/// <summary>高速移動時のすり抜けを防ぐ連続判定に使用する直前座標です。</summary>
	Vector3 previousPosition_{};
	/// <summary>通常弾と3種類の竜巻軌道を切り替える移動方式です。</summary>
	EnemyProjectileMotionType motionType_ = EnemyProjectileMotionType::Linear;
	/// <summary>螺旋軌道の生成時に固定するワールド座標中心です。</summary>
	Vector3 orbitCenter_{};
	float orbitAngle_ = 0.0f;
	float orbitRadius_ = 0.0f;
	float orbitAngularSpeed_ = 0.0f;
	float orbitRadialSpeed_ = 0.0f;
	float orbitHeight_ = 0.0f;
	/// <summary>追尾対象への非所有参照です。</summary>
	GameObject* homingTarget_ = nullptr;
	/// <summary>60FPS時の1フレームあたりの移動量です。</summary>
	float speed_ = 0.12f;
	/// <summary>プレイヤーへ与えるダメージ量です。</summary>
	float attack_ = 1.0f;
	/// <summary>表示と衝突判定に使用する弾の大きさです。</summary>
	float size_ = 0.22f;
	/// <summary>未命中でも弾を破棄するまでの秒数です。</summary>
	float lifeTime_ = 6.0f;
	/// <summary>生成後の経過秒数です。</summary>
	float elapsedTime_ = 0.0f;
	/// <summary>衝突処理済みかを表します。</summary>
	bool hit_ = false;
};
