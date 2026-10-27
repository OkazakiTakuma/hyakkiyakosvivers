#include "ResultScene.h"

#include "SceneManager.h"
#include "helpers/SceneUiUtility.h"
#include "GameTime.h"
#include "TextureManager.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <Xinput.h>

namespace {
using SceneUiUtility::CreateColorSprite;
using SceneUiUtility::CreateTextObject;
using SceneUiUtility::DrawColorSprite;
using SceneUiUtility::DrawRotatedColorSprite;

constexpr const char* kResultFogTexturePath = "Resources/Texture/fog_transition.png";

float Saturate(float value) {
	return (std::clamp)(value, 0.0f, 1.0f);
}

float SmoothStep01(float value) {
	value = Saturate(value);
	return value * value * (3.0f - 2.0f * value);
}
}

/// <summary>
/// SceneManagerに保存された最終戦績から、固定表示するリザルトUIを構築します。
/// </summary>
void ResultScene::Initialize() {
	backgroundSprite_ = CreateColorSprite();
	panelSprite_ = CreateColorSprite();
	for (auto& sprite : decorationSprites_) sprite = CreateColorSprite();
	TextureManager::GetInstance()->LoadTexture(kResultFogTexturePath);
	for (auto& sprite : fogSprites_) {
		sprite = std::make_unique<Sprite>();
		sprite->Initialize(kResultFogTexturePath);
		sprite->SetAnchorPoint({0.0f, 0.0f});
	}
	presentationElapsed_ = 0.0f;

	const std::string playerName = sceneManager_ ? sceneManager_->GetSelectedPlayerTypeName() : "Unknown";
	const std::string stageName = sceneManager_
	    ? sceneManager_->GetGameplayStageDisplayName(sceneManager_->GetSelectedGameplayStageId())
	    : "平原";
	// SceneManagerが保持したスナップショットを読み、ゲームプレイシーンへ再アクセスせず表示を構築する。
	const StageResultData resultData = sceneManager_ ? sceneManager_->GetStageResultData() : StageResultData{};
	isStageClear_ = resultData.stageCleared;
	clearTextObject_ = CreateTextObject(
		isStageClear_ ? "妖異鎮めたり" : "力尽きた……", 58.0f, TextComponent::Anchor::Center, "Yu Mincho");
	subtitleTextObject_ = CreateTextObject(
		isStageClear_ ? "STAGE CLEAR" : "GAME OVER", 18.0f, TextComponent::Anchor::Center, "Yu Mincho");
	// 秒数は画面上で読みやすい「分:秒」形式へ丸めて変換する。
	const int totalSeconds = (std::max)(0, static_cast<int>(std::round(resultData.survivalTimeSeconds)));
	const int minutes = totalSeconds / 60;
	const int seconds = totalSeconds % 60;
	std::ostringstream resultStream;
	// 今回の獲得額と、加算後のゲーム全体の所持金を並べて表示する。
	resultStream << playerName << "　｜　" << stageName
	             << "\n撃破　" << resultData.defeatedEnemyCount << "体　　　生存　"
	             << std::setfill('0') << std::setw(2) << minutes << ':' << std::setw(2) << seconds
	             << "\n獲得　" << resultData.moneyEarned << " G　　　所持　"
	             << (sceneManager_ ? sceneManager_->GetMoney() : resultData.moneyEarned) << " G";
	resultTextObject_ = CreateTextObject(resultStream.str(), 24.0f, TextComponent::Anchor::Center, "Yu Mincho");

	// 装備欄を共通形式へ整形し、特殊レベルsuperだけはSUPER表記にする。
	auto makeEquipmentText = [](const char* heading, const std::vector<StageResultEquipment>& equipment) {
		std::ostringstream stream;
		stream << heading << '\n';
		if (equipment.empty()) {
			stream << "なし";
		} else {
			for (const StageResultEquipment& item : equipment) {
				stream << "\n" << item.name << "  "
				       << (item.level == "super" ? "SUPER" : "Lv." + item.level);
			}
		}
		return stream.str();
	};
	attackEquipmentTextObject_ = CreateTextObject(
		makeEquipmentText("攻撃の心得", resultData.attacks), 22.0f, TextComponent::Anchor::TopCenter, "Yu Mincho");
	statusEquipmentTextObject_ = CreateTextObject(
		makeEquipmentText("身に宿る加護", resultData.statuses), 22.0f, TextComponent::Anchor::TopCenter, "Yu Mincho");
	instructionTextObject_ = CreateTextObject(
	    "決定：ステージ選択　　Y / R：再挑戦　　B / Q：タイトル",
	    17.0f, TextComponent::Anchor::Center, "Yu Mincho");
}

