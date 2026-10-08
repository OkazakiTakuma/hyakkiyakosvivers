#pragma once
#include "Component.h"
#include "GameObject.h"
#include "../../Player/Player.h"
#include <algorithm>
#include <string>

/// <summary>経験値アイテムをプレイヤーへ吸着させ、接触時に経験値を付与します。</summary>
class ExperienceComponent : public Component {
public:
	void Update() override {
		GameObject* owner = GetOwner();
		if (!owner || !target_) {
			return;
		}

		// キャラクターとドロップの原点の高さが異なっても安定して拾えるよう、水平距離で判定する。
		Vector3 toTarget = target_->GetTransform().translate - owner->GetTransform().translate;
		Vector3 horizontalToTarget = toTarget;
		horizontalToTarget.y = 0.0f;
		const float distance = Length(horizontalToTarget);
		if (!isAttracting_ && distance > attractDistance_) {
			return;
		}
		// 一度吸着範囲に入ったら、プレイヤーが移動し続けても回収まで追尾を継続する。
		isAttracting_ = true;
		Player* player = target_->GetComponent<Player>();
		if (distance <= collectDistance_) {
			// ボス強化ドロップはシーン側で装備スロットへ反映するため、ここでは経験値を加算しない。
			if (player) {
				if (!bossUpgradeReward_) {
					player->AddExperience(experience_);
				}
				collected_ = true;
			}
			return;
		}

		// 距離の割合移動では移動中のプレイヤーと等速になるため、一定速度で追いかける。
		// 実効移動速度より必ず速くし、移動入力を続けていても距離を縮める。
		const float frameScale = GameTime::GetFrameScale60();
		const float playerSpeed = player ? player->GetEffectiveMoveSpeed() : 0.0f;
		const float chaseSpeed = (std::max)(attractSpeed_, playerSpeed + kPlayerSpeedMargin);
		const float maxMoveDistance = chaseSpeed * frameScale;
		const float targetDistance = Length(toTarget);
		if (targetDistance <= maxMoveDistance || targetDistance <= MathConstants::kNormalizationEpsilon) {
			owner->GetTransform().translate = target_->GetTransform().translate;
			return;
		}

		owner->GetTransform().translate =
		    owner->GetTransform().translate + (maxMoveDistance / targetDistance) * toTarget;
	}

	void SetExperience(int experience) { experience_ = experience < 0 ? 0 : experience; }
	int GetExperience() const { return experience_; }
	void SetModelFilePath(const std::string& modelFilePath) { modelFilePath_ = modelFilePath; }
	const std::string& GetModelFilePath() const { return modelFilePath_; }
	void SetTarget(GameObject* target) {
		target_ = target;
		isAttracting_ = false;
	}
	GameObject* GetTarget() const { return target_; }
	void SetAttractDistance(float distance) { attractDistance_ = distance < 0.0f ? 0.0f : distance; }
	float GetAttractDistance() const { return attractDistance_; }
	void SetCollectDistance(float distance) { collectDistance_ = distance < 0.0f ? 0.0f : distance; }
	float GetCollectDistance() const { return collectDistance_; }
	void SetAttractSpeed(float speed) { attractSpeed_ = speed < 0.0f ? 0.0f : (speed > 1.0f ? 1.0f : speed); }
	float GetAttractSpeed() const { return attractSpeed_; }
	bool IsCollected() const { return collected_; }
	/// <summary>通常経験値ではなく、ボス撃破時の装備強化報酬として扱うかを設定します。</summary>
	void SetBossUpgradeReward(bool enabled) { bossUpgradeReward_ = enabled; }
	bool IsBossUpgradeReward() const { return bossUpgradeReward_; }
	/// <summary>1個の報酬を拾った際に行う強化抽選回数を設定します。</summary>
	void SetBossUpgradeCount(int count) { bossUpgradeCount_ = count < 1 ? 1 : count; }
	int GetBossUpgradeCount() const { return bossUpgradeCount_; }
	void MarkBossUpgradeApplied() { bossUpgradeApplied_ = true; }
	bool IsBossUpgradeApplied() const { return bossUpgradeApplied_; }
	/// <summary>別の経験値へ集約済みとして、回収対象から除外し削除処理へ渡します。</summary>
	void MarkConsumedByCompression() { consumedByCompression_ = true; }
	bool IsConsumedByCompression() const { return consumedByCompression_; }

private:
	/// <summary>回収時にプレイヤーへ与える経験値です。</summary>
	int experience_ = 1;
	/// <summary>プレイヤーへの吸着を開始する距離です。</summary>
	float attractDistance_ = 5.0f;
	/// <summary>回収完了と判定する距離です。</summary>
	float collectDistance_ = 0.6f;
	/// <summary>60 FPS時の1フレームに進む最低距離です。</summary>
	float attractSpeed_ = 0.14f;
	/// <summary>移動中のプレイヤーへ追いつくために加える速度差です。</summary>
	static constexpr float kPlayerSpeedMargin = 0.12f;
	std::string modelFilePath_ = "enemy_drop.obj";
	/// <summary>吸着対象となるGameObjectへの非所有参照です。</summary>
	GameObject* target_ = nullptr;
	/// <summary>通常の接触回収が完了したかを表します。</summary>
	bool collected_ = false;
	/// <summary>一度吸着範囲に入り、回収まで追尾を継続する状態かを表します。</summary>
	bool isAttracting_ = false;
	/// <summary>経験値加算を行わず、シーン側のボス報酬処理へ渡すための識別フラグです。</summary>
	bool bossUpgradeReward_ = false;
	/// <summary>報酬オブジェクトの個数ではなく、この1個に内包される強化回数です。</summary>
	int bossUpgradeCount_ = 1;
	/// <summary>回収後に同じ報酬を複数フレーム適用しないための完了フラグです。</summary>
	bool bossUpgradeApplied_ = false;
	/// <summary>圧縮処理によって消費され、通常回収から除外されたかを表します。</summary>
	bool consumedByCompression_ = false;
};
