/*
  ESPNowReceiver
  Author: Randy S. Bancroft
  Email: CannonFodderSE@gmail.com

  Receives X-Plane Datarefs sent via Espressif's ESP-Now communication protocol.

  This is a basic template to send and recieve Datarefs.

  Chosen Datarefs are stored in datarefs_array[].

  Function checkReceivedKey(String key, float value) determines what happens
  when a Dataref is received.

  This code is in the public domain.
*/


// ESP32 by Espressif Systems
// https://github.com/espressif/arduino-esp32
// Intalled with boards manager
#include <esp_now.h>
// WiFi  installed as part of ESP32 boards package
#include "WiFi.h"

// TM16XX LEDs and Buttons
// https://github.com/maxint-rd/TM16xx
// Installed with Library Manager
#include <TM1637.h>
// Installed as part of TM16XX LEDs and Buttons
#include <TM16xxDisplay.h>

// ************************************************************************************************
// Roll over configuration
  // If the millis() count maxes out and rolls over to zero
  // we want to reset the board to prevent unexpected behavior
  // from happening
  // It takes 49 days of continuios use to roll over.
  ulong rollover = 0;
// End Roll over configuration
// ************************************************************************************************

// ************************************************************************************************
// X-Plane configuration
  // Maximum length of Dataref key
  // At the time of writing this program, the longest official Dataref was
  // between 170 to 180 charaters
  // All data in a single ESP-Now packet cannot exceed 250 bytes
  const int datarefKey_length = 200;

  // Automatically update all Datarefs this board is maintaining
  ulong upDatePeriodicty = 5000;  // Send all switch setings every upDatePeriodicty milliseconds
  ulong nextUpdate = 0;           // When next update is to be sent to X-Plane

  // Array containing X-Plane Datarefs we are going to handle
  String datarefs_array[]{
    "sim/cockpit2/gauges/indicators/compass_heading_deg_mag",
    "sim/cockpit2/gauges/indicators/heading_electric_deg_mag_pilot",
    "sim/cockpit/radios/com1_freq_hz",
    "sim/cockpit/radios/com2_freq_hz",
    "sim/cockpit2/gauges/indicators/airspeed_kts_pilot",
    "sim/cockpit2/gauges/indicators/altitude_ft_pilot",
    "sim/cockpit2/gauges/indicators/slip_deg",
    "sim/cockpit2/annunciators/fuel_pressure[0]",
    "sim/cockpit2/gauges/indicators/heading_electric_deg_mag_copilot"
  };

  // Calculate the number of Datarefs in the datarefs_array above
  int datarefs_array_length = sizeof(datarefs_array) / sizeof(String);

// EndX-Plane configuration
// ************************************************************************************************

// ************************************************************************************************
// WiFi Configuration
  #define WIFI_CHANNEL 0  // Should match transmitter

  // SSID and password for ESP-Now Network
  // Ensure the match your Controller board
  const char* ssid = "Dummy";
  const char* password = "dummy!";

// End WiFi Configuration
// ************************************************************************************************

// ************************************************************************************************
// ESP-Now Configuration
  bool incomingDataReady = false;  //Flag indicating data has been received and placed in the buffer
  int systemID = 31416;            // System uniq ID in case mulitiple systems are near each other on the same ESP-Now network.  We only handle data with this ID
  char CONTROLLER[1]{ 0x2B };      // ACSII character for plus sign, +.  Indicates ESP-Now packet is from controller
  char INSTRUMENT[1]{ 0x2D };      // ACSII character for minus sign, -.  Indicates ESP-Now packet is from instrument board

  // Define the data structure for ESPNow packet
  typedef struct ESPNowMsgStructure {
    // Origin of ESP-Now packet.
    // + for contoller
    // - for instrument cluster
    char origin[1];
    // See "int systemID" above
    int sysID;
    // Float value for data to be sent to X-Plane
    float datarefValue;
    // Dataref key for data being sent
    // datarefKey_length is set in "X-Plane configuration" section above
    char datarefKey[datarefKey_length];
  } ESPNowMsgStructure;

  // Create a structured objects
  // Structure for out going data
  ESPNowMsgStructure ESPNowDataOut;
  // Structure for in coming data
  ESPNowMsgStructure ESPNowDataIn;

  // MAC Address of responder - edit as required
  // All set to 0xFF means broadcast to all listening stations within our ESP-Now network
  uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

  // Peer info
  esp_now_peer_info_t peerInfo;

  // callback when ESP-Now data is recv from Master Controller
  void OnDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int data_len) {
    Serial.println(".");
    memcpy(&ESPNowDataIn, data, sizeof(ESPNowDataIn));  // Copy incoming ESP-Now packet to ESPNowDataIn buffer
    incomingDataReady = true;
  }

  // Callback function called when ESP-Now data is sent to Master Controller
  void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
    String(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  }