/// <summary>
/// リザルト画面から選べる各遷移先への入力を処理します。
/// </summary>
void ResultScene::Update() {
	presentationElapsed_ += GameTime::GetUnscaledDeltaTime();
	if (!sceneManager_) {
		return;
	}
	// 遷移直前の決定入力が残って、リザルトを即座に飛ばしてしまうのを防ぐ。
	if (presentationElapsed_ < 0.35f) return;
	Input* input = Input::GetInstance();
	// A系入力はステージ再選択、Y/Rは同条件で再挑戦、B/Qはタイトルへ戻る。
	if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) ||
	    input->TriggerGamepadButton(XINPUT_GAMEPAD_A)) {
		sceneManager_->ChangeScene("STAGE_SELECT");
		return;
	}
	if (input->TriggerKey(DIK_R) || input->TriggerGamepadButton(XINPUT_GAMEPAD_Y)) {
		sceneManager_->ChangeScene("GAMEPLAY");
		return;
	}
	if (input->TriggerKey(DIK_Q) || input->TriggerGamepadButton(XINPUT_GAMEPAD_B)) {
		sceneManager_->ChangeScene("TITLE");
	}
}

/// <summary>
/// 解像度に合わせて中央パネルを配置し、戦績と装備を描画します。
/// </summary>
void ResultScene::Draw2D() {
	DirectXCommon* dxCommon = SpriteCommon::GetInstance()->GetDxCommon();
	if (!dxCommon || !backgroundSprite_ || !panelSprite_) {
		return;
	}
	const float screenWidth = static_cast<float>(dxCommon->GetRenderWidth());
	const float screenHeight = static_cast<float>(dxCommon->GetRenderHeight());
	const float panelWidth = (std::min)(960.0f, screenWidth - 60.0f);
	const float panelHeight = (std::min)(650.0f, screenHeight - 36.0f);
	const float panelX = (screenWidth - panelWidth) * 0.5f;
	const float panelY = (screenHeight - panelHeight) * 0.5f;
	const float reveal = SmoothStep01(presentationElapsed_ / 0.55f);
	const float textReveal = SmoothStep01((presentationElapsed_ - 0.18f) / 0.55f);
	const Vector4 accent = isStageClear_
		? Vector4{0.72f, 0.55f, 0.25f, reveal}
		: Vector4{0.66f, 0.20f, 0.15f, reveal};
	const Vector4 paleText = {0.91f, 0.87f, 0.76f, textReveal};

	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	DrawColorSprite(backgroundSprite_.get(), 0.0f, 0.0f, screenWidth, screenHeight,
		{0.006f, 0.009f, 0.022f, 1.0f});

	// 夜霧を低い不透明度で流し、ゲームプレイから切り離された単色メニューに見せない。
	const float fogScale[][2] = {{1.12f, 1.42f}, {1.48f, 1.02f}, {1.82f, 1.62f}};
	const float fogSpeed[][2] = {{-0.018f, 0.006f}, {0.014f, 0.010f}, {-0.010f, -0.005f}};
	for (size_t index = 0; index < fogSprites_.size(); ++index) {
		Sprite* fog = fogSprites_[index].get();
		if (!fog) continue;
		EulerTransform transform = fog->GetTransform();
		transform.translate = {-screenWidth * 0.06f, -screenHeight * 0.08f, 0.0f};
		fog->SetTransform(transform);
		fog->SetSize({screenWidth * 1.12f, screenHeight * 1.16f});
		fog->SetColor({0.32f, 0.37f, 0.46f, (0.14f - index * 0.025f) * reveal});
		EulerTransform uv = fog->GetUVTransform();
		uv.scale = {fogScale[index][0], fogScale[index][1], 1.0f};
		uv.translate = {presentationElapsed_ * fogSpeed[index][0], presentationElapsed_ * fogSpeed[index][1], 0.0f};
		fog->SetUVTransform(uv);
		fog->Update();
		fog->Draw();
	}

	// 漆黒の札を二重の鈍い金縁で囲み、内部を戦績と二つの装備札へ分ける。
	DrawColorSprite(decorationSprites_[0].get(), panelX + 12.0f, panelY + 16.0f, panelWidth, panelHeight,
		{0.0f, 0.0f, 0.0f, 0.52f * reveal});
	DrawColorSprite(panelSprite_.get(), panelX, panelY, panelWidth, panelHeight,
		{0.025f, 0.028f, 0.043f, 0.96f * reveal});
	DrawColorSprite(decorationSprites_[1].get(), panelX + 3.0f, panelY + 3.0f, panelWidth - 6.0f, 2.0f, accent);
	DrawColorSprite(decorationSprites_[2].get(), panelX + 3.0f, panelY + panelHeight - 5.0f, panelWidth - 6.0f, 2.0f, accent);
	DrawColorSprite(decorationSprites_[3].get(), panelX + 3.0f, panelY + 3.0f, 2.0f, panelHeight - 6.0f, accent);
	DrawColorSprite(decorationSprites_[4].get(), panelX + panelWidth - 5.0f, panelY + 3.0f, 2.0f, panelHeight - 6.0f, accent);
	DrawColorSprite(decorationSprites_[5].get(), panelX + 34.0f, panelY + 142.0f, panelWidth - 68.0f, 2.0f,
		{accent.x, accent.y, accent.z, 0.62f * reveal});
	DrawColorSprite(decorationSprites_[6].get(), panelX + 48.0f, panelY + 164.0f, panelWidth - 96.0f, 126.0f,
		{0.07f, 0.075f, 0.09f, 0.86f * reveal});
	const float equipmentY = panelY + 316.0f;
	const float equipmentWidth = (panelWidth - 118.0f) * 0.5f;
	DrawColorSprite(decorationSprites_[7].get(), panelX + 48.0f, equipmentY, equipmentWidth, 220.0f,
		{0.045f, 0.05f, 0.066f, 0.92f * reveal});
	DrawColorSprite(decorationSprites_[8].get(), panelX + 70.0f + equipmentWidth, equipmentY, equipmentWidth, 220.0f,
		{0.045f, 0.05f, 0.066f, 0.92f * reveal});
	DrawColorSprite(decorationSprites_[9].get(), screenWidth * 0.5f - 1.0f, equipmentY + 20.0f, 2.0f, 180.0f,
		{accent.x, accent.y, accent.z, 0.42f * reveal});
	DrawRotatedColorSprite(decorationSprites_[10].get(), panelX + 35.0f, panelY + 72.0f, 13.0f, 13.0f,
		0.785398f, accent);
	DrawRotatedColorSprite(decorationSprites_[11].get(), panelX + panelWidth - 35.0f, panelY + 72.0f, 13.0f, 13.0f,
		0.785398f, accent);
	DrawColorSprite(decorationSprites_[12].get(), panelX + 90.0f, panelY + panelHeight - 76.0f,
		panelWidth - 180.0f, 1.0f, {0.42f, 0.35f, 0.23f, 0.52f * reveal});

	clearTextObject_->GetTransform().translate = {screenWidth * 0.5f, panelY + 64.0f, 0.0f};
	clearTextObject_->GetComponent<TextComponent>()->SetColor(paleText);
	subtitleTextObject_->GetTransform().translate = {screenWidth * 0.5f, panelY + 116.0f, 0.0f};
	subtitleTextObject_->GetComponent<TextComponent>()->SetColor(
		{accent.x, accent.y, accent.z, 0.90f * textReveal});
	resultTextObject_->GetTransform().translate = {screenWidth * 0.5f, panelY + 226.0f, 0.0f};
	resultTextObject_->GetComponent<TextComponent>()->SetColor(paleText);
	attackEquipmentTextObject_->GetTransform().translate = {panelX + 48.0f + equipmentWidth * 0.5f, equipmentY + 24.0f, 0.0f};
	statusEquipmentTextObject_->GetTransform().translate = {panelX + 70.0f + equipmentWidth * 1.5f, equipmentY + 24.0f, 0.0f};
	attackEquipmentTextObject_->GetComponent<TextComponent>()->SetLayoutBounds(equipmentWidth - 30.0f, 180.0f);
	statusEquipmentTextObject_->GetComponent<TextComponent>()->SetLayoutBounds(equipmentWidth - 30.0f, 180.0f);
	attackEquipmentTextObject_->GetComponent<TextComponent>()->SetColor(paleText);
	statusEquipmentTextObject_->GetComponent<TextComponent>()->SetColor(paleText);
	instructionTextObject_->GetTransform().translate = {screenWidth * 0.5f, panelY + panelHeight - 41.0f, 0.0f};
	instructionTextObject_->GetComponent<TextComponent>()->SetColor({0.64f, 0.62f, 0.57f, textReveal});
	clearTextObject_->Draw2D();
	subtitleTextObject_->Draw2D();
	resultTextObject_->Draw2D();
	attackEquipmentTextObject_->Draw2D();
	statusEquipmentTextObject_->Draw2D();
	instructionTextObject_->Draw2D();
}

/// <summary>
/// リザルト専用の表示リソースと終了状態を初期化します。
/// </summary>
void ResultScene::Finalize() {
	isStageClear_ = false;
	presentationElapsed_ = 0.0f;
	backgroundSprite_.reset();
	panelSprite_.reset();
	for (auto& sprite : fogSprites_) sprite.reset();
	for (auto& sprite : decorationSprites_) sprite.reset();
	clearTextObject_.reset();
	subtitleTextObject_.reset();
	resultTextObject_.reset();
	attackEquipmentTextObject_.reset();
	statusEquipmentTextObject_.reset();
	instructionTextObject_.reset();
	BaseScene::Finalize();
}
