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
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>

#include <flutter/method_channel.h>
#include <flutter/basic_message_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <flutter/standard_message_codec.h>

#include <map>
#include <memory>
#include <string>
#include <sstream>
#include <iomanip>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;

#define GUID_FORMAT "%08x-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"
#define GUID_ARG(guid) guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]

namespace {
    using flutter::EncodableMap;
    using flutter::EncodableValue;

    union uint16_t_union {
        uint16_t uint16;
        byte bytes[sizeof(uint16_t)];
    };

    std::vector<uint8_t> to_bytevc(IBuffer buffer)
    {
        //auto rangeFirst = buffer.data();
        //return std::vector<uint8_t>{ rangeFirst, rangeFirst + buffer.Length() };
        auto reader = DataReader::FromBuffer(buffer);
        auto result = std::vector<uint8_t>(reader.UnconsumedBufferLength());
        reader.ReadBytes(result);
        return result;
    }

    IBuffer from_bytevc(std::vector<uint8_t> bytes)
    {
        auto writer = DataWriter();
        writer.WriteBytes(bytes);
        return writer.DetachBuffer();
    }

    std::string to_hexstring(std::vector<uint8_t> bytes)
    {
        auto ss = std::stringstream();
        for (auto b : bytes)
            ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(b);
        return ss.str();
    }
    
    std::string to_uuidstr(guid guid)
    {
        char chars[36 + 1];
        sprintf_s(chars, GUID_FORMAT, GUID_ARG(guid));
        return std::string{ chars };
    }

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

        std::unique_ptr<flutter::BasicMessageChannel<EncodableValue>> scanResultMessage;
        std::unique_ptr<flutter::BasicMessageChannel<EncodableValue>> connectorMessage;
        std::unique_ptr<flutter::BasicMessageChannel<EncodableValue>> clientMessage;

        BluetoothLEAdvertisementWatcher bluetoothLEWatcher{ nullptr };
        event_token bluetoothLEWatcherReceivedToken;
        void BluetoothLEWatcher_Received(BluetoothLEAdvertisementWatcher sender, BluetoothLEAdvertisementReceivedEventArgs args);

        BluetoothLEDevice bluetoothLEDevice{ nullptr };
        fire_and_forget ConnectAsync(uint64_t deviceId);
        event_token bluetoothLEDeviceConnectionStatusChangedToken;
        void BluetoothLEDevice_ConnectionStatusChanged(BluetoothLEDevice sender, IInspectable args);
        void Clean();

        std::map<std::string, GattCharacteristic> gattCharacteristics{};
        IAsyncOperation<GattCharacteristic> GetCharacteristicAsync(std::string service, std::string characteristic);

        fire_and_forget SetNotifiableAsync(std::string service, std::string characteristic, std::string bleInputProperty);

        fire_and_forget WriteValueAsync(std::string service, std::string characteristic, std::vector<uint8_t> value, std::string bleOutputProperty);

