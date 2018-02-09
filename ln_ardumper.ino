/*
 * Arduino Mega/2560 universal eprom dumper
 * By Jonathan Gevaryahu AKA Lord Nightmare
 * Version 0.97 (20160919)
 * 
 * Note: be sure to set your rom size, width (and, if width is 16 bits,
 * endianness) FIRST, down below!
 * 
 * Wire Mapping:
 * (D0 and D1 (0 and 1) are for RX and TX)
 * 13 is the built-in LED
 * 22 thru 45 are A0 thru A23
 * A0 thru A16 are D0 thru D16
 * 8 is /CE
 * 9 is /OE
 * 
 * Be sure to attach the VCC and GND of your eprom to the arduino's 5V and GND respectively!
 * Some eproms require VPP (if a separate pin) to be tied to +5v, be sure to observe this if needed.
 * I do not recommend using this for dumping eproms which require 12v and -5v in addition to 5v.
 * 
 * For 16 bit wide roms which have the low address pin called 'A1' instead of 'A0'
 * connect dumper A0 to rom A1, dumper A1 to rom A2, etc.
 * 
 * This dumper does correctly handle Mostek mask roms which latch address on the
 * falling edge of /CE.
 * 
 * 
 * Copyright (c) 2016, Jonathan Gevaryahu 
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
 
// set rom min address and dump length here. min is almost always going to be 0 and for 16-bit roms must be on an even byte boundary
#define MINADDRESS 0x00000000
#define DUMPSIZE 0x10000
// set word (16 bit) mode here by uncommenting the following line and commenting out the one afterward
//#define WORDMODE 1
#undef WORDMODE
// if WORDMODE is defined, uncomment the line below for big endian order
//#define ENDIAN_BIG 1

int ledPin = 13;
int cePin = 8;
int oePin = 9;

void setup()
{
  // set up serial as 9600,N,8,1
  Serial.begin(9600);
  // pin 13 is the LED, and turn it off
  pinMode(ledPin, OUTPUT); digitalWrite(ledPin, LOW);
  // pin 8 is /CE, and turn it on
  pinMode(cePin, OUTPUT); digitalWrite(cePin, HIGH);
  // pin 9 is /OE, and turn it on
  pinMode(oePin, OUTPUT); digitalWrite(oePin, HIGH);
  // pins 22 thru 45 are address pins A0 thru A23, turn them off
  for (int thisPin = 22; thisPin < 46; thisPin++)
  {
    pinMode(thisPin, OUTPUT);
    digitalWrite(thisPin, LOW);
  }
  // pins Analog0 thru Analog15 are D0 thru D15; (A0 is 54, A1 is 55...)
  for (int thisPin = 54; thisPin < 70; thisPin++)
  {
    pinMode(thisPin, INPUT_PULLUP);
  }
}

// print a hex digit including up to numDigits leading zeroes
void print_hex(uint32_t input, int numDigits)
{
  uint32_t mask = ~(0xFFFFFFFF << ((numDigits-1)<<2));
  // while input  0x0001
  //       mask   0x0FFF (shifted right by 4)
  // still equals 0x0001 
  // print a 0
  // shift mask
  //       mask   0x00FF (shifted right by 8)
  // still equals 0x0001
  // print a 0
  // shift mask
  //       mask   0x000F (shifted right by 12)
  // still equals 0x0001
  // print a 0
  // shift mask
  //       mask   0x0000 (shifted right by 16)
  // mask is 0, bail out
  // print number in hex
  while ((input == (input&mask)) && (mask))
  {
    Serial.print("0");
    mask >>= 4;
  }
  Serial.print(input, HEX);
}

void loop()
{
  uint8_t ihexBuffer[16];
  uint8_t ihexBufferCount = 0;
  uint32_t startAddress = (uint32_t)MINADDRESS;
  uint32_t endAddress = (uint32_t)MINADDRESS+(uint32_t)DUMPSIZE;
  uint32_t lastAddressHigh = startAddress&0xFFFF0000;
  // now start reading data
  for (uint32_t address = startAddress; address < endAddress; address++)
  {
    // check if we overflowed a 16 bit page
    if (lastAddressHigh != (address&0xFFFF0000))
    {
      uint8_t checksum = 0;
      // write an extended address record for the new high address
      Serial.print(":02000004");
      checksum += (uint8_t)0x02;
      checksum += (uint8_t)0x00;
      checksum += (uint8_t)0x00;
      checksum += (uint8_t)0x04;
      print_hex(address>>16, 4);
      checksum += (uint8_t)((address>>16)&0xFF);
      checksum += (uint8_t)((address>>24)&0xFF);
      print_hex((uint8_t)(checksum^0xFF)+1, 2);
      Serial.println();
      lastAddressHigh = address&0xFFFF0000;
    }
    setAddressPins(address);
    // no need wait for stabilize, typically
    digitalWrite(cePin, LOW);
    digitalWrite(oePin, LOW);
    delayMicroseconds(1); // wait for stabilize, should only need 450ns or so max
#ifdef WORDMODE
    uint16_t tempDataRead = readDataPins();
#ifdef ENDIAN_BIG
    ihexBuffer[ihexBufferCount++] = (uint8_t)((tempDataRead>>8)&0xFF);
    ihexBuffer[ihexBufferCount++] = (uint8_t)(tempDataRead&0xFF);
#else
    ihexBuffer[ihexBufferCount++] = (uint8_t)(tempDataRead&0xFF);
    ihexBuffer[ihexBufferCount++] = (uint8_t)((tempDataRead>>8)&0xFF);
#endif
#else
    ihexBuffer[ihexBufferCount++] = (uint8_t)readDataPins();
#endif
    digitalWrite(cePin, HIGH);
    digitalWrite(oePin, HIGH);

    // if buffer is full OR we're on the last byte OR we're about to overflow a 16 bit page, dump a line
    if ((ihexBufferCount == 16) || (address == endAddress-1) || (lastAddressHigh != ((address+1)&0xFFFF0000)))
    {
      uint8_t checksum = 0;
      Serial.print(":");
      print_hex(ihexBufferCount,2);
      checksum += (uint8_t)ihexBufferCount;
      print_hex((address-(ihexBufferCount-1))&0xFFFF, 4);
      checksum += (uint8_t)(address-(ihexBufferCount-1))&0x00FF;
      checksum += (uint8_t)((address-(ihexBufferCount-1))&0xFF00)>>8;
      Serial.print("00");
      checksum += (uint8_t)0x00;
      for (int i = 0; i < ihexBufferCount; i++)
      {
        print_hex(ihexBuffer[i],2);
        checksum += ihexBuffer[i];
      }
      print_hex((uint8_t)(checksum^0xFF)+1,2);
      Serial.println();
      ihexBufferCount = 0; 
    }
    lastAddressHigh = address&0xFFFF0000;
  }
  Serial.println(":00000001FF"); // write an EOF record
  Serial.end(); // stop serial
  while (1) // we're done, blink the led forever
  {
     digitalWrite(13, LOW);
     delay(100); 
     digitalWrite(13, HIGH);
     delay(100);  
  };
}

void setAddressPins(uint32_t address)
{
  uint32_t tempAddress = address;
  for (int thisPin = 22; thisPin < 46; thisPin++)
  {
    if (tempAddress&1)
    {
      digitalWrite(thisPin, HIGH);
    }
    else
    {
      digitalWrite(thisPin, LOW);
    }
    tempAddress>>=1;
  }
}

uint16_t readDataPins()
{
  uint16_t tempData = 0;
#ifdef WORDMODE
  for (int thisPin = 69; thisPin >= 54; thisPin--)
#else
  for (int thisPin = 61; thisPin >= 54; thisPin--)
#endif
  {
    tempData<<=1;
    if (digitalRead(thisPin) == HIGH) tempData |= 1;
  }
  return tempData;
}

