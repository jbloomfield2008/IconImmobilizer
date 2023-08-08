#include "mcbcomm.h"

static const char* TAG = "MCB";

MCB::MCB()
{
	// do nothing
}

void MCB::begin(HardwareSerial* serialinf, uint8_t rxPin, uint8_t txPin)
{
	serial = serialinf;
	serial->begin(38400, SERIAL_8N1, rxPin, txPin);
	serial->setTimeout(1000);
}

void MCB::getBoardInfo()
{
  ESP_LOGV(TAG, "getBoardInfo called");
	// Open comms with the controller
  BoardInfo info;
	write((uint8_t*)&info, 3);
	uint8_t buffer[39];
	read(buffer, 39);
}

uint16_t MCB::readValue(uint16_t addr)
{
  ESP_LOGV("MCB", "readValue called with address %d", addr);
	readMemEx(addr);
	return readValueResponse();
}

bool MCB::writeValue(uint16_t addr, uint16_t value)
{
  ESP_LOGV("MCB", "writeValue called with address %d, value %d", addr, value);
	writeMemEx(addr, value);
	return readStatus() == 0;
}

void MCB::writeMemEx(uint32_t addr, uint16_t value)
{
  WriteMemEx msg;
	msg.addr = addr;
	msg.value = value;
	msg.checksum = calcChecksum((uint8_t*)&msg, 10);
  write((uint8_t*)&msg, 11);
}

void MCB::readMemEx(uint32_t addr)
{
  ReadMemEx msg;
	msg.addr = addr;
	msg.checksum = calcChecksum((uint8_t*)&msg, 8);
  write((uint8_t*)&msg, 9);
}

uint8_t MCB::calcChecksum(uint8_t* data, uint8_t len)
{
	data++; // skip over the SOM
	uint8_t sum = 0; // cmd field
	for (int i = 1; i < len; i++)
		sum += *data++; // add len and data
	return 0x100 - sum;
}

void MCB::read(uint8_t* data, uint16_t size)
{
 uint16_t start = millis();
	while (serial->available() < size &&
	       millis() - start < 200)
	{
		delay(100);
	}
	serial->readBytes(data, size);
  ESP_LOG_BUFFER_HEXDUMP(TAG, data, size, ESP_LOG_INFO);
	while (serial->available() > 0)
	{
		serial->read();
	}
}

void MCB::write(uint8_t* data, uint16_t size)
{
  ESP_LOG_BUFFER_HEXDUMP(TAG, data, size, ESP_LOG_INFO);
	serial->write(data, size);
	delay(200);
}

uint16_t MCB::readValueResponse()
{
  StatusMsg msg;
	read((uint8_t*)&msg, 5);
	if (msg.som == 0x2b && msg.status == 0)
	{
		return msg.value;
	}
	return -1;
}

uint8_t MCB::readStatus()
{
  StatusMsg msg;
	read((uint8_t*)&msg, 3);
	if (msg.som == 0x2b)
	{
		return msg.status;
	}
	return -1;
}