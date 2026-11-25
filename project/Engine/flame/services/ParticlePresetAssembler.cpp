#include "services/ParticlePresetAssembler.h"

#include "particle/ParticleEmitterComponent.h"

// 読み込んだ値を実行時コンポーネントへまとめて反映する。
void ApplyParticlePreset(ParticleEmitterComponent* emitter, const ParticlePresetData& preset) {
	if (!emitter) return;
	emitter->SetTexture(preset.textureFilePath);
	emitter->SetIsActive(preset.isActive);
	emitter->SetFrequency(preset.frequency);
	emitter->SetBlendMode(preset.blendMode);
	emitter->SetMeshType(preset.meshType);
	emitter->SetParam(preset.emitParam);
}

// 実行時コンポーネントの現在値を、保存可能な値オブジェクトへ写し取る。
bool CaptureParticlePreset(const ParticleEmitterComponent* emitter, ParticlePresetData& preset) {
	if (!emitter) return false;
	preset.textureFilePath = emitter->GetTextureFilePath();
	preset.isActive = emitter->GetIsActive();
	preset.frequency = emitter->GetFrequency();
	preset.blendMode = emitter->GetBlendMode();
	preset.meshType = emitter->GetMeshType();
	preset.emitParam = emitter->GetParam();
	return true;
}
