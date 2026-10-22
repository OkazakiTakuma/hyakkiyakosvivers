#pragma once

// BaseScene のエディター処理を複数の翻訳単位へ分割するための内部実装です。
// flame 外部から直接参照しないでください。

#include "base_scene/BaseScene.h"
#include "StringUtility.h"
#include "helpers/BaseSceneEditorGeometry.h"
#include "helpers/BaseSceneResourceCatalog.h"
#include "repositories/EnemyStatusRepository.h"
#include "repositories/ParticlePresetRepository.h"
#include "repositories/PlayerStatusRepository.h"

#include <array>

#ifdef USE_IMGUI
#include "../../../../../imgui/ImGuizmo.h"
#include <cctype>
#include <limits>

namespace {
constexpr float kProjectThumbnailSize = 64.0f;

/// <summary>Inspectorへ公開するComponentの取得方法と共通UI機能をまとめた型消去登録です。</summary>
struct EditorComponentRegistration {
	const char* label;
	Component* (*find)(GameObject* object);
	bool supportsGravity;
};

template<class T>
Component* FindEditorComponent(GameObject* object) {
	return object ? object->GetComponent<T>() : nullptr;
}

const std::array<EditorComponentRegistration, 13>& GetEditorComponentRegistrations() {
	// 新しいComponentをInspectorへ追加するときは、この表へ1行登録するだけで
	// 選択リスト・有効状態・共通重力UIへ同時に反映される。
	static const std::array<EditorComponentRegistration, 13> registrations = {{
		{"Object3d", &FindEditorComponent<Object3dComponent>, true},
		{"PointLight", &FindEditorComponent<PointLightComponent>, false},
		{"GlowBillboard", &FindEditorComponent<GlowBillboardComponent>, false},
		{"Sprite", &FindEditorComponent<SpriteComponent>, true},
		{"Text", &FindEditorComponent<TextComponent>, true},
		{"Camera", &FindEditorComponent<CameraComponent>, true},
		{"ParticleEmitter", &FindEditorComponent<ParticleEmitterComponent>, true},
		{"Player", &FindEditorComponent<Player>, true},
		{"PlayerAttack", &FindEditorComponent<PlayerAttackComponent>, true},
		{"EnemySpawnPoint", &FindEditorComponent<EnemySpawnPointComponent>, true},
		{"Enemy", &FindEditorComponent<EnemyComponent>, true},
		{"OBBCollider", &FindEditorComponent<OBBColliderComponent>, true},
		{"SphereCollider", &FindEditorComponent<SphereColliderComponent>, true}
	}};
	return registrations;
}

bool InputTextMultilineString(const char* label, std::string& value, const ImVec2& size = ImVec2(0.0f, 72.0f)) {
	std::array<char, 1024> buffer{};
	const size_t copyLength = (std::min)(value.size(), buffer.size() - 1);
	std::memcpy(buffer.data(), value.data(), copyLength);
	if (!ImGui::InputTextMultiline(label, buffer.data(), buffer.size(), size)) {
		return false;
	}
	value = buffer.data();
	return true;
}

bool SelectionTextureCombo(const char* label, std::string& textureFilePath) {
	const std::vector<std::string> textures = CollectResourceTexturePaths();
	std::vector<std::string> labels;
	labels.reserve(textures.size() + 1);
	labels.push_back("None");
	labels.insert(labels.end(), textures.begin(), textures.end());
	int selectedIndex = 0;
	for (int index = 0; index < static_cast<int>(textures.size()); ++index) {
		if (textures[index] == textureFilePath) {
			selectedIndex = index + 1;
			break;
		}
	}
	const std::vector<const char*> labelPointers = MakeLabelPointers(labels);
	if (!ImGui::Combo(label, &selectedIndex, labelPointers.data(), static_cast<int>(labelPointers.size()))) {
		return false;
	}
	textureFilePath = selectedIndex == 0 ? "" : textures[selectedIndex - 1];
	return true;
}

void DrawProjectAssetDragSource(const std::string& label, const char* payloadType, const std::string& path) {
	ImGui::Selectable(label.c_str(), false);
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
		ImGui::SetDragDropPayload(payloadType, path.c_str(), path.size() + 1);
		ImGui::Text("%s", label.c_str());
		ImGui::EndDragDropSource();
	}
}

void DrawProjectTextureDragSource(const std::string& texturePath) {
	TextureManager* textureManager = TextureManager::GetInstance();
	if (!textureManager->LoadTexture(texturePath)) {
		ImGui::TextDisabled("%s (load failed)", texturePath.c_str());
		return;
	}
	const D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = textureManager->GetSRVHandleGPU(texturePath);
	const DirectX::TexMetadata& metadata = textureManager->GetTextureMetadata(texturePath);
	const float width = static_cast<float>(metadata.width);
	const float height = static_cast<float>(metadata.height);
	const float maxSide = (std::max)(width, height);
	const ImVec2 imageSize = maxSide > 0.0f
	    ? ImVec2(kProjectThumbnailSize * width / maxSide, kProjectThumbnailSize * height / maxSide)
	    : ImVec2(kProjectThumbnailSize, kProjectThumbnailSize);
	const std::string fileName = StringUtility::PathToUtf8(StringUtility::Utf8ToPath(texturePath).filename());
	const std::string buttonId = "##SpriteTexture_" + texturePath;

	ImGui::BeginGroup();
	ImGui::PushID(texturePath.c_str());
	ImGui::BeginChild("ThumbnailFrame", ImVec2(kProjectThumbnailSize + 12.0f, kProjectThumbnailSize + 12.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	const ImVec2 cursor = ImGui::GetCursorPos();
	ImGui::SetCursorPos(ImVec2(
	    cursor.x + (kProjectThumbnailSize - imageSize.x) * 0.5f,
	    cursor.y + (kProjectThumbnailSize - imageSize.y) * 0.5f
	));
	ImGui::ImageButton(buttonId.c_str(), static_cast<ImTextureID>(textureHandle.ptr), imageSize);
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
		ImGui::SetDragDropPayload("CG2_ASSET_SPRITE", texturePath.c_str(), texturePath.size() + 1);
		ImGui::Image(static_cast<ImTextureID>(textureHandle.ptr), imageSize);
		ImGui::Text("%s", texturePath.c_str());
		ImGui::EndDragDropSource();
	}
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Image(static_cast<ImTextureID>(textureHandle.ptr), ImVec2(kProjectThumbnailSize * 2.0f, kProjectThumbnailSize * 2.0f));
		ImGui::Text("%s", texturePath.c_str());
		ImGui::Text("%.0f x %.0f", width, height);
		ImGui::EndTooltip();
	}
	ImGui::EndChild();
	ImGui::TextWrapped("%s", fileName.c_str());
	ImGui::PopID();
	ImGui::EndGroup();
}

bool IsSpriteTexturePath(const std::string& path) {
	std::string extension = StringUtility::PathToUtf8(StringUtility::Utf8ToPath(path).extension());
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
		return static_cast<char>(std::tolower(value));
	});
	return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
}
}
#endif


