#pragma once

#include "core/GameObject.h"
#include "../../2d/Sprite.h"
#include "../../2d/TextComponent.h"

#include <memory>
#include <string>

/// <summary>
/// シーン固有UIで繰り返し使う、テキストと単色矩形の生成・描画処理を提供します。
/// </summary>
namespace SceneUiUtility {

inline constexpr const char* kColorSpriteTexturePath = "Resources/human/white.png";

inline std::unique_ptr<Sprite> CreateColorSprite() {
	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize(kColorSpriteTexturePath);
	return sprite;
}

inline std::unique_ptr<GameObject> CreateTextObject(
	const std::string& text,
	float fontSize,
	TextComponent::Anchor anchor = TextComponent::Anchor::Center,
	const std::string& fontName = {}) {
	auto object = std::make_unique<GameObject>();
	TextComponent* textComponent = object->AddComponent<TextComponent>();
	textComponent->SetText(text);
	if (!fontName.empty()) {
		textComponent->SetFontName(fontName);
	}
	textComponent->SetFontSize(fontSize);
	textComponent->SetAnchor(anchor);
	return object;
}

inline std::unique_ptr<GameObject> CreateTextObject(
	const std::string& text,
	float fontSize,
	const Vector4& color,
	const std::string& fontName = {}) {
	auto object = CreateTextObject(text, fontSize, TextComponent::Anchor::Center, fontName);
	object->GetComponent<TextComponent>()->SetColor(color);
	return object;
}

inline void DrawColorSprite(
	Sprite* sprite,
	float x,
	float y,
	float width,
	float height,
	const Vector4& color) {
	if (!sprite) {
		return;
	}
	EulerTransform transform = sprite->GetTransform();
	transform.translate = {x, y, 0.0f};
	transform.rotate.z = 0.0f;
	sprite->SetAnchorPoint({0.0f, 0.0f});
	sprite->SetTransform(transform);
	sprite->SetSize({width, height});
	sprite->SetColor(color);
	sprite->Update();
	sprite->Draw();
}

inline void DrawRotatedColorSprite(
	Sprite* sprite,
	float centerX,
	float centerY,
	float width,
	float height,
	float rotation,
	const Vector4& color) {
	if (!sprite) {
		return;
	}
	EulerTransform transform = sprite->GetTransform();
	transform.translate = {centerX, centerY, 0.0f};
	transform.rotate.z = rotation;
	sprite->SetAnchorPoint({0.5f, 0.5f});
	sprite->SetTransform(transform);
	sprite->SetSize({width, height});
	sprite->SetColor(color);
	sprite->Update();
	sprite->Draw();
}

} // namespace SceneUiUtility
