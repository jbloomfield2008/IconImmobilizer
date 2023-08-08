#include <HardwareSerial.h>

/**
 * This class provides a partial implementation of the NXP FreeMaster MCB
 * protocol. This is not a full implementation, rather only enough to
 * support what's required for a 2022 Icon Golf Cart with a new LVTong
 * controller.
 */
class MCB {
    public:
        /** Default constructor. */
        MCB();
        /**
         * Must be called to configure and begin communication with
         * the cart's controller. Note the ESP32 supports software
         * assignable pins to each of the 3 UARTs, giving extra
         * flexiblity to the wiring and coding of the firmware. In
         * addition, this method will initialize the serial comms with
         * a baud rate of 38400.
         *
         * @param serial The hardware serial interface to use.
         * @param rxPin The RX pin to attach to the serial interface.
         * @param txPin The TX pin to attach to the serial interface.
         */
        void begin(HardwareSerial* serial, uint8_t rxPin, uint8_t txPin);
        /**
         * Called to get the board information. This starts the
         * communication with the controller.
         */
        void getBoardInfo();
        /**
         * Reads a value from a given memory address. A value of 0xFFFF
         * indicates an error was returned by the controller. This
         * method will only read 2 bytes from the given memory address,
         * which is all that is currently required.
         */
        uint16_t readValue(uint16_t addr);
        /**
         * Writes a new value to the a given region in the controllers
         * memory. Again, this method will only write 2 bytes to the
         * controller.
         */
        bool writeValue(uint16_t addr, uint16_t value);
    private:
        void writeMemEx(uint32_t addr, uint16_t value);
        void readMemEx(uint32_t addr);
        uint8_t calcChecksum(uint8_t* data, uint8_t len);
        void read(uint8_t* data, uint16_t size);
        void write(uint8_t* data, uint16_t size);
        uint16_t readValueResponse();
        uint8_t readStatus();

        HardwareSerial* serial;

        struct BoardInfo {
            uint8_t som = 0x2b;
            uint8_t cmd = 0xC0;
            uint8_t checksum = 0x40;
        };

        struct WriteMemEx {
            uint8_t som = 0x2b;
            uint8_t cmd = 0x05;
            uint8_t len = 0x07;
            uint8_t addr_len = 0x02;
            uint32_t addr;
            uint16_t value;
            uint8_t checksum;
        };

        struct ReadMemEx {
            uint8_t som = 0x2b;
            uint8_t cmd = 0x04;
            uint8_t len = 0x05;
            uint8_t addr_len = 0x02;
            uint32_t addr;
            uint8_t checksum;
        };

        struct StatusMsg {
            uint8_t som;
            uint8_t status;
            uint16_t value;
            uint8_t checksum;
        };
};