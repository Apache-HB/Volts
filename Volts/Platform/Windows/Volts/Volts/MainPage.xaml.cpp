﻿//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include <.././../../Interface/EmulatorWindow.h>

#include "pch.h"
#include "MainPage.xaml.h"

using namespace Volts;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace Volts
{
    struct EmulatorWindow
    {
        virtual void WriteLog(const char* Channel, char Severity, const char* Message) = 0;
    };
}



struct WEmuWindow : public Volts::EmulatorWindow
{
    MainPage^ Parent;
    WEmuWindow(MainPage^ Page)
        : Parent(Page)
    {}

    virtual void WriteLog(const char* Channel, char Severity, const char* Message) override
    {
        std::string s_str = std::string(Message);
        std::wstring wid_str = std::wstring(s_str.begin(), s_str.end());
        const wchar_t* w_char = wid_str.c_str();
        Platform::String^ p_string = ref new Platform::String(w_char);
        Parent->SetTXT(p_string);
    }
};

MainPage::MainPage()
{
	InitializeComponent();
    HMODULE Mod = LoadPackagedLibrary(L"VoltCore.dll", 0);

    using VMain = int(*)(void*);

    VMain Entry = (VMain)GetProcAddress(Mod, "VoltsMain");

    if (Entry == nullptr)
    {
        if (Mod == nullptr)
            TXT->Text = "Mod&EntryNull";
        else
            TXT->Text = "EntryNull";
    }

    WEmuWindow* Emu = new WEmuWindow(this);

    Entry((void*)Emu);
}
