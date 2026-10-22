#pragma once
#include "core/Component.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "Vector.h"
#include <Windows.h>
#include <dwrite.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h>

#pragma comment(lib, "dwrite.lib")

/// <summary>DirectWriteで文字列をRGBAテクスチャへ変換し、Spriteとして2D描画します。</summary>
class TextComponent : public Component {
public:
	TextComponent()
		: runtimeTextureKey_("__runtime_text_component_" + std::to_string(nextRuntimeTextureId_.fetch_add(1, std::memory_order_relaxed))) {}

	enum class Anchor {
		TopLeft,
		TopCenter,
		TopRight,
		CenterLeft,
		Center,
		CenterRight,
		BottomLeft,
		BottomCenter,
		BottomRight
	};

	void Draw2D() override {
		// 文字やフォントの変更時だけテクスチャを再生成し、通常フレームの負荷を抑える。
		if (text_.empty() || !GetOwner()) {
			return;
		}
		EnsureTextSprite();
		if (!textSprite_) {
			return;
		}
		const EulerTransform& transform = GetOwner()->GetTransform();
		EulerTransform spriteTransform = textSprite_->GetTransform();
		spriteTransform.translate = transform.translate;
		textSprite_->SetTransform(spriteTransform);
		textSprite_->SetAnchorPoint(AnchorToRate(anchor_));
		textSprite_->SetColor(color_);
		textSprite_->Update();
		SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
		textSprite_->Draw();
	}

	void SetText(const std::string& text) { if (text_ != text) { text_ = text; isTextureDirty_ = true; } }
	const std::string& GetText() const { return text_; }
	void SetFontName(const std::string& fontName) { const std::string value = fontName.empty() ? "Default" : fontName; if (fontName_ != value) { fontName_ = value; isTextureDirty_ = true; } }
	const std::string& GetFontName() const { return fontName_; }
	void SetFontSize(float fontSize) { const float value = fontSize < 1.0f ? 1.0f : fontSize; if (fontSize_ != value) { fontSize_ = value; isTextureDirty_ = true; } }
	float GetFontSize() const { return fontSize_; }
	/// <summary>文字を収める最大領域を指定します。幅を指定すると自動改行し、高さを超える場合はフォントを縮小します。</summary>
	void SetLayoutBounds(float maxWidth, float maxHeight) {
		const float width = maxWidth > 0.0f ? maxWidth : 4096.0f;
		const float height = maxHeight > 0.0f ? maxHeight : 4096.0f;
		if (layoutMaxWidth_ != width || layoutMaxHeight_ != height) {
			layoutMaxWidth_ = width;
			layoutMaxHeight_ = height;
			isTextureDirty_ = true;
		}
	}
	void SetColor(const Vector4& color) { color_ = color; }
	const Vector4& GetColor() const { return color_; }
	void SetAnchor(Anchor anchor) { anchor_ = anchor; }
	Anchor GetAnchor() const { return anchor_; }