        std::map<std::string, event_token> gattCharacteristicValueChangedTokens{};
        void GattCharacteristic_ValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args);
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
        plugin->connectorMessage = std::make_unique<flutter::BasicMessageChannel<EncodableValue>>(
            registrar->messenger(),
            "sample_plugin/message.connector",
            &flutter::StandardMessageCodec::GetInstance());
        plugin->clientMessage = std::make_unique<flutter::BasicMessageChannel<EncodableValue>>(
            registrar->messenger(),
            "sample_plugin/message.client",
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
        OutputDebugString((L"HandleMethodCall " + to_hstring(methodName) + L"\n").c_str());
        if (methodName == "startScan")
        {
            bluetoothLEWatcher = BluetoothLEAdvertisementWatcher();
            bluetoothLEWatcherReceivedToken = bluetoothLEWatcher.Received({ this, &SamplePlugin::BluetoothLEWatcher_Received });
            bluetoothLEWatcher.Start();
            result->Success(nullptr);
        }
        else if (methodName == "stopScan")
        {
            if (bluetoothLEWatcher)
            {
                bluetoothLEWatcher.Stop();
                bluetoothLEWatcher.Received(bluetoothLEWatcherReceivedToken);
            }
            bluetoothLEWatcher = nullptr;
            result->Success(nullptr);
        }
        else if (methodName == "connect")
        {
            auto args = method_call.arguments()->MapValue();
            auto deviceId = std::stoll(args[EncodableValue("deviceId")].StringValue());
            ConnectAsync(deviceId);
            result->Success(nullptr);
        }
        else if (methodName == "disconnect")
        {
            result->Success(nullptr);
            connectorMessage->Send(EncodableValue(EncodableMap{
                {EncodableValue("ConnectionState"), EncodableValue("disconnected")},
            }));
            Clean();
        }
        else if (methodName == "discoverServices")
        {
            result->Success(nullptr);
            // No need for Windows API
            connectorMessage->Send(EncodableValue(EncodableMap({
                {EncodableValue("ServiceState"), EncodableValue("discovered")},
            })));
        }
        else if (methodName == "setNotifiable")
        {
            auto args = method_call.arguments()->MapValue();
            auto service = args[EncodableValue("service")].StringValue();
            auto characteristic = args[EncodableValue("characteristic")].StringValue();
            auto bleInputProperty = args[EncodableValue("bleInputProperty")].StringValue();
            SetNotifiableAsync(service, characteristic, bleInputProperty);
            result->Success(nullptr);
        }
        else if (methodName == "writeValue")
        {
            auto args = method_call.arguments()->MapValue();
            auto service = args[EncodableValue("service")].StringValue();
            auto characteristic = args[EncodableValue("characteristic")].StringValue();
            auto value = args[EncodableValue("value")].ByteListValue();
            auto bleOutputProperty = args[EncodableValue("bleOutputProperty")].StringValue();
            WriteValueAsync(service, characteristic, value, bleOutputProperty);
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

        auto data = to_bytevc(manufacturerData.Data());
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

    fire_and_forget SamplePlugin::ConnectAsync(uint64_t deviceId)
    {
        auto device = co_await BluetoothLEDevice::FromBluetoothAddressAsync(deviceId);
        auto servicesResult = co_await device.GetGattServicesAsync();
        if (servicesResult.Status() != GattCommunicationStatus::Success)
        {
            OutputDebugString((L"GetGattServicesAsync error: " + to_hstring((int32_t) servicesResult.Status()) + L"\n").c_str());
            connectorMessage->Send(EncodableValue(EncodableMap{
                {EncodableValue("ConnectionState"), EncodableValue("disconnected")},
            }));
            return;
        }
        bluetoothLEDevice = device;
        bluetoothLEDeviceConnectionStatusChangedToken = bluetoothLEDevice.ConnectionStatusChanged({ this, &SamplePlugin::BluetoothLEDevice_ConnectionStatusChanged });
        connectorMessage->Send(EncodableValue(EncodableMap{
            {EncodableValue("ConnectionState"), EncodableValue("connected")},
        }));
    }

    void SamplePlugin::BluetoothLEDevice_ConnectionStatusChanged(BluetoothLEDevice sender, IInspectable args)
    {
        OutputDebugString((L"ConnectionStatusChanged " + to_hstring((int32_t) sender.ConnectionStatus()) + L"\n").c_str());
        if (sender.ConnectionStatus() == BluetoothConnectionStatus::Disconnected)
        {
            connectorMessage->Send(EncodableValue(EncodableMap{
                {EncodableValue("ConnectionState"), EncodableValue("disconnected")},
            }));
            Clean();
        }
    }

    void SamplePlugin::Clean()
    {
        for (auto it = gattCharacteristics.cbegin(); it != gattCharacteristics.cend(); it = gattCharacteristics.erase(it))
            it->second.ValueChanged(std::exchange(gattCharacteristicValueChangedTokens[it->first], {}));

        if (bluetoothLEDevice)
            bluetoothLEDevice.ConnectionStatusChanged(bluetoothLEDeviceConnectionStatusChangedToken);
        bluetoothLEDevice = nullptr;
    }

    IAsyncOperation<GattCharacteristic> SamplePlugin::GetCharacteristicAsync(std::string service, std::string characteristic)
    {
        // opertator `[]` of colllection requires `CopyConstructible` and `Assignable`
        if (gattCharacteristics.count(characteristic) == 0)
        {
            auto serviceResult = co_await bluetoothLEDevice.GetGattServicesAsync();
            if (serviceResult.Status() != GattCommunicationStatus::Success)
                return nullptr;
            GattDeviceService gattService{ nullptr };
            for (auto s : serviceResult.Services())
                if (to_uuidstr(s.Uuid()) == service)
                    gattService = s;

            auto characteristicResult = co_await gattService.GetCharacteristicsAsync();
            if (characteristicResult.Status() != GattCommunicationStatus::Success)
                return nullptr;
            GattCharacteristic gattCharacteristic{ nullptr };
            for (auto c : characteristicResult.Characteristics())
                if (to_uuidstr(c.Uuid()) == characteristic)
                    gattCharacteristic = c;

            gattCharacteristics.insert(std::make_pair(characteristic, gattCharacteristic));
        }
        return gattCharacteristics.at(characteristic);
    }

    fire_and_forget SamplePlugin::SetNotifiableAsync(std::string service, std::string characteristic, std::string bleInputProperty)
    {
        auto gattCharacteristic = co_await GetCharacteristicAsync(service, characteristic);
        auto descriptorValue = bleInputProperty == "notification" ? GattClientCharacteristicConfigurationDescriptorValue::Notify
            : bleInputProperty == "indication" ? GattClientCharacteristicConfigurationDescriptorValue::Indicate
            : GattClientCharacteristicConfigurationDescriptorValue::None;
        auto writeDescriptorStatus = co_await gattCharacteristic.WriteClientCharacteristicConfigurationDescriptorAsync(descriptorValue);
        if (writeDescriptorStatus != GattCommunicationStatus::Success)
            OutputDebugString((L"WriteClientCharacteristicConfigurationDescriptorAsync " + to_hstring((int32_t) writeDescriptorStatus) + L"\n").c_str());

        if (bleInputProperty != "disabled")
            gattCharacteristicValueChangedTokens[characteristic] = gattCharacteristic.ValueChanged({ this, &SamplePlugin::GattCharacteristic_ValueChanged });
        else
            gattCharacteristic.ValueChanged(std::exchange(gattCharacteristicValueChangedTokens[characteristic], {}));

        clientMessage->Send(EncodableValue(EncodableMap({
            {EncodableValue("characteristicConfig"), EncodableValue(characteristic)},
        })));
    }

    fire_and_forget SamplePlugin::WriteValueAsync(std::string service, std::string characteristic, std::vector<uint8_t> value, std::string bleOutputProperty)
    {
        auto gattCharacteristic = co_await GetCharacteristicAsync(service, characteristic);
        auto writeOption = bleOutputProperty == "withoutResponse" ? GattWriteOption::WriteWithoutResponse : GattWriteOption::WriteWithResponse;
        auto writeValueStatus = co_await gattCharacteristic.WriteValueAsync(from_bytevc(value), writeOption);
        OutputDebugString((L"WriteValueAsync " + to_hstring(characteristic) + L", " + to_hstring(to_hexstring(value)) + L", " + to_hstring((int32_t) writeValueStatus) + L"\n").c_str());
    }

    void SamplePlugin::GattCharacteristic_ValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args)
    {
        auto uuid = to_uuidstr(sender.Uuid());
        auto bytes = to_bytevc(args.CharacteristicValue());
        OutputDebugString((L"GattCharacteristic_ValueChanged " + to_hstring(uuid) + L", " + to_hstring(to_hexstring(bytes)) + L"\n").c_str());
        clientMessage->Send(EncodableValue(EncodableMap({
            {EncodableValue("characteristicValue"), EncodableValue(EncodableMap({
                {EncodableValue("characteristic"), EncodableValue(uuid)},
                {EncodableValue("value"), EncodableValue(bytes)},
            }))},
        })));
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
