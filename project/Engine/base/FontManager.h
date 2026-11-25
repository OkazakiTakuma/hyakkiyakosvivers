#pragma once

#include <string>
#include <dwrite_3.h>
#include <wrl/client.h>

/// <summary>Resources/Fonts 内のフォントをDirectWriteの専用コレクションに読み込みます。</summary>
class FontManager {
public:
	static FontManager* GetInstance();
	bool LoadFromDirectory(const std::wstring& directory);
	IDWriteFontCollection* GetCollection() const { return collection_.Get(); }
	void Finalize();

private:
	FontManager() = default;
	FontManager(const FontManager&) = delete;
	FontManager& operator=(const FontManager&) = delete;
	Microsoft::WRL::ComPtr<IDWriteFontCollection1> collection_;
};
