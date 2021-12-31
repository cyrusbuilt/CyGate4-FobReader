/**
 * @file main.cpp
 * @author cyrus (cyrusbuilt at gmail dot com)
 * @brief Firmware for CyGate4 - Fob Reader.  Intended to run on the Arduino
 * Micro Rev3. Works with MiFare MFRC522 RFID readers and is intended to
 * interface with the CyGate4 over I2C.
 * @version 1.0
 * @date 2021-12-31
 * 
 * @copyright Copyright (c) Cyrus Brunner 2021
 * 
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "LED.h"
#include "Buzzer.h"
#include "MFRC522.h"

#define FIRMWARE_VERSION "1.0"

#define DEBUG_BAUD_RATE 9600
#define ADDRESS_BASE 0x10

#define PIN_PWR_LED 7
#define PIN_ACT_LED 6
#define PIN_PIEZO 5
#define PIN_MFRC522_RESET 8
#define PIN_MFRC522_SS 12
#define PIN_ADDRESS_A0 11
#define PIN_ADDRESS_A1 10
#define PIN_ADDRESS_A2 9

enum class CommandType : byte {
	SELF_TEST = 0x00,
	CHECK_CARD = 0x01,
	BAD_CARD = 0x02
};

// Global vars
LED pwrLED(PIN_PWR_LED, NULL);
LED actLED(PIN_ACT_LED, NULL);
Buzzer piezo(PIN_PIEZO, NULL, "");
MFRC522 reader(PIN_MFRC522_SS, PIN_MFRC522_RESET);
byte nuidPICC[4];  // TODO Should this be volatile?
volatile byte command = 0xFF;
const short addrPins[3] = {
	PIN_ADDRESS_A0,
	PIN_ADDRESS_A1,
	PIN_ADDRESS_A2
};

/**
 * @brief Print Hex values to the serial console
 * 
 * @param buffer The buffer containing the values to print.
 * @param bufferSize The buffer size.
 */
void printHex(byte *buffer, byte bufferSize) {
	for (byte i = 0; i < bufferSize; i++) {
    	Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    	Serial.print(buffer[i], HEX);
  	}
}

/**
 * @brief Instructs the MFRC522 to execute a self-test.
 */
void performSelfTest() {
	Serial.print(F("INFO: Performing self test ..."));
	bool result = reader.PCD_PerformSelfTest();
	Serial.println(result ? F("PASS") : F("FAIL"));
	Wire.write((uint8_t)result);
}

/**
 * @brief Handles the bad-card signal from the CyGate4 host controller by
 * blinking the "ACT" LED and activating the piezo buzzer 3 times at 200ms
 * apart. 
 */
void badCard() {
	Serial.println(F("ERROR: Host controller indicates bad card!"));
	for (uint8_t i = 0; i < 3; i++) {
		actLED.on();
		piezo.on();
		delay(200);
		piezo.off();
		actLED.off();
	}
}

/**
 * @brief Clears the currently stored NUID (tag).
 */
void clearNUID() {
	for (size_t i = 0; i < sizeof(nuidPICC); i++) {
		nuidPICC[i] = 0xFF;
	}
}

/**
 * @brief Sends the tag to the CyGate4 host over I2C.
 */
void sendCard() {
	Serial.println(F("INFO: Sending card data"));
	Wire.write(nuidPICC, sizeof(nuidPICC));
	clearNUID();
}

/**
 * @brief Handles data received on the I2C bus.
 * @param byteCount The number of bytes received.
 */
void commBusReceiveHandler(int byteCount) {
	// NOTE: We *should* only be recieving single-byte commands.
	command = Wire.read();
	Serial.print(F("INFO: Received command: "));
	Serial.println(command, HEX);
}

/**
 * @brief Handles requests received on the I2C bus and acknowledges valid
 * commands by sending the appropriate data back to the host.
 */
void commBusRequestHandler() {
	switch (command) {
		case (byte)CommandType::SELF_TEST:
			performSelfTest();
			break;
		case (byte)CommandType::CHECK_CARD:
			sendCard();
			break;
		case (byte)CommandType::BAD_CARD:
			badCard();
			break;
		default:
			Serial.print(F("WARN: Unrecognized command received: 0x"));
			Serial.println(command, HEX);
			break;
	}
}

/**
 * @brief Retrieves card/fob details from the reader and dumps the details
 * to the serial console.
 */
