#pragma once

#include "base_scene/StageResultData.h"

/// <summary>
/// 1回のゲームプレイに属する進行状態を所有します。
/// シーン描画やEntity管理から独立させ、戦績と一時停止理由の正本を一か所に保ちます。
/// </summary>
class GameplaySession final {
public:
	/// <summary>入力受付先とゲーム時間の停止理由を表します。</summary>
	enum class Mode {
		Playing,
		Intro,
		Paused,
		LevelUpSelection,
		BossRewardPresentation,
		BossReward,
		Finished
	};

	/// <summary>新しい挑戦を開始できる初期状態へ戻します。</summary>
	void Reset();
	/// <summary>進行モードを変更し、GameTimeの一時停止状態も同時に更新します。</summary>
	void SetMode(Mode mode);
	Mode GetMode() const { return mode_; }
	bool IsSelectionActive() const {
		return mode_ == Mode::LevelUpSelection || mode_ == Mode::BossRewardPresentation || mode_ == Mode::BossReward;
	}

	/// <summary>リザルト表示用の生存時間を、負値を除外して加算します。</summary>
	void AdvanceTime(float deltaTime);
	/// <summary>撃破数を1体分加算します。</summary>
	void RecordEnemyDefeat() { ++defeatedEnemyCount_; }
	/// <summary>今回の挑戦で獲得したGを、負値を除外して加算します。</summary>
	void AddChallengeMoney(int amount);
	/// <summary>ステージクリア条件を満たしたことを記録します。</summary>
	void MarkStageCleared() { isStageCleared_ = true; }

	bool IsStageCleared() const { return isStageCleared_; }
	int GetChallengeMoney() const { return challengeMoneyEarned_; }
	/// <summary>現在の戦績を、シーン破棄後も保持できる所有型へ変換します。</summary>
	StageResultData MakeResultData() const;

private:
	// リザルトに必要な値の正本をこのクラスへ集約し、BaseSceneとの二重管理を防ぐ。
	Mode mode_ = Mode::Playing;
	bool isStageCleared_ = false;
	int defeatedEnemyCount_ = 0;
	int challengeMoneyEarned_ = 0;
	float survivalTimeSeconds_ = 0.0f;
};