// End ESP-Now Configuration
// ************************************************************************************************

// ************************************************************************************************
// Display Configuration
  // Module connection pins (Digital Pins) definitions
  // GPIO pin number
  // 4-digit display
  #define CLK4_1 2
  #define DIO4_1 3
  // 6-digit display
  #define CLK6_1 8
  #define DIO6_1 16
  

  //TM16xx(byte dataPin, byte clockPin, byte strobePin, byte maxDisplays, byte nDigitsUsed, bool activateDisplay=true,	byte intensity=7);
  // 4-digit display
  TM1637 module4_1(DIO4_1, CLK4_1, 4, true, 7);
  // 6-digit display
  TM1637 module6_1(DIO6_1, DIO6_1, 6, true, 7);

  // 4-digit display
  TM16xxDisplay display4_1(&module4_1, 4);  
  // 6-digit display
  TM16xxDisplay display6_1(&module6_1, 6);  

// End Display Configuration
// ************************************************************************************************

// ************************************************************************************************
// Input/Output Configuration
  // Variables that will change depending on switch state:
  bool currentState = false;
  const int isActivated = LOW;
  const int isDeactivated = HIGH;

  // Debounce variables to prevent switch noise from creating multiple switch changes on a single change
  unsigned long debounce = 0;
  unsigned long debounceThreshold = 150;

  // Switch/button array using structures
  struct switchStructure {
    char datarefKey[100];    // contains Dataref key
    int16_t activated;       // Value when switch is set
    int currentState;        // Last read state of switch
    unsigned long debounce;  // Millis time since last read
    uint8_t Pin;             // GPIO pin used for switch
    uint8_t PullUp;          // Indicate if pin configured for internal pull-up resistor
    int16_t multipleInput;   // Holds current value for switches that have multiple inputs, i.e. ignition key switch
  } switchStructure;

  // Initialize of an array of switch structures
  struct switchStructure switches[] = {
    { "sim/cockpit/electrical/nav_lights_on", 1, 1, 0, 7, INPUT_PULLUP, 0 }
  };

  // Calculate the number of switches in the switches above
  int switch_array_length = sizeof(switches) / sizeof(switchStructure);

// End Input/Output Configuration
// ************************************************************************************************

void setup() {
  // Set up Serial Monitor
  Serial.begin(115200);

  //WiFi setup
  WiFiSetup();

  // ESP-Now setup
  // Call before UDPSetup
  ESPNowSetup();

  // UDP setup
  UDPSetup();

  // Display setup
  displaySetup();

  // Hardware setup for switches and buttons
  hardwareSetup();
}

