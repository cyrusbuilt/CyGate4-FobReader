# CyGate4-FobReader

Proximity Fob/Card reader add-on for CyGate4

## Synopsis

This is a proximity fob/card reader add-on for [CyGate4](https://github.com/cyrusbuilt/CyGate4).  It is designed to be a carrier board for the the [MiFare RC522](https://www.sunfounder.com/products/rfid-kit-red?gclid=Cj0KCQiArvX_BRCyARIsAKsnTxO6bHAqdLXI3AX8OYaqy2ZX4cgSUb_q4Uc7XjE1UnNScZQvjEgl5SEaAoUVEALw_wcB). This device can read all fobs/cards supported by the MiFare MFRC522 and connects to the CyGate4 over I2C (aka 2-wire) bus. It can be powered by the CyGate4 itself or via an external 5VDC supply. This board can store up to 5 scanned cards in memory before overwriting the oldest scans. As scanned cards are read from the controller, they are removed from the buffer. Support for the CyGate4 Fob Reader is built-in to the CyGate4 firmware. This board uses a 7bit addressing scheme that allows up to 8 readers to be attached to a single CyGate4 at any given time. As long as the address is configured appropriately, they will be auto-detected and initialized during CyGate4's boot cycle.

## Operation

On power-up, the PWR LED will turn on and the device will immediately boot and log debug messages to the serial console. It will make itself available on the I2C bus (if connected) at the configured address. Presenting a MiFare-comptible prox card/fob will trigger a read. If a valid read occurs, the tag will be stored in memory.  When the read occurs, the ACT LED will flash once and the piezo will beep once.  This will happen whether it is connected to a CyGate4 host or not. If the tag sent to CyGate4 is determined to be invalid, it will send a command back indicating a bad card which will cause this reader to beep and flash 3 times in ~1 second.

## Configuration

The board has 3 jumpers (J4 - J6) that are used to configure the I2C bus address. Each jumper *MUST* be jumpered to either (+) (binary 1) or (-) (binary 0) and cannot be left open. The 3 values are bit-OR'ed together along with an address offset to compute the bus address.

## Command Set and Communication Protocol

The following commands can be sent to the device and will return a response. Since I have not timed each of the responses yet,
so as a general rule of thumb, I would allow at least 10ms between sending the command and expecting the response.

```
DETECT = 0xFA
INIT = 0xFB
GET_FIRMWARE = 0xFC
SELF_TEST = 0xDC
GET_TAGS = 0xFD
GET_AVAILABLE = 0xFE
BAD_CARD = 0xDD
GET_MIFARE_VERSION = 0xDB
```

DETECT: Asks the reader to identify itself. Sends back a one-byte ACK response if present: (DETECT_ACK = 0xDA).

INIT: Instructs the reader to initialize itself. Currently, this will only return a command ACK (INIT = 0xFB).

GET_FIRMWARE: This instructs the reader to report it's firmware version which will send the following packet:

```
Byte 0 = ACK (GET_FIRMWARE = 0xFC)
Byte 1 = Version length (# of bytes representing version string)
Byte 2 - n = The version string
```

SELF_TEST: Instructs the reader to perform a self-test. This will return a packet containing self-test result:

```
Byte 0 = ACK (SELF_TEST = 0xDC)
Byte 1 = Result (1 = Pass, 0 = Fail)
```

GET_TAGS: Instructs the reader to return any tags it has stored in memory (currently limited to 1 (the most recent) but will be up to 5 in the future). This will return the following packet data:

```
Byte 0 = ACK (GET_TAGS = 0xFD)
Byte 1 = Record count (1)
Byte 2 = Tag length in bytes (up to 10)
Byte 3 - 13 = Tag serial number (right-padded with 0xFF)
```

GET_AVAILABLE: Instructs the reader to indicate whether or not any tags are present. Sends back the following packet:

```
Byte 0 = ACK (GET_AVAILABLE = 0xFE)
Byte 1 = Result (1 = Available, 0 = No tags)
```

BAD_CARD: Instructs the reader to signal to the user that a bad card was presented (access denied). Sends back a one-byte ACK (BAD_CARD = 0xDD).

GET_MIFARE_VERSION: Instructs the reader to return the MiFare RFID reader version. Sends back the following packet:

```
Byte 0 = ACK (GET_MIFARE_VERSION = 0xDB)
Byte 1 = MiFare version (ie. 0x92)
```