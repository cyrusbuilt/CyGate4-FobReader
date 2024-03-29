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
#include "LED.h"
#include "Buzzer.h"
#include "MFRC522.h"
#include "Modbus.h"

#define FIRMWARE_VERSION "1.0"

#define DEBUG_BAUD_RATE 9600
#define MODBUS_BAUD_RATE 115200
#define ADDRESS_BASE 0x10
#define MODBUS_HOST_ADDR 0

// Pin definitions
#define PIN_PWR_LED 7
#define PIN_ACT_LED 6
#define PIN_PIEZO 5
#define PIN_MFRC522_RESET 8
#define PIN_MFRC522_SS 12
#define PIN_ADDRESS_A0 11
#define PIN_ADDRESS_A1 10
#define PIN_ADDRESS_A2 9
#define PIN_MODBUS_ENABLE 2

// I2C packet data
#define DETECT_ACK 0xDA
#define TAG_PRESENCE_SIZE 2
#define TAG_PACKET_SIZE 14
#define TAG_DATA_SIZE 4
#define READER_FW_SIZE 2
#define SELF_TEST_SIZE 2

/**
 * @brief Possible device commands.
 */
enum class CommandType : byte {
	DETECT = 0xFA,
	INIT = 0xFB,
	GET_FIRMWARE = 0xFC,
	SELF_TEST = 0xDC,
	GET_TAGS = 0xFD,
	GET_AVAILABLE = 0xFE,
	BAD_CARD = 0xDD,
	GET_MIFARE_VERSION = 0xDB
};

// Global vars
LED pwrLED(PIN_PWR_LED, NULL);
LED actLED(PIN_ACT_LED, NULL);
Buzzer piezo(PIN_PIEZO, NULL, "");
MFRC522 reader(PIN_MFRC522_SS, PIN_MFRC522_RESET);
byte nuidPICC[TAG_DATA_SIZE];  // TODO Should this be volatile?
//volatile byte command = 0xFF;
//volatile bool processCommand = false;
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
	// Serial.print(F("INFO: Performing self test ..."));
	bool result = reader.PCD_PerformSelfTest();
	// Serial.println(result ? F("PASS") : F("FAIL"));

	// Serial.println(F("INFO: Sending result"));
	byte packet[SELF_TEST_SIZE];
	packet[0] = (byte)CommandType::SELF_TEST;
	packet[1] = (byte)result;
	// Serial.print(F("DEBUG: Sending packet = "));
	// printHex(packet, SELF_TEST_SIZE);
	// Serial.println();
	Modbus.write(MODBUS_HOST_ADDR, packet, SELF_TEST_SIZE);
	delete[] packet;
}

/**
 * @brief Handles the bad-card signal from the CyGate4 host controller by
 * blinking the "ACT" LED and activating the piezo buzzer 3 times at 200ms
 * apart. 
 */
void badCard() {
	// Serial.println(F("ERROR: Host controller indicates bad card!"));
	byte response[1];
	response[0] = (byte)CommandType::BAD_CARD;
	// Serial.print(F("DEBUG: Sending response = 0x"));
	// Serial.println(response, HEX);
	Modbus.write(MODBUS_HOST_ADDR, response, 1);
	for (uint8_t i = 0; i < 3; i++) {
		actLED.on();
		piezo.on();
		delay(200);
		piezo.off();
		actLED.off();
	}

	delete[] response;
}

/**
 * @brief Clears the currently stored NUID (tag).
 */
void clearNUID() {
	for (size_t i = 0; i < sizeof(nuidPICC); i++) {
		nuidPICC[i] = 0x00;
	}
}

/**
 * @brief Sends an acknowledgement back to the host indicating that
 * we are, in fact, a CyGate4 Fob Reader and we are present.
 */
void sendDetectAck() {
	// Serial.println(F("INFO: Sending detect ACK."));
	byte response[1];
	response[0] = (byte)DETECT_ACK;
	// Serial.print(F("DEBUG: Sending response = 0x"));
	// Serial.println(response, HEX);
	Modbus.write(MODBUS_HOST_ADDR, response, 1);
	delete[] response;
}

/**
 * @brief Sends a packet containing the firmware version back to the host.
 */
