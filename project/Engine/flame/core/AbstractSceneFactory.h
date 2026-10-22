#pragma once
#include"base_scene/BaseScene.h"
#include<string>

class AbstractSceneFactory {
public:

	virtual ~AbstractSceneFactory() = default;

	/// <summary>
	/// Scene を作成し、利用できる状態にします。
	/// </summary>
	/// <param name="sceneName">対象となるシーン名を指定します。</param>
	virtual std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) = 0;
};

