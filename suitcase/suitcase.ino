/*********************************************************************
 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

#include <string.h>
#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"
#include "suitcase_constants.h"

#include "BluefruitConfig.h"

#if SOFTWARE_SERIAL_AVAILABLE
  #include <SoftwareSerial.h>
#endif

    #define FACTORYRESET_ENABLE         1
    #define MINIMUM_FIRMWARE_VERSION    "0.6.6"
    #define MODE_LED_BEHAVIOUR          "MODE"
/*=========================================================================*/

/* ...hardware SPI, using SCK/MOSI/MISO hardware SPI pins and then user selected CS/IRQ/RST */
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

/* ...software SPI, using SCK/MOSI/MISO user-defined SPI pins and then user selected CS/IRQ/RST */
//Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_SCK, BLUEFRUIT_SPI_MISO,
//                             BLUEFRUIT_SPI_MOSI, BLUEFRUIT_SPI_CS,
//                             BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);


// A small helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

// function prototypes
uint8_t readPacket(Adafruit_BLE *ble, uint16_t timeout);
float parsefloat(uint8_t *buffer);
void printHex(const uint8_t * data, const uint32_t numBytes);
int button2speed(char button);
void forward(int motor, int throttle);
void backward(int motor, int throttle);
void turnleft(int throttle, int direction);
void turnright(int throttle, int direction); 
void siren(); 

// Global variables
extern uint8_t packetbuffer[];// the packet buffer
int throttle = 0; 
bool hit = 0; 
long duration, dist; 


/**************************************************************************/
/*
   Sets up the HW an the BLE module. Set up motor pinout 
*/
/**************************************************************************/
void setup(void)
{
  while (!Serial);  // required for Flora & Micro
  delay(500);

  Serial.begin(115200);
  Serial.println(F("Adafruit Bluefruit App Controller Example"));
  Serial.println(F("-----------------------------------------"));

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  if ( FACTORYRESET_ENABLE )
  {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ){
      error(F("Couldn't factory reset"));
    }
  }

  /* Disable command echo from Bluefruit */
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();

  Serial.println(F("Please use Adafruit Bluefruit LE app to connect in Controller mode"));
  Serial.println(F("Then activate/use the sensors, color picker, game controller, etc!"));
  Serial.println();

  ble.verbose(false);  // debug info is a little annoying after this point!

  /* Wait for connection */
  while (! ble.isConnected()) {
      delay(500);
  }

  Serial.println(F("******************************"));

  // LED Activity command is only supported from 0.6.6
  if ( ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) )
  {
    // Change Mode LED Activity
    Serial.println(F("Change LED activity to " MODE_LED_BEHAVIOUR));
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
  }

  // Set Bluefruit to DATA mode
  Serial.println( F("Switching to DATA mode!") );
  ble.setMode(BLUEFRUIT_MODE_DATA);

  Serial.println(F("******************************"));

  /* Set up motor pins */
  pinMode(ENA, OUTPUT); 
  pinMode(ENB, OUTPUT); 
  pinMode(IN1, OUTPUT); 
  pinMode(IN2, OUTPUT); 
  pinMode(IN3, OUTPUT); 
  pinMode(IN4, OUTPUT); 

  /* Set up sensor pins */ 
  pinMode(ECHO, INPUT); 
  pinMode(TRIG, OUTPUT); 

  /* Buzzer pins */
  pinMode(BUZZ, OUTPUT);

}