void getCardInfo() {
	Serial.println(F("INFO: Read card info: "));
	reader.PICC_DumpDetailsToSerial(&reader.uid);
}

/**
 * @brief Checks to see if a compatible RFID tag was scanned.
 * @return true if a compatible tag was scanned.
 * @return false if an incompatible tag was scanned.
 */
bool isValidPiccType() {
	Serial.print(F("INFO: PICC type: "));
  	MFRC522::PICC_Type piccType = reader.PICC_GetType(reader.uid.sak);
  	Serial.println(reader.PICC_GetTypeName(piccType));

	// Check is the PICC of Classic MIFARE type
  	if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
    	piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    	piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    	Serial.println(F("ERROR: Tag is not of type MIFARE Classic."));
    	return false;
  	}

	return true;
}

/**
 * @brief Reads new tags and stores them in the buffer. If the read tag
 * already exists in the internal buffer, then it is ignored.
 */
void readNewTag() {
	if (reader.uid.uidByte[0] != nuidPICC[0] || 
    	reader.uid.uidByte[1] != nuidPICC[1] || 
    	reader.uid.uidByte[2] != nuidPICC[2] || 
    	reader.uid.uidByte[3] != nuidPICC[3] ) {
    	Serial.println(F("INFO: New card detected."));

		// Store NUID into nuidPICC array
    	for (byte i = 0; i < 4; i++) {
      		nuidPICC[i] = reader.uid.uidByte[i];
    	}

		Serial.print(F("The NUID tag is (hex): "));
    	printHex(reader.uid.uidByte, reader.uid.size);
		Serial.println();
	}
	else {
		Serial.println(F("WARN: Card read previously."));
	}
}

/**
 * @brief Puts the RFID reader in idle mode.
 */
void idleReader() {
	reader.PICC_HaltA();
	reader.PCD_StopCrypto1();
}

/**
 * @brief Initializes the RS-232 serial console.
 */
void initSerial() {
	Serial.begin(DEBUG_BAUD_RATE);
	while (!Serial) {
		delay(1);
	}

	Serial.print(F("INIT: CyGate4-FobReader v"));
	Serial.print(FIRMWARE_VERSION);
	Serial.println(F(" booting..."));
}

/**
 * @brief Initializes outputs (LEDs, piezo). 
 */
void initOutputs() {
	Serial.print(F("INIT: Initializing output devices ..."));
	pwrLED.init();
	pwrLED.on();

	actLED.init();
	actLED.off();

	piezo.init();
	piezo.off();
	Serial.println(F("DONE"));
}

/**
 * @brief Initializes the SPI bus and the MFRC522.
 */
void initReader() {
	Serial.print(F("INIT: Initializing MFRC522 ..."));
	SPI.begin();
	reader.PCD_Init();
	delay(4);
	Serial.println(F("DONE"));
	reader.PCD_DumpVersionToSerial();
}

/**
 * @brief Initializes the I2C communication bus which allows the CyGate4 host
 * controller to connect.
 */
void initCommBus() {
	Serial.print(F("INIT: Initializing I2C comm bus... "));

	byte addressOffset = 0;
	for (uint8_t i = sizeof(addrPins) - 1; i >= 0; i--) {
		pinMode(addrPins[i], INPUT_PULLUP);
		delay(1);

		addressOffset <<= 1;
		if (digitalRead(addrPins[i]) == HIGH) {
			addressOffset |= 0x01;
		}
	}

	int busAddress = ADDRESS_BASE + addressOffset;
	Wire.begin(busAddress);
	Wire.onReceive(commBusReceiveHandler);
	Wire.onRequest(commBusRequestHandler);
	
	Serial.println(F("DONE"));
	Serial.print(F("INIT: I2C bus address: "));
	Serial.println(busAddress, HEX);
}

/**
 * @brief Boot sequence. Initializes the firmware.
 */
void setup() {
	initSerial();
	initOutputs();
	initReader();
	initCommBus();
	Serial.println(F("INIT: Boot sequence complete."));
}

/**
 * @brief Main program loop. Checks for new cards being presented to the reader.
 */
void loop() {
	if (!reader.PICC_IsNewCardPresent() || !reader.PICC_ReadCardSerial()) {
		return;
	}

	piezo.on();
	actLED.on();

	getCardInfo();
	if (!isValidPiccType()) {
		piezo.off();
		actLED.off();
		return;
	}

	readNewTag();

	piezo.off();
	actLED.off();
	idleReader();
}