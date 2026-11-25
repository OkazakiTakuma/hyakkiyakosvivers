#include "base_scene/runtime/GameplaySession.h"

#include "GameTime.h"

#include <algorithm>

void GameplaySession::Reset() {
	// 再読み込み前のポーズ状態や戦績を次の挑戦へ持ち越さない。
	mode_ = Mode::Playing;
	isStageCleared_ = false;
	defeatedEnemyCount_ = 0;
	challengeMoneyEarned_ = 0;
	survivalTimeSeconds_ = 0.0f;
	GameTime::SetPaused(false);
}

void GameplaySession::SetMode(Mode mode) {
	mode_ = mode;
	// Playing以外では戦闘時間とComponent更新に使うDeltaTimeを停止する。
	const bool shouldPause = mode == Mode::Intro ||
		mode == Mode::Paused ||
		mode == Mode::LevelUpSelection ||
		mode == Mode::BossRewardPresentation ||
		mode == Mode::BossReward ||
		mode == Mode::Finished;
	GameTime::SetPaused(shouldPause);
}

void GameplaySession::AdvanceTime(float deltaTime) {
	// 呼び出し元の異常値でリザルト時間が巻き戻らないよう負値を無視する。
	survivalTimeSeconds_ += (std::max)(0.0f, deltaTime);
}

void GameplaySession::AddChallengeMoney(int amount) {
	challengeMoneyEarned_ += (std::max)(0, amount);
}

StageResultData GameplaySession::MakeResultData() const {
	// 内部状態への参照を渡さず、SceneManagerが保持できる値へコピーする。
	StageResultData result;
	result.defeatedEnemyCount = defeatedEnemyCount_;
	result.moneyEarned = challengeMoneyEarned_;
	result.survivalTimeSeconds = survivalTimeSeconds_;
	result.stageCleared = isStageCleared_;
	return result;
}
