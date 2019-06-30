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
#include "DeviceInformationService.h"

const static char DEVICE_NAME[] = "USB Device";

static events::EventQueue event_queue(/* event count */ 16 * EVENTS_EVENT_SIZE);

class USB_Device ;

static const uint16_t uuid16_list[]        = {GattService::UUID_HUMAN_INTERFACE_DEVICE_SERVICE};
static char msg[25] = {'\0'};
static uint8_t key_press_scan_buff[50] = {'\0'};
static uint8_t modifyKey[50] = {'\0'};
static char keyData;
USB_Device* demoPtr = NULL;

void passkeyDisplayCallback(Gap::Handle_t handle, const SecurityManager::Passkey_t passkey)
{
  printf("Input passKey: ");
  for (unsigned i = 0; i < Gap::ADDR_LEN; i++) {
    printf("%c ", passkey[i]);
  }
  printf("\r\n");
}

void securitySetupCompletedCallback(Gap::Handle_t handle, SecurityManager::SecurityCompletionStatus_t status)
{
  if (status == SecurityManager::SEC_STATUS_SUCCESS) {
    printf("Security success\r\n", status);
  } else {
    printf("Security failed\r\n", status);
  }
}

void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *params);

class USB_Device : ble::Gap::EventHandler {
  public:
    USB_Device(BLE &ble, events::EventQueue &event_queue) :
      _ble(ble),
      _event_queue(event_queue),
      _led1(LED1, 1),
      _connected(false),
      _hid_uuid(GattService::UUID_HUMAN_INTERFACE_DEVICE_SERVICE),
      _hid_service(ble),
      _deviceInfo(ble, "ARM", "CYNTEC", "SN1", "hw-rev1", "fw-rev1", "soft-rev1"),
      _adv_data_builder(_adv_buffer) { }

    void start() {
      _ble.gap().setEventHandler(this);

      _ble.init(this, &USB_Device::on_init_complete);

      /* Create advertising parameters and payload */
      bool enableBonding = true;
      bool requireMITM = true;
      //const uint8_t passkeyValue[6] = {0x00,0x00,0x00,0x00,0x00,0x00};
      _ble.initializeSecurity(enableBonding, requireMITM, SecurityManager::IO_CAPS_DISPLAY_ONLY); //IO_CAPS_DISPLAY_ONLY, IO_CAPS_NONE
      _ble.onDisconnection(disconnectionCallback);
      _ble.securityManager().onPasskeyDisplay(passkeyDisplayCallback);
      _ble.securityManager().onSecuritySetupCompleted(securitySetupCompletedCallback);
      _ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME, (uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME));
      _ble.accumulateAdvertisingPayload(GapAdvertisingData::KEYBOARD);
      _ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
      _ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, (uint8_t *)uuid16_list, sizeof(uuid16_list));
      _ble.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
      _ble.setAdvertisingInterval(1000);

      _event_queue.call_every(500, this, &USB_Device::blink);

      _event_queue.dispatch_forever();

      public_startAdvertising();


    }    

    void update_keyboard_value(int i) {
      if (_connected) {
        _hid_service.updateReport(modifyKey[i], key_press_scan_buff[i]);
      }
    }

    bool connected() {
      return _ble.getGapState().connected;
    }

    void public_startAdvertising(){
      start_advertising();
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

    void start_advertising()
    {
      _ble.startAdvertising();
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

    DeviceInformationService _deviceInfo;

    uint8_t _adv_buffer[ble::LEGACY_ADVERTISING_MAX_SIZE];
    ble::AdvertisingDataBuilder _adv_data_builder;
};

/** Schedule processing of events from the BLE middleware in the event queue. */
void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context) {
  event_queue.call(Callback<void()>(&context->ble, &BLE::processEvents));
}

void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *params)
{
  demoPtr->public_startAdvertising(); // restart advertising
}

void onKey(uint8_t key)
{
  printf("Key: %c\r\n", key);

  int index_w, index_b;
  index_w = index_b = 0;

  keyData = key;

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

  // TODO 

  index_b++;

  if (keyData == 0x0a && demoPtr != NULL && demoPtr -> connected())
  {
    for (int i = 0; i < index_b; i++)
    {
      demoPtr -> update_keyboard_value(i);
      wait(0.03);
    }
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
    printf("Keyboard has been detected\r\n");
    keyboard.attach(onKey);

    // wait until the keyboard is disconnected
    while(keyboard.connected())
      Thread::wait(500);
  }
}

int main()
{

  Thread keyboardTask(keyboard_task, NULL, osPriorityNormal, 1024 * 4);
  BLE &ble = BLE::Instance();
  ble.onEventsToProcess(schedule_ble_events);
  USB_Device demo(ble, event_queue);
  demoPtr = &demo;

  demo.start();


  return 0;
}

