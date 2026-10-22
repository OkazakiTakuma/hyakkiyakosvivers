#pragma once
#include "core/AbstractSceneFactory.h"

class SceneFactory : public AbstractSceneFactory {
public:
	/// <summary>
	/// Scene を作成し、利用できる状態にします。
	/// </summary>
	/// <param name="sceneName">対象となるシーン名を指定します。</param>
	std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) override;
};
