#include "ImGuiManager.h"
#include "StringUtility.h"
#include "FontManager.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "../../../imgui/imgui_internal.h"
#include "LineCommon.h"
#include "object/Object3dCommon.h"
#include "particle/ParticleManager.h"
#include "PostEffect.h"
#include "sky/SkyBoxCommon.h"
#include "SpriteCommon.h"
#include "instancing/InstancingModelCommon.h"
#include "TextureManager.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include "../flame/repositories/EnemyStatusRepository.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace {
float ClampLayoutValue(float value, float minValue, float maxValue) {
	if (maxValue < minValue) {
		return maxValue;
	}
	if (value < minValue) {
		return minValue;
	}
	if (value > maxValue) {
		return maxValue;
	}
	return value;
}

#ifdef USE_IMGUI
/// <summary>
/// ImGui に日本語グリフを含むフォントを設定します。
/// </summary>
ImFont* LoadJapaneseFont() {
	ImGuiIO& io = ImGui::GetIO();
	const std::array<const char*, 5> fontPaths = {
	    "C:/Windows/Fonts/meiryo.ttc",
	    "C:/Windows/Fonts/YuGothM.ttc",
	    "C:/Windows/Fonts/YuGothR.ttc",
	    "C:/Windows/Fonts/msgothic.ttc",
	    "C:/Windows/Fonts/meiryob.ttc",
	};

	ImFontConfig fontConfig{};
	fontConfig.MergeMode = false;
	fontConfig.PixelSnapH = true;
	fontConfig.OversampleH = 2;
	fontConfig.OversampleV = 2;

	for (const char* fontPath : fontPaths) {
		if (!std::filesystem::exists(StringUtility::Utf8ToPath(fontPath))) {
			continue;
		}

		ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, 18.0f, &fontConfig, io.Fonts->GetGlyphRangesJapanese());
		if (font) {
			io.FontDefault = font;
			return font;
		}
	}

	ImFont* defaultFont = io.Fonts->AddFontDefault();
	io.FontDefault = defaultFont;
	return defaultFont;
}
#endif
}

ImGuiManager* ImGuiManager::GetInstance() {
	static ImGuiManager instance;
	return &instance;
}

/// <summary>
/// 必要なリソースを準備し、オブジェクトを初期化します。
/// </summary>
/// <param name="winApp">ウィンドウ管理オブジェクトを指定します。</param>
/// <param name="dxCommon">DirectX 共通処理へアクセスするための参照を指定します。</param>
void ImGuiManager::Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]] DirectXCommon* dxCommon) {
#ifdef USE_IMGUI
	dxcommon = dxCommon;
	gameWindowHandle_ = winApp->GetHwnd();

	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	LoadJapaneseFont();
	LoadGameFonts();
#ifdef IMGUI_HAS_DOCK
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
	io.ConfigViewportsNoAutoMerge = false;
	io.ConfigViewportsNoTaskBarIcon = false;
#endif

	ImGui_ImplWin32_Init(winApp->GetHwnd());

	ImGui_ImplDX12_InitInfo initInfo{};
	SrvManager* srvManager = SrvManager::GetInstance();

	initInfo.Device = dxCommon->GetDevice().Get();
	initInfo.NumFramesInFlight = static_cast<int>(dxCommon->GetSwapChainResourceCount());
	initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	initInfo.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	initInfo.SrvDescriptorHeap = srvManager->GetDescriptorHeap().Get();

	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle) {
		SrvManager* srvmanager = SrvManager::GetInstance();
		uint32_t index = srvmanager->Allocate();
		*out_cpu_desc_handle = srvmanager->GetCPUDescriptorHandle(index);
		*out_gpu_desc_handle = srvmanager->GetGPUDescriptorHandle(index);
	};

	initInfo.CommandQueue = dxCommon->GetCommandQueue().Get();
	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle) {
	};

	ImGui_ImplDX12_Init(&initInfo);
	performanceMonitor_.Initialize();
	nextHotReloadScanTime_ = {};
	hotReloadScanRunning_ = false;
#endif
}