void loop() {
  // Check for incoming ESP-Now data
  // and is it from a Controller
  // and it has the same system ID
  switch (incomingDataReady && ESPNowDataIn.origin[0] == CONTROLLER[0] && ESPNowDataIn.sysID == systemID) {
    // All criteria met
    case true:
      {
        // Check to see if received Dataref key is one we handle
        checkReceivedKey(ESPNowDataIn.datarefKey, ESPNowDataIn.datarefValue);

        // Data read.  Reset incomingDataReady variable
        incomingDataReady = false;

        // Loop back test
        // Sennds a copy of the recieved packet back to Controller
        // Uncomment next line to enable
        //sendESPNow(key, value);
      }
      break;
    default:
      break;
  }

  // Check switch inputs
  checkSwitch();

  // Check for millis() rollover
  // Take 49 days to occur
  checkForRollOver();

  // Send current state of all switches
  // See upDatePeriodicty variable in "X-Plane configuration" above
  switch (nextUpdate < millis()) {
    case true:
      sendCurrentStatusForAllSwitches();
      nextUpdate = millis() + upDatePeriodicty;  // Send next in number of milliseconds set by updatePeriodicty
      break;
    default:
      break;
  }
}

void WiFiSetup() {
  // Set ESP32 as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  // Set up WiFi using SSID, password and WiFi Channel from the "WiFi Configuration" section above
  WiFi.begin(ssid, password, WIFI_CHANNEL, NULL, false);
}

void UDPSetup() {
  // Nothing to do
}

void displaySetup() {
  module4_1.clearDisplay();
  //module6_1.clearDisplay();

  module4_1.setupDisplay(true, 7);
  //module6_1.setupDisplay(true, 7);
}


void hardwareSetup() {
  // Initialize the pushbutton pins as set in the structured switches array above
  for (int p = 0; p < switch_array_length; p++) {
    // Set pin mode
    pinMode(switches[p].Pin, switches[0].PullUp);
    // Set currentState for each pin in array
    switches[p].currentState = digitalRead(switches[p].Pin);
  }
}

void ESPNowSetup() {
  // Initilize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    log_e("Error initializing ESP-NOW");
    return;
  }
  // Register the send callback
  esp_now_register_send_cb(OnDataSent);

  // Register callback function
  esp_now_register_recv_cb(OnDataRecv);

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    log_e("Failed to add peer");
    return;
  }
}

void checkForRollOver() {
  // Check for millis() rollover
  // Take 49 days to occur
  switch (millis() < rollover) {
    case true:
      // reboot ESP32
      ESP.restart();
      break;
    default:
      rollover = millis();
      break;
  }
}

int check_dataref_key(String key) {
  // Check received Dataref key to see if it matches any we are handling
  // Returns element array index number
  // Returns -1 if Dataref is not found
  int arrayElement = 0;
  while (true) {
    switch (key == datarefs_array[arrayElement]) {
      // Found Dataref
      // Return index number in array
      case true:
        return arrayElement;
        break;
      default:
        break;
    }
    switch (arrayElement > datarefs_array_length) {
      // Reached end of array and Dataref not found
      // return -1
      case true:
        return -1;
        break;
      default:
        break;
    }
    arrayElement++;
  }
  // Catch all for other conditions
  // Should never get here
  return -1;
}

