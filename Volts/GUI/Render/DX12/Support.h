#pragma once

#include "Volts/Core/Logger/Logger.h"
#include "Config.h"
#include "Render/Render.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include <wrl.h>
#include <shellapi.h>

#define VALIDATE(...) if(FAILED(__VA_ARGS__)) { VFATAL("[%s]:%s DX12 Call failed", __FILE__, __LINE__); exit(1); }


namespace Volts::DX12Support
{
    template<typename T>
    using Ptr = Microsoft::WRL::ComPtr<T>;

    struct DX12Device : RSX::Device
    {
        DX12Device() {}
        DX12Device(Ptr<IDXGIAdapter1> Dev)
            : Handle(Dev)
        {
            Dev->GetDesc(&Desc);
        }

        virtual std::wstring Name() const override
        {
            return Desc.Description;
        }
    private:
        Ptr<IDXGIAdapter1> Handle;
        DXGI_ADAPTER_DESC Desc;
    };

    bool CanTear();
}