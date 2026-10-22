#pragma once
#include "core/AbstractSceneFactory.h"
#include "camera/Camera.h"
#include "core/FlameWork.h"
#include "core/GameObject.h"
#include "SceneManager.h"
#include <memory>

class Game : public FlameWork {
public:
	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	void Initialize() override;
	/// <summary>
	/// 毎フレームの状態更新を行います。
	/// </summary>
	void Update() override;
	/// <summary>
	/// 現在の状態をもとに描画処理を行います。
	/// </summary>
	void Draw() override;
	/// <summary>
	/// 確保したリソースを解放し、終了処理を行います。
	/// </summary>
	void Finalize() override;
	bool IsEndRequest() const override { return endRequest; }	

private:
	std::unique_ptr<Camera> camera = nullptr;
	bool endRequest = false;
	std::unique_ptr<AbstractSceneFactory> sceneFactory = nullptr;
	std::unique_ptr<SceneManager> sceneManager = nullptr;
	std::unique_ptr<GameObject> recordingIndicator_ = nullptr;

};

