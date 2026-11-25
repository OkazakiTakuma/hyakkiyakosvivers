#pragma once

#include "struct.h"

#include <string>

/// <summary>パーティクルプリセットとして保存・読込する値データです。</summary>
struct ParticlePresetData {
	// 値が省略された古いJSONでも、ParticleEmitter本体と同じ初期状態へ復元する。
	std::string textureFilePath;
	bool isActive = true;
	float frequency = 0.0f;
	BlendMode blendMode = kBlendModeNormal;
	ParticleMeshType meshType = kMeshTypeQuad;
	ParticleEmitParam emitParam{};
};
