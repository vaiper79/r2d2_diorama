/*
 * Copyright (c) 2016 RedBear
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

 /*
  * Extra comments by JMK Engineer: https://www.youtube.com/channel/UC9AIBld20gHCi1OdCz3K8wA && https://github.com/jmkuss/BLE_Nano2_Demo/blob/master/BLE_SimplePeripheral_10_4_18.ino
  */

/*
 * R2D2_head.h, code that will eventually be running on a RedBear BLE Nano v2 inside a Bandai 1/12th R2D2's head..All this to control some LEDs..go big (small) or go home. 
 */

// 
#include <nRF5x_BLE_API.h>

#define DEVICE_NAME       "R2D2_head"
#define TXRX_BUF_LEN      20

String input;
String peerMAC;
int peerHandle;
uint8_t buf[TXRX_BUF_LEN];
uint16_t bytesRead = 20;
const byte numChars = 32;
char receivedChars[numChars];   // an array to store the received data

String rx;

boolean newData = false;

// Create ble instance
BLE                       ble;
Timeout                   timeout;

static uint8_t rx_buf[TXRX_BUF_LEN];
static uint8_t tx_buf[TXRX_BUF_LEN];
static uint8_t rx_buf_num;
static uint8_t tx_buf_num;
static uint8_t rx_state=0;

// The uuid of service and characteristics
static const uint8_t service1_uuid[]        = {0x6E, 0x40, 0x00, 0x01, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E};
static const uint8_t service1_rx_uuid[]     = {0x6E, 0x40, 0x00, 0x03, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E};
static const uint8_t service1_tx_uuid[]     = {0x6E, 0x40, 0x00, 0x02, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E};
static const uint8_t uart_base_uuid_rev[]   = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E};

uint8_t tx_value[TXRX_BUF_LEN] = {0,};
uint8_t rx_value[TXRX_BUF_LEN] = {0,};

// Create characteristic
GattCharacteristic  characteristic1(service1_tx_uuid, tx_value, 1, TXRX_BUF_LEN, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE );
GattCharacteristic  characteristic2(service1_rx_uuid, rx_value, 1, TXRX_BUF_LEN, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY);
GattCharacteristic *uartChars[] = {&characteristic1, &characteristic2};
GattService         uartService(service1_uuid, uartChars, sizeof(uartChars) / sizeof(GattCharacteristic *));

DeviceInformationService *deviceInfo;

void disconnectionCallBack(const Gap::DisconnectionCallbackParams_t *params) {
  Serial.println("Disconnected ...");
  Serial.print("Disconnected handle : ");
  Serial.println(params->handle, HEX);
  Serial.print("Disconnected reason : ");
  Serial.println(params->reason, HEX);
  Serial.println("Restart advertising ");
  ble.startAdvertising();
}

void connectionCallBack( const Gap::ConnectionCallbackParams_t *params ) {
  uint8_t index;

  Serial.println("Connecting...");

  if(params->role == Gap::PERIPHERAL) {
    Serial.println("Peripheral ");
  }

  Serial.print("The conn handle : ");
  Serial.println(params->handle, HEX);
  Serial.print("The conn role : ");
  Serial.println(params->role, HEX);

  Serial.print("The peerAddr type : ");
  Serial.println(params->peerAddrType, HEX);
  Serial.print("  The peerAddr : ");
  
  for(index=0; index<6; index++) {
    Serial.print(params->peerAddr[index], HEX);
    Serial.print(" ");
    peerMAC += String(params->peerAddr[index], HEX);
  }
  
  // Enforces that only the approved device can connect.. 
  if (peerMAC != "a9e8668682fc") {
    ble.disconnect(peerHandle, Gap::LOCAL_HOST_TERMINATED_CONNECTION);  
    Serial.println("Wrong peer... ");
  }
  peerMAC = ""; // Clear the variable


  Serial.println(" ");
 
  Serial.print("The ownAddr type : ");
  Serial.println(params->ownAddrType, HEX);
  Serial.print("  The ownAddr : ");
  for(index=0; index<6; index++) {
    Serial.print(params->ownAddr[index], HEX);
    Serial.print(" ");
  }
  Serial.println(" ");

  Serial.print("The min connection interval : ");
  Serial.println(params->connectionParams->minConnectionInterval, HEX);
  Serial.print("The max connection interval : ");
  Serial.println(params->connectionParams->maxConnectionInterval, HEX);
  Serial.print("The slaveLatency : ");
  Serial.println(params->connectionParams->slaveLatency, HEX);
  Serial.print("The connectionSupervisionTimeout : ");
  Serial.println(params->connectionParams->connectionSupervisionTimeout, HEX);

  Serial.println("Connected...");
}