void sendFirmware() {
	// Serial.println(F("INFO: Sending firmware version"));
	String fw = String(FIRMWARE_VERSION);
	int strBufLen = fw.length() + 1;
	byte strBuf[strBufLen];
	fw.getBytes(strBuf, strBufLen);

	size_t size = strBufLen + 2;
	
	byte* buffer = new byte[size];
	buffer[0] = (byte)CommandType::GET_FIRMWARE;
	buffer[1] = strBufLen;
	for (int i = 0; i < strBufLen; i++) {
		buffer[i + 2] = strBuf[i];
	}

	// Serial.print(F("DEBUG: sending bytes "));
	// for (size_t i = 0; i < size; i++) {
	// 	Serial.print(F("0x"));
	// 	Serial.print(buffer[i], HEX);
	// 	Serial.print(F(" "));
	// }

	// Serial.println();
	Modbus.write(MODBUS_HOST_ADDR, buffer, size);
	delete[] buffer;
}

bool hasTagData() {
	// Serial.print(F("DEBUG: Tag data: "));
	// printHex(nuidPICC, TAG_DATA_SIZE);
	// Serial.println();

	bool result = false;
	for (uint8_t i = 0; i < TAG_DATA_SIZE; i++) {
		if (nuidPICC[i] != 0x00) {
			result = true;
			break;
		}
	}

	return result;
}

/**
 * @brief Checks to see if a new tag is present.
 * 
 * @return true if a tag has been scanned that isn't stored in memory yet.
 * @return false if the tag has already been recently scanned.
 */
bool isNewTagPresent() {
	bool present = reader.uid.uidByte[0] != nuidPICC[0] || 
    	reader.uid.uidByte[1] != nuidPICC[1] || 
    	reader.uid.uidByte[2] != nuidPICC[2] || 
    	reader.uid.uidByte[3] != nuidPICC[3];
	Serial.print(F("DEBUG: Is new tag data = "));
	Serial.println(present ? F("Yes") : F("No"));
	return present;
}

/**
 * @brief Sends a packet to the host indicating a new tag is present.
 */
void sendTagPresence() {
	// Serial.println(F("INFO: Sending tag presence"));

	byte packet[TAG_PRESENCE_SIZE];
	packet[0] = (byte)CommandType::GET_AVAILABLE;
	packet[1] = (byte)hasTagData();

	// Serial.print(F("DEBUG: Sending packet: "));
	// printHex(packet, TAG_PRESENCE_SIZE);
	// Serial.println();
	
	Modbus.write(MODBUS_HOST_ADDR, packet, TAG_PRESENCE_SIZE);
	delete[] packet;
}

/**
 * @brief Sends the tag to the CyGate4 host over I2C.
 */
void sendCard() {
	//Serial.println(F("INFO: Sending card data"));
	byte packet[TAG_PACKET_SIZE];
	packet[0] = (byte)CommandType::GET_TAGS;
	packet[1] = 1;  // TODO Change this when we support more than one tag.
	packet[2] = TAG_DATA_SIZE;

	// Copy tag bytes
	for (uint8_t i = 0; i < packet[2]; i++) {
		packet[3 + i] = nuidPICC[i];
	}

	// Serial.print(F("DEBUG: Sending packet = "));
	// printHex(packet, TAG_PACKET_SIZE);
	// Serial.println();

	Modbus.write(MODBUS_HOST_ADDR, packet, TAG_PACKET_SIZE);
	delete[] packet;
	clearNUID();
}

/**
 * @brief Sends a packet to the host containing the RFID reader version.
 */
void sendMiFareVersion() {
	//Serial.println(F("INFO: Sending RFID reader version"));
	byte packet[READER_FW_SIZE];
	packet[0] = (byte)CommandType::GET_MIFARE_VERSION;
	packet[1] = reader.PCD_ReadRegister(MFRC522::VersionReg);

	// Serial.print(F("DEBUG: Sending packet = "));
	// printHex(packet, READER_FW_SIZE);
	// Serial.println();

	Modbus.write(MODBUS_HOST_ADDR, packet, READER_FW_SIZE);
	delete[] packet;
}

void sendInitAck() {
	byte buffer[1];
	buffer[0] = (byte)CommandType::INIT;
	Modbus.write(MODBUS_HOST_ADDR, buffer, 1);
	delete[] buffer;
}

/**
 * @brief Handles data received on the I2C bus.
 * @param byteCount The number of bytes received.
 */
// void commBusReceiveHandler(int byteCount) {
// 	// NOTE: We *should* only be recieving single-byte commands.
// 	command = Wire.read();
// 	processCommand = true;
// }

/**
 * @brief Handles requests received on the I2C bus and acknowledges valid
 * commands by sending the appropriate data back to the host.
 */
