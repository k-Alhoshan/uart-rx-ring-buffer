#include <Arduino.h>
#include "RingBuffer.h"
#define MY_F_CPU 16000000UL //Mega 2560 clock speed
#define BAUD 9600
#define MYUBRR (MY_F_CPU / 16 / BAUD - 1) 

void setup() {
  UBRR1H = (unsigned char) (MYUBRR >> 8); 
  UBRR1L = (unsigned char) MYUBRR;
  UCSR1B = (1 << RXEN1) | (1 << TXEN1) | (1 << RXCIE1); 
  UCSR1C = (1 << UCSZ11) | (1 << UCSZ10); 
  Serial.begin(9600);

  while (!(UCSR1A & (1 << UDRE1))){} 
  UDR1 = 'X'; 
}

RingBuffer<uint8_t, 32> uartRxBuffer;

ISR(USART1_RX_vect){
  uint8_t incoming = UDR1;
  uartRxBuffer.push(incoming);
}

void loop() {
  uint8_t data;
  if (uartRxBuffer.pop(data)) {
    Serial.print("Received: ");
    Serial.println(data);
  }

  if (uartRxBuffer.OverflowCount() > 0){
    //blink LED later
    uartRxBuffer.resetOverflowCount();
  }
}