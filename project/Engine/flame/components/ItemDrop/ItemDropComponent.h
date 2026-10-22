#pragma once
#include "core/Component.h"
#include "core/GameObject.h"
#include "../../../../Player/Player.h"

/// <summary>敵が落とす特殊アイテムの種類です。</summary>
enum class ItemDropType {
	/// <summary>プレイヤーのHPを指定量回復します。</summary>
	Health,
	/// <summary>フィールド上の通常経験値をすべてプレイヤーへ吸着させます。</summary>
	CollectAllExperience,
	/// <summary>回収時に今回の挑戦の獲得金へ加算されるコインです。</summary>
	Money
};

/// <summary>特殊アイテムの接触回収と、プレイヤーへ直接作用する効果を管理します。</summary>
class ItemDropComponent : public Component {
public:
	/// <summary>プレイヤーとの距離に応じて待機、吸着、回収を切り替えます。</summary>
	void Update() override {
		GameObject* owner = GetOwner();
		if (!owner || !target_ || collected_) {
			return;
		}

		// キャラクターとアイテムの原点の高さが異なっても拾えるよう、水平距離で判定する。
		Vector3 toTarget = target_->GetTransform().translate - owner->GetTransform().translate;
		toTarget.y = 0.0f;
		const float distance = Length(toTarget);
		if (distance > attractDistance_) {
			return;
		}
		if (distance > collectDistance_) {
			// 回収範囲へ入ったアイテムをプレイヤーの足元へ吸着させる。
			owner->GetTransform().translate = owner->GetTransform().translate + attractSpeed_ * toTarget;
			return;
		}

		Player* player = target_->GetComponent<Player>();
		if (!player) {
			return;
		}
		// 回復はコンポーネント内で完結する。経験値全回収とコインはシーン全体を参照するため、
		// collected_だけを立ててBaseScene::UpdateItemDropsへ効果適用を委譲する。
		if (type_ == ItemDropType::Health) {
			player->SetCurrentHealth(player->GetCurrentHealth() + healAmount_);
			effectApplied_ = true;
		}
		collected_ = true;
	}

	void SetType(ItemDropType type) { type_ = type; }
	ItemDropType GetType() const { return type_; }
	void SetTarget(GameObject* target) { target_ = target; }
	GameObject* GetTarget() const { return target_; }
	void SetHealAmount(float amount) { healAmount_ = amount < 0.0f ? 0.0f : amount; }
	float GetHealAmount() const { return healAmount_; }
	void SetMoneyAmount(int amount) { moneyAmount_ = amount < 0 ? 0 : amount; }
	int GetMoneyAmount() const { return moneyAmount_; }
	void SetAttractDistance(float distance) { attractDistance_ = distance < 0.0f ? 0.0f : distance; }
	float GetAttractDistance() const { return attractDistance_; }
	void SetAttractSpeed(float speed) { attractSpeed_ = speed < 0.0f ? 0.0f : (speed > 1.0f ? 1.0f : speed); }
	float GetAttractSpeed() const { return attractSpeed_; }
	void SetCollectDistance(float distance) { collectDistance_ = distance < 0.0f ? 0.0f : distance; }
	float GetCollectDistance() const { return collectDistance_; }
	bool IsCollected() const { return collected_; }
	bool IsEffectApplied() const { return effectApplied_; }
	void MarkEffectApplied() { effectApplied_ = true; }

private:
	/// <summary>回収時に発動する効果の種類です。</summary>
	ItemDropType type_ = ItemDropType::Health;
	/// <summary>吸着および回収対象となるプレイヤーへの非所有参照です。</summary>
	GameObject* target_ = nullptr;
	/// <summary>Healthタイプを回収した際のHP回復量です。</summary>
	float healAmount_ = 25.0f;
	/// <summary>Moneyタイプを回収した時に加算する金額です。</summary>
	int moneyAmount_ = 0;
	/// <summary>プレイヤーへの吸着を開始する水平距離です。</summary>
	float attractDistance_ = 3.0f;
	/// <summary>1フレームでプレイヤーまで進む割合です。</summary>
	float attractSpeed_ = 0.12f;
	/// <summary>アイテムを取得済みと判定する水平距離です。</summary>
	float collectDistance_ = 0.65f;
	/// <summary>プレイヤーとの接触による回収が完了したかを示します。</summary>
	bool collected_ = false;
	/// <summary>回収後の効果が適用済みで、安全に削除できるかを示します。</summary>
	bool effectApplied_ = false;
};

