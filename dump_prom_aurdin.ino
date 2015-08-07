/*
  Dump PROM
  
  Peter Bortas 2015
 */
// FIXME: Just search the whole space calculated from addrsize
#define PROMSIZE 1024 // Size of PROM in adressable units. The size of that unit depends on the PROM
#define DATASIZE 8 // Width of the data bus
#define ADDRSIZE 9 // Width of the address bus (FIXME: 10 really, but ran out of pins)
#define STABDELAY 100 // Time in milliseconds between setting the address and reading the data. 2716 needs 450ns from setting thte addresses, so theoreticly "1" would be OK.
#define REREADS 10 // Times each bit should be retried with STABDELAY in between, for verifying flakey pins/addresses. Must be at least 1 to get any reads.

// NOTE: Each address will take DATASIZExREREADSxSTABDELAY to read. So that is 8s per byte with 8x10x100

#define ADDRSTARTPIN 2
#define DATASTARTPIN 12

int address = 0; // Address to dump

void end(const char* message)
{
   Serial.print(message);
   Serial.print("\r\n");
   Serial.println();
   delay(10000);
   Serial.end();
 
   while(1) 
     delay(100); // FIXME: Does this save any energy on Arduino?
}

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }  

  // Code and instructions are written for Uno, so the analogue pins had to be used.
  // This is just a sanity check. Remove the it after making sure you have the wireing 
  // correct on your setup.
  if(A0 != 14 || A5 != 19)
    end("FATAL: Code assumes A0 through A5 is assigned to pin 14-19");

  // TODO: Set up pins based on size variables?
  // Address pins
  pinMode(2, OUTPUT); //ADDRSTARTPIN
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  // need 11 address lines really, but we are out, so ground 2716:A10 manually, and then rerun with it high.
  // DATA we want to read. 
  // FIXME: Store a shunk in memory and reuse TX/RX by opening and closing the serial line while not dumping?
  //        Could be really simle, just do it after each byte. Then there would be one spare line. :)
  pinMode(12, INPUT); //DATASTARTPIN
  pinMode(13, INPUT);
  pinMode(14, INPUT);
  pinMode(15, INPUT);
  pinMode(16, INPUT);
  pinMode(17, INPUT);
  pinMode(18, INPUT);
  pinMode(19, INPUT);
}

byte verifyRead(int pin)
{
  byte reads[REREADS];
  byte i, bad_reads=0;
  
  for(i = 0; i < REREADS; i++) {
    delay(STABDELAY);
    reads[i] = digitalRead(pin);
    if(i > 0)
      if(reads[i-1] != reads[i])
        bad_reads++;
  }
  
  if(bad_reads) {
    Serial.print("Bad reads on pin ");
    Serial.print(pin, DEC);
    Serial.print(":\t");
    for(i = 0; i < REREADS; i++)
      Serial.print(reads[i], DEC);
    Serial.println();
  }

  // Just return the last measurement we took regardless of failures
  return reads[REREADS-1];
}

int bitReverse(int num)
{
  return ((num & 0x01) << 7)
       | ((num & 0x02) << 5)
       | ((num & 0x04) << 3)
       | ((num & 0x08) << 1)
       | ((num & 0x10) >> 1)
       | ((num & 0x20) >> 3)
       | ((num & 0x40) >> 5)
       | ((num & 0x80) >> 7);
}

// the loop function runs over and over again forever
void loop() {
  int bit;
  
  Serial.print("Dump starting\r\n");

  while(address < PROMSIZE) {
    int bit, data = 0;

    for(bit=0; bit<ADDRSIZE; bit++) {
      if((address>>bit)&1)
        digitalWrite(ADDRSTARTPIN+bit, HIGH);
      else
        digitalWrite(ADDRSTARTPIN+bit, LOW);
    }
    for(bit=0; bit<DATASIZE; bit++) { 
      data = (data<<1) | verifyRead(DATASTARTPIN+bit);
    }
    Serial.print(address, HEX);
    Serial.print("\t");
    Serial.print(data, DEC);
    Serial.print("\t");
    Serial.print(data, HEX);
    Serial.print("\trev: ");
    Serial.print(bitReverse(data), HEX);    
    Serial.println();
    
    address++;
  }

  // Set all address lines to low just to be on the safe side
  for(bit=0; bit<ADDRSIZE; bit++) {
    digitalWrite(ADDRSTARTPIN+bit, LOW);
  }
  
  end("Dump completed");
}

