# Interrupt-Driven UART Ring Buffer (AVR / ATmega2560)

Fixed-capacity ring buffer with template used to capture UART bytes
inside an ISR, decoupling asynchronous hardware interrupts from the main
program loop. Built and tested on an Arduino Mega 2560 using PlatformIO.

![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-orange.svg)
![Framework](https://img.shields.io/badge/framework-Arduino-00979D.svg)
![Board](https://img.shields.io/badge/Board-Arduino%20Mega%202560-00979D.svg)

## Purpose

UART hardware receives data asynchronously, completely independent of what the main loop is doing. The
hardware only holds one received byte at a time in its data register. If
the program isn't ready to read it before the next byte arrives, that
data is silently lost.

The standard fix is the pattern this project implements:

1. **Interrupt-driven capture.** 
Instead of continuously polling for a byte in `loop()` (which only checks whenever `loop()` gets around to it), an ISR fires immediately the instant a byte is received regardless of what the CPU was doing, guaranteeing no byte is missed
at the hardware level.

2. **A ring buffer as a queue.** The ISR's only job is to grab the byte
and push it into the buffer, then return as fast as possible. This decouples *when data arrives* from
*when the program processes it*, `loop()` can drain the buffer 
whenever it's ready, at its own pace.

3. **Bounded memory with overflow detection.** The buffer can't grow
forever, so if the incoming bytes outpace the `loop()`, the buffer reports it via an overflow counter instead of
corrupting data or crashing silently.

## The head == tail ambiguity

A ring buffer tracks two indexes in a fixed-size array:

- `tail` — the next empty slot to **write** to (advanced on push)
- `head` — the oldest unread item, the next slot to **read** from
  (advanced on pop)

The natural way to check if the buffer is empty is `head == tail`. But
that condition is also true when the buffer is completely full:
after enough pushes, `tail` wraps all the way around the array and
lands back on the exact same index as `head`. Both states produce the same `head == tail` comparison: an empty
buffer with nothing written yet, and a full buffer where every slot
has been written and is still unread. The two indexes alone can't tell them apart,
because wrapping with modulo throws away how many full laps `tail` has
completed relative to `head`.

This project resolves the ambiguity with a `counter_` member: incremented
on every push, decremented on every pop. `counter_ == 0` unambiguously
means empty; `counter_ == Capacity` unambiguously means full,
independent of what `head` and `tail` happen to equal.

## Design notes

- **Templated on `<typename T, uint8_t Capacity>`**, so the same class
  works for any element type and any fixed capacity, not just
  `uint8_t` UART bytes.

- **`volatile` on all state shared between the ISR and `loop()`**
  (`head_`, `tail_`, `counter_`, `overFlowCount_`). Without this, the
  compiler is free to cache these values in a register inside `loop()`
  and never re-check memory, meaning `loop()` could spin forever
  believing the buffer is empty even after the ISR has pushed new data.

- **Configurable overwrite policy.** `OverwritePolicy::Reject` (default)
  drops new data and increments the overflow counter when full.
  `OverwritePolicy::Overwrite` instead discards the oldest unread byte
  to make room for the newest one.

- **`push`/`pop` return `bool`** so the caller can detect failure
  (buffer full on push, buffer empty on pop) rather than getting silent
  garbage.

## Hardware setup

- Arduino Mega 2560
- UART1 (pins 18/TX1, 19/RX1) used for the buffered channel, UART0 is
  reserved for the USB connection used to upload code and run the
  Serial Monitor, so it can't double as the test channel.
- A jumper wire connects pin 18 (TX1) directly to pin 19 (RX1), creating
  a hardware loopback: anything transmitted out UART1 is immediately
  received back on the same UART, without needing a second device.

## How it works, register by register

```cpp
UBRR1H = (unsigned char) (MYUBRR >> 8); // Baud rate register, upper 4 bits
UBRR1L = (unsigned char) MYUBRR;        // Baud rate register, lower 8 bits
```
`UBRR1H`/`UBRR1L` together hold a 12-bit value that tells the hardware
how to divide the CPU clock down to the target baud rate (9600 here).
Split across two registers because AVR registers are 8 bits wide.

```cpp
UCSR1B = (1 << RXEN1) | (1 << TXEN1) | (1 << RXCIE1);
```
- `RXEN1` — enables the receiver hardware (listens on pin 19)
- `TXEN1` — enables the transmitter hardware (drives pin 18)
- `RXCIE1` — enables the RX Complete interrupt, so `ISR(USART1_RX_vect)`
  works automatically whenever a full byte has been received

```cpp
UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
```
Sets the frame's data bit count to 8 (the "8" in "8N1"). Parity and stop
bit settings live in other bits of this same register and are left at
their power-on defaults (no parity, 1 stop bit).

```cpp
while (!(UCSR1A & (1 << UDRE1))) {}
UDR1 = 'X';
```
`UDRE1` in `UCSR1A` is a status flag set by hardware when the transmit
buffer is free. Since transmitting a byte takes real time (~1ms at 9600
baud) and the CPU runs far faster, writing to `UDR1` again before the
previous byte finishes shifting out would corrupt the transmission, so
this loop blocks until it's safe to load the next byte.

`UDR1` is a single register name that maps to two separate physical
buffers: writing to it loads the transmit buffer,
reading from it retrieves the receive buffer.

## Verifying it works

The loopback jumper makes end-to-end testing possible without a second
device:

1. `setup()` transmits a test byte (`'X'`, ASCII 88) out UART1.
2. The jumper wire carries that signal from pin 18 back into pin 19.
3. UART1's receiver reconstructs the byte and triggers
   `ISR(USART1_RX_vect)`.
4. The ISR reads `UDR1` and pushes the byte into `uartRxBuffer`.
5. `loop()` pops the byte and prints it over USB Serial (UART0), visible
   in the Serial Monitor at 9600 baud.

Expected output:
```
Received: 88
```

## Possible additions later on

- Stress test overflow detection by transmitting more bytes than the
  buffer's capacity (32) faster than `loop()` can drain them, and confirm
  `overflowCount()` increments.
- Add a second ring buffer plus the `UDRE1` interrupt for fully
  non-blocking transmission, instead of the current polling wait in
  `loop()`.

## License

Released under the [MIT License](LICENSE).