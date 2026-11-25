#pragma once
#include <cstdint>
#include <memory>

// 実体を必要とする描画・Windows依存はcppへ閉じ込め、基盤ヘッダの利用側へ伝播させない。
class D3DResourceLeakChecker;
class DirectXCommon;
class ImGuiManager;
class WinApp;

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
	FlameWork();
	virtual ~FlameWork();

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

	bool IsEndRequest() const { return endRequest_; }

protected:
	/// <summary>派生ゲームから共通メインループへ安全に終了を要求します。</summary>
	void RequestEnd() { endRequest_ = true; }

private:
	/// <summary>ウィンドウ終了とゲーム固有終了要求をまとめる唯一の状態です。</summary>
	bool endRequest_ = false;
	std::unique_ptr<WinApp> winApp = nullptr;
	std::unique_ptr<D3DResourceLeakChecker> resourceLeakChecker_ = nullptr;
	std::unique_ptr<DirectXCommon> dxCommon = nullptr;
	std::unique_ptr<ImGuiManager> imguiManager = nullptr;
};