/**************************************************************************/
/*!
    @brief  Constantly poll for new command or response data
*/
/**************************************************************************/
void loop(void)
{
  int dir = 1; 
  /* Wait for new data to arrive */
  uint8_t len = readPacket(&ble, BLE_READPACKET_TIMEOUT);
  if (len == 0) return;

  /* Send and accept packet from the ultrasonic sensor */ 
  digitalWrite(TRIG, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  duration = pulseIn(ECHO, HIGH);

  // convert the time into a distance
  dist = (duration / 2) / 29.1;
  Serial.print(dist); 
  Serial.print("here's dist");
  Serial.println(); 


  // Buttons
  if (packetbuffer[1] == 'B') {
    uint8_t buttnum = packetbuffer[2] - '0';
    boolean pressed = packetbuffer[3] - '0';

    // set car speed 
    throttle = button2speed(buttnum); 
    Serial.print ("Throttle "); Serial.print(throttle);
    Serial.println(); 
    // Warn the user when the car is about to hit something 
    if (dist < 10) {
      siren(); 
      brake(); 
    } else {
      digitalWrite(BUZZ, LOW); 
    }

  // Control motor behavior 
    switch (buttnum) 
    {
      case 5:   // front
        dir = 1;  // set direction forward
        if (pressed) {
              // If there's a barrier in front of it, brake
              if (dist < 10) {
                siren(); 
                brake(); 
              } else {
                forward(1, throttle); 
                forward(0, throttle);    
                digitalWrite(BUZZ, LOW); 
              }

        } else {
          brake(); 
        }
        break; 
      case 6:  // back 
        dir = 0; 
        if (pressed) {
          backward(1, throttle); 
          backward(0, throttle);
        } else 
          brake(); 
       break; 
      case 7:   // left
        if (pressed) {
          turnleft(throttle, dir);
        } else
          brake(); 
        break;
      case 8:  // right 
        if (pressed)
          turnright(throttle, dir); 
        else
          brake(); 
        break; 
    }
    
    Serial.print (" Button "); Serial.print(buttnum);
    if (pressed) {
      Serial.println(" pressed");
    } else {
      Serial.println(" released");
    }
  }

  // GPS Location
  if (packetbuffer[1] == 'L') {
    float lat, lon, alt;
    lat = parsefloat(packetbuffer+2);
    lon = parsefloat(packetbuffer+6);
    alt = parsefloat(packetbuffer+10);
    Serial.print("GPS Location\t");
    Serial.print("Lat: "); Serial.print(lat, 4); // 4 digits of precision!
    Serial.print('\t');
    Serial.print("Lon: "); Serial.print(lon, 4); // 4 digits of precision!
    Serial.print('\t');
    Serial.print(alt, 4); Serial.println(" meters");
  }
}

/* Make specified motor go backward with certain speed */ 
void backward(int motor, int throttle) 
{
  // left motor 
  if (motor) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH); 
    analogWrite(ENA, throttle); 
  } else {    
    digitalWrite(IN3, LOW); 
    digitalWrite(IN4, HIGH); 
    analogWrite(ENB, throttle); 
  }
}

/* Make specified motor go forward with certain speed */ 
void forward(int motor, int throttle) 
{
  // left motor 
  if (motor) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW); 
    analogWrite(ENA, throttle); 
  } else {    
    digitalWrite(IN3, HIGH); 
    digitalWrite(IN4, LOW); 
    analogWrite(ENB, throttle); 
  }
}

/* Turn left with certain speed in specified direction */ 
void turnleft(int throttle, int direction)
{
    driveMotorA(throttle >> 2, direction);
    driveMotorB(throttle, direction); 
}

/* Turn right with certain speed in specified direction */ 
void turnright(int throttle, int direction)
{
    driveMotorA(throttle, direction);
    driveMotorB(throttle >> 2, direction); 
}

/* Steer Motor A in direction with throttle specified */ 
void driveMotorA(int throttle, int direction)
{
  if (direction)
    forward(1, throttle); 
  else 
    backward(1, throttle); 
}

/* Steer Motor B in direction with throttle specified */ 
void driveMotorB(int throttle, int direction)
{
  if (direction)
    forward(0, throttle); 
  else 
    backward(0, throttle); 
}


/* Stop the car */ 
void brake() 
{
  analogWrite(ENA, 0); 
  analogWrite(ENB, 0); 
}

/* Convert button number to 4 levels of PWM duty cycles */ 
int button2speed(uint8_t button)
{
  switch (button)
  {
    case 1:
      throttle = 63; 
      break;
    case 2:
      throttle = 127; 
      break;
    case 3:
      throttle = 191;
      break; 
    case 4:
      throttle = 255; 
      break;
  }
  return (throttle);  
}

/* Play the sound of a siren */ 
void siren()
{
  tone(3, C4, 100); 
  tone(3, A5, 100); 
  tone(3, G4, 100); 
}
