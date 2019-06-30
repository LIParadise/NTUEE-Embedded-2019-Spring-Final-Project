#include <mbed.h>
#include "ble/BLE.h"
#include "pretty_printer.h"
#include "USBHostKeyboard.h"
#include "KeyboardService.h"
#include "examples_common.h"
#include "DeviceInformationService.h"
#include <events/mbed_events.h>

const static char DEVICE_NAME[] = "__ HUMW __";
static const uint16_t uuid16_list[] = {GattService::UUID_HUMAN_INTERFACE_DEVICE_SERVICE,
                                       GattService::UUID_DEVICE_INFORMATION_SERVICE,
                                       GattService::UUID_BATTERY_SERVICE};

static events::EventQueue event_queue(/* event count */ 16 * EVENTS_EVENT_SIZE);

class USB_Device;

USB_Device *demoPtr = NULL;

void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *params);

void passkeyDisplayCallback(Gap::Handle_t handle, const SecurityManager::Passkey_t passkey)
{
    HID_DEBUG("HID_DEBUG Input passKey: ");
    printf("printf    Input passKey: ");
    for (unsigned i = 0; i < Gap::ADDR_LEN; i++)
    {
        HID_DEBUG("%c ", passkey[i]);
        printf("%c ", passkey[i]);
    }
    HID_DEBUG("HID_DEBUG \r\n");
    printf("printf    \r\n");
}

void securitySetupCompletedCallback(Gap::Handle_t handle, SecurityManager::SecurityCompletionStatus_t status)
{
    if (status == SecurityManager::SEC_STATUS_SUCCESS)
    {
        HID_DEBUG("hid_debug Security success\r\n", status);
        printf("printf    Security Success\r\n");
    }
    else
    {
        HID_DEBUG("hid_debug Security failed\r\n", status);
        printf("printf    Security Failed\r\n");
    }
}

static void securitySetupInitiatedCallback(Gap::Handle_t, bool allowBonding, bool requireMITM, SecurityManager::SecurityIOCapabilities_t iocaps)
{
    HID_DEBUG("hid_debug Security setup initiated\r\n");
    printf("printf    Security setup initiated\r\n");
}

void connectionCallback(const Gap::ConnectionCallbackParams_t *params)
{
    HID_DEBUG("hid_debug connected\r\n");
    printf("printf    connected\r\n");
}

class USB_Device : ble::Gap::EventHandler
{
public:
    USB_Device(BLE &ble, events::EventQueue &event_queue) : _ble(ble),
                                                            _event_queue(event_queue),
                                                            _led1(LED1, 1),
                                                            _connected(false),
                                                            _hid_uuid(GattService::UUID_HUMAN_INTERFACE_DEVICE_SERVICE),
                                                            _hid_service(ble),
                                                            _deviceInfo(ble, "ARM", "CYNTEC", "SN1", "hw-rev1", "fw-rev1", "soft-rev1"),
                                                            _adv_data_builder(_adv_buffer) {}

    void start()
    {
        _ble.gap().setEventHandler(this);
        _ble.init(this, &USB_Device::on_init_complete);
        _event_queue.call_every(500, this, &USB_Device::blink);
        _event_queue.dispatch_forever();
    }

    void public_startAdvertising()
    {
        start_advertising();
    }

    void send_string(const char *c)
    {
        if (!_connected)
        {
            HID_DEBUG("hid_debug not connected yet...");
            printf("printf    not connected yet...");
        }
        else
        {
            int len = strlen(c);
            _hid_service.printf(c);
            HID_DEBUG("hid_debug sending %d chars\r\n", len);
            printf("printf    sending %d chars\r\n", len);
        }
    }

private:
    /** Callback triggered when the ble initialization process has finished */
    void on_init_complete(BLE::InitializationCompleteCallbackContext *params)
    {
        if (params->error != BLE_ERROR_NONE)
        {
            HID_DEBUG("HID_DEBUG Ble initialization failed.");
            printf("printf    Ble initialization failed.");
            return;
        }

        print_mac_address();

        public_startAdvertising();
    }