void ImGuiManager::LoadGameFonts() {
#ifdef USE_IMGUI
	fontNames_.clear();
	fonts_.clear();

	ImGuiIO& io = ImGui::GetIO();
	auto registerFont = [this](const std::string& name, ImFont* font) {
		if (!font || fonts_.contains(name)) {
			return;
		}
		fonts_[name] = font;
		fontNames_.push_back(name);
	};

	registerFont("Default", io.FontDefault ? io.FontDefault : io.Fonts->AddFontDefault());

	ImFontConfig fontConfig{};
	fontConfig.MergeMode = false;
	fontConfig.PixelSnapH = true;
	fontConfig.OversampleH = 2;
	fontConfig.OversampleV = 2;

	const std::array<std::pair<const char*, const char*>, 5> windowsFonts = {
	    std::pair<const char*, const char*>{"Meiryo", "C:/Windows/Fonts/meiryo.ttc"},
	    std::pair<const char*, const char*>{"Yu Gothic", "C:/Windows/Fonts/YuGothM.ttc"},
	    std::pair<const char*, const char*>{"Yu Gothic Regular", "C:/Windows/Fonts/YuGothR.ttc"},
	    std::pair<const char*, const char*>{"MS Gothic", "C:/Windows/Fonts/msgothic.ttc"},
	    std::pair<const char*, const char*>{"Meiryo Bold", "C:/Windows/Fonts/meiryob.ttc"},
	};
	for (const auto& [name, path] : windowsFonts) {
		if (!std::filesystem::exists(StringUtility::Utf8ToPath(path))) {
			continue;
		}
		registerFont(name, io.Fonts->AddFontFromFileTTF(path, 32.0f, &fontConfig, io.Fonts->GetGlyphRangesJapanese()));
	}

	// TextComponentの選択肢にはDirectWriteが認識するファミリー名を使う。
	auto* collection = FontManager::GetInstance()->GetCollection();
	if (!collection) return;
	for (UINT32 index = 0; index < collection->GetFontFamilyCount(); ++index) {
		Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
		Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> familyNames;
		if (FAILED(collection->GetFontFamily(index, &family)) || FAILED(family->GetFamilyNames(&familyNames))) continue;
		UINT32 nameIndex = 0;
		BOOL found = FALSE;
		familyNames->FindLocaleName(L"en-us", &nameIndex, &found);
		if (!found) nameIndex = 0;
		UINT32 length = 0;
		if (FAILED(familyNames->GetStringLength(nameIndex, &length))) continue;
		std::wstring name(length + 1, L'\0');
		if (SUCCEEDED(familyNames->GetString(nameIndex, name.data(), length + 1))) {
			name.resize(length);
			fontNames_.push_back(StringUtility::PathToUtf8(std::filesystem::path(name)));
		}
	}
#endif
}

/// <summary>
/// 確保したリソースを解放し、終了処理を行います。
/// </summary>
void ImGuiManager::Finalize() {
#ifdef USE_IMGUI
	if (hotReloadScanFuture_.valid()) {
		hotReloadScanFuture_.wait();
		hotReloadScanFuture_.get();
	}
	hotReloadScanRunning_ = false;
	performanceMonitor_.Finalize();
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	dxcommon = nullptr;
	gameWindowHandle_ = nullptr;
#endif
}

void ImGuiManager::Begin() {
#ifdef USE_IMGUI
	performanceMonitor_.Update();
	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX12_NewFrame();
	ImGui::NewFrame();
	SetupExternalEditorDockSpaces();
	DrawGameViewWindow();
	DrawUtilityWindows();
#endif
}

