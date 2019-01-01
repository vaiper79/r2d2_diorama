/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/*
 * This sketch demonstrate the central API(). A additional bluefruit
 * that has bleuart as peripheral is required for the demo.
 */
#include <bluefruit.h>

BLEClientDis  clientDis;
BLEClientUart clientUart;

String peerMAC ="";

void setup(){
  Serial.begin(115200);
  delay(2000); // Getting gibberish at the beginning without at least 2 seconds delay..

  Serial.print("\n\n\n\n\n");
  Serial.println("R2D2 Base");
  Serial.println("-----------------------------------\n");
  
  // Initialize Bluefruit with maximum connections as Peripheral = 0, Central = 1
  // SRAM usage required by SoftDevice will increase dramatically with number of connections
  Bluefruit.begin(0, 1);
 
  
  Bluefruit.setName("R2D2_base");

  // Configure DIS client
  clientDis.begin();

  // Init BLE Central Uart Serivce
  clientUart.begin();
  clientUart.setRxCallback(bleuart_rx_callback);

  // Increase Blink rate to different from PrPh advertising mode
  Bluefruit.setConnLedInterval(250);

  // Callbacks for Central
  Bluefruit.Central.setConnectCallback(connect_callback);
  Bluefruit.Central.setDisconnectCallback(disconnect_callback);

  /* Start Central Scanning
   * - Enable auto scan if disconnected
   * - Interval = 100 ms, window = 80 ms
   * - Don't use active scan
   * - Start(timeout) with timeout = 0 will scan forever (until connected)
   */
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(160, 80); // in unit of 0.625 ms
  Bluefruit.Scanner.filterUuid(BLEUART_UUID_SERVICE);
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Scanner.start(0);                   // // 0 = Don't stop scanning after n seconds
}

/**
 * Callback invoked when scanner pick up an advertising data
 * @param report Structural advertising data
 */
void scan_callback(ble_gap_evt_adv_report_t* report){
  PRINT_LOCATION();
  uint8_t len = 0;
  uint8_t buffer[32];
  memset(buffer, 0, sizeof(buffer));

  //Starting
  Serial.println("Initializing...");
  
  // MAC is in little endian --> print reverse
  Serial.print("Remote MAC:   ");
  Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
  Serial.print("\n");
  Serial.println("Should match: DB:90:37:48:DE:F5");

  // Put peer address in variable for comparison
  for (int index = 5; -1 < index; index--) {
    peerMAC += report->peer_addr.addr[index];
  }
  
  // The MAC of the peer in decimal..make sure we only connect to the right device
  if (peerMAC == "2191445572222245"){   
    
    // Check if advertising contain BleUart service
    if ( Bluefruit.Scanner.checkReportForService(report, clientUart ) ){
      Serial.print("BLE UART service detected. Connecting ... ");
      
      // Connect to device with bleuart service in advertising
      Bluefruit.Central.connect(report);
    }else{      
      
      // For Softdevice v6: after received a report, scanner will be paused
      // We need to call Scanner resume() to continue scanning
      Bluefruit.Scanner.resume();
    }
  }
  // Reset the variable for next run..
  peerMAC = "";
}

/**
 * Callback invoked when an connection is established
 * @param conn_handle
 */
void connect_callback(uint16_t conn_handle){
  Serial.println("Connected");
  Serial.print("Dicovering DIS ... ");
  if ( clientDis.discover(conn_handle) ){
    Serial.println("Found it");
    char buffer[32+1];
    
    // read and print out Manufacturer
    memset(buffer, 0, sizeof(buffer));
    if ( clientDis.getManufacturer(buffer, sizeof(buffer)) ){
      //Serial.print("Manufacturer: ");
      //Serial.println(buffer);

      // Crude way of making sure the correct peers are peering.. 
      // Should try to kill this process earlier, during scan..but haven't figured out the 
      // format of the UUID filters.... 
      if (String(buffer) == "Industrial Automaton"){
        Serial.println("Hello R2..we meet again!");
      }else{
        Bluefruit.Central.disconnect(conn_handle);
        Serial.println("These aren't the droids you're looking for.. ");
      }
    }

    // read and print out Model Number
    memset(buffer, 0, sizeof(buffer));
    if (clientDis.getModel(buffer, sizeof(buffer))){
      //Serial.print("Model: ");
      //Serial.println(buffer);
    }
    //Serial.println();
  }  

  Serial.print("Discovering BLE Uart Service ... ");

  if ( clientUart.discover(conn_handle) ){
    Serial.println("Found it");

    Serial.println("Enable TXD's notify");
    clientUart.enableTXD();

    Serial.println("Ready to receive from peripheral");
  }else{
    Serial.println("Found NONE");
    
    // disconnect since we couldn't find bleuart service
    Bluefruit.Central.disconnect(conn_handle);
  }    
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason){
  (void) conn_handle;
  (void) reason;
  
  Serial.println("Disconnected");
  Serial.print("\n\n\n\n\n");
}

/**
 * Callback invoked when uart received data
 * @param uart_svc Reference object to the service where the data 
 * arrived. In this example it is clientUart
 */
void bleuart_rx_callback(BLEClientUart& uart_svc){
  Serial.print("[RX]: ");
  while ( uart_svc.available() ){
    Serial.print( (char) uart_svc.read() );
  }
  Serial.println();
}

void loop()
{
  if ( Bluefruit.Central.connected() )
  {
    // Not discovered yet
    if ( clientUart.discovered() )
    {
      // Discovered means in working state
      // Get Serial input and send to Peripheral
      if ( Serial.available() )
      {
        delay(2); // delay a bit for all characters to arrive
        
        char str[20+1] = { 0 };
        Serial.readBytes(str, 20);
        
        clientUart.print( str );
        Serial.println(str);
      }
    }
  }
}
