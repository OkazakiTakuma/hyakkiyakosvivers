#pragma once

#include <string>

/// <summary>自動デモがキャラクター選択画面へ要求できる最小操作です。</summary>
class IDemoPlayerSelectionControl {
public:
	virtual ~IDemoPlayerSelectionControl() = default;
	virtual bool BeginDemoPlayerConfirmation(const std::string& playerTypeName) = 0;
};

/// <summary>自動デモの見どころを画面案内へ渡します。</summary>
class IDemoGameplayControl {
public:
	virtual ~IDemoGameplayControl() = default;
	virtual const char* GetDemoParticleCue() const = 0;
};
