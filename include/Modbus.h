#ifndef __MODBUS_H
#define __MODBUS_H

#include <Arduino.h>

#define MODBUS_PACKET_TERMINATOR '\0'

typedef struct
{
	uint8_t targetAddr;
	uint8_t srcAddr;
	size_t payloadSize;
	byte* payload;
} ModbusPacket;

class ModbusClass {
public:
	ModbusClass();
	void enable();
	void disable();
	bool isEnabled();
	void begin(Stream* serial, uint8_t address, short enablePin);
	bool dataAvailable();
	bool readPacket(ModbusPacket* packet);
	void loop();
	void sendPacket(ModbusPacket* packet);
	void setOnRecieve(void (*onReceivePacket)(ModbusPacket* packet));
	void write(uint8_t target, const byte* buffer, size_t size);

private:
	void writeBytes(const byte buffer[], size_t size);
	Stream* _serial;
	uint8_t _address;
	short _enablePin;
	void (*onRecievePacket)(ModbusPacket* packet);
};

extern ModbusClass Modbus;

#endif