void ImGuiManager::SetupExternalEditorDockSpaces() {
#if defined(USE_IMGUI) && defined(IMGUI_HAS_DOCK)
	static bool isLayoutBuilt = false;
	static bool wasFullscreen = false;
	static ImVec2 previousClientSize = {};
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	const ImGuiID dockspaceId = ImGui::GetID("UnityStyleEditorDockSpace");
	const ImGuiDockNodeFlags dockspaceFlags =
	    ImGuiDockNodeFlags_PassthruCentralNode |
	    ImGuiDockNodeFlags_NoDockingOverCentralNode;
	const bool isFullscreen = gameWindowHandle_ ? (GetWindowLongPtr(gameWindowHandle_, GWL_STYLE) & WS_OVERLAPPEDWINDOW) == 0 : false;
	RECT clientRect{};
	if (gameWindowHandle_) {
		GetClientRect(gameWindowHandle_, &clientRect);
	}
	const ImVec2 clientSize = ImVec2(
	    static_cast<float>(clientRect.right - clientRect.left),
	    static_cast<float>(clientRect.bottom - clientRect.top)
	);
	const ImVec2 layoutSize = clientSize.x > 0.0f && clientSize.y > 0.0f ? clientSize : viewport->Size;
	const float topHeight = ClampLayoutValue(layoutSize.y * 0.08f, 44.0f, 72.0f);
	const float leftWidth = ClampLayoutValue(layoutSize.x * 0.18f, 140.0f, layoutSize.x * 0.35f);
	const float rightWidth = ClampLayoutValue(layoutSize.x * 0.24f, 180.0f, layoutSize.x * 0.40f);
	const float bottomHeight = ClampLayoutValue(layoutSize.y * 0.24f, 120.0f, layoutSize.y * 0.40f);
	const float heightAfterTop = layoutSize.y - topHeight;
	const float widthAfterLeft = layoutSize.x - leftWidth;
	const float topRatio = layoutSize.y > 0.0f ? topHeight / layoutSize.y : 0.08f;
	const float leftRatio = layoutSize.x > 0.0f ? leftWidth / layoutSize.x : 0.18f;
	const float rightRatio = widthAfterLeft > 0.0f ? rightWidth / widthAfterLeft : 0.24f;
	const float bottomRatio = heightAfterTop > 0.0f ? bottomHeight / heightAfterTop : 0.24f;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::DockSpaceOverViewport(dockspaceId, viewport, dockspaceFlags);
	ImGui::PopStyleColor();

	if (isFullscreen != wasFullscreen || layoutSize.x != previousClientSize.x || layoutSize.y != previousClientSize.y) {
		isLayoutBuilt = false;
		wasFullscreen = isFullscreen;
		previousClientSize = layoutSize;
	}

	if (!isLayoutBuilt) {
		ImGui::DockBuilderRemoveNode(dockspaceId);
		ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace | dockspaceFlags);
		ImGui::DockBuilderSetNodePos(dockspaceId, viewport->Pos);
		ImGui::DockBuilderSetNodeSize(dockspaceId, layoutSize);

		ImGuiID mainId = dockspaceId;
		ImGuiID topId = 0;
		ImGuiID leftId = 0;
		ImGuiID rightId = 0;
		ImGuiID bottomId = 0;

		ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Up, topRatio, &topId, &mainId);
		ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, leftRatio, &leftId, &mainId);
		ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, rightRatio, &rightId, &mainId);
		ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, bottomRatio, &bottomId, &mainId);

		ImGui::DockBuilderDockWindow("GameView", mainId);
		ImGui::DockBuilderDockWindow("Editor Toolbar", topId);
		ImGui::DockBuilderDockWindow("Component Manager", leftId);
		ImGui::DockBuilderDockWindow("Hierarchy", leftId);
		ImGui::DockBuilderDockWindow("Scene Objects", leftId);
		ImGui::DockBuilderDockWindow("Component Inspector", rightId);
		ImGui::DockBuilderDockWindow("Player Inspector", rightId);
		ImGui::DockBuilderDockWindow("Enemy Inspector", rightId);
		ImGui::DockBuilderDockWindow("Player Attack Inspector", rightId);
		ImGui::DockBuilderDockWindow("Inspector", rightId);
		ImGui::DockBuilderDockWindow("Object Inspector", rightId);
		ImGui::DockBuilderDockWindow("Scene Manager", rightId);
		ImGui::DockBuilderDockWindow("PostEffect Settings", bottomId);
		ImGui::DockBuilderDockWindow("Post Effects", bottomId);
		ImGui::DockBuilderDockWindow("Particle Editor", bottomId);
		ImGui::DockBuilderDockWindow("Project", bottomId);
		ImGui::DockBuilderDockWindow("Console", bottomId);
		ImGui::DockBuilderDockWindow("Hot Reload", bottomId);

		ImGui::DockBuilderFinish(dockspaceId);
		isLayoutBuilt = true;
	}
#endif
}

