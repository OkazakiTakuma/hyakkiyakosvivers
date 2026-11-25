#include "StageSelectScene.h"

#include "SceneManager.h"
#include "helpers/SceneUiUtility.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <Xinput.h>

namespace {
constexpr int kCardsPerRow = 3;
constexpr int kRowsPerPage = 2;
constexpr int kCardsPerPage = kCardsPerRow * kRowsPerPage;
constexpr float kReturnToTitleTransitionSeconds = 1.65f;
constexpr const char* kStageCardTexturePath = "Resources/UI/World/stage_ema_card.png";
using SceneUiUtility::CreateColorSprite;
using SceneUiUtility::CreateTextObject;
using SceneUiUtility::DrawColorSprite;
}

void StageSelectScene::Initialize() {
	selectedStageIndex_ = 0;
	if (sceneManager_) {
		sceneManager_->RefreshGameplayStagePatterns();
		// demo はF1自動デモとエディター検証専用のため、通常の選択肢には含めない。
		stageIds_ = sceneManager_->GetSelectableGameplayStageIds();
		const auto found = std::find(stageIds_.begin(), stageIds_.end(), sceneManager_->GetSelectedGameplayStageId());
		if (found != stageIds_.end()) {
			selectedStageIndex_ = static_cast<int>(std::distance(stageIds_.begin(), found));
		}
		// 前回選択したステージが現在ロック中なら、安全な初期選択であるdefaultへ戻す。
		if (!stageIds_.empty() && !sceneManager_->IsGameplayStageUnlocked(stageIds_[selectedStageIndex_])) {
			selectedStageIndex_ = 0;
		}
	}
	CreateUi();
}

void StageSelectScene::CreateUi() {
	backgroundSprite_ = CreateColorSprite();
	for (auto& sprite : frameAccentSprites_) sprite = CreateColorSprite();
	cardBorderSprites_.clear();
	cardSprites_.clear();
	cardAccentSprites_.clear();
	stageTextObjects_.clear();
	titleTextObject_ = SceneUiUtility::CreateHeadingText("御境選び", 46.0f, {0.98f, 0.84f, 0.53f, 1.0f}, DWRITE_FONT_WEIGHT_EXTRA_BOLD);
	selectedStageTextObject_ = CreateTextObject("", 22.0f);
	instructionTextObject_ = CreateTextObject(
	    "A / D・左右キー: 境を巡る    Space / Enter・Pad A: 出立    Q・Pad B: 戻る", 18.0f);
	pageTextObject_ = CreateTextObject("", 18.0f);
	for (const std::string& stageId : stageIds_) {
		cardBorderSprites_.push_back(CreateColorSprite());
		auto cardSprite = std::make_unique<Sprite>();
		cardSprite->Initialize(kStageCardTexturePath);
		cardSprites_.push_back(std::move(cardSprite));
		cardAccentSprites_.push_back(CreateColorSprite());
		stageTextObjects_.push_back(CreateTextObject(MakeStageDescription(stageId), 22.0f));
	}
	// 末尾の1枚は草原プレビューを暗くする専用オーバーレイ。カード用の添字には含めない。
	cardBorderSprites_.push_back(CreateColorSprite());
}

std::string StageSelectScene::MakeStageDescription(const std::string& stageId) const {
	std::string displayName = sceneManager_ ? sceneManager_->GetGameplayStageDisplayName(stageId) : stageId;
	if (stageId != "default" && stageId != "stage2") {
		std::transform(displayName.begin(), displayName.end(), displayName.begin(), [](unsigned char character) {
			return static_cast<char>(std::toupper(character));
		});
	}
	if (sceneManager_ && !sceneManager_->IsGameplayStageUnlocked(stageId)) {
		// ImGuiで設定した前提ステージ名をカードへ表示し、開放方法をプレイヤーへ伝える。
		std::string prerequisite = sceneManager_->GetGameplayStageUnlockPrerequisite(stageId);
		const std::string prerequisiteId = prerequisite;
		prerequisite = sceneManager_->GetGameplayStageDisplayName(prerequisite);
		if (prerequisiteId != "default" && prerequisiteId != "stage2") {
			std::transform(prerequisite.begin(), prerequisite.end(), prerequisite.begin(), [](unsigned char character) {
				return static_cast<char>(std::toupper(character));
			});
		}
		return displayName + "\n\nLOCKED\n" + prerequisite + "クリアで開放";
	}
	if (stageId == "default") {
		return displayName + "\n\n逢魔が時の草原\n夕陽の下、四方へ続く妖の原";
	}
	if (stageId == "stage2") {
		return displayName + "\n\n妖の古道\n塀に囲まれ、東西へ巡る夜道";
	}
	return displayName + "\n\nカスタムステージ\n配置パターン: " + stageId;
}

