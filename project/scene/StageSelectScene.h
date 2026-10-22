#pragma once

#include "base_scene/BaseScene.h"

/// <summary>プレイヤー選択後に、ゲームプレイで使用する配置パターンを選択します。</summary>
class StageSelectScene : public BaseScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw2D() override;
	void Draw3D() override {}
	void Finalize() override;
	void SetSceneManager(SceneManager* manager) override { sceneManager_ = manager; }

private:
	void CreateUi();
	std::string MakeStageDescription(const std::string& stageId) const;

	SceneManager* sceneManager_ = nullptr;
	std::vector<std::string> stageIds_;
	int selectedStageIndex_ = 0;
	std::unique_ptr<Sprite> backgroundSprite_;
	std::vector<std::unique_ptr<Sprite>> cardBorderSprites_;
	std::vector<std::unique_ptr<Sprite>> cardSprites_;
	std::unique_ptr<GameObject> titleTextObject_;
	std::unique_ptr<GameObject> selectedStageTextObject_;
	std::unique_ptr<GameObject> instructionTextObject_;
	std::unique_ptr<GameObject> pageTextObject_;
	std::vector<std::unique_ptr<GameObject>> stageTextObjects_;
};
