#pragma once

#include "Render/Render.h"

#include "imgui/imgui.h"
#include "imgui/examples/imgui_impl_vulkan.h"

#include "Support.h"

namespace Volts::RSX
{
    struct Vulkan : Render
    {
        Vulkan();
        virtual ~Vulkan();
        virtual void Attach(GUI::Frame* Handle) override;
        virtual void Detach() override;
        virtual const String Name() const override { return "Vulkan"; }
        virtual const String Description() const override { return "Vulkan3D"; }
        virtual Device* Devices(U32* Count) override;
        virtual void UpdateVSync(bool NewMode) override;

        virtual void BeginRender() override;
        virtual void PresentRender() override;

        virtual void Resize(GUI::Size NewSize) override;

        virtual void Windowed() override;
        virtual void Borderless() override;
        virtual void Fullscreen() override;
    private:
        GUI::Frame* Frame;
        VkPresentModeKHR VSyncMode;

        void CreateInstance();
        VkInstance Instance;

        void QueryDevices();
        U32 DeviceCount = 0;
        VulkanSupport::VulkanDevice* RenderDevices = nullptr;
    };
}