void StageSelectScene::Update() {
	if (stageIds_.empty() || !sceneManager_) {
		return;
	}
	Input* input = Input::GetInstance();
	const int stageCount = static_cast<int>(stageIds_.size());
	if (input->TriggerKey(DIK_A) || input->TriggerKey(DIK_LEFT) || input->TriggerGamepadLeft()) {
		selectedStageIndex_ = (selectedStageIndex_ + stageCount - 1) % stageCount;
	}
	if (input->TriggerKey(DIK_D) || input->TriggerKey(DIK_RIGHT) || input->TriggerGamepadRight()) {
		selectedStageIndex_ = (selectedStageIndex_ + 1) % stageCount;
	}
	if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) || input->TriggerGamepadButton(XINPUT_GAMEPAD_A)) {
		const std::string& selectedStageId = stageIds_[selectedStageIndex_];
		// ロック中カードも内容確認のため選択はできるが、次のキャラクター選択への決定だけを拒否する。
		if (sceneManager_->IsGameplayStageUnlocked(selectedStageId)) {
			sceneManager_->SetSelectedGameplayStageId(selectedStageId);
			sceneManager_->ChangeScene("PLAYER_SELECT");
		}
		return;
	}
	if (input->TriggerKey(DIK_Q) || input->TriggerGamepadButton(XINPUT_GAMEPAD_B)) {
		sceneManager_->ChangeScene(
			"TITLE", SceneTransitionType::FadeWhite,
			kReturnToTitleTransitionSeconds, kReturnToTitleTransitionSeconds);
	}
}

