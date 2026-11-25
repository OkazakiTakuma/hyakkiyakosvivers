#pragma once

#include <d3d12.h>
#include "DirectXTex.h"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mfidl.h>
#include <wrl.h>

struct ID3D12CommandQueue;
struct ID3D12Resource;
struct IMFSinkWriter;

/// <summary>
/// バックバッファを PNG または H.264 MP4 として CaptureFiles に保存します。
/// </summary>
class ScreenCapture {
public:
	// IMFSinkWriter の完全型を参照できる実装ファイル側で生成処理を定義する。
	ScreenCapture();
	~ScreenCapture();

	/// <summary>次に処理する完成フレームを PNG として保存するよう予約します。</summary>
	void RequestScreenshot();
	/// <summary>録画を開始し、録画中の場合は MP4 を確定して停止します。</summary>
	void ToggleRecording(uint32_t width, uint32_t height);
	/// <summary>
	/// PRESENT 状態のバックバッファを CPU 側へ読み戻し、予約済みスクリーンショットまたは録画へ渡します。
	/// </summary>
	void ProcessFrame(ID3D12CommandQueue* commandQueue, ID3D12Resource* backBuffer, uint32_t width, uint32_t height);
	/// <summary>録画ファイルを確定し、Media Foundation の利用を終了します。</summary>
	void Finalize();

	/// <summary>MP4録画セッションが開始済みかを返します。</summary>
	bool IsRecording() const { return recording_; }

private:
	/// <summary>指定解像度の H.264 MP4 出力を初期化します。</summary>
	bool StartRecording(uint32_t width, uint32_t height);
	/// <summary>Sink Writer を確定して録画ファイルを閉じます。</summary>
	void StopRecording();
	/// <summary>DirectXTex の画像を Media Foundation の動画サンプルへ変換して書き込みます。</summary>
	bool WriteVideoFrame(const DirectX::Image& image);
	/// <summary>実行ファイル横の CaptureFiles に日時付き保存パスを生成します。</summary>
	std::filesystem::path MakeCapturePath(const wchar_t* prefix, const wchar_t* extension) const;

	// スクリーンショット要求は、描画が完成するまで保持して次の ProcessFrame で消費する。
	bool screenshotRequested_ = false;
	bool recording_ = false;
	// MFStartup と MFShutdown を必ず一度ずつ対応させるための状態。
	bool mediaFoundationStarted_ = false;
	// 録画途中の解像度変更を検出し、不正なサイズのフレーム混在を防ぐ。
	uint32_t recordingWidth_ = 0;
	uint32_t recordingHeight_ = 0;
	/// <summary>正常に書き込んだ動画フレームの累計です。</summary>
	uint64_t videoFrameIndex_ = 0;
	// Media Foundation が要求する単調増加タイムスタンプの直前値。
	LONGLONG lastSampleTime_ = -1;
	/// <summary>動画サンプルの時刻を実時間から計算するための録画開始時刻です。</summary>
	std::chrono::steady_clock::time_point recordingStartTime_{};
	/// <summary>Sink Writerへ登録したH.264映像ストリームの識別子です。</summary>
	DWORD videoStreamIndex_ = 0;
	/// <summary>MP4のエンコードとファイル書き込みを担当するMedia Foundationオブジェクトです。</summary>
	Microsoft::WRL::ComPtr<IMFSinkWriter> sinkWriter_;
};