void ImGuiManager::DrawUtilityWindows() {
#ifdef USE_IMGUI
	if (ImGui::Begin("Console")) {
		ImGui::Text("Ready %.1f FPS", ImGui::GetIO().Framerate);
		const float cpuUsage = performanceMonitor_.GetCpuUsagePercent();
		ImGui::Text("CPU Usage : %.1f%%", cpuUsage);
		ImGui::ProgressBar(cpuUsage / 100.0f, ImVec2(-1.0f, 0.0f));
		if (performanceMonitor_.IsGpuUsageAvailable()) {
			const float gpuUsage = performanceMonitor_.GetGpuUsagePercent();
			const float gpu3DUsage = performanceMonitor_.GetGpu3DUsagePercent();
			ImGui::Text("GPU Usage : %.1f%%", gpuUsage);
			ImGui::ProgressBar(gpuUsage / 100.0f, ImVec2(-1.0f, 0.0f));
			ImGui::Text("GPU 3D    : %.1f%%", gpu3DUsage);
			ImGui::ProgressBar(gpu3DUsage / 100.0f, ImVec2(-1.0f, 0.0f));
		} else {
			ImGui::Text("GPU Usage : N/A");
		}
		if (gameWindowHandle_) {
			const LONG_PTR style = GetWindowLongPtr(gameWindowHandle_, GWL_STYLE);
			ImGui::Text("F11: %s", (style & WS_OVERLAPPEDWINDOW) ? "Windowed" : "Fullscreen");
		}
	}
	ImGui::End();
#endif
}

void ImGuiManager::DrawGameViewWindow() {
#ifdef USE_IMGUI
	isGameViewVisible_ = false;
	gameViewWindow_ = nullptr;
	const ImGuiWindowFlags windowFlags =
	    ImGuiWindowFlags_NoBackground |
	    ImGuiWindowFlags_NoScrollbar |
	    ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::SetNextWindowBgAlpha(0.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
#ifdef IMGUI_HAS_DOCK
	ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
#endif
	if (ImGui::Begin("GameView", nullptr, windowFlags)) {
		gameViewWindow_ = ImGui::GetCurrentWindow();
		gameViewContentPosition_ = ImGui::GetCursorScreenPos();
		gameViewContentSize_ = ImGui::GetContentRegionAvail();
		isGameViewVisible_ = gameViewContentSize_.x > 1.0f && gameViewContentSize_.y > 1.0f;
		DrawEditorBackgroundMask(gameViewContentPosition_, gameViewContentSize_);
		if (ImGui::BeginDragDropTarget()) {
			auto acceptAssetPayload = [this](const char* payloadType, DroppedAssetPayload::Type assetType) {
				const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadType);
				if (!payload || !payload->Data || payload->DataSize <= 0) {
					return;
				}
				droppedAssetPayload_.type = assetType;
				droppedAssetPayload_.path.assign(static_cast<const char*>(payload->Data), static_cast<size_t>(payload->DataSize));
				if (!droppedAssetPayload_.path.empty() && droppedAssetPayload_.path.back() == '\0') {
					droppedAssetPayload_.path.pop_back();
				}
				hasDroppedAssetPayload_ = true;
			};
			acceptAssetPayload("CG2_ASSET_MODEL", DroppedAssetPayload::Type::Model);
			acceptAssetPayload("CG2_ASSET_ANIM_MODEL", DroppedAssetPayload::Type::AnimatedModel);
			acceptAssetPayload("CG2_ASSET_SPRITE", DroppedAssetPayload::Type::SpriteTexture);
			ImGui::EndDragDropTarget();
		}
	}
	ImGui::End();
#ifdef IMGUI_HAS_DOCK
	ImGui::PopStyleColor();
#endif
	ImGui::PopStyleColor(2);
#endif
}

bool ImGuiManager::CalculateGameViewRenderRect(float& left, float& top, float& width, float& height) const {
#ifdef USE_IMGUI
	if (!dxcommon || !isGameViewVisible_) {
		return false;
	}
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (!viewport || viewport->Size.x <= 1.0f || viewport->Size.y <= 1.0f) {
		return false;
	}
	const float renderWidth = static_cast<float>(dxcommon->GetRenderWidth());
	const float renderHeight = static_cast<float>(dxcommon->GetRenderHeight());
	const float scaleX = renderWidth / viewport->Size.x;
	const float scaleY = renderHeight / viewport->Size.y;
	left = (std::clamp)((gameViewContentPosition_.x - viewport->Pos.x) * scaleX, 0.0f, renderWidth);
	top = (std::clamp)((gameViewContentPosition_.y - viewport->Pos.y) * scaleY, 0.0f, renderHeight);
	const float right = (std::clamp)(left + gameViewContentSize_.x * scaleX, left, renderWidth);
	const float bottom = (std::clamp)(top + gameViewContentSize_.y * scaleY, top, renderHeight);
	width = right - left;
	height = bottom - top;
	return width > 1.0f && height > 1.0f;
#else
	(void)left;
	(void)top;
	(void)width;
	(void)height;
	return false;
#endif
}

