#include "core/FlameWork.h"
#include <strsafe.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <minidumpapiset.h>
#include <Windows.h>
#include <wrl/client.h>
#include <excpt.h>
#include <memory>
#include <filesystem>
#include <particle/ParticleManager.h>
#include <SpriteCommon.h>
#include <TextureManager.h>
#include <camera/Camera.h>
#include <model/ModelManager.h>
#include <object/Object3dCommon.h>
#include <D3DResourceLeakChecker.h>
#include"LineCommon.h"
#include"LineDrawer.h"
#include "particle/TrailRenderer.h"
#include "instancing/InstancingModelCommon.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include <FontManager.h>
#include <Input.h>
#include <SrvManager.h>
#include <WinApp.h>
#include"PostEffect.h"
#include "GameTime.h"
using namespace Logger;
using namespace StringUtility;

static std::wstring GetExecutableDirectory() {
	wchar_t buffer[MAX_PATH]{};
	GetModuleFileNameW(nullptr, buffer, MAX_PATH);
	return std::filesystem::path(buffer).parent_path().wstring();
}
using namespace Microsoft::WRL;
static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filepath[MAX_PATH] = { 0 };
	StringCchPrintfW(filepath, MAX_PATH, L"Dump\\%04d-%02d-%02d_%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFileW(filepath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	DWORD processID = GetCurrentProcessId();
	DWORD threadID = GetCurrentThreadId();
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation = { 0 };
	minidumpInformation.ThreadId = threadID;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;
	MiniDumpWriteDump(GetCurrentProcess(), processID, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

	return EXCEPTION_EXECUTE_HANDLER;
}


/// <summary>
/// 必要なリソースを準備し、オブジェクトを初期化します。
/// </summary>
void FlameWork::Initialize() {
#pragma region Setup
	SetUnhandledExceptionFilter(ExportDump);

	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	winApp = std::make_unique<WinApp>();
	winApp->Initialize();
	// シーンやTextComponentを生成する前に、DirectWriteから参照できるよう
	// 配布物に含めたフォントをプロセス専用で登録する。
	std::filesystem::path fontDirectory = std::filesystem::path(GetExecutableDirectory()) / L"Resources" / L"Fonts";
	// 開発環境では作業ディレクトリがproject直下になるため、
	// 出力先にFontsが無い場合はそちらも確認する。
	if (!std::filesystem::exists(fontDirectory)) {
		fontDirectory = std::filesystem::current_path() / L"Resources" / L"Fonts";
	}
	FontManager::GetInstance()->LoadFromDirectory(fontDirectory.wstring());


#if defined(_DEBUG)
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController1 = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController1)))) {
		debugController1->EnableDebugLayer();
#ifdef ENABLE_D3D12_GPU_VALIDATION
		debugController1->SetEnableGPUBasedValidation(TRUE);
#else
		debugController1->SetEnableGPUBasedValidation(FALSE);
#endif
	}

#endif

	OutputDebugStringA("Hello, World!\n");
	resourceLeakChecker_ = std::make_unique<D3DResourceLeakChecker>();

	Input::GetInstance()->Initialize(winApp.get());
	dxCommon = std::make_unique<DirectXCommon>();
	dxCommon->Initialize(winApp.get());
	SrvManager::GetInstance()->Initialize(dxCommon.get());
	ImGuiManager::GetInstance()->Initialize(winApp.get(), dxCommon.get());
	TextureManager::GetInstance()->Initialize(dxCommon.get());
	PostEffect::GetInstance()->Initialize(dxCommon.get());
	SpriteCommon::GetInstance()->Initialize(dxCommon.get());
	SkyBoxCommon::GetInstance()->Initialize(dxCommon.get());
	LineCommon::GetInstance()->Initialize(dxCommon.get());
	LineDrawer::GetInstance()->Initialize();
	TrailRenderer::GetInstance()->Initialize(dxCommon.get());
	Object3dCommon::GetInstance()->Initialize(dxCommon.get());
	ModelManager::GetInstance()->Initialize(dxCommon.get());
	ParticleManager::GetInstance()->Initialize(dxCommon.get());
	InstancingModelCommon::GetInstance()->Initialize(dxCommon.get());
	GameTime::Initialize();

}

/// <summary>
/// 毎フレームの状態更新を行います。
/// </summary>
void FlameWork::Update() {
	GameTime::Update();
	if (winApp->ProcessMessage()) {
		endRequest = true;
		return;
	}
	if (dxCommon) {
		Object3dCommon::GetInstance()->EnsureInitialized(dxCommon.get());
		ModelManager::GetInstance()->EnsureInitialized(dxCommon.get());
		dxCommon->ResizeIfNeeded();
		dxCommon->BeginFrame();
	}
}

void FlameWork::Draw() {}

/// <summary>
/// Fullscreen の状態を切り替えます。
/// </summary>
void FlameWork::ToggleFullscreen() {
	if (winApp) {
		winApp->ToggleFullscreen();
	}
	if (dxCommon) {
		dxCommon->ResizeIfNeeded();
	}
}

float FlameWork::GetRenderAspectRatio() const {
	const int32_t width = GetRenderWidth();
	const int32_t height = GetRenderHeight();
	if (height <= 0) {
		return 1.0f;
	}
	return static_cast<float>(width) / static_cast<float>(height);
}

int32_t FlameWork::GetRenderWidth() const {
	if (dxCommon) {
		return dxCommon->GetRenderWidth();
	}
	return WinApp::kClientWidth;
}

int32_t FlameWork::GetRenderHeight() const {
	if (dxCommon) {
		return dxCommon->GetRenderHeight();
	}
	return WinApp::kClientHeight;
}

/// <summary>
/// 確保したリソースを解放し、終了処理を行います。
/// </summary>
void FlameWork::Finalize() {
	ParticleManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();
	InstancingModelCommon::GetInstance()->Finalize();
	Object3dCommon::GetInstance()->Finalize();
	TrailRenderer::GetInstance()->Finalize();
	LineDrawer::GetInstance()->Finalize();
	LineCommon::GetInstance()->Finalize();
	SkyBoxCommon::GetInstance()->Finalize();
	SpriteCommon::GetInstance()->Finalize();
	PostEffect::GetInstance()->Finalize();
	TextureManager::GetInstance()->Finalize();
	// TextComponentが使い終わった後、登録したフォントを解放する。
	FontManager::GetInstance()->Finalize();

	ImGuiManager::GetInstance()->Finalize();
	SrvManager::GetInstance()->Finalize();

	if (dxCommon) {
		dxCommon->Release();
		dxCommon.reset();
	}
	if (winApp) {
		winApp->Finalize();
		winApp.reset();
	}
	resourceLeakChecker_.reset();
}

void FlameWork::Run() {
	Initialize();
	while (true) {
		Update();
		if (IsEndRequest() || endRequest) {
			break;
		}
		Draw();
	}
	Finalize();
}

