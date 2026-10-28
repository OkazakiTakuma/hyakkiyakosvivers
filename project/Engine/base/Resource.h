#pragma once
#include "Matrix.h"
#include "Vector.h"
#include "DirectXTex.h"
#include <Windows.h>
#include <cassert>
#include <chrono>
#include <codecvt>
#include <cstdint>
#include <d3d12.h>
#include <dbghelp.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <format>
#include <fstream>
#include <locale>
#include <math.h>
#include <sstream>
#include <string>
#include <strsafe.h>
#include <wrl.h>
#pragma comment(lib, "DirectXTex.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")



/// <summary>DirectXリソースの寿命をComPtrで共有管理する軽量ラッパーです。</summary>
class ResourceObject  
{  
public:  
	/// <summary>管理対象のDirectXリソースを共有参照として受け取ります。</summary>
    ResourceObject(const Microsoft::WRL::ComPtr<ID3D12Resource>& resource)  
        : resource_(resource) {}

    ~ResourceObject() = default;

	/// <summary>保持しているGPUリソースの共有参照を返します。</summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> Get() { return resource_; }

private:  
	/// <summary>参照カウント付きで保持するGPUリソースです。</summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
};