float ImGuiManager::GetGameViewAspectRatio() const {
	float left = 0.0f;
	float top = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	return CalculateGameViewRenderRect(left, top, width, height) && height > 0.0f ? width / height : 0.0f;
}

bool ImGuiManager::ApplyGameViewRenderArea() {
#ifdef USE_IMGUI
	float left = 0.0f;
	float top = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	if (!CalculateGameViewRenderRect(left, top, width, height)) {
		return false;
	}
	auto commandList = dxcommon->GetCommandList();
	D3D12_VIEWPORT viewport{};
	viewport.TopLeftX = left;
	viewport.TopLeftY = top;
	viewport.Width = width;
	viewport.Height = height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	D3D12_RECT scissor{};
	scissor.left = static_cast<LONG>(std::floor(left));
	scissor.top = static_cast<LONG>(std::floor(top));
	scissor.right = static_cast<LONG>(std::ceil(left + width));
	scissor.bottom = static_cast<LONG>(std::ceil(top + height));
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissor);
	return true;
#else
	return false;
#endif
}

void ImGuiManager::RestoreFullRenderArea() {
#ifdef USE_IMGUI
	if (!dxcommon) {
		return;
	}
	auto commandList = dxcommon->GetCommandList();
	D3D12_VIEWPORT viewport{};
	viewport.Width = static_cast<float>(dxcommon->GetRenderWidth());
	viewport.Height = static_cast<float>(dxcommon->GetRenderHeight());
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	D3D12_RECT scissor{};
	scissor.right = dxcommon->GetRenderWidth();
	scissor.bottom = dxcommon->GetRenderHeight();
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissor);
#endif
}

bool ImGuiManager::ConsumeDroppedAsset(DroppedAssetPayload& outPayload) {
	if (!hasDroppedAssetPayload_) {
		return false;
	}
	outPayload = droppedAssetPayload_;
	droppedAssetPayload_ = {};
	hasDroppedAssetPayload_ = false;
	return true;
}

std::vector<std::string> ImGuiManager::GetAvailableFontNames() const {
	return fontNames_;
}

ImFont* ImGuiManager::GetFont(const std::string& fontName) const {
#ifdef USE_IMGUI
	auto it = fonts_.find(fontName);
	if (it != fonts_.end() && it->second) {
		return it->second;
	}
	auto defaultIt = fonts_.find("Default");
	if (defaultIt != fonts_.end() && defaultIt->second) {
		return defaultIt->second;
	}
	return ImGui::GetFont();
#else
	(void)fontName;
	return nullptr;
#endif
}

void ImGuiManager::DrawEditorBackgroundMask(const ImVec2& gameViewPosition, const ImVec2& gameViewSize) {
#ifdef USE_IMGUI
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	const ImVec2 viewportMin = viewport->Pos;
	const ImVec2 viewportMax = ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
	const ImVec2 gameMin = ImVec2(
	    ClampLayoutValue(gameViewPosition.x, viewportMin.x, viewportMax.x),
	    ClampLayoutValue(gameViewPosition.y, viewportMin.y, viewportMax.y)
	);
	const ImVec2 gameMax = ImVec2(
	    ClampLayoutValue(gameViewPosition.x + gameViewSize.x, viewportMin.x, viewportMax.x),
	    ClampLayoutValue(gameViewPosition.y + gameViewSize.y, viewportMin.y, viewportMax.y)
	);

	ImVec4 maskColor = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
	maskColor.w = 1.0f;
	ImDrawList* drawList = ImGui::GetBackgroundDrawList(viewport);
	const ImU32 color = ImGui::ColorConvertFloat4ToU32(maskColor);

	drawList->AddRectFilled(viewportMin, ImVec2(viewportMax.x, gameMin.y), color);
	drawList->AddRectFilled(ImVec2(viewportMin.x, gameMax.y), viewportMax, color);
	drawList->AddRectFilled(ImVec2(viewportMin.x, gameMin.y), ImVec2(gameMin.x, gameMax.y), color);
	drawList->AddRectFilled(ImVec2(gameMax.x, gameMin.y), ImVec2(viewportMax.x, gameMax.y), color);
#endif
}

