#include "DemoDirector.h"

bool DemoDirector::Toggle() {
	active_ = !active_;
	restartRequested_ = active_;
	resultElapsed_ = 0.0f;
	return active_;
}

bool DemoDirector::ShouldStartGameplay(
	const std::string& currentSceneName, float unscaledDeltaTime) {
	if (!active_) return false;

	if (restartRequested_) {
		restartRequested_ = false;
		return true;
	}
	if (currentSceneName != "RESULT") return false;

	resultElapsed_ += unscaledDeltaTime;
	if (resultElapsed_ < 4.0f) return false;
	resultElapsed_ = 0.0f;
	return true;
}
