#pragma once

#include "data/ParticlePresetData.h"

class ParticleEmitterComponent;

/// <summary>値データを実行時パーティクルComponentへ反映します。</summary>
void ApplyParticlePreset(ParticleEmitterComponent* emitter, const ParticlePresetData& preset);

/// <summary>実行時パーティクルComponentの現在値を保存用データへ変換します。</summary>
bool CaptureParticlePreset(const ParticleEmitterComponent* emitter, ParticlePresetData& preset);