void handleCommand(byte command) {
	// if (!processCommand) {
	// 	return;
	// }

	// Serial.println(F("INFO: I2C request received"));
	// Serial.print(F("INFO: Received command: 0x"));
	// if (command < 16) {
	// 	Serial.print(F("0"));
	// }

	// Serial.println(command, HEX);
	// if (command == 0xFF) {
	// 	// We get a single 0xFF every time the host (master) device does a bus scan,
	// 	// sort of like the I2C equivalent of a 'ping'. We can just ignore these.
	// 	Serial.println(F("INFO: Bus scan from master"));
	// }

	
	switch (command) {
		case (byte)CommandType::INIT:
			// TODO Not sure exactly what to do here just yet.
			// Call setup() again?
			// Soft-reboot (asm jmp 0)?
			// Just re-init the reader?
			// For now, just ack.
			//Serial.println(F("INFO: Sending init ACK."));
			//Wire.write((byte)CommandType::INIT);
			sendInitAck();
			break;
		case (byte)CommandType::DETECT:
			sendDetectAck();
			break;
		case (byte)CommandType::GET_FIRMWARE:
			sendFirmware();
			break;
		case (byte)CommandType::SELF_TEST:
			performSelfTest();
			break;
		case (byte)CommandType::GET_TAGS:
			sendCard();
			break;
		case (byte)CommandType::BAD_CARD:
			badCard();
			break;
		case (byte)CommandType::GET_AVAILABLE:
			sendTagPresence();
			break;
		case (byte)CommandType::GET_MIFARE_VERSION:
			sendMiFareVersion();
			break;
		default:
			// Serial.print(F("WARN: Unrecognized command received: 0x"));
			// Serial.println(command, HEX);
			break;
	}

	//processCommand = false;
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
	if (isNewTagPresent()) {
    	Serial.println(F("INFO: New card detected."));

		// Store NUID into nuidPICC array
    	for (uint8_t i = 0; i < TAG_DATA_SIZE; i++) {
      		nuidPICC[i] = reader.uid.uidByte[i];
    	}

		Serial.print(F("INFO: The NUID tag is (hex): "));
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

int getDeviceAddress() {
	byte addressOffset = 0;
	for (uint8_t i = 0; i < 3; i++) {
		pinMode(addrPins[i], INPUT_PULLUP);
		delay(1);

		addressOffset <<= 1;
		if (digitalRead(addrPins[i]) == HIGH) {
			addressOffset |= 0x01;
		}
	}

	int busAddress = ADDRESS_BASE + addressOffset;
	return busAddress;
}

void onReceiveHandler(ModbusPacket* packet) {
	if (packet->payloadSize == 1) {
		// We should only be getting single-byte commands.
		handleCommand(packet->payload[0]);
	}
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
	Serial.print(F("INIT: "));
	reader.PCD_DumpVersionToSerial();
}


void initModbus() {
	int address = getDeviceAddress();
	Serial.print(F("INIT: Initializing modbus on address 0x"));
	Serial.print(address, HEX);
	Serial.print(F(" ..."));
	Serial1.begin(MODBUS_BAUD_RATE);
	Modbus.begin(&Serial1, address, PIN_MODBUS_ENABLE);
	Modbus.setOnRecieve(onReceiveHandler);
	Serial.println(F("DONE"));
}

/**
 * @brief Initializes the I2C communication bus which allows the CyGate4 host
 * controller to connect.
 */
// void initCommBus() {
// 	Serial.print(F("INIT: Initializing I2C comm bus... "));

// 	byte addressOffset = 0;
// 	for (uint8_t i = 0; i < 3; i++) {
// 		pinMode(addrPins[i], INPUT_PULLUP);
// 		delay(1);

// 		addressOffset <<= 1;
// 		if (digitalRead(addrPins[i]) == HIGH) {
// 			addressOffset |= 0x01;
// 		}
// 	}

// 	int busAddress = ADDRESS_BASE + addressOffset;
// 	Wire.begin(busAddress);
// 	Wire.onReceive(commBusReceiveHandler);
// 	Wire.onRequest(commBusRequestHandler);
	
// 	Serial.println(F("DONE"));
// 	Serial.print(F("INIT: I2C bus address: "));
// 	Serial.println(busAddress, HEX);
// }

/**
 * @brief Boot sequence. Initializes the firmware.
 */
void setup() {
	initSerial();
	initOutputs();
	initReader();
	//initCommBus();
	initModbus();
	Serial.println(F("INIT: Boot sequence complete."));
}

/**
 * @brief Main program loop. Checks for new cards being presented to the reader.
 */
void loop() {
	Modbus.loop();
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