	static Vector2 AnchorToRate(Anchor anchor) {
		switch (anchor) {
		case Anchor::TopCenter:
			return {0.5f, 0.0f};
		case Anchor::TopRight:
			return {1.0f, 0.0f};
		case Anchor::CenterLeft:
			return {0.0f, 0.5f};
		case Anchor::Center:
			return {0.5f, 0.5f};
		case Anchor::CenterRight:
			return {1.0f, 0.5f};
		case Anchor::BottomLeft:
			return {0.0f, 1.0f};
		case Anchor::BottomCenter:
			return {0.5f, 1.0f};
		case Anchor::BottomRight:
			return {1.0f, 1.0f};
		case Anchor::TopLeft:
		default:
			return {0.0f, 0.0f};
		}
	}
	void Finalize() override { textSprite_.reset(); }

private:
	/// <summary>DirectWriteの文字描画結果をBitmapRenderTargetへ渡すレンダラーです。</summary>
	class BitmapTextRenderer final : public IDWriteTextRenderer {
	public:
		// DirectWriteのレイアウト描画を、既存のSpriteへ渡せるビットマップへ変換する。
		explicit BitmapTextRenderer(IDWriteBitmapRenderTarget* target, IDWriteRenderingParams* renderingParams) {
			target_ = target;
			renderingParams_ = renderingParams;
		}
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override {
			if (!object) return E_POINTER;
			*object = nullptr;
			if (riid == __uuidof(IUnknown) || riid == __uuidof(IDWriteTextRenderer)) {
				*object = static_cast<IDWriteTextRenderer*>(this);
				AddRef();
				return S_OK;
			}
			if (riid == __uuidof(IDWritePixelSnapping)) {
				*object = static_cast<IDWritePixelSnapping*>(this);
				AddRef();
				return S_OK;
			}
			return E_NOINTERFACE;
		}
		ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount_; }
		ULONG STDMETHODCALLTYPE Release() override {
			const ULONG value = --refCount_;
			if (value == 0) delete this;
			return value;
		}
		HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) override {
			if (!disabled) return E_POINTER;
			*disabled = FALSE;
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* transform) override {
			if (!transform) return E_POINTER;
			transform->m11 = 1.0f; transform->m12 = 0.0f;
			transform->m21 = 0.0f; transform->m22 = 1.0f;
			transform->dx = 0.0f; transform->dy = 0.0f;
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixelsPerDip) override {
			if (!pixelsPerDip) return E_POINTER;
			*pixelsPerDip = 1.0f;
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT baselineOriginX, FLOAT baselineOriginY,
			DWRITE_MEASURING_MODE measuringMode, const DWRITE_GLYPH_RUN* glyphRun,
			const DWRITE_GLYPH_RUN_DESCRIPTION* glyphRunDescription, IUnknown*) override {
			// DirectWriteのグリフ単位の描画結果をGDI互換のターゲットへ書き込む。
			return target_->DrawGlyphRun(baselineOriginX, baselineOriginY,
				measuringMode, glyphRun, renderingParams_.Get(), RGB(255, 255, 255), nullptr);
		}
		HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) override { return S_OK; }
		HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) override { return S_OK; }
		HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) override { return S_OK; }

	private:
		std::atomic<ULONG> refCount_{1};
		Microsoft::WRL::ComPtr<IDWriteBitmapRenderTarget> target_;
		Microsoft::WRL::ComPtr<IDWriteRenderingParams> renderingParams_;
	};

	static IDWriteFactory* GetDirectWriteFactory() {
		// DirectWriteのファクトリは全TextComponentで共有し、毎回の生成コストを避ける。
		static Microsoft::WRL::ComPtr<IDWriteFactory> factory;
		static const HRESULT result = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(factory.GetAddressOf()));
		return SUCCEEDED(result) ? factory.Get() : nullptr;
	}

	/// <summary>UTF-8文字列をWindows描画APIで使用するUTF-16へ変換します。</summary>
	static std::wstring ToWideString(const std::string& text) {
		if (text.empty()) return {};
		const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
		if (length <= 0) return {};
		std::wstring result(static_cast<size_t>(length), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), length);
		return result;
	}

	/// <summary>設定変更時にDirectWriteで文字テクスチャと描画Spriteを再構築します。</summary>
	void EnsureTextSprite() {
		if (!isTextureDirty_ && textSprite_) return;
		const std::wstring wideText = ToWideString(text_);
		if (wideText.empty()) {
			textSprite_.reset();
			isTextureDirty_ = false;
			return;
		}
		// DirectWriteはフォント名からフォントフェイスを解決するため、
		// FontManagerが登録済みのリソースフォントも通常のファミリー名で利用できる。
		IDWriteFactory* factory = GetDirectWriteFactory();
		if (!factory) return;
		const std::wstring requestedFont = fontName_ == "Default" ? L"Meiryo" : ToWideString(fontName_);
		Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
		DWRITE_TEXT_METRICS metrics{};
		// 指定領域がある文字は折り返し、それでも縦に収まらない場合だけ少しずつ縮小する。
		const bool hasLayoutBounds = layoutMaxWidth_ < 4096.0f || layoutMaxHeight_ < 4096.0f;
		const float minimumFontSize = (std::min)(fontSize_, 8.0f);
		for (float fittedFontSize = fontSize_; fittedFontSize >= minimumFontSize; fittedFontSize -= 1.0f) {
			Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
			if (FAILED(factory->CreateTextFormat(requestedFont.c_str(), nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL, fittedFontSize, L"ja-jp", &format))) return;
			// 日本語や空白を含まない長い名前でも欄外へ出ないよう、領域指定時は文字単位で改行する。
			format->SetWordWrapping(hasLayoutBounds ? DWRITE_WORD_WRAPPING_CHARACTER : DWRITE_WORD_WRAPPING_NO_WRAP);
			Microsoft::WRL::ComPtr<IDWriteTextLayout> candidate;
			if (FAILED(factory->CreateTextLayout(wideText.c_str(), static_cast<UINT32>(wideText.size()), format.Get(),
				layoutMaxWidth_, 4096.0f, &candidate))) return;
			DWRITE_TEXT_METRICS candidateMetrics{};
			if (FAILED(candidate->GetMetrics(&candidateMetrics))) return;
			layout = candidate;
			metrics = candidateMetrics;
			if (!hasLayoutBounds || candidateMetrics.height <= layoutMaxHeight_ || fittedFontSize <= minimumFontSize) break;
		}
		// 先にレイアウトを計測し、文字列に必要な大きさだけのRenderTargetを確保する。
		const int width = (std::max)(1L, static_cast<LONG>(std::ceil(metrics.widthIncludingTrailingWhitespace)) + 6);
		const int height = (std::max)(1L, static_cast<LONG>(std::ceil(metrics.height)) + 6);
		Microsoft::WRL::ComPtr<IDWriteGdiInterop> gdiInterop;
		if (FAILED(factory->GetGdiInterop(&gdiInterop))) return;
		Microsoft::WRL::ComPtr<IDWriteBitmapRenderTarget> renderTarget;
		if (FAILED(gdiInterop->CreateBitmapRenderTarget(nullptr, width, height, &renderTarget))) return;
		Microsoft::WRL::ComPtr<IDWriteRenderingParams> renderingParams;
		if (FAILED(factory->CreateRenderingParams(&renderingParams))) return;
		// BitmapRenderTargetはDirectWriteで描画できるメモリDCを提供する。
		HDC renderDc = renderTarget->GetMemoryDC();
		PatBlt(renderDc, 0, 0, width, height, BLACKNESS);
		BitmapTextRenderer* renderer = new BitmapTextRenderer(renderTarget.Get(), renderingParams.Get());
		const HRESULT drawResult = layout->Draw(nullptr, renderer, 3.0f, 3.0f);
		renderer->Release();
		if (FAILED(drawResult)) return;

		// BitmapRenderTargetの内容を32bit DIBへコピーし、GPU転送用RGBAへ変換する。
		// Sprite/TextureManagerを既存のまま利用するため、ここがDirectWriteと2D描画の境界になる。
		HDC copyDc = CreateCompatibleDC(renderDc);
		BITMAPINFO bitmapInfo{};
		bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bitmapInfo.bmiHeader.biWidth = width;
		bitmapInfo.bmiHeader.biHeight = -height;
		bitmapInfo.bmiHeader.biPlanes = 1;
		bitmapInfo.bmiHeader.biBitCount = 32;
		bitmapInfo.bmiHeader.biCompression = BI_RGB;
		void* bitmapPixels = nullptr;
		HBITMAP bitmap = CreateDIBSection(copyDc, &bitmapInfo, DIB_RGB_COLORS, &bitmapPixels, nullptr, 0);
		if (!bitmap || !bitmapPixels) { if (bitmap) DeleteObject(bitmap); DeleteDC(copyDc); return; }
		HGDIOBJ previousBitmap = SelectObject(copyDc, bitmap);
		PatBlt(copyDc, 0, 0, width, height, BLACKNESS);
		BitBlt(copyDc, 0, 0, width, height, renderDc, 0, 0, SRCCOPY);

		std::vector<uint8_t> rgbaPixels(static_cast<size_t>(width) * height * 4);
		const uint8_t* bgraPixels = static_cast<const uint8_t*>(bitmapPixels);
		for (size_t pixel = 0; pixel < static_cast<size_t>(width) * height; ++pixel) {
			const uint8_t coverage = (std::max)({bgraPixels[pixel * 4], bgraPixels[pixel * 4 + 1], bgraPixels[pixel * 4 + 2]});
			rgbaPixels[pixel * 4] = 255;
			rgbaPixels[pixel * 4 + 1] = 255;
			rgbaPixels[pixel * 4 + 2] = 255;
			rgbaPixels[pixel * 4 + 3] = coverage;
		}

		SelectObject(copyDc, previousBitmap);
		DeleteObject(bitmap);
		DeleteDC(copyDc);

		TextureManager::GetInstance()->CreateTextureFromRGBA(runtimeTextureKey_, static_cast<uint32_t>(width), static_cast<uint32_t>(height), rgbaPixels);
		textSprite_ = std::make_unique<Sprite>();
		textSprite_->Initialize(runtimeTextureKey_);
		textSprite_->SetSize({static_cast<float>(width), static_cast<float>(height)});
		isTextureDirty_ = false;
	}

	/// <summary>次回のテクスチャ生成に使用する表示文字列です。</summary>
	std::string text_ = "Text";
	std::string fontName_ = "Default";
	float fontSize_ = 32.0f;
	float layoutMaxWidth_ = 4096.0f;
	float layoutMaxHeight_ = 4096.0f;
	Vector4 color_ = {1.0f, 1.0f, 1.0f, 1.0f};
	Anchor anchor_ = Anchor::TopLeft;
	/// <summary>文字テクスチャの再生成が必要かを表します。</summary>
	bool isTextureDirty_ = true;
	/// <summary>生成済み文字テクスチャを表示するSpriteです。</summary>
	std::unique_ptr<Sprite> textSprite_;
	std::string runtimeTextureKey_;
	static inline std::atomic<uint64_t> nextRuntimeTextureId_ = 0;
};
