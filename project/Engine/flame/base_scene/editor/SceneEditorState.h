#pragma once

#include <cstddef>
#include <string>

/// <summary>エディタの選択対象と、エディタから指定されたアクティブカメラを管理します。</summary>
class SceneEditorState final {
public:
	/// <summary>オブジェクト選択とアクティブカメラ指定を解除します。</summary>
	void Reset() {
		selectedObjectIndex_ = -1;
		activeCameraObjectName_.clear();
	}

	/// <summary>選択インデックスが現在のEntity数の範囲内かを返します。</summary>
	bool HasValidSelection(std::size_t objectCount) const {
		return selectedObjectIndex_ >= 0 &&
			selectedObjectIndex_ < static_cast<int>(objectCount);
	}
	int GetSelectedObjectIndex() const { return selectedObjectIndex_; }
	void SelectObject(int index) { selectedObjectIndex_ = index; }
	void ClearSelection() { selectedObjectIndex_ = -1; }
	/// <summary>末尾Entityを選択し、空なら未選択へ戻します。</summary>
	void SelectLastOrClear(std::size_t objectCount) {
		selectedObjectIndex_ = objectCount == 0 ? -1 : static_cast<int>(objectCount) - 1;
	}
	/// <summary>Entity削除後も選択インデックスが範囲外にならないよう補正します。</summary>
	void ClampSelection(std::size_t objectCount) {
		if (objectCount == 0) {
			selectedObjectIndex_ = -1;
		} else if (selectedObjectIndex_ >= static_cast<int>(objectCount)) {
			selectedObjectIndex_ = static_cast<int>(objectCount) - 1;
		}
	}

	const std::string& GetActiveCameraName() const { return activeCameraObjectName_; }
	bool HasActiveCamera() const { return !activeCameraObjectName_.empty(); }
	void SetActiveCameraName(const std::string& name) { activeCameraObjectName_ = name; }
	void ClearActiveCamera() { activeCameraObjectName_.clear(); }

private:
	// Entityへのポインタではなくインデックスと名前を保持し、再読み込み後の無効参照を避ける。
	int selectedObjectIndex_ = -1;
	std::string activeCameraObjectName_;
};
