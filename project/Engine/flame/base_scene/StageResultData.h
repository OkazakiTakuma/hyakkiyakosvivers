#pragma once

#include <string>
#include <vector>

/// <summary>リザルトへ表示する装備1枠分の名前と最終レベルです。</summary>
struct StageResultEquipment {
	/// <summary>画面へ表示する攻撃またはステータス装備の名前です。</summary>
	std::string name;
	/// <summary>通常レベルやSuperなど、終了時の強化状態を表す表示文字列です。</summary>
	std::string level;
};

/// <summary>ゲームプレイ終了時に確定し、リザルトシーンへ渡す戦績です。</summary>
struct StageResultData {
	/// <summary>終了時点で有効だった攻撃装備です。</summary>
	std::vector<StageResultEquipment> attacks;
	/// <summary>終了時点で有効だったステータス装備です。</summary>
	std::vector<StageResultEquipment> statuses;
	/// <summary>今回のゲームプレイ中にプレイヤーが倒した敵の総数です。</summary>
	int defeatedEnemyCount = 0;
	/// <summary>今回のゲームプレイ中にドロップから回収した金額です。</summary>
	int moneyEarned = 0;
	/// <summary>一時停止時間を除いたゲーム進行時間です。</summary>
	float survivalTimeSeconds = 0.0f;
	/// <summary>trueなら最終ボス撃破、falseならプレイヤー死亡による終了です。</summary>
	bool stageCleared = false;
};

