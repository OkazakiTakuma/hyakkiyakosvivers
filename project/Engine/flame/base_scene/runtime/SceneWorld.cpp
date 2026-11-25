#include "base_scene/runtime/SceneWorld.h"

void SceneWorld::Update() {
	for (const auto& entity : entities_) {
		entity->Update();
	}
}

void SceneWorld::Draw2D() {
	for (const auto& entity : entities_) {
		entity->Draw2D();
	}
}

void SceneWorld::Draw3D() {
	for (const auto& entity : entities_) {
		entity->Draw3D();
	}
}

void SceneWorld::Clear() {
	entities_.clear();
	componentIndex_.clear();
	nameIndex_.clear();
}

Entity* SceneWorld::FindByName(const std::string& name) const {
	// 空名を検索対象にすると未設定Entityへ誤接続するため、明示的に失敗させる。
	if (name.empty()) {
		return nullptr;
	}
	const auto found = nameIndex_.find(name);
	return found != nameIndex_.end() ? found->second : nullptr;
}

void SceneWorld::push_back(std::unique_ptr<Entity> entity) {
	if (!entity) return;
	Entity* rawEntity = entity.get();
	// World所属後の名前・Component変更だけを通知し、生成途中の再構築を避ける。
	rawEntity->indexChangedCallback_ = [this]() { RebuildIndexes(); };
	entities_.push_back(std::move(entity));
	IndexEntity(rawEntity);
}

SceneWorld::iterator SceneWorld::erase(iterator position) {
	const iterator next = entities_.erase(position);
	RebuildIndexes();
	return next;
}

SceneWorld::iterator SceneWorld::erase(iterator first, iterator last) {
	const iterator next = entities_.erase(first, last);
	RebuildIndexes();
	return next;
}

void SceneWorld::IndexEntity(Entity* entity) {
	if (!entity) return;
	nameIndex_.insert_or_assign(entity->GetName(), entity);
	for (const auto& component : entity->components_) {
		componentIndex_[std::type_index(typeid(*component))].push_back(entity);
	}
}

void SceneWorld::RebuildIndexes() {
	componentIndex_.clear();
	nameIndex_.clear();
	for (const auto& entity : entities_) {
		IndexEntity(entity.get());
	}
}

std::string SceneWorld::MakeUniqueName(const std::string& baseName) const {
	// 空名には保存・検索で扱える既定名を与える。
	const std::string resolvedBaseName = baseName.empty() ? "Entity" : baseName;
	if (!FindByName(resolvedBaseName)) {
		return resolvedBaseName;
	}
	// 元の名前を1個目として扱い、重複時は _2 から順に空きを探す。
	for (int suffix = 2;; ++suffix) {
		const std::string candidate = resolvedBaseName + "_" + std::to_string(suffix);
		if (!FindByName(candidate)) {
			return candidate;
		}
	}
}
