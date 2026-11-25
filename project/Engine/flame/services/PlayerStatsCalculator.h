#pragma once

#include "data/PlayerStatsData.h"
#include "data/PlayerStatusItemDefinition.h"

/// <summary>
/// 基礎能力とロード済み強化定義だけから実効能力を計算します。
/// ファイル、Component、描画へ依存しないため、単体で結果を検証できます。
/// </summary>
PlayerStats ApplyPlayerStatusItems(
	const PlayerStats& baseStats,
	const PlayerStatusItemDefinitions& definitions);
