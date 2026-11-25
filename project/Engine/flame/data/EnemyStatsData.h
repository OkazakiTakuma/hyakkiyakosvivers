#pragma once

#include "Vector.h"

#include <string>

/// <summary>敵が使用する移動・攻撃パターンです。</summary>
enum class EnemyBehaviorType {
	Chase,
	Shooter,
	Charger,
	NightSlashBoss,
	SelfDestruct,
	TornadoBoss,
	BurstShooter,
	BarrierBoss
};

/// <summary>敵タイプごとにJSONから読み込む、Component非依存の能力・行動設定です。</summary>
struct EnemyStats {
	float health = 10.0f;
	float attack = 1.0f;
	float speed = 0.05f;
	/// <summary>武器のノックバック段階を相殺する0～3の耐性です。</summary>
	int knockbackResistance = 0;
	float shootingInterval = 1.0f;
	float spawnsPerMinute = 12.0f;
	int experience = 1;
	std::string experienceModelFilePath = "enemy_drop.obj";
	/// <summary>空の場合は行動Strategyが提供する標準モデルを使用します。</summary>
	std::string modelFilePath;
	/// <summary>glTF等のモデル読込時に使用するリソース内ディレクトリです。</summary>
	std::string modelDirectory;
	bool usesAnimatedModel = false;
	/// <summary>モデルのbinに格納された撃破時アニメーションクリップ名です。</summary>
	std::string defeatAnimationName;
	/// <summary>行動Strategyが色を上書きしていない通常時に使用するモデル色です。</summary>
	Vector4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
	bool overridesBaseColor = false;
	/// <summary>敵撃破時に回復アイテムを落とす確率です（0.0～1.0）。</summary>
	float healthItemDropChance = 0.05f;
	/// <summary>敵撃破時に経験値全回収アイテムを落とす確率です（0.0～1.0）。</summary>
	float collectExperienceItemDropChance = 0.05f;
	/// <summary>この敵が落とす回復アイテムのHP回復量です。</summary>
	float healthItemHealAmount = 25.0f;
	/// <summary>撃破時に通常アイテムとは別のボス強化報酬を生成するかを表します。</summary>
	bool dropsBossUpgrade = false;
	EnemyBehaviorType behavior = EnemyBehaviorType::Chase;
	// Shooter専用: preferredDistanceを中心とした許容帯の中で停止し、近すぎる場合は後退する。
	float preferredDistance = 7.0f;
	float distanceTolerance = 1.5f;
	// 敵弾はEnemyShotRequestへコピーされ、シーン側で実体化される。
	float projectileSpeed = 0.12f;
	float projectileSize = 0.22f;
	float projectileLifeTime = 6.0f;
	int burstShotCount = 5;
	float burstSpreadAngle = 0.18f;
	// Charger専用: 接近、予兆、方向固定済みの突進、硬直を構成する調整値。
	float chargeTriggerDistance = 7.0f;
	float chargeDuration = 1.2f;
	float dashSpeed = 0.28f;
	float dashDuration = 0.65f;
	float dashRecovery = 1.0f;
	// NightSlashBoss専用: プレイヤーの左右を交互に横切る連続斬りの調整値。
	float comboTriggerDistance = 8.0f;
	float comboWindup = 0.8f;
	float comboDashSpeed = 0.48f;
	float comboDashDuration = 0.24f;
	float comboSlashPause = 0.16f;
	float comboRecovery = 1.8f;
	float comboSideOffset = 1.8f;
	int comboDashCount = 4;
	// 最終ダッシュだけに掛ける速度倍率。接触ダメージ倍率は実行側で管理する。
	float finisherSpeedMultiplier = 1.35f;
	// NightSlashBoss専用: 連続斬りの間に挟む全方位弾幕・扇状弾幕の共通調整値。
	float bossRangedWindup = 0.75f;
	float bossRangedInterval = 0.32f;
	int bossRangedWaves = 3;
	int bossRadialShotCount = 12;
	int bossAimedShotCount = 5;
	float bossAimedSpreadAngle = 0.22f;
	float bossProjectileAttackMultiplier = 0.65f;
	// TornadoBoss専用: ボス中心から外側へ広がる4方向竜巻。
	int bossTornadoCount = 4;
	float bossTornadoInitialRadius = 1.4f;
	float bossTornadoAngularSpeed = 2.4f;
	float bossTornadoRadialSpeed = 0.035f;
	float bossTornadoSize = 0.65f;
	float bossTornadoLifeTime = 5.0f;
	float bossTornadoAttackMultiplier = 0.8f;
	float bossTornadoTriggerDistance = 10.0f;
	float bossTornadoWindup = 1.0f;
	float bossTornadoRecovery = 2.0f;
	// TornadoBoss専用: プレイヤーを囲んで中心へ狭まる包囲竜巻。
	int bossConvergingTornadoCount = 8;
	float bossConvergingTornadoInitialRadius = 9.0f;
	float bossConvergingTornadoAngularSpeed = -2.0f;
	float bossConvergingTornadoRadialSpeed = 0.035f;
	float bossConvergingTornadoSize = 0.55f;
	float bossConvergingTornadoLifeTime = 4.0f;
	float bossConvergingTornadoAttackMultiplier = 0.55f;
	// TornadoBoss専用: プレイヤーを追跡する単体の巨大竜巻。
	float bossGiantTornadoSpeed = 0.035f;
	float bossGiantTornadoSize = 2.2f;
	float bossGiantTornadoLifeTime = 7.0f;
	float bossGiantTornadoAttackMultiplier = 1.2f;
	float bossGiantTornadoSpawnOffset = 2.2f;
	float selfDestructTriggerDistance = 2.2f;
	float selfDestructFuseDuration = 0.8f;
	float selfDestructRadius = 3.0f;
	/// <summary>生成位置を無視し、プレイヤーが向いている方向の正面へ出現するかを表します。</summary>
	bool selfDestructSpawnInFront = false;
	/// <summary>正面出現時にプレイヤーから離す距離です。</summary>
	float selfDestructSpawnDistance = 4.5f;
	/// <summary>標準敵を1.0とした表示・コライダー・被弾半径の倍率です。</summary>
	float sizeScale = 1.0f;
};
