#pragma once

#include <array>
#include <cstddef>
#include <string>

/// <summary>攻撃装備とステータス装備で共通して使用する最大スロット数です。</summary>
inline constexpr std::size_t kPlayerEquipmentSlotCount = 3;

/// <summary>プレイヤーが装備している攻撃スロット1枠分の設定です。</summary>
struct PlayerAttackSlot {
	/// <summary>このスロットを実行するかどうかです。</summary>
	bool enabled = false;
	/// <summary>攻撃定義の名前と、使用する1～5またはsuperのレベルです。</summary>
	std::string attackName;
	std::string attackLevel = "1";
};

/// <summary>プレイヤーへ加算適用するステータス強化スロット1枠分の設定です。</summary>
struct PlayerStatusSlot {
	/// <summary>このスロットの能力補正を適用するかどうかです。</summary>
	bool enabled = false;
	/// <summary>強化アイテム定義の名前と、使用する1～5のレベルです。</summary>
	std::string statusName;
	std::string level = "1";
};

/// <summary>プレイヤータイプごとの基礎能力、装備、保存対象の状態です。</summary>
struct PlayerStats {
	/// <summary>プレイヤータイプを識別する名前です。</summary>
	std::string name = "Default";
	/// <summary>最大HPの基準値と、100を等倍とするHP倍率です。</summary>
	float baseHealth = 100.0f;
	float health = 100.0f;
	/// <summary>100を等倍とする攻撃力倍率です。</summary>
	float attack = 100.0f;
	/// <summary>1増えるごとに被ダメージを1%軽減する防御値です。</summary>
	float defense = 0.0f;
	/// <summary>移動速度の基準値と、100を等倍とする速度倍率です。</summary>
	float baseSpeed = 0.1f;
	float speed = 100.0f;
	/// <summary>攻撃間隔と弾サイズへ適用する倍率です。</summary>
	float attackSpeed = 100.0f;
	float attackSize = 100.0f;
	/// <summary>表示モデルとコライダーへ適用する一様倍率です。</summary>
	float sizeScale = 1.0f;
	/// <summary>被ダメージ後に再度ダメージを受けない秒数です。</summary>
	float damageInvincibilityDuration = 1.0f;
	/// <summary>現在のレベルと累積経験値です。</summary>
	int level = 1;
	int experience = 0;
	/// <summary>100を等倍とする獲得経験値倍率です。</summary>
	float experienceCorrection = 100.0f;
	/// <summary>表示モデルとアニメーションモデル判定です。</summary>
	std::string modelFilePath;
	std::string modelDirectory = "/";
	bool isAnimationModel = false;
	/// <summary>右手へ追従表示する装備モデルと、その読込ディレクトリです。空なら装備を表示しません。</summary>
	std::string rightHandEquipmentModelFilePath;
	std::string rightHandEquipmentModelDirectory = "/";
	/// <summary>各画面の演出で再生するアニメーションクリップ名です。</summary>
	std::string gameplayStartAnimationName;
	std::string gameplayDefeatAnimationName;
	std::string selectionEntranceAnimationName;
	/// <summary>開始・選択演出の動き方を選ぶIDと、選択演出の再生時間です。</summary>
	std::string entrancePresentationStyle = "pop";
	float selectionEntranceDuration = 1.2f;
	std::array<PlayerAttackSlot, kPlayerEquipmentSlotCount> attackSlots{};
	std::array<PlayerStatusSlot, kPlayerEquipmentSlotCount> statusSlots{};
};