bool ImGuiManager::DetectFileChanges(
	const std::filesystem::path& root,
	const std::vector<std::string>& extensions,
	std::unordered_map<std::string, std::filesystem::file_time_type>& timestamps,
	bool recursive) {
	std::error_code error;
	if (!std::filesystem::exists(root, error)) {
		return false;
	}

	bool changed = false;
	auto inspectPath = [&](const std::filesystem::path& path) {
		if (!extensions.empty()) {
			const std::string extension = StringUtility::PathToUtf8(path.extension());
			if (std::find(extensions.begin(), extensions.end(), extension) == extensions.end()) {
				return;
			}
		}
		const auto writeTime = std::filesystem::last_write_time(path, error);
		if (error) {
			error.clear();
			return;
		}
		const std::string key = StringUtility::PathToUtf8(path.lexically_normal());
		// 初回発見時は監視開始時点の基準値として登録し、起動直後の一斉リロードを防ぐ。
		auto [it, inserted] = timestamps.emplace(key, writeTime);
		if (!inserted && it->second != writeTime) {
			it->second = writeTime;
			changed = true;
		}
	};

	if (std::filesystem::is_regular_file(root, error)) {
		inspectPath(root);
		return changed;
	}
	if (recursive) {
		for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, error), end; it != end; it.increment(error)) {
			if (error) {
				error.clear();
				continue;
			}
			if (it->is_regular_file(error)) {
				inspectPath(it->path());
			}
		}
	} else {
		for (const auto& entry : std::filesystem::directory_iterator(root, error)) {
			if (entry.is_regular_file(error)) {
				inspectPath(entry.path());
			}
		}
	}
	return changed;
}

bool ImGuiManager::ReloadShaders() {
#ifdef USE_IMGUI
	if (!dxcommon) {
		return false;
	}

	hotReloadError_.clear();
	const std::filesystem::path shaderDirectory = "Resources/Shader";
	std::error_code error;
	// PSOを触る前に全HLSLを検証する。1つでも失敗した場合は現在の正常なPSOを維持する。
	for (const auto& entry : std::filesystem::recursive_directory_iterator(shaderDirectory, error)) {
		if (error || !entry.is_regular_file() || entry.path().extension() != ".hlsl") {
			continue;
		}
		const std::wstring fileName = entry.path().filename().wstring();
		const wchar_t* profile = fileName.find(L".VS.") != std::wstring::npos ? L"vs_6_0" : L"ps_6_0";
		std::string compileError;
		if (!dxcommon->TryCompileShader(entry.path().wstring(), profile, compileError)) {
			hotReloadError_ = compileError.empty() ? "Shader compilation failed." : compileError;
			hotReloadStatus_ = "Shader reload failed";
			return false;
		}
	}

	// GPUが古いPSOを参照している間にComPtrを差し替えないよう、先に完了を待つ。
	dxcommon->FlushGPU();
	// 共通HLSLを複数の描画系が共有しているため、関連するPSOをまとめて再生成する。
	SpriteCommon::GetInstance()->ReloadPipelineState();
	LineCommon::GetInstance()->ReloadPipelineState();
	Object3dCommon::GetInstance()->ReloadPipelineState();
	SkyBoxCommon::GetInstance()->ReloadPipelineState();
	ParticleManager::GetInstance()->ReloadPipelineState();
	InstancingModelCommon::GetInstance()->ReloadPipelineState();
	PostEffect::GetInstance()->ReloadPipelineState();
	hotReloadStatus_ = "Shaders reloaded";
	return true;
#else
	return false;
#endif
}

bool ImGuiManager::ReloadTextures() {
#ifdef USE_IMGUI
	if (!dxcommon) {
		return false;
	}
	// 既存SRVが参照する旧リソースを破棄する前に、GPU側の利用完了を保証する。
	dxcommon->FlushGPU();
	const size_t count = TextureManager::GetInstance()->ReloadAllTextures();
	hotReloadStatus_ = "Textures reloaded: " + std::to_string(count);
	hotReloadError_.clear();
	return true;
#else
	return false;
#endif
}

