#pragma once

#include "data/ParticlePresetData.h"

#include <string>
#include <vector>

/// <summary>パーティクルプリセットのJSON読込と保存だけを担当します。</summary>
namespace ParticlePresetRepository {

inline constexpr const char* kFilePath = "Resources/Data/emit_status.json";

std::vector<std::string> LoadNames();

/// <summary>名前に対応するプリセットを読み込み、存在しなければfalseを返します。</summary>
bool Load(const std::string& presetName, ParticlePresetData& preset);

/// <summary>プリセット値を指定名で保存します。</summary>
bool Save(const std::string& presetName, const ParticlePresetData& preset);

} // namespace ParticlePresetRepository