void checkReceivedKey(String key, float value) {
  // Here we do what is need for each Dataref we handle
  // Create your own functions to light indicators,
  // send data to displays, move stepper motors or servos
  // for gauges or buzzers, etc...
  // Need to have as many caseses as array elements
  String vstring = String(value, 1);  // Convert float value to strin with one decimal place
  int svalueLen = vstring.length();   // Length of vstring
  String temp ="";                    // temp string variable
  
  switch (check_dataref_key(key)) {
    case 0:  // matched first Dataref in datarefs_array
      // code what to do for selected Dataref

      // This example is receiving Magnetic Compass bearing
      // and displaing it on a 4-digit display designated as display4_1
      // Pad left side of Vstring with space to rigth align in 4 digit display
      while (vstring.length() < 5) {
        vstring = " " + vstring;
      }
      // Send to display
      display4_1.println(vstring);
      break;

    case 1:  // matched first Dataref in datarefs_array
      // code what to do for selected Dataref
      //Serial.println(key + " = " + String(value));
      break;

    case 2:  // matched first Dataref in datarefs_array
      // code what to do for selected Dataref
      
      // This example is receiving radios com1 frequency in khz
      // and displaing it on a 6-digit display designated as display6_1
      {
        vstring = String(value / 100, 2);
        // Pad left side of Vstring with space to rigth align in 6 digit display
        while (vstring.length() < 7) {
          vstring = " " + vstring;
        }

        display6_1.println(stringToSixDigit(vstring));
      }
      break;

    case 3:  // matched first Dataref in datarefs_array
      // code what to do for selected Dataref
      //Serial.println(key + " = " + String(value));
      break;

    case 4:  // matched first Dataref in datarefs_array
      // code what to do for selected Dataref
      //Serial.println(key + " = " + String(value));
      break;

    case 5:  // matched first Dataref in datarefs_array
      // code what to do for selected Dataref
      //Serial.println(key + " = " + String(value));
      break;

    case 6:  // matched first Dataref in datarefs_array
      // code what to do for selected Dataref
      //Serial.println(key + " = " + String(value));
      break;

    case 7:  // matched first Dataref in datarefs_array
      // code what to do for selected Dataref
      //Serial.println(key + " = " + String(value));
      break;

    case 8:  // matched first Dataref in datarefs_array
      // code what to do for selected Dataref
      //Serial.println(key + " = " + String(value));
      break;

    case 9:  // matched first Dataref in datarefs_array
      // code what to do for selected Dataref
      //Serial.println(key + " = " + String(value));
      break;

    case 10:  // matched first Dataref in datarefs_array
      // code what to do for selected Dataref
      //Serial.println(key + " = " + String(value));
      break;

    default:
      // code what to do if no Dataref is matched
      // usually nothing
      //Serial.println("\tno key matched" + key + " = " + String(value));
      break;
  }
}

void sendESPNow(String key, float value) {
  // Broadcast X-Plane dataref over ESP-Now
  // Debug
    // Serial.print(key);
    // Serial.print(" - ");
    // Serial.println(value);
  // End Debug

  char datarefKeyBuffer[datarefKey_length];  // datarefKey_length = 200
  ESPNowDataOut.origin[0] = INSTRUMENT[0];            // ASCII code for - (minus sign)
  ESPNowDataOut.sysID = systemID;            // System ID as set in "ESP-Now Configuration" section above

  key.toCharArray(datarefKeyBuffer, key.length() + 1);  // Convert key from string to character array
  strcpy(ESPNowDataOut.datarefKey, datarefKeyBuffer);   // Copy it to the ESPNowDataOut structure

  ESPNowDataOut.datarefValue = value;  // Add the float value to the ESPNowDataOut structure

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&ESPNowDataOut, sizeof(ESPNowDataOut));

  if (result == ESP_OK) {
    log_i("Sending confirmed");
  } else {
    log_e("Sending error");
  }
  delay(1);  // Delay to prevent buffer over run
}

void checkSwitch() {
  // Check switches for state change
  // Send changes to Controller
  for (int s = 0; s < switch_array_length; s++) {
    int buttonState = 0;  // variable for reading the pushbutton status

    // Have we passed the bounce range since last reading?
    // If so, read button
    unsigned long timer = millis() - switches[s].debounce;
    switch (timer > debounceThreshold) {
      case true:
        {
          // read the state of the pushbutton value:
          buttonState = digitalRead(switches[s].Pin);
          int16_t activatedValue = switches[s].activated;
          int16_t currentState = switches[s].currentState;
          // ButtonState has changed
          // Update array and send appropriate message to controller
          switch (buttonState != currentState) {
            case true:
              switches[s].debounce = millis();  // Reset debounce
              switch (buttonState) {
                case isActivated:
                  switches[s].currentState = isActivated;
                  switches[s].multipleInput = activatedValue;
                  deactivateMatchingDataref(s);  // Update all matching Dataref key in array
                  SetMultiplesForDataref(s, true);
                  sendESPNow(switches[s].datarefKey, activatedValue);
                  break;
                default:
                  switches[s].currentState = isDeactivated;
                  switches[s].multipleInput = 0;  // Update all matching Dataref key in array
                  SetMultiplesForDataref(s, false);
                  sendESPNow(switches[s].datarefKey, 0);
                  break;
              }
              break;
          }
        }
        break;
      default:
        break;
    }
  }
}