    void start_advertising()
    {
        ble::AdvertisingParameters adv_parameters(
            ble::advertising_type_t::CONNECTABLE_UNDIRECTED,
            ble::adv_interval_t(ble::millisecond_t(1000)));

        _adv_data_builder.setFlags();
        _adv_data_builder.setAppearance(ble::adv_data_appearance_t::KEYBOARD);
        _adv_data_builder.setLocalServiceList(mbed::make_Span(&_hid_uuid, 1));
        _adv_data_builder.setName(DEVICE_NAME);

        /* Setup advertising */

        ble_error_t error = _ble.gap().setAdvertisingParameters(
            ble::LEGACY_ADVERTISING_HANDLE,
            adv_parameters);

        if (error)
        {
            printf("printf    _ble.gap().setAdvertisingParameters() failed\r\n");
            HID_DEBUG("hid_debug _ble.gap().setAdvertisingParameters() failed\r\n");
            return;
        }

        error = _ble.gap().setAdvertisingPayload(
            ble::LEGACY_ADVERTISING_HANDLE,
            _adv_data_builder.getAdvertisingData());

        if (error)
        {
            printf("printf    _ble.gap().setAdvertisingPayload() failed\r\n");
            HID_DEBUG("hid_debug _ble.gap().setAdvertisingPayload() failed\r\n");
            return;
        }

        /* Start advertising */

        error = _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);

        if (error)
        {
            printf("printf    _ble.gap().startAdvertising() failed\r\n");
            HID_DEBUG("hid_debug _ble.gap().startAdvertising() failed\r\n");
            return;
        }

        printf("printf    started advertising\r\n");
        HID_DEBUG("hid_debug started advertising\r\n");
    }

    void blink(void)
    {
        _led1 = !_led1;
    }

private:
    /* Event handler */

    void onDisconnectionComplete(const ble::DisconnectionCompleteEvent &)
    {
        _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);
        _connected = false;
        HID_DEBUG("hid_debug disconnected.\r\n");
        printf("printf    disconnected.\r\n");
    }

    virtual void onConnectionComplete(const ble::ConnectionCompleteEvent &event)
    {
        if (event.getStatus() == BLE_ERROR_NONE)
        {
            _connected = true;
        }
        HID_DEBUG("hid_debug connected.\r\n");
        printf("printf    connected.\r\n");
    }

private:
    BLE &_ble;
    events::EventQueue &_event_queue;
    DigitalOut _led1;

    bool _connected;

    UUID _hid_uuid;

    KeyboardService _hid_service;

    DeviceInformationService _deviceInfo;

    uint8_t _adv_buffer[ble::LEGACY_ADVERTISING_MAX_SIZE];
    ble::AdvertisingDataBuilder _adv_data_builder;
};

void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context)
{
    event_queue.call(Callback<void()>(&context->ble, &BLE::processEvents));
}

void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *params)
{
    demoPtr->public_startAdvertising(); // restart advertising
}

void onKey(uint8_t key)
{
    HID_DEBUG("HID_DEBUG Key: %c\r\n", key);
    printf("printf    Key: %c\r\n", key);
    char tmp_key = key;
    char *keyPtr = &tmp_key;
    demoPtr->send_string(keyPtr);
}

void keyboard_task(void const *)
{

    USBHostKeyboard keyboard;

    while (1)
    {
        // try to connect a USB keyboard
        while (!keyboard.connect())
            Thread::wait(500);

        // when connected, attach handler called on keyboard event
        HID_DEBUG("HID_DEBUG Keyboard has been detected\r\n");
        printf("printf    Keyboard has been detected\r\n");
        keyboard.attach(onKey);

        // wait until the keyboard is disconnected
        while (keyboard.connected())
            Thread::wait(500);
    }
}

int main()
{
    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(schedule_ble_events);
    USB_Device demo(ble, event_queue);
    demoPtr = &demo;
    Thread keyboardTask(keyboard_task, NULL, osPriorityNormal, 1024 * 4);

    demo.start();

    return 0;
}
