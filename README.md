# X-Plane-Instruments-via-ESP-Now
Purpose of this project is to send and receive X-Plane Datarefs for DIY cockpits.  Secondary goal is to reduce some of the necessary wiring by using wireless modules that can be customized for specific areas.  For example one module can be configured to handle lighting and electrical switches, another for displays and another for gauges.  Only power needs to be routed to the modules.  This way an instument cluster can be maintained as a stand alone module that can be easily moved around or placed on a workbench.

Modules are based on ESP32 boards using ESP-Now wireless protocol in broadcast mode to communicate with each other.  Broadcast mode is used vice unicast mode, one to one communication, due to unicast being limited to a maximum of 20 connections.  It is theoretically unlimited in broadcast mode. See Espessif's page here:  https://docs.espressif.com/projects/esp-faq/en/latest/application-solution/esp-now.html#what-is-the-maximum-number-of-devices-that-can-be-controlled-by-esp-now

ESP-NOW has a limited signal range, typically around 220 meters under ideal  conditions. The actual range can vary depending on factors like environmental   interference, antenna design, and obstacles in the communication path.  Don't think any cockpit will exceed 220 meters.

X-Plane uses UDP to send information and receive inputs in what they call Dataref packets.  It used to be able to do this through the serial port.  In version 12 the serial port feature  was removed. This project was designed and tested with version 12.  The manual for exchanging data with X-plane can be found in its installation folder "X-Plane 12/Instructions/Exchanging Data with X-Plane.rtfd/TXT.rtf".

Controller board receives UDP Dataref packets over WiFi then broadcasts the data to all listening Instrument boards.  The ESP-Now packet includes a marker indicating it was from the controller, a system ID number, value for the Dataref sent and Dataref key for a specific function. Only one Dataref per packet.  Since it is broadcast there is no filtering nor routing.  It also receives ESP-Now packets from Instrument boards and relays them to X-Plane via UDP.

Instrument board listens for ESP-Now packets.  It verifies they are from a Controller with a matching system ID.  Compares the Dataref to a list of Datarefs it handles.  Then executes the desired function.  Currently there is only support for TM1637 4 and 6 digit displays.  Switch positions are scanned and a corresponding Dataref is sent to X-Plane when the state changes.  All switch positions are sent at an interval designated at time of compile.  Have tested lighting indicators, displaying data on TM1637 4 and 6 digit displays and switches.

I've test this using a generic ESP32-S3-USB-OTG  and a Seeed Studio XIAO ESP32C3. See links below:

https://www.aliexpress.us/item/3256804864979510.html?pvid=22266b55-fb71-4d55-9fbd-767172ff569a

https://www.seeedstudio.com/Seeed-XIAO-ESP32C3-p-5431.html

I get no kick backs from these links.

Recommend setting "Partition Scheme" to "Huge APP" in the Arduino IDE when compiling.  Tried a few other schemes and they would cause the board to crash repeatedly.

Requires Espressif's ESP32 version 3.0.0 board manager or higher. When I started this project the ESP32 board manager was on version 2.0.16.  It was upgraded to version 3.0.0 while I was developing.  This cause the programs to no longer compile.  After days of researching I discovered the "OnDataRecv" call back function was modified.

Before 3.0.0 it used the below format:

     void OnDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len)

From 3.0.0 on it uses the below format:

	void OnDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int data_len)
	