void gattServerWriteCallBack(const GattWriteCallbackParams *Handler) {
  uint16_t index;
  
  if (Handler->handle == characteristic1.getValueAttribute().getHandle()) {
    Serial.print("[RX]: ");
    for(index=0; index<Handler->len; index++) {
      Serial.write(Handler->data[index]);
      rx += Handler->data[index];
    }
  }
  Serial.print(rx);
  if (rx == "7911010"){ // On
    Serial.println(" = ON");
    digitalWrite(D13, HIGH);
  }
  if (rx == "7910210210"){ // Off
    Serial.println(" = OFF");
    digitalWrite(D13, LOW);
  }
  rx = "";  
}

void m_uart_rx_handle() {
  //update characteristic data
  ble.updateCharacteristicValue(characteristic2.getValueAttribute().getHandle(), rx_buf, rx_buf_num);
  memset(rx_buf, 0x00,20);
  rx_state = 0;
}

void uart_handle(uint32_t id, SerialIrq event) {
  // Serial rx IRQ
  if(event == RxIrq) {
    if (rx_state == 0) {
      rx_state = 1;
      timeout.attach_us(m_uart_rx_handle, 100000);
      rx_buf_num=0;
    }
    while(Serial.available()) {
      if(rx_buf_num < 20) {
        rx_buf[rx_buf_num] = Serial.read();
        rx_buf_num++;
      }
      else
        Serial.read();
    }
  }
}

void setup() {
  // put your setup code here, to run once
  Serial.begin(115200);
  Serial.print("\n\n\n\n\n");
  Serial.println("R2D2 Head");
  Serial.println("-----------------------------------\n");
  Serial.println("Initialzing... ");
  
  pinMode(D13, OUTPUT);
  
  // Init ble
  ble.init();

  deviceInfo = new DeviceInformationService(ble, "Industrial Automaton", "R2 Series", "D2", "R", "fw-rev1", "soft-rev1");
  
  ble.onConnection(connectionCallBack);
  ble.onDisconnection(disconnectionCallBack);
  ble.onDataWritten(gattServerWriteCallBack);
  
  ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
  ble.accumulateAdvertisingPayload(GapAdvertisingData::SHORTENED_LOCAL_NAME,
                                   (const uint8_t *)"TXRX", sizeof("R2D2") - 1);
  ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_128BIT_SERVICE_IDS,
                                   (const uint8_t *)uart_base_uuid_rev, sizeof(uart_base_uuid_rev));

  ble.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
  // add service
  ble.addService(uartService);
  // set device name
  ble.setDeviceName((const uint8_t *)DEVICE_NAME);
  // set tx power,valid values are -40, -20, -16, -12, -8, -4, 0, 4
  ble.setTxPower(4);
  // set adv_interval, 100ms in multiples of 0.625ms.
  ble.setAdvertisingInterval(160);
  // set adv_timeout, in seconds
  ble.setAdvertisingTimeout(0);
  // start advertising
  ble.startAdvertising();
}

void loop() {
  // put your main code here, to run repeatedly:

    recvWithEndMarker();
    showNewData();

    

  ble.waitForEvent();
}


void recvWithEndMarker() {
    char endMarker = '\n';
    char r;
    
    while (Serial.available() > 0 && newData == false) {
        r = Serial.read();

        if (r != endMarker) {
            tx_buf[tx_buf_num] = r;
            tx_buf_num++;
            if (tx_buf_num >= numChars) {
               tx_buf_num = numChars - 1;
            }
        }
        else {
            tx_buf[tx_buf_num] = '\0'; // terminate the string
            tx_buf_num = 0;
            newData = true;
        }
    }
    
}

void showNewData() {
    if (newData == true) {
        Serial.print("[TX]");
        newData = false;
        ble.updateCharacteristicValue(characteristic1.getValueAttribute().getHandle(), tx_buf, sizeof(tx_buf));

    }
}
