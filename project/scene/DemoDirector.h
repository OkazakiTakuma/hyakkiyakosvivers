#pragma once

#include <string>

/// <summary>F1デモの開始とリザルトからの再上映を管理します。</summary>
class DemoDirector {
public:
	bool Toggle();
	bool IsActive() const { return active_; }
	bool ShouldStartGameplay(const std::string& currentSceneName, float unscaledDeltaTime);

private:
	bool active_ = false;
	float resultElapsed_ = 0.0f;
	bool restartRequested_ = false;
};
