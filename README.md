# ESP32-AP
This program uses esp-idf to create an access point on an ESP32 aswell as a tcp server on the device so devices that are connected to the access point can send commands to the ESP32.

The commands that I have implemented are:

	IRRX	-> 		Starts IR recieving mode

		This mode saves all recorded IR frames and sends the address 
		and command to the connected device.
	
	IRTX	-> 		Starts IR transmitting mode

		After the mode has been activated, the esp32 will accept two 
		hexadecimals(address, command) in the format: `#### ####` and 
		transmit it as a NEC frame.
		
	RPLY	-> 		Replays recorded IR frames when in IRTX mode
	
	exit	-> 		Leave the current mode or close the connection


This program is not complete, I wish to make use of this code for later projects with my ESP32's.