void ImGuiManager::StartCppBuild() {
#ifdef USE_IMGUI
	if (cppBuildRunning_) {
		return;
	}
	const std::filesystem::path solutionPath = std::filesystem::absolute("CG2_DirectX.sln");
	// 実行中EXEはWindowsにロックされるため、プロセスごとの別フォルダーへ出力する。
	const std::filesystem::path outputDirectory = std::filesystem::absolute(std::filesystem::path("../generated/HotReload") / std::to_string(GetCurrentProcessId()));
	std::error_code error;
	std::filesystem::create_directories(outputDirectory, error);
	rebuiltExecutablePath_ = outputDirectory / "CG2_DirectX.exe";
	hotReloadStatus_ = "Building C++ (Development)...";
	hotReloadError_.clear();
	cppBuildRunning_ = true;
	// MSBuild中もゲームループとImGuiを止めないよう、ビルドはワーカースレッドで実行する。
	cppBuildFuture_ = std::async(std::launch::async, [solutionPath, outputDirectory]() {
		// vswhere経由でVisual Studioの配置に依存せずMSBuildを解決する。
		const std::wstring script =
			L"$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'; "
			L"if (!(Test-Path $vswhere)) { exit 2 }; "
			L"$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild/**/Bin/MSBuild.exe' | Select-Object -First 1; "
			L"if (!$msbuild) { exit 3 }; "
			L"& $msbuild '" + solutionPath.wstring() + L"' /m /t:Build /p:Configuration=Development /p:Platform=x64 /p:OutDir='" + outputDirectory.wstring() + L"\\'; "
			L"exit $LASTEXITCODE";
		const std::wstring command = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"" + script + L"\"";
		return _wsystem(command.c_str());
	});
#endif
}

bool ImGuiManager::LaunchRebuiltExecutable() {
#ifdef USE_IMGUI
	if (!std::filesystem::exists(rebuiltExecutablePath_)) {
		return false;
	}
	std::wstring commandLine = L"\"" + rebuiltExecutablePath_.wstring() + L"\"";
	std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
	mutableCommandLine.push_back(L'\0');
	// Resources/... の相対パスを従来どおり解決できるよう、作業ディレクトリは引き継ぐ。
	const std::wstring workingDirectory = std::filesystem::current_path().wstring();
	STARTUPINFOW startupInfo{};
	startupInfo.cb = sizeof(startupInfo);
	PROCESS_INFORMATION processInfo{};
	const BOOL launched = CreateProcessW(
		rebuiltExecutablePath_.c_str(), mutableCommandLine.data(), nullptr, nullptr, FALSE, 0, nullptr,
		workingDirectory.c_str(), &startupInfo, &processInfo);
	if (!launched) {
		return false;
	}
	CloseHandle(processInfo.hThread);
	CloseHandle(processInfo.hProcess);
	return true;
#else
	return false;
#endif
}

bool ImGuiManager::PollCppBuild() {
#ifdef USE_IMGUI
	if (!cppBuildRunning_ || !cppBuildFuture_.valid() || cppBuildFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
		return false;
	}
	cppBuildRunning_ = false;
	const int result = cppBuildFuture_.get();
	if (result != 0) {
		hotReloadStatus_ = "C++ build failed";
		hotReloadError_ = "MSBuild exit code: " + std::to_string(result);
		return false;
	}
	if (!LaunchRebuiltExecutable()) {
		hotReloadStatus_ = "Build succeeded, restart failed";
		hotReloadError_ = "Could not launch: " + StringUtility::PathToUtf8(rebuiltExecutablePath_);
		return false;
	}
	hotReloadStatus_ = "Restarting rebuilt application...";
	// 新プロセスの起動成功後だけ旧プロセスへ通常終了を要求する。
	restartRequested_ = true;
	return true;
#else
	return false;
#endif
}