void StageSelectScene::Draw2D() {
	DirectXCommon* dxCommon = SpriteCommon::GetInstance()->GetDxCommon();
	if (!dxCommon || !backgroundSprite_) {
		return;
	}
	const float screenWidth = static_cast<float>(dxCommon->GetRenderWidth());
	const float screenHeight = static_cast<float>(dxCommon->GetRenderHeight());
	const int stageCount = static_cast<int>(stageIds_.size());
	const int pageCount = (std::max)(1, (stageCount + kCardsPerPage - 1) / kCardsPerPage);
	const int currentPage = selectedStageIndex_ / kCardsPerPage;
	const int pageStart = currentPage * kCardsPerPage;
	const int pageEnd = (std::min)(pageStart + kCardsPerPage, stageCount);
	const float sideMargin = 70.0f;
	const float horizontalGap = 22.0f;
	const float verticalGap = 18.0f;
	const float availableWidth = screenWidth - sideMargin * 2.0f;
	const float cardWidth = (std::min)(300.0f, (availableWidth - horizontalGap * 2.0f) / 3.0f);
	// 横長の絵馬テクスチャが縦へ間延びしない高さを上限にする。
	const float cardHeight = (std::min)(168.0f, (screenHeight - 245.0f - verticalGap) / 2.0f);
	const float firstRowY = 145.0f;

	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	const bool isGrasslandSelected =
		selectedStageIndex_ >= 0 && selectedStageIndex_ < stageCount && stageIds_[selectedStageIndex_] == "default";
	if (isGrasslandSelected) {
		if (backgroundSprite_->GetTextureFilePath() != "Resources/terrain/grass.png") {
			backgroundSprite_->SetTexture("Resources/terrain/grass.png");
		}
		DrawColorSprite(backgroundSprite_.get(), 0.0f, 0.0f, screenWidth, screenHeight, {0.94f, 0.55f, 0.28f, 1.0f});
		// ステージ1の低い夕陽に合わせ、草原へ赤紫の薄暮レイヤーを重ねる。
		DrawColorSprite(cardBorderSprites_.back().get(), 0.0f, 0.0f, screenWidth, screenHeight, {0.12f, 0.025f, 0.08f, 0.48f});
	} else {
		if (backgroundSprite_->GetTextureFilePath() != "Resources/human/white.png") {
			backgroundSprite_->SetTexture("Resources/human/white.png");
		}
		DrawColorSprite(backgroundSprite_.get(), 0.0f, 0.0f, screenWidth, screenHeight, {0.02f, 0.04f, 0.075f, 1.0f});
	}
	// 上下の朱線と金の細線で画面全体を神社の扁額のように縁取る。
	DrawColorSprite(frameAccentSprites_[0].get(), 0.0f, 0.0f, screenWidth, 5.0f, {0.68f, 0.07f, 0.035f, 1.0f});
	DrawColorSprite(frameAccentSprites_[1].get(), 0.0f, screenHeight - 5.0f, screenWidth, 5.0f, {0.68f, 0.07f, 0.035f, 1.0f});
	DrawColorSprite(frameAccentSprites_[2].get(), screenWidth * 0.5f - 150.0f, 91.0f, 300.0f, 2.0f,
		{0.92f, 0.68f, 0.24f, 0.9f});
	for (int row = 0; row < kRowsPerPage; ++row) {
		const int rowStart = pageStart + row * kCardsPerRow;
		const int rowEnd = (std::min)(rowStart + kCardsPerRow, pageEnd);
		const int rowCount = rowEnd - rowStart;
		if (rowCount <= 0) {
			continue;
		}
		const float rowWidth = cardWidth * rowCount + horizontalGap * (rowCount - 1);
		const float startX = (screenWidth - rowWidth) * 0.5f;
		const float cardY = firstRowY + row * (cardHeight + verticalGap);
		for (int column = 0; column < rowCount; ++column) {
			const int index = rowStart + column;
			const float cardX = startX + column * (cardWidth + horizontalGap);
			const bool selected = index == selectedStageIndex_;
			const bool unlocked = !sceneManager_ || sceneManager_->IsGameplayStageUnlocked(stageIds_[index]);
			// ロック中は選択色より暗色を優先し、操作不能であることをカード全体で表現する。
			DrawColorSprite(cardBorderSprites_[index].get(), cardX - 5.0f, cardY - 5.0f, cardWidth + 10.0f, cardHeight + 10.0f,
			    !unlocked ? Vector4{0.20f, 0.19f, 0.19f, 1.0f} :
			    selected ? Vector4{0.96f, 0.70f, 0.22f, 1.0f} : Vector4{0.34f, 0.19f, 0.13f, 1.0f});
			DrawColorSprite(cardSprites_[index].get(), cardX, cardY, cardWidth, cardHeight,
			    !unlocked ? Vector4{0.30f, 0.30f, 0.30f, 0.94f} :
			    selected ? Vector4{1.0f, 1.0f, 1.0f, 1.0f} : Vector4{0.82f, 0.82f, 0.80f, 0.98f});
			// 選択中だけ絵馬の下端に金の光を添え、生成素材の朱線と競合させない。
			DrawColorSprite(cardAccentSprites_[index].get(), cardX + 20.0f, cardY + cardHeight - 5.0f,
				cardWidth - 40.0f, 3.0f, selected && unlocked
					? Vector4{1.0f, 0.72f, 0.20f, 0.95f} : Vector4{0.0f, 0.0f, 0.0f, 0.0f});
			Entity* textObject = stageTextObjects_[index].get();
			textObject->GetTransform().translate = {cardX + cardWidth * 0.5f, cardY + cardHeight * 0.5f, 0.0f};
			TextComponent* cardText = textObject->GetComponent<TextComponent>();
			// 絵馬の漆枠と金具を避けた内側だけを本文領域にする。
			cardText->SetLayoutBounds((std::max)(1.0f, cardWidth - 58.0f), (std::max)(1.0f, cardHeight - 42.0f));
			cardText->SetColor(
			    !unlocked ? Vector4{0.46f, 0.44f, 0.42f, 1.0f} :
			    selected ? Vector4{0.29f, 0.055f, 0.035f, 1.0f} : Vector4{0.14f, 0.12f, 0.10f, 1.0f});
			textObject->Draw2D();
		}
	}

	titleTextObject_->GetTransform().translate = {screenWidth * 0.5f, 48.0f, 0.0f};
	const std::string selectedStageName = stageCount > 0
		? (sceneManager_ ? sceneManager_->GetGameplayStageDisplayName(stageIds_[selectedStageIndex_]) : stageIds_[selectedStageIndex_])
		: "---";
	selectedStageTextObject_->GetComponent<TextComponent>()->SetText("次に踏み入る境　「" + selectedStageName + "」");
	selectedStageTextObject_->GetComponent<TextComponent>()->SetColor({0.91f, 0.82f, 0.68f, 1.0f});
	selectedStageTextObject_->GetTransform().translate = {screenWidth * 0.5f, 100.0f, 0.0f};
	instructionTextObject_->GetTransform().translate = {screenWidth * 0.5f, screenHeight - 32.0f, 0.0f};
	std::ostringstream pageStream;
	pageStream << (currentPage + 1) << " / " << pageCount;
	pageTextObject_->GetComponent<TextComponent>()->SetText(pageStream.str());
	pageTextObject_->GetTransform().translate = {screenWidth * 0.5f, screenHeight - 65.0f, 0.0f};
	titleTextObject_->Draw2D();
	selectedStageTextObject_->Draw2D();
	instructionTextObject_->Draw2D();
	pageTextObject_->Draw2D();
}

void StageSelectScene::Finalize() {
	backgroundSprite_.reset();
	for (auto& sprite : frameAccentSprites_) sprite.reset();
	cardBorderSprites_.clear();
	cardSprites_.clear();
	cardAccentSprites_.clear();
	titleTextObject_.reset();
	selectedStageTextObject_.reset();
	instructionTextObject_.reset();
	pageTextObject_.reset();
	stageTextObjects_.clear();
	stageIds_.clear();
	BaseScene::Finalize();
}
