#pragma once
#include "Audio.h"
#include "camera/Camera.h"
#include "D3DResourceLeakChecker.h"
#include "DirectXCommon.h"
#include "core/FlameWork.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "Matrix.h"
#include "model/ModelManager.h"
#include "object/Object3d.h"
#include "object/Object3dCommon.h"
#include "particle/ParticleEmitter.h"
#include "particle/ParticleManager.h"
#include "Resource.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "sky/SkyBoxCommon.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "Vector.h"
#include "WinApp.h"
#include "struct.h"
#include <Windows.h>
#include <d3d12.h>
#include <string>
#include <strsafe.h>
#include <wrl.h>
#include "core/AbstractSceneFactory.h"

#pragma comment(lib, "DirectXTex.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

/// <summary>
/// エンジン各サブシステムを生成・初期化し、メインループと終了順序を管理します。
/// アプリケーション固有の初期化と終了処理は派生クラスへ委譲します。
/// </summary>
class FlameWork {
public:
	virtual ~FlameWork() = default;

	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	virtual void Initialize();
	/// <summary>
	/// 毎フレームの状態更新を行います。
	/// </summary>
	virtual void Update();
	/// <summary>
	/// 現在の状態をもとに描画処理を行います。
	/// </summary>
	virtual void Draw();
	/// <summary>
	/// 確保したリソースを解放し、終了処理を行います。
	/// </summary>
	virtual void Finalize();
	void Run();
	/// <summary>
	/// Fullscreen の状態を切り替えます。
	/// </summary>
	void ToggleFullscreen();
	float GetRenderAspectRatio() const;
	int32_t GetRenderWidth() const;
	int32_t GetRenderHeight() const;

	virtual bool IsEndRequest() const { return endRequest; };


private:
	bool endRequest = false;
	std::unique_ptr<WinApp> winApp = nullptr;
	std::unique_ptr<D3DResourceLeakChecker> resourceLeakChecker_ = nullptr;
	std::unique_ptr<DirectXCommon> dxCommon = nullptr;
	std::unique_ptr<ImGuiManager> imguiManager = nullptr;
	std::unique_ptr<AbstractSceneFactory> sceneFactory = nullptr;
};