bool ImGuiManager::UpdateHotReload(const std::string& sceneJsonPath, const std::function<bool()>& reloadScene) {
#ifdef USE_IMGUI
	// futureをブロックせず確認し、ビルド中も毎フレームUIを更新する。
	PollCppBuild();

	// NTFSの再帰列挙と更新日時取得は描画スレッドを止めないよう、500msごとに非同期実行する。
	const auto now = std::chrono::steady_clock::now();
	uint32_t detectedChanges = 0;
	if (hotReloadScanRunning_ && hotReloadScanFuture_.valid() &&
		hotReloadScanFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
		detectedChanges = hotReloadScanFuture_.get();
		hotReloadScanRunning_ = false;
	}
	if (!hotReloadScanRunning_ && now >= nextHotReloadScanTime_) {
		nextHotReloadScanTime_ = now + std::chrono::milliseconds(500);
		hotReloadScanRunning_ = true;
		hotReloadScanFuture_ = std::async(std::launch::async, [this, sceneJsonPath]() {
			uint32_t changes = 0;
			if (DetectFileChanges("Resources/Shader", {".hlsl", ".hlsli"}, shaderTimestamps_)) changes |= 1u << 0;
			if (!sceneJsonPath.empty() && DetectFileChanges(sceneJsonPath, {".json"}, sceneTimestamps_)) changes |= 1u << 1;
			if (DetectFileChanges("Resources", {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds"}, textureTimestamps_)) changes |= 1u << 2;
			if (DetectFileChanges("Resources/Data/enemy_status.json", {".json"}, enemyStatusTimestamps_)) changes |= 1u << 3;
			bool sourceChanged = false;
			sourceChanged |= DetectFileChanges("Engine", {".h", ".hpp", ".cpp"}, cppTimestamps_);
			sourceChanged |= DetectFileChanges("Player", {".h", ".hpp", ".cpp"}, cppTimestamps_);
			sourceChanged |= DetectFileChanges("scene", {".h", ".hpp", ".cpp"}, cppTimestamps_);
			sourceChanged |= DetectFileChanges("main.cpp", {".cpp"}, cppTimestamps_);
			if (sourceChanged) changes |= 1u << 4;
			return changes;
		});
	}

	const bool shaderChanged = (detectedChanges & (1u << 0)) != 0;
	const bool sceneChanged = (detectedChanges & (1u << 1)) != 0;
	const bool textureChanged = (detectedChanges & (1u << 2)) != 0;
	const bool enemyStatusChanged = (detectedChanges & (1u << 3)) != 0;
	const bool cppChanged = (detectedChanges & (1u << 4)) != 0;
	if (enemyStatusChanged) {
		InvalidateEnemyStatsCache();
	}

	// ファイル変更による自動実行と、下のImGuiボタンによる手動実行は同じ処理へ集約する。
	if (autoReloadShaders_ && shaderChanged) {
		ReloadShaders();
	}
	if (autoReloadScene_ && sceneChanged && reloadScene) {
		hotReloadStatus_ = reloadScene() ? "Scene JSON reloaded" : "Scene JSON reload skipped";
	}
	if (autoReloadTextures_ && textureChanged) {
		ReloadTextures();
	}
	if (autoReloadCpp_ && cppChanged && !cppBuildRunning_) {
		StartCppBuild();
	}

	if (ImGui::Begin("Hot Reload")) {
		ImGui::Checkbox("Auto HLSL", &autoReloadShaders_);
		ImGui::SameLine();
		if (ImGui::Button("Reload HLSL")) {
			ReloadShaders();
		}
		ImGui::Checkbox("Auto Scene JSON", &autoReloadScene_);
		ImGui::SameLine();
		if (ImGui::Button("Reload Scene") && reloadScene) {
			hotReloadStatus_ = reloadScene() ? "Scene JSON reloaded" : "Scene JSON reload skipped";
		}
		ImGui::Checkbox("Auto Textures", &autoReloadTextures_);
		ImGui::SameLine();
		if (ImGui::Button("Reload Textures")) {
			ReloadTextures();
		}
		ImGui::Checkbox("Auto C++ build + restart", &autoReloadCpp_);
		ImGui::SameLine();
		if (cppBuildRunning_) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button(cppBuildRunning_ ? "Building..." : "Build C++ & Restart")) {
			StartCppBuild();
		}
		if (cppBuildRunning_) {
			ImGui::EndDisabled();
		}
		ImGui::Separator();
		ImGui::TextWrapped("Status: %s", hotReloadStatus_.c_str());
		if (!hotReloadError_.empty()) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
			ImGui::TextWrapped("%s", hotReloadError_.c_str());
			ImGui::PopStyleColor();
		}
	}
	ImGui::End();
	return restartRequested_;
#else
	(void)sceneJsonPath;
	(void)reloadScene;
	return false;
#endif
}

void ImGuiManager::End() {
#ifdef USE_IMGUI
	ImGui::Render();
#endif
}

/// <summary>
/// 現在の状態をもとに描画処理を行います。
/// </summary>
void ImGuiManager::Draw() {
#ifdef USE_IMGUI
	auto commandList = dxcommon->GetCommandList();

	ID3D12DescriptorHeap* ppHeaps[] = { SrvManager::GetInstance()->GetDescriptorHeap().Get() };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	// ============================

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

#endif
}
