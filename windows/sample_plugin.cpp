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
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Storage.Streams.h>

#include <flutter/method_channel.h>
#include <flutter/basic_message_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <flutter/standard_message_codec.h>

#include <map>
#include <memory>
#include <string>

using namespace winrt;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Storage::Streams;

namespace {
    using flutter::EncodableMap;
    using flutter::EncodableValue;

    union uint16_t_union {
        uint16_t uint16;
        byte bytes[sizeof(uint16_t)];
    };

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
            const flutter::MethodCall<EncodableValue>& method_call,
            std::unique_ptr<flutter::MethodResult<EncodableValue>> result);

        BluetoothLEAdvertisementWatcher bluetoothLEWatcher{ nullptr };
        event_token bluetoothLEWatcherReceivedToken;
        void BluetoothLEWatcher_Received(BluetoothLEAdvertisementWatcher sender, BluetoothLEAdvertisementReceivedEventArgs args);

        std::unique_ptr<flutter::BasicMessageChannel<EncodableValue>> scanResultMessage;
    };

    // static
    void SamplePlugin::RegisterWithRegistrar(
        flutter::PluginRegistrarWindows* registrar)
    {
        // *** Replace "sample_plugin" with your plugin's channel name in this call.
        auto channel =
            std::make_unique<flutter::MethodChannel<EncodableValue>>(
                registrar->messenger(), "sample_plugin",
                &flutter::StandardMethodCodec::GetInstance());

        auto plugin = std::make_unique<SamplePlugin>();

        channel->SetMethodCallHandler(
            [plugin_pointer = plugin.get()](const auto& call, auto result) {
            plugin_pointer->HandleMethodCall(call, std::move(result));
        });

        plugin->scanResultMessage = std::make_unique<flutter::BasicMessageChannel<EncodableValue>>(
            registrar->messenger(),
            "sample_plugin/message.scanResult",
            &flutter::StandardMessageCodec::GetInstance());

        registrar->AddPlugin(std::move(plugin));
    }

    SamplePlugin::SamplePlugin() {}

    SamplePlugin::~SamplePlugin() {}


    void SamplePlugin::HandleMethodCall(
        const flutter::MethodCall<EncodableValue>& method_call,
        std::unique_ptr<flutter::MethodResult<EncodableValue>> result)
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

    std::vector<uint8_t> parseManufacturerData(BluetoothLEAdvertisement advertisement)
    {
        if (advertisement.ManufacturerData().Size() == 0)
            return std::vector<uint8_t>();

        auto manufacturerData = advertisement.ManufacturerData().GetAt(0);
        // FIXME Compat with REG_DWORD_BIG_ENDIAN
        uint8_t* prefix = uint16_t_union{ manufacturerData.CompanyId() }.bytes;
        auto result = std::vector<uint8_t>{ prefix, prefix + sizeof(uint16_t_union) };

        auto reader = DataReader::FromBuffer(manufacturerData.Data());
        auto data = std::vector<uint8_t>(reader.UnconsumedBufferLength());
        reader.ReadBytes(data);
        result.insert(result.end(), data.begin(), data.end());
        return result;
    }

    void SamplePlugin::BluetoothLEWatcher_Received(BluetoothLEAdvertisementWatcher sender, BluetoothLEAdvertisementReceivedEventArgs args)
    {
        OutputDebugString((L"Received " + to_hstring(args.BluetoothAddress()) + L"\n").c_str());
        auto manufacturerData = parseManufacturerData(args.Advertisement());
        scanResultMessage->Send(EncodableValue(EncodableMap{
            {EncodableValue("name"), EncodableValue(to_string(args.Advertisement().LocalName()))},
            {EncodableValue("deviceId"), EncodableValue(std::to_string(args.BluetoothAddress()))},
            {EncodableValue("manufacturerData"), EncodableValue(manufacturerData)},
            {EncodableValue("rssi"), EncodableValue(args.RawSignalStrengthInDBm())},
        }));
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
