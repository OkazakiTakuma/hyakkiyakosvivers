#pragma once

#include "base_scene/BaseScene.h"

#include <array>

/// <summary>プレイヤー選択後に、ゲームプレイで使用する配置パターンを選択します。</summary>
class StageSelectScene : public BaseScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw2D() override;
	void Draw3D() override {}
	void Finalize() override;

private:
	void CreateUi();
	std::string MakeStageDescription(const std::string& stageId) const;

	std::vector<std::string> stageIds_;
	int selectedStageIndex_ = 0;
	std::unique_ptr<Sprite> backgroundSprite_;
	std::array<std::unique_ptr<Sprite>, 3> frameAccentSprites_;
	std::vector<std::unique_ptr<Sprite>> cardBorderSprites_;
	std::vector<std::unique_ptr<Sprite>> cardSprites_;
	std::vector<std::unique_ptr<Sprite>> cardAccentSprites_;
	std::unique_ptr<Entity> titleTextObject_;
	std::unique_ptr<Entity> selectedStageTextObject_;
	std::unique_ptr<Entity> instructionTextObject_;
	std::unique_ptr<Entity> pageTextObject_;
	std::vector<std::unique_ptr<Entity>> stageTextObjects_;
};
