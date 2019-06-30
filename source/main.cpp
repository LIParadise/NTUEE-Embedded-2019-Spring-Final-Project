#include <events/mbed_events.h>
#include <mbed.h>
#include "ble/BLE.h"
#include "ble/gap/Gap.h"
#include "ble/services/HeartRateService.h"
#include "pretty_printer.h"
#include "USBHostMSD.h"
#include "USBHostMouse.h"
#include "USBHostKeyboard.h"
#include "FATFileSystem.h"
#include "HIDService.h"

const static char DEVICE_NAME[] = "USB Device";

static events::EventQueue event_queue(/* event count */ 16 * EVENTS_EVENT_SIZE);

DigitalOut led(LED1);

void onKey(uint8_t key)
{
    printf("Key: %c\r\n", key);
    static char msg[25] = {'\0'};
    static uint8_t key_press_scan_buff[50] = {'\0'};
    static uint8_t modifyKey[50] = {'\0'};
    int index_w, index_b;
    index_w = index_b = 0;

    char keyData = key;
    
    msg[index_w++] = keyData;
    if (keyData <= 0x39 && keyData >= 0x30)
    { //number
        if (keyData == 0x30)
        {
            modifyKey[index_b] = 0x00;
            key_press_scan_buff[index_b] = 0x27;
            index_b++;
            key_press_scan_buff[index_b] = 0x73;
        }
        else
        {
            modifyKey[index_b] = 0x00;
            key_press_scan_buff[index_b] = keyData - 0x13;
            index_b++;
            key_press_scan_buff[index_b] = 0x73;
        }
    }
    else if (keyData <= 0x7a && keyData >= 0x61)
    { //lowercase letters
        modifyKey[index_b] = 0x00;
        key_press_scan_buff[index_b] = keyData - 0x5d;
        index_b++;
        key_press_scan_buff[index_b] = 0x73;
    }
    else if (keyData <= 0x5a && keyData >= 0x41)
    { //uppercase letters
        modifyKey[index_b] = 0x02;
        key_press_scan_buff[index_b] = keyData - 0x3d;
        index_b++;
        key_press_scan_buff[index_b] = 0x73;
    }
    else if (keyData == 0x20)
    { //space
        modifyKey[index_b] = 0x00;
        key_press_scan_buff[index_b] = 0x2c;
        index_b++;
        key_press_scan_buff[index_b] = 0x73;
    }
    else
    {
        modifyKey[index_b] = 0x00;
        //key_press_scan_buff[index_b] = 0x73;          //this is dummy data.
        //msg[index_w+1] = '\0';
    }
    index_b++;
    if (keyData == 0x0a && ble.getGapState().connected)
    {
        flip_lock = true;
        RFSWIO = 0; // RF Switch to BLE and lockup temporary
        for (int i = 0; i < index_b; i++)
        {
            hidService.updateReport(modifyKey[i], key_press_scan_buff[i]);
            wait(0.03);
        }
        flip_lock = false;

        flip_lock = true;
        RFSWIO = 1; // RF Switch to WiFi and lockup temporary
        socket.send(msg, sizeof(msg));
        flip_lock = false;

        index_b = 0;
        index_w = 0;
        memset(modifyKey, 0, 50);
        memset(msg, 0, 25);
        memset(key_press_scan_buff, 0, 50);
    }
}
void keyboard_task(void const *) {
    
    USBHostKeyboard keyboard;
    
    while(1) {
        // try to connect a USB keyboard
        while(!keyboard.connect())
            Thread::wait(500);
    
        // when connected, attach handler called on keyboard event
        printf("Keyboard has been detected");
        keyboard.attach(onKey);
        
        // wait until the keyboard is disconnected
        while(keyboard.connected())
            Thread::wait(500);
    }
}

class USB_Device : ble::Gap::EventHandler {
public:
    USB_Device(BLE &ble, events::EventQueue &event_queue) :
        _ble(ble),
        _event_queue(event_queue),
        _led1(LED1, 1),
        _connected(false),
        _hid_uuid(GattService::UUID_HUMAN_INTERFACE_DEVICE_SERVICE),
        _hid_service(ble),
        _adv_data_builder(_adv_buffer) { }

    void start() {
        _ble.gap().setEventHandler(this);

        _ble.init(this, &USB_Device::on_init_complete);

        _event_queue.call_every(500, this, &USB_Device::blink);
        _event_queue.call_every(1, this, &USB_Device::update_keyboard_value);

        _event_queue.dispatch_forever();
    }

private:
    /** Callback triggered when the ble initialization process has finished */
    void on_init_complete(BLE::InitializationCompleteCallbackContext *params) {
        if (params->error != BLE_ERROR_NONE) {
            printf("Ble initialization failed.");
            return;
        }

        print_mac_address();

        start_advertising();
    }

    void start_advertising() {
        /* Create advertising parameters and payload */

        ble::AdvertisingParameters adv_parameters(
            ble::advertising_type_t::CONNECTABLE_UNDIRECTED,
            ble::adv_interval_t(ble::millisecond_t(1000))
        );

        _adv_data_builder.setFlags();
        _adv_data_builder.setAppearance(ble::adv_data_appearance_t::KEYBOARD);
        _adv_data_builder.setLocalServiceList(mbed::make_Span(&_hid_uuid, 1));
        _adv_data_builder.setName(DEVICE_NAME);

        /* Setup advertising */

        ble_error_t error = _ble.gap().setAdvertisingParameters(
            ble::LEGACY_ADVERTISING_HANDLE,
            adv_parameters
        );

        if (error) {
            printf("_ble.gap().setAdvertisingParameters() failed\r\n");
            return;
        }

        error = _ble.gap().setAdvertisingPayload(
            ble::LEGACY_ADVERTISING_HANDLE,
            _adv_data_builder.getAdvertisingData()
        );

        if (error) {
            printf("_ble.gap().setAdvertisingPayload() failed\r\n");
            return;
        }

        /* Start advertising */

        error = _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);

        if (error) {
            printf("_ble.gap().startAdvertising() failed\r\n");
            return;
        }
    }

    void update_keyboard_value() {
        if (_connected) {
            _hid_service.updateReport(modifyKey[i], key_press_scan_buff[i]);
        }
    }

    void blink(void) {
        _led1 = !_led1;
    }

private:
    /* Event handler */

    void onDisconnectionComplete(const ble::DisconnectionCompleteEvent&) {
        _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);
        _connected = false;
        printf( "disconnected.\r\n" );
    }

    virtual void onConnectionComplete(const ble::ConnectionCompleteEvent &event) {
        if (event.getStatus() == BLE_ERROR_NONE) {
            _connected = true;
        }
        printf( "connected.\r\n" );
    }

private:
    BLE &_ble;
    events::EventQueue &_event_queue;
    DigitalOut _led1;

    bool _connected;

    UUID _hid_uuid;

    HIDService _hid_service;

    uint8_t _adv_buffer[ble::LEGACY_ADVERTISING_MAX_SIZE];
    ble::AdvertisingDataBuilder _adv_data_builder;
};

/** Schedule processing of events from the BLE middleware in the event queue. */
void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context) {
    event_queue.call(Callback<void()>(&context->ble, &BLE::processEvents));
}

int main()
{
    Thread keyboardTask(keyboard_task, NULL, osPriorityNormal, 1024 * 4);
    
    while(1) {
        led=!led;
        Thread::wait(500);
    }
    
    
    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(schedule_ble_events);

    USB_Device demo(ble, event_queue);
    demo.start();

    return 0;
}

