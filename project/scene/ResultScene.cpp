#include "ResultScene.h"

#include "SceneManager.h"
#include "helpers/SceneUiUtility.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <Xinput.h>

namespace {
using SceneUiUtility::CreateColorSprite;
using SceneUiUtility::CreateTextObject;
using SceneUiUtility::DrawColorSprite;
}

/// <summary>
/// SceneManagerに保存された最終戦績から、固定表示するリザルトUIを構築します。
/// </summary>
void ResultScene::Initialize() {
	backgroundSprite_ = CreateColorSprite();
	panelSprite_ = CreateColorSprite();

	const std::string playerName = sceneManager_ ? sceneManager_->GetSelectedPlayerTypeName() : "Unknown";
	const std::string stageName = sceneManager_
	    ? sceneManager_->GetGameplayStageDisplayName(sceneManager_->GetSelectedGameplayStageId())
	    : "平原";
	// SceneManagerが保持したスナップショットを読み、ゲームプレイシーンへ再アクセスせず表示を構築する。
	const StageResultData resultData = sceneManager_ ? sceneManager_->GetStageResultData() : StageResultData{};
	isStageClear_ = resultData.stageCleared;
	clearTextObject_ = CreateTextObject(isStageClear_ ? "STAGE CLEAR" : "GAME OVER", 64.0f);
	// 秒数は画面上で読みやすい「分:秒」形式へ丸めて変換する。
	const int totalSeconds = (std::max)(0, static_cast<int>(std::round(resultData.survivalTimeSeconds)));
	const int minutes = totalSeconds / 60;
	const int seconds = totalSeconds % 60;
	std::ostringstream resultStream;
	// 今回の獲得額と、加算後のゲーム全体の所持金を並べて表示する。
	resultStream << "プレイヤー: " << playerName << "    ステージ: " << stageName
	             << "\n今回の獲得金: " << resultData.moneyEarned << " G    所持金: "
	             << (sceneManager_ ? sceneManager_->GetMoney() : resultData.moneyEarned) << " G"
	             << "\n撃破数: " << resultData.defeatedEnemyCount << "    生存時間: "
	             << std::setfill('0') << std::setw(2) << minutes << ':' << std::setw(2) << seconds;
	resultTextObject_ = CreateTextObject(resultStream.str(), 25.0f);

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
	attackEquipmentTextObject_ = CreateTextObject(makeEquipmentText("ATTACK EQUIPMENT", resultData.attacks), 23.0f);
	statusEquipmentTextObject_ = CreateTextObject(makeEquipmentText("STATUS EQUIPMENT", resultData.statuses), 23.0f);
	instructionTextObject_ = CreateTextObject(
	    "Space / Enter・Pad A: ステージ選択へ    R・Pad Y: 同じステージを再挑戦    Q・Pad B: タイトルへ",
	    18.0f);
}

/// <summary>
/// リザルト画面から選べる各遷移先への入力を処理します。
/// </summary>
void ResultScene::Update() {
	if (!sceneManager_) {
		return;
	}
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
	const float panelWidth = (std::min)(720.0f, screenWidth - 80.0f);
	const float panelHeight = (std::min)(620.0f, screenHeight - 50.0f);
	const float panelX = (screenWidth - panelWidth) * 0.5f;
	const float panelY = (screenHeight - panelHeight) * 0.5f;

	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	// 背景の上へ中央パネルを重ね、その中を概要・装備2列・操作説明に分ける。
	DrawColorSprite(backgroundSprite_.get(), 0.0f, 0.0f, screenWidth, screenHeight, {0.015f, 0.025f, 0.06f, 1.0f});
	DrawColorSprite(panelSprite_.get(), panelX, panelY, panelWidth, panelHeight, {0.04f, 0.12f, 0.22f, 0.98f});

	clearTextObject_->GetTransform().translate = {screenWidth * 0.5f, panelY + 62.0f, 0.0f};
	// クリアはシアン、死亡は赤にして終了理由を視覚的にも区別する。
	clearTextObject_->GetComponent<TextComponent>()->SetColor(
	    isStageClear_ ? Vector4{0.35f, 0.95f, 1.0f, 1.0f} : Vector4{1.0f, 0.28f, 0.22f, 1.0f});
	resultTextObject_->GetTransform().translate = {screenWidth * 0.5f, panelY + 145.0f, 0.0f};
	attackEquipmentTextObject_->GetTransform().translate = {panelX + panelWidth * 0.27f, panelY + 340.0f, 0.0f};
	statusEquipmentTextObject_->GetTransform().translate = {panelX + panelWidth * 0.73f, panelY + 340.0f, 0.0f};
	instructionTextObject_->GetTransform().translate = {screenWidth * 0.5f, panelY + panelHeight - 40.0f, 0.0f};
	clearTextObject_->Draw2D();
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
	backgroundSprite_.reset();
	panelSprite_.reset();
	clearTextObject_.reset();
	resultTextObject_.reset();
	attackEquipmentTextObject_.reset();
	statusEquipmentTextObject_.reset();
	instructionTextObject_.reset();
	BaseScene::Finalize();
}
