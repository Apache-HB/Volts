#pragma once

#include <Meta/Aliases.h>
#include "GUI/Frame.h"

namespace Volts::RSX
{
    using namespace Cthulhu;

    struct Device
    {
        virtual ~Device() {}
        virtual const char* Name() const = 0;
    };

    // each renderer can be fed a window that it will render to
    struct Render
    {
        virtual ~Render() {}
        // this starts the renderer and gives it the window handle it needs
        virtual void Attach(GUI::Frame* Handle) = 0;
        // this detaches the renderer from the window and shuts down the backend
        virtual void Detach() = 0;

        virtual void UpdateVSync(bool Enabled) = 0;

        virtual const char* Name() const = 0;
        virtual const char* Description() const = 0;

        virtual Device* Devices(U32* Count) = 0;
        virtual void SetDevice(RSX::Device* Device) = 0;

        // override this to specifiy any extra options the render backend may have
        virtual void Options() {}

        virtual void Windowed() = 0;
        virtual void Fullscreen() = 0;
        virtual void Borderless() = 0;

        virtual void Resize(GUI::Size NewSize) = 0;

        virtual void BeginRender() = 0;
        virtual void PresentRender() = 0;
    };
}