// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "sample_plugin.h"

// This must be included before VersionHelpers.h.
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <map>
#include <memory>
#include <string>

using namespace winrt;
using namespace Windows::Devices::Bluetooth::Advertisement;

namespace {
    // *** Rename this class to match the linux pluginClass in your pubspec.yaml.
    class SamplePlugin : public flutter::Plugin
    {
    public:
        static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);

        SamplePlugin();

        virtual ~SamplePlugin();

    private:
        // Called when a method is called on this plugin's channel from Dart.
        void HandleMethodCall(
            const flutter::MethodCall<flutter::EncodableValue>& method_call,
            std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

        BluetoothLEAdvertisementWatcher bluetoothLEWatcher{ nullptr };
        event_token bluetoothLEWatcherReceivedToken;
        void BluetoothLEWatcher_Received(BluetoothLEAdvertisementWatcher sender, BluetoothLEAdvertisementReceivedEventArgs args);
    };

    // static
    void SamplePlugin::RegisterWithRegistrar(
        flutter::PluginRegistrarWindows* registrar)
    {
        // *** Replace "sample_plugin" with your plugin's channel name in this call.
        auto channel =
            std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
                registrar->messenger(), "sample_plugin",
                &flutter::StandardMethodCodec::GetInstance());

        auto plugin = std::make_unique<SamplePlugin>();

        channel->SetMethodCallHandler(
            [plugin_pointer = plugin.get()](const auto& call, auto result) {
            plugin_pointer->HandleMethodCall(call, std::move(result));
        });

        registrar->AddPlugin(std::move(plugin));
    }

    SamplePlugin::SamplePlugin() {}

    SamplePlugin::~SamplePlugin() {}


    void SamplePlugin::HandleMethodCall(
        const flutter::MethodCall<flutter::EncodableValue>& method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result)
    {
        auto methodName = method_call.method_name();
        if (methodName == "startScan")
        {
            OutputDebugString(L"HandleMethodCall startScan\n");
            bluetoothLEWatcher = BluetoothLEAdvertisementWatcher();
            bluetoothLEWatcherReceivedToken = bluetoothLEWatcher.Received({ this, &SamplePlugin::BluetoothLEWatcher_Received });
            bluetoothLEWatcher.Start();
            result->Success(nullptr);
        }
        else if (methodName == "stopScan")
        {
            OutputDebugString(L"HandleMethodCall stopScan\n");
            if (bluetoothLEWatcher)
            {
                bluetoothLEWatcher.Stop();
                bluetoothLEWatcher.Received(bluetoothLEWatcherReceivedToken);
            }
            bluetoothLEWatcher = nullptr;
            result->Success(nullptr);
        }
        else
        {
            result->NotImplemented();
        }
    }
    
    void SamplePlugin::BluetoothLEWatcher_Received(BluetoothLEAdvertisementWatcher sender, BluetoothLEAdvertisementReceivedEventArgs args)
    {
        OutputDebugString((L"Received " + std::to_wstring(args.BluetoothAddress()) + L"\n").c_str());
    }
}  // namespace

void SamplePluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
    // The plugin registrar wrappers owns the plugins, registered callbacks, etc.,
    // so must remain valid for the life of the application.
    static auto* plugin_registrars =
        new std::map<FlutterDesktopPluginRegistrarRef,
        std::unique_ptr<flutter::PluginRegistrarWindows>>;
    auto insert_result = plugin_registrars->emplace(
        registrar, std::make_unique<flutter::PluginRegistrarWindows>(registrar));

    SamplePlugin::RegisterWithRegistrar(insert_result.first->second.get());
}
