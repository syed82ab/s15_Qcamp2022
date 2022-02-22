/*
  Arduino program for debugging purpose and to transmit/receive
  messages between two parties (Mission 1). 
  Implemented features:
  1. Send Blinking feature
  2. Recv Blinking feature
  3. Sending a short message (4 bytes)
  4. Recving a short message (4 bytes)
 Author: Qcumber (2018) 
*/

#include <IRremote.hpp>
// #include "PinDefinitionsAndMore.h"

#define SEND_PWM_BY_TIMER
// Parameters
const int serial_timeout = 100; // 100 ms 
const int send_pin = 10;  // The library sets it to 3 (fixed).
const int recv_pin = 11; // Pin 11
unsigned int carrier_freq = 38; // 38 kHz
const int irrecv_timeout = 100; // 100 ms 

// More useless parameters
int blink_time = 1000;     // 200 ms
int blink_num = 20;       // 20 blinks
int blink_obtime = 10000; // 10 seconds
int blink_rstime = 200;   // 200 ms timeout 

uint32_t reverseBits( uint32_t val )
{
  uint32_t ret = 0;
  for (uint8_t i = 0; i < 32; i++)
  {
    ret = (ret << 1) | ((val >> i) & 1);
  }
  return ret;
}

void setup() {
  // initialize the serial port:
  Serial.begin(38400);
  Serial.setTimeout(serial_timeout);
  IrSender.begin(send_pin, DISABLE_LED_FEEDBACK);
  IrReceiver.begin(recv_pin);
}

void loop() {
  // Select the best response
  while (!Serial.available()); // Listen to serial input
  char serbuf[16] = ""; // Reinitialise buffer (16 bytes, char array size 16)
  Serial.readBytesUntil(' ', serbuf, 15); // Until whitespace
  // Obtain which input command (enumerated)
  int enumc = -1; // default choice
  int maxChoice = 5;
  char sercmd[maxChoice][8] = {"HELP", "SBLINK", "RBLINK", "SEND", "RECV"};
  for (int c=0; c<maxChoice; c++){
    if (strcasecmp(sercmd[c],serbuf) == 0){ 
      enumc = c;// Obtain match
    }
  }
  
  // Declaring some other parameters
  unsigned long timeNow;
  unsigned long timeEnd;
  int readout;
  char strbuf[4] = "";
  unsigned long value;
  
  // Switching between different cases
  switch(enumc){
    case 0: //HELP
      Serial.print("Classical Communication Implementation (Mission 1)\n");
      Serial.print("HELP       Print help statement\n");
      Serial.print("SBLINK     Send blinking feature\n");
      Serial.print("RBLINK     Recv blinking feature\n");
      Serial.print("SEND X     Send a short message X (4 bytes)\n");
      Serial.print("RECV       Receive a short message (4 bytes)\n");
      break; 
    case 1: //SBLINK
      // Serial.print("Perform some blinking features \n");
      for (int i=0; i<blink_num; i++){
        IrSender.enableIROut(carrier_freq);
        // bright
        analogWrite(send_pin, 127);
        delay(blink_time);
        // dark
        analogWrite(send_pin, 0);
        delay(blink_time);    
      }
      Serial.println("Task done.");
      break;
    case 2: //RBLINK
      // Serial.print("Trying to recv blinking features \n");
      // listen for blink during blink_obtime
      timeNow = millis(); // get time now
      timeEnd = timeNow + blink_obtime;
      while (timeNow < timeEnd){
        readout = digitalRead(recv_pin);
        if (!readout){    
          // Detected an IR light blinking
          Serial.println("BLINK!");
          delay(blink_rstime);
        } 
        timeNow = millis();
      }
      Serial.println("Task done.");
      break;
    case 3: //SEND X
      // Serial.print("Trying to send a word \n");
      while(!Serial.available()); // Wait for X
      Serial.readBytes(strbuf, 4); // Read 4 characters each time
      value = (unsigned long) strbuf[0] << 24 
              | (unsigned long) strbuf[1] << 16
              | (unsigned long) strbuf[2] << 8
              | (unsigned long) strbuf[3];
      Serial.print(value, HEX);  // Debug                   
      IrSender.sendNECMSB(reverseBits(value), 32);   // Send the characters
      break;
    case 4: //RECV
      //Serial.print("Trying to recv a word \n");
      while (true){
        // Decode result 
        if (IrReceiver.decode()) {
          Serial.print(IrReceiver.decodedIRData.decodedRawData, HEX); // Print HEX characters
          IrReceiver.resume();
          break;
        }
        // Cancel operation (if escape char received)
        if (Serial.available()) {
          // Read the incoming byte:
          readout = Serial.read();
          // Escape character is # (int value 35)
          if (readout == 35){ 
            Serial.println("Listening interrupted!");
            break;
          }
        }
        delay(irrecv_timeout);
      }
      break;
    default:
      Serial.print("Unknown command\n");
      break;
  }
}
