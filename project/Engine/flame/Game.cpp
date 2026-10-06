#include "Game.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "LineDrawer.h"
#include "particle/TrailRenderer.h"
#include "object/Object3dCommon.h"
#include "particle/ParticleManager.h"
#include"LineCommon.h"
#include "PostEffect.h"
#include "SceneFactory.h"
#include "SceneManager.h"
#include "sky/SkyBoxCommon.h"
#include "TextComponent.h"
#include <DbgHelp.h>
#include <strsafe.h>
#include <Xinput.h>

/// <summary>
/// 必要なリソースを準備し、オブジェクトを初期化します。
/// </summary>
void Game::Initialize() {

	FlameWork::Initialize();
	camera = std::make_unique<Camera>();
	camera->SetTranslate({ 0.0f, 0.0f, -20.0f });
	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());

	ParticleManager::GetInstance()->SetCamera(camera.get());
	sceneFactory = std::make_unique<SceneFactory>();
	sceneManager = std::make_unique<SceneManager>();
	sceneManager->SetFallbackCamera(camera.get());
	sceneManager->SetSceneFactory(sceneFactory.get());
	sceneManager->ChangeScene("TITLE");
	SkyBoxCommon::GetInstance()->SetDefaultCamera(camera.get());

	// 録画状態は画面上で確認できるようにするが、録画データには含めない画面専用 UI とする。
	recordingIndicator_ = std::make_unique<GameObject>();
	recordingIndicator_->SetName("ScreenRecordingIndicator");
	TextComponent* recordingText = recordingIndicator_->AddComponent<TextComponent>();
	recordingText->SetText("● REC");
	recordingText->SetFontSize(28.0f);
	recordingText->SetColor({1.0f, 0.08f, 0.08f, 1.0f});
	recordingText->SetAnchor(TextComponent::Anchor::TopRight);
}

/// <summary>
/// 毎フレームの状態更新を行います。
/// </summary>
void Game::Update() {
	ImGuiManager::GetInstance()->Begin();

	FlameWork::Update();
	if (FlameWork::IsEndRequest()) {
		return;
	}
	// ImGuiフレーム内で監視と操作UIを更新する。C++再ビルド成功時だけ旧EXEを通常終了させる。
	if (ImGuiManager::GetInstance()->UpdateHotReload(
		sceneManager ? sceneManager->GetCurrentSceneJsonPath() : std::string(),
		[this]() { return sceneManager && sceneManager->ReloadCurrentSceneJson(); })) {
		endRequest = true;
		return;
	}
	Input::GetInstance()->Update();

	// F11 に加えて Pad の BACK（View）ボタンにも同じ操作を割り当てる。
	// シーン更新より前に共通処理として判定するため、タイトルを含む全シーンで
	// コントローラからウィンドウ／フルスクリーン表示を切り替えられる。
	if (Input::GetInstance()->TriggerKey(DIK_F11) ||
		Input::GetInstance()->TriggerGamepadButton(XINPUT_GAMEPAD_BACK)) {
		ToggleFullscreen();
	}
	if (Input::GetInstance()->TriggerKey(DIK_F10)) {
		// 実際の保存は描画完了後に行い、途中のバックバッファを取得しない。
		SpriteCommon::GetInstance()->GetDxCommon()->RequestScreenshot();
	}
	if (Input::GetInstance()->TriggerKey(DIK_F9)) {
		// F9 を押すたびに MP4 録画の開始／停止を切り替える。
		SpriteCommon::GetInstance()->GetDxCommon()->ToggleScreenRecording();
	}
	if (camera) {
		float aspectRatio = GetRenderAspectRatio();
#ifdef USE_IMGUI
		const float gameViewAspectRatio = ImGuiManager::GetInstance()->GetGameViewAspectRatio();
		if (gameViewAspectRatio > 0.0f) {
			aspectRatio = gameViewAspectRatio;
		}
#endif
		camera->SetAspectRatio(aspectRatio);
		camera->Update();
	}
	PostEffect::GetInstance()->UpdateHotkeys();

	sceneManager->Update();

	// ゲームプレイ中のEscapeはポーズメニューが使用する。ほかの画面では従来どおり終了する。
	if (Input::GetInstance()->TriggerKey(DIK_ESCAPE) &&
	    (!sceneManager || sceneManager->GetCurrentSceneName() != "GAMEPLAY")) {
		endRequest = true;
	}
}

/// <summary>
/// 現在の状態をもとに描画処理を行います。
/// </summary>
void Game::Draw() {
#pragma region Setup

	const auto drawHdrScene = [this]() {
		SkyBoxCommon::GetInstance()->SetDraw();
		sceneManager->DrawSkyBox();

		Object3dCommon::GetInstance()->SetDraw();
		sceneManager->Draw3D();
		Camera* defaultCamera = Object3dCommon::GetInstance()->GetDefaultCamera();
		TrailRenderer::GetInstance()->Draw(defaultCamera);
		LineDrawer::GetInstance()->Draw(defaultCamera);
	};

	PostEffect* postEffect = PostEffect::GetInstance();
	// 第2段階: 通常描画より先に、ライト視点から各オブジェクトの深度を書き込みます。
	Object3dCommon* object3dCommon = Object3dCommon::GetInstance();
	object3dCommon->BeginShadowMapPass();
	sceneManager->DrawShadowMap();
	object3dCommon->EndShadowMapPass();
	// 3Dは常にHDRへ描画します。Master OFF時も表示にはHDRからsRGBへの変換が必要です。
	postEffect->PreDrawScene();
	drawHdrScene();
	postEffect->PostDrawScene();

	SkyBoxCommon::GetInstance()->GetDxCommon()->PreDraw();
	ImGuiManager::GetInstance()->ApplyGameViewRenderArea();
	postEffect->Draw();

	// UIはトーンマッピング後に描き、文字やメニューがBloomでにじむことを防ぎます。
	SpriteCommon::GetInstance()->SetDraw(BlendMode::kBlendModeNone);
	sceneManager->Draw2D();
	ImGuiManager::GetInstance()->RestoreFullRenderArea();
	postEffect->DrawImGui();
	ImGuiManager::GetInstance()->End();
	ImGuiManager::GetInstance()->Draw();

	DirectXCommon* dxCommon = SpriteCommon::GetInstance()->GetDxCommon();
	if (dxCommon) {
		// REC 表示を描く前に録画フレームを確定し、保存動画へインジケーターが映り込むのを防ぐ。
		dxCommon->CaptureFrameBeforeOverlay();
	}
	if (recordingIndicator_ && dxCommon && dxCommon->IsScreenRecording()) {
		// 画面サイズが変わっても右上から一定の余白を保つ。
		recordingIndicator_->GetTransform().translate = {
			static_cast<float>(dxCommon->GetRenderWidth()) - 20.0f,
			16.0f,
			0.0f};
		recordingIndicator_->Draw2D();
	}

	SpriteCommon::GetInstance()->GetDxCommon()->PostDraw();

#pragma endregion
}

/// <summary>
/// 確保したリソースを解放し、終了処理を行います。
/// </summary>
void Game::Finalize() {
	recordingIndicator_.reset();
	sceneManager.reset();
	sceneFactory.reset();
	camera.reset();
	FlameWork::Finalize();
}
