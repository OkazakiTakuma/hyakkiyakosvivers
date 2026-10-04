#pragma once

#include <chrono>

/// <summary>フレーム間隔と一時停止状態をアプリケーション全体で共有します。</summary>
class GameTime final {
public:
	/// <summary>計測基準時刻と初期フレーム時間を設定します。</summary>
	static void Initialize() {
		previousTime_ = Clock::now();
		deltaTime_ = 1.0f / kReferenceFrameRate;
		isInitialized_ = true;
	}

	/// <summary>前フレームからの経過時間を計測します。</summary>
	static void Update() {
		const Clock::time_point now = Clock::now();
		if (!isInitialized_) {
			Initialize();
			return;
		}

		const float elapsed = std::chrono::duration<float>(now - previousTime_).count();
		previousTime_ = now;
		if (elapsed > 0.0f) {
			// デバッガ停止やウィンドウ操作後に、ゲーム時間が一度に進みすぎるのを防ぐ。
			deltaTime_ = elapsed > kMaxDeltaTime ? kMaxDeltaTime : elapsed;
		}
	}

	static void SetPaused(bool paused) { isPaused_ = paused; }
	static bool IsPaused() { return isPaused_; }
	static float GetDeltaTime() { return isPaused_ ? 0.0f : deltaTime_; }
	static float GetFrameRate() { return deltaTime_ > 0.0f ? 1.0f / deltaTime_ : 0.0f; }
	static float GetFrameScale60() { return GetDeltaTime() * kReferenceFrameRate; }

private:
	static constexpr float kReferenceFrameRate = 60.0f;
	static constexpr float kMaxDeltaTime = 0.25f;
	/// <summary>システム時刻の変更に影響されない経過時間計測用クロックです。</summary>
	using Clock = std::chrono::steady_clock;
	/// <summary>直前のUpdateを実行した時刻です。</summary>
	static inline Clock::time_point previousTime_{};
	/// <summary>前フレームからの経過秒数です。</summary>
	static inline float deltaTime_ = 1.0f / kReferenceFrameRate;
	/// <summary>初回計測が完了しているかを表します。</summary>
	static inline bool isInitialized_ = false;
	/// <summary>ゲーム内時間を停止しているかを表します。</summary>
	static inline bool isPaused_ = false;
};
