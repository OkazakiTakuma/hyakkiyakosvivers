#pragma once

#include "Vector.h"

#include <string>
#include <unordered_map>
#include <vector>

/// <summary>攻撃1種類の特定レベルにおける発射・威力設定です。</summary>
struct PlayerAttackLevelStats {
	/// <summary>JSON内で識別する1～5またはsuperのレベル名です。</summary>
	std::string level = "1";
	/// <summary>レベルアップ選択画面へ表示する説明と画像です。</summary>
	std::string choiceDescription;
	std::string choiceTextureFilePath;
	/// <summary>プレイヤー倍率を適用する前の基礎攻撃力です。</summary>
	float attack = 100.0f;
	/// <summary>移動量または特殊攻撃の速度パラメーターです。</summary>
	float speed = 0.3f;
	/// <summary>100を等倍とする弾の表示・判定サイズです。</summary>
	float size = 100.0f;
	/// <summary>1回の発射で生成する弾数と、弾ごとの角度・ローカル生成位置です。</summary>
	int shotCount = 1;
	std::vector<float> angles = {0.0f};
	std::vector<Vector3> spawnOffsets = {{0.0f, 0.5f, 1.2f}};
	/// <summary>弾の表示モデルと追尾設定です。</summary>
	std::string modelFilePath = "sphere.obj";
	bool homing = false;
	float homingAccuracy = 1.0f;
	/// <summary>再発射、同一対象への再命中、弾の生存時間です。</summary>
	float attackInterval = 0.5f;
	float repeatHitInterval = 0.0f;
	float lifeTime = 3.0f;
	/// <summary>特殊軌道の距離と、命中後に弾を残すための設定です。</summary>
	float travelDistance = 6.0f;
	int pierceCount = 0;
	int knockback = 0;
	bool infinitePierce = false;
};

/// <summary>攻撃名と、選択可能なレベル設定のまとまりです。</summary>
struct PlayerAttackStats {
	/// <summary>JSONキーやセーブデータで使用する、変更しない内部攻撃名です。</summary>
	std::string name = "Straight";
	/// <summary>通常武器と進化武器の画面表示名です。未設定時は内部攻撃名へフォールバックします。</summary>
	std::string displayName;
	std::string superDisplayName;
	const std::string& GetDisplayName(bool isSuper = false) const {
		if (isSuper && !superDisplayName.empty()) return superDisplayName;
		return displayName.empty() ? name : displayName;
	}
	/// <summary>表示名やJSONキーから独立して、発射Strategyを選択する安定IDです。</summary>
	std::string patternId = "default";
	/// <summary>攻撃名から独立して、生成する弾の表示方式を選択する安定IDです。</summary>
	std::string visualId = "model";
	/// <summary>通常レベルで共通使用する選択画面画像です。</summary>
	std::string choiceTextureFilePath;
	/// <summary>super解放に必要なステータスアイテム名とレベルです。</summary>
	std::string superConditionStatusName;
	std::string superConditionStatusLevel = "1";
	/// <summary>この攻撃で選択可能な全レベル定義です。</summary>
	std::vector<PlayerAttackLevelStats> levels;
};

/// <summary>攻撃名からロード済み定義を検索するためのコレクションです。</summary>
using PlayerAttackDefinitions = std::unordered_map<std::string, PlayerAttackStats>;
