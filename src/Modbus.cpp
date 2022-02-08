#include "Modbus.h"

ModbusClass::ModbusClass() {
	this->onRecievePacket = NULL;
}

void ModbusClass::enable() {
	digitalWrite(this->_enablePin, HIGH);
}

void ModbusClass::disable() {
	digitalWrite(this->_enablePin, LOW);
}

void ModbusClass::begin(Stream* serial, uint8_t address, short enablePin) {
	this->_serial = serial;
	this->_address = address;
	this->_enablePin = enablePin;
	pinMode(this->_enablePin, OUTPUT);
	this->enable();
}

bool ModbusClass::dataAvailable() {
	return this->_serial->available() > 0;
}

bool ModbusClass::isEnabled() {
	return digitalRead(this->_enablePin) == HIGH;
}

void ModbusClass::writeBytes(const byte buffer[], size_t size) {
	// Don't try to send if not enabled.
	if (!this->isEnabled()) {
		return;
	}

	this->_serial->write(buffer, size);
	this->_serial->flush();
}

bool ModbusClass::readPacket(ModbusPacket* packet) {
	if (!this->isEnabled()) {
		return false;
	}

	bool result = false;
	int idx = 0;
	const int bufSize = this->_serial->available();
	byte buffer[bufSize];

	while (this->_serial->available() > 0) {
		int val = this->_serial->read();
		if ((char)val == MODBUS_PACKET_TERMINATOR) {
			break;
		}

		buffer[idx] = (byte)val;
		idx++;
	}

	// Was this packet intended for us?
	if (buffer[0] == this->_address) {
		packet->targetAddr = (uint8_t)buffer[0];
		packet->targetAddr = (uint8_t)buffer[1];
		packet->payloadSize = (size_t)buffer[2];
		
		byte payload[packet->payloadSize];
		for (size_t i = 0; i < packet->payloadSize; i++) {
			payload[i] = buffer[i + 3];
		}

		packet->payload = payload;
		result = true;
	}
	else {
		packet = NULL;
	}

	delete[] buffer;
	return result;
}

void ModbusClass::loop() {
	ModbusPacket* packet;
	if (readPacket(packet)) {
		if (packet != NULL) {
			// If we got a valid packet intended for us, but the payload is
			// empty, then this a ping (discovery) request. Swap the
			// addresses and send it back (ACK).
			if (packet->payloadSize == 0) {
				byte buffer[4];
				buffer[0] = (byte)packet->srcAddr;
				buffer[1] = (byte)this->_address;
				buffer[2] = (byte)0;
				buffer[3] = (byte)MODBUS_PACKET_TERMINATOR;
				
				this->writeBytes(buffer, 4);
				delete[] buffer;
			}
			else {
				// Otherwise, if we have a receive handler, fire the event and
				// pass the packet data.
				if (this->onRecievePacket != NULL) {
					this->onRecievePacket(packet);
				}
			}
		}
	}
}

void ModbusClass::sendPacket(ModbusPacket* packet) {
	if (packet == NULL) {
		return;
	}

	const uint8_t packetSize = packet->payloadSize + 3;
	byte buffer[packetSize];
	buffer[0] = (byte)packet->targetAddr;
	buffer[1] = (byte)this->_address;       // Ensure our source address is set correctly.
	buffer[2] = (byte)packet->payloadSize;

	for (size_t i = 0; i < packet->payloadSize; i++) {
		buffer[i + 3] = (byte)packet->payload[i];
	}

	buffer[packetSize] = (byte)MODBUS_PACKET_TERMINATOR;

	this->writeBytes(buffer, packetSize);
	delete[] buffer;
}

void ModbusClass::setOnRecieve(void (*onRecievePacket)(ModbusPacket* packet)) {
	this->onRecievePacket = onRecievePacket;
}

void ModbusClass::write(uint8_t target, const byte* buffer, size_t size) {
	ModbusPacket* packet;
	packet->targetAddr = target;
	packet->srcAddr = this->_address;
	packet->payloadSize = size;
	packet->payload = (byte *)buffer;
	this->sendPacket(packet);
	delete packet;
}

ModbusClass Modbus;