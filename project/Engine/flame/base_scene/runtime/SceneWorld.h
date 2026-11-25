#pragma once

#include "core/Entity.h"

#include <cstddef>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

/// <summary>
/// シーン内Entityの所有者です。生成物の寿命と共通更新・描画・検索の窓口を一か所にまとめます。
/// </summary>
class SceneWorld final {
public:
	/// <summary>Entityを所有する内部コンテナ型です。</summary>
	using Storage = std::vector<std::unique_ptr<Entity>>;
	using iterator = Storage::iterator;
	using const_iterator = Storage::const_iterator;

	/// <summary>所有するすべてのEntityを更新します。</summary>
	void Update();
	/// <summary>所有するすべてのEntityの2D描画を行います。</summary>
	void Draw2D();
	/// <summary>所有するすべてのEntityの3D描画を行います。</summary>
	void Draw3D();
	/// <summary>所有するEntityをすべて破棄します。</summary>
	void Clear();

	/// <summary>名前が完全一致するEntityを検索し、見つからなければnullptrを返します。</summary>
	Entity* FindByName(const std::string& name) const;
	/// <summary>既存名と重複しない名前を、連番付きで生成します。</summary>
	std::string MakeUniqueName(const std::string& baseName) const;
	/// <summary>指定Component型を持つEntityだけを、World全走査なしで返します。</summary>
	template<class T>
	const std::vector<Entity*>& GetEntitiesWith() const {
		static const std::vector<Entity*> empty;
		const auto found = componentIndex_.find(std::type_index(typeid(T)));
		return found != componentIndex_.end() ? found->second : empty;
	}

	// 既存の各Systemを段階的に移す間だけ使用する限定的なコンテナ操作です。
	// Entityの所有権自体は常にSceneWorldに残ります。
	iterator begin() { return entities_.begin(); }
	iterator end() { return entities_.end(); }
	const_iterator begin() const { return entities_.begin(); }
	const_iterator end() const { return entities_.end(); }
	bool empty() const { return entities_.empty(); }
	std::size_t size() const { return entities_.size(); }
	std::unique_ptr<Entity>& back() { return entities_.back(); }
	const std::unique_ptr<Entity>& back() const { return entities_.back(); }
	std::unique_ptr<Entity>& operator[](std::size_t index) { return entities_[index]; }
	const std::unique_ptr<Entity>& operator[](std::size_t index) const { return entities_[index]; }
	void push_back(std::unique_ptr<Entity> entity);
	iterator erase(iterator position);
	iterator erase(iterator first, iterator last);
	void clear() { Clear(); }

private:
	void IndexEntity(Entity* entity);
	void RebuildIndexes();
	// unique_ptrで所有し、Entityの寿命をSceneWorldへ一本化する。
	Storage entities_;
	/// <summary>Componentの実型から、その型を持つEntity一覧を引く実行時索引です。</summary>
	std::unordered_map<std::type_index, std::vector<Entity*>> componentIndex_;
	std::unordered_map<std::string, Entity*> nameIndex_;
};