void deactivateMatchingDataref(int key) {
  // Switches may have multiple inputs
  // For example ignition switch has 5 positions with 4 inputs
  // Positions are OFF, LEFT, RIGHT, BOTH and START
  // Only one associated input is active per position
  // Inputs attached to LEFT, RIGHT, BOTH and START
  // with OFF having no input
  // loop through array and update current state for all inputs for associate Dataref
  int16_t activatedValue = switches[key].activated;
  for (int multiples = 0; multiples < switch_array_length; multiples++) {
    switch (String(switches[multiples].datarefKey) == String(switches[key].datarefKey)) {
      case true:
        switch (multiples != key) {
          case true:
            switches[multiples].currentState = isDeactivated;
            break;
          default:
            break;
        }

        break;
      default:
        break;
    }
  }
}

void SetMultiplesForDataref(int key, bool active) {
  // Switches may have multiple inputs
  // For example ignition switch has 5 positions with 4 inputs
  // Positions are OFF, LEFT, RIGHT, BOTH and START
  // Only one associated input is active per position
  // Inputs attached to LEFT, RIGHT, BOTH and START
  // with OFF having no input
  // loop through array and update multipleInput for all inputs for associate Dataref
  String input = String(switches[key].datarefKey);
  int16_t newValue = 0;
  switch(active){
    case true:
      newValue = switches[key].activated;
      break;
    default:
      break;  
  }
  for (int scan = 0; scan < switch_array_length; scan++) {
    String matrix = String(switches[scan].datarefKey);
    switch(matrix == input){
      case true:
        switches[scan].multipleInput = newValue;
        break;
      default:
        break;
    }
  }
}

void sendCurrentStatusForAllSwitches() {
  // Send the current status for all Datarefs in Array
  // Ensures X-Plane has current switch positions
  // Periodicity is set in loop statement
  // Recommend every 5 to 10 seconds to prevent over saturating X-Plane's input
  for (int current = 0; current < switch_array_length; current++) {
    String currentDataref = switches[current].datarefKey;
    int16_t currentState = switches[current].multipleInput;
    for (int lookup = switch_array_length - 1; lookup >= current; lookup--) {  // Run backwards through array so we don't look up elements already processed
      String lookupDataref = switches[lookup].datarefKey;
      switch (currentDataref == lookupDataref) {                               // Did we find another element with the same Dataref
        case true:                                                             // Found another element with the same Dataref
          switch(current == lookup){
            case true:
              sendESPNow(currentDataref, currentState);                        // Send activated state message to Controller
              break;
            default:
              lookup = current;
              break;
          }
          break;
        default:                                                               // No other element with matching Dataref
          break;
      }
    }
  }
}

String stringToSixDigit(String in) {
  // Place digits in correct order on 6-digit TM1637 display
  // Displays I tested with the digit would be rearranged
  // Sent -> 123456
  // Displayed -> 321654
  // int decPt = in.indexOf('.');
  int decPt = in.indexOf('.');
  String temp = in.substring(0, decPt) + in.substring(decPt + 1);
  int crossRefTab[]{2, 1, 0, 5, 4, 3, 6};
  char buff[8];
  buff[0] = temp[2];
  buff[1] = temp[1];
  buff[2] = temp[0];
  buff[3] = temp[5];
  buff[4] = temp[4];
  buff[5] = temp[3];
  buff[6] = 0x00;
  buff[7] = 0x00;
  decPt = crossRefTab[decPt - 1]+1;
  memmove(buff + decPt + 1, buff + decPt, 6-decPt);
  memset (buff + decPt, '.',1);
  Serial.println("decpt = " + String(decPt) + " | " + String(buff));
  return String(buff);
}
