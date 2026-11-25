#include "FontManager.h"

#include <Windows.h>
#include <filesystem>

#pragma comment(lib, "dwrite.lib")

FontManager* FontManager::GetInstance() {
	static FontManager instance;
	return &instance;
}

bool FontManager::LoadFromDirectory(const std::wstring& directory) {
	namespace fs = std::filesystem;
	collection_.Reset();
	if (!fs::is_directory(directory)) {
		return false;
	}

	// DirectWriteに同梱ファイルを直接渡し、OSへのフォントのインストールを不要にする。
	using Microsoft::WRL::ComPtr;
	ComPtr<IDWriteFactory5> factory;
	if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory5),
		reinterpret_cast<IUnknown**>(factory.GetAddressOf())))) {
		return false;
	}
	ComPtr<IDWriteFontSetBuilder1> builder;
	if (FAILED(factory->CreateFontSetBuilder(&builder))) {
		return false;
	}

	for (const fs::directory_entry& entry : fs::directory_iterator(directory)) {
		if (!entry.is_regular_file()) {
			continue;
		}
		const std::wstring extension = entry.path().extension().wstring();
		// TrueType、OpenType、TrueType Collectionを配布フォントとして扱う。
		if (extension != L".ttf" && extension != L".otf" && extension != L".ttc") {
			continue;
		}

		ComPtr<IDWriteFontFile> file;
		if (SUCCEEDED(factory->CreateFontFileReference(entry.path().c_str(), nullptr, &file))) {
			builder->AddFontFile(file.Get());
		}
	}
	ComPtr<IDWriteFontSet> fontSet;
	return SUCCEEDED(builder->CreateFontSet(&fontSet)) &&
		fontSet->GetFontCount() > 0 &&
		SUCCEEDED(factory->CreateFontCollectionFromFontSet(fontSet.Get(), &collection_));
}

void FontManager::Finalize() {
	collection_.Reset();
}
