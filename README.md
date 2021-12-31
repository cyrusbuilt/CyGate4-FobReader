# CyGate4-FobReader

Proximity Fob/Card reader add-on for CyGate4

## Synopsis

This is a proximity fob/card reader add-on for [CyGate4](https://github.com/cyrusbuilt/CyGate4).  It is designed to be a carrier board for the the [MiFare RC522](https://www.sunfounder.com/products/rfid-kit-red?gclid=Cj0KCQiArvX_BRCyARIsAKsnTxO6bHAqdLXI3AX8OYaqy2ZX4cgSUb_q4Uc7XjE1UnNScZQvjEgl5SEaAoUVEALw_wcB). This device can read all fobs/cards supported by the MiFare MFRC522 and connects to the CyGate4 over I2C (aka 2-wire) bus. It can be powered by the CyGate4 itself or via an external 5VDC supply. This board can store up to 5 scanned cards in memory before overwriting the oldest scans. As scanned cards are read from the controller, they are removed from the buffer. Support for the CyGate4 Fob Reader is built-in to the CyGate4 firmware. This board uses a 7bit addressing scheme that allows up to 8 readers to be attached to a single CyGate4 at any given time. As long as the address is configured appropriately, they will be auto-detected and initialized during CyGate4's boot cycle.