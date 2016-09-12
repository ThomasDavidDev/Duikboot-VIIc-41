/*******************************************************************************************************************
 * GP Duikboot VII c/41 2013-2016
 * by:       * Michiel Caron
 *           * Christophe Dupont
 *           * Joachim Pauwels
 *           * Thomas Devoogdt
 * links:    * http://viic41.blogspot.be/
 *           * https://www.youtube.com/channel/UCqY24WBCvpEZ_ps5mDEgGUQ
 *           * https://github.com/ThomasDavidDev/Duikboot-VIIc-41
 *******************************************************************************************************************/
/*ESC Calibratie*/
/* 
 * Throtle omhoog blijven houden.
 * Ontvanger aanleggen.
 * Wachten tot 3x tuut.
 * Throtle beneden, boven, beneden,...
 */

/*
 *                                              +-----+
 *                 +----[PWR]-------------------| USB |--+
 *                 |                            +-----+  |
 *                 |         GND/RST2  [ ][ ]            |
 *                 |       MOSI2/SCK2  [ ][ ]  A5/SCL[ ] |   
 *                 |          5V/MISO2 [ ][ ]  A4/SDA[ ] |   
 *                 |                             AREF[ ] |
 *                 |                              GND[ ] |
 *                 | [ ]N/C                    SCK/13[ ] |   Duik V (Direction Angle vf)
 *                 | [ ]IOREF                 MISO/12[ ] |   Duik A (Direction Angle vb)
 *                 | [ ]RST                   MOSI/11[ ]~|   Propellers
 *                 | [ ]3V3    +---+               10[ ]~|   Buiten Tank B
 *                 | [ ]5v    -| A |-               9[ ]~|   Buiten Tank A
 *                 | [ ]GND   -| R |-               8[ ] |   Binnen Tank B
 *                 | [ ]GND   -| D |-                    |
 *                 | [ ]Vin   -| U |-               7[ ] |   Binnen Tank A
 *                 |          -| I |-               6[ ]~|   LED B
 *          Druk   | [ ]A0    -| N |-               5[ ]~|   LED R
 *       Voltage   | [ ]A1    -| O |-               4[ ] |   Water Sensor
 * Water Koeling   | [ ]A2     +---+           INT1/3[ ]~|   STUUR (Direction Angle)
 *       LED OUT   | [ ]A3                     INT0/2[ ] |   PPM
 *           XYZ   | [ ]A4/SDA  RST SCK MISO     TX>1[ ] |   USB
 *           XYZ   | [ ]A5/SCL  [ ] [ ] [ ]      RX<0[ ] |   USB
 *                 |            [ ] [ ] [ ]              |
 *                 |  UNO_R3    GND MOSI 5V  ____________/
 *                  \_______________________/
 */

/*INCLUDE*/
#include <ADXL345.h>      //Library by Adafruit:           https://github.com/adafruit/Adafruit_ADXL345
#include <PPMdecode.h>    //Library by Thomas Devoogdt:    https://github.com/ThomasDavidDev/PPMdecode
#include <ColorLed.h>     //Library by Thomas Devoogdt:    
#include <Wire.h>         //Library by Nicholas Zambetti:  https://github.com/arduino/Arduino/tree/master/hardware/arduino/avr/libraries/Wire
#include <Servo.h>        //Library by Michael Margolis:   https://github.com/arduino/Arduino/tree/master/libraries/Servo
#include <Event.h>        //Library by Simon Monk:         http://www.doctormonk.com/2012/01/arduino-timer-library.html
#include <Timer.h>        //Library by Simon Monk:         http://www.doctormonk.com/2012/01/arduino-timer-library.html
#include <PID_v1.h>       //Library by Brett Beauregard:   https://github.com/br3ttb/Arduino-PID-Library/

/*Define's*/
//#define led 13

//#define gyroServoOffset 30  //Max hoek die door servo's worden gecorrigeerd.
#define speedOffset 20       //Zone waarbinnen speedStop actief is. (voor joystick)
#define servoOffset 7       //Zone waarbinnen servoOffset actief is. (voor joystick)

#define speedMin  15      //ESC max snelheid links.
#define speedStop 90      //ESC geen beweging.
#define speedMax  165     //ESC max snelheid rechts.

#define directAngleStart    90  //Servo's neutrale positie
#define directVBAngleStart  90
#define directVFAngleStart  90

//Calibration 2016/09/08
#define directAngleMin  63   //Servo Mapping Direction Angle [abs: 47 - 132]
#define directAngleMax  117
#define directAngleVAngleFrontMin  0 //Servo Mapping Vertical Direction Angle [abs: 0 - 75]
#define directAngleVAngleFrontMax  75
#define directAngleVAngleBackMin  27 //Servo Mapping Vertical Direction Angle [abs: 27 - 115]
#define directAngleVAngleBackMax  115

#define maxDiepteBuitenTank   25      //Max diepte waarbij pompen nog nut heeft.
//#define snorkel             20      //Diepte Snorkel
#define delayTankOutDown      38500   //Pomptijd omlaag
#define delayTankOutUp        38500   //Pomptijd omhoog
#define delayTankIns          10000    //Pomptijd binnentank
#define maxDiepte             150     //cm
//#define minDuikSnelheid     -1      //1cm / 5s
//#define criDuikSnelheid     -10     //10cm / 5s Kritieke duiksnelheid

//Vervang de getallen in PPM om de afstandsbediening in te stellen.
#define PPMdirectionHorizontal  PPM[1]  //Right Stick - Left - Right
#define PPMdirectionVertical    PPM[0]  //Right Stick - Up - Down
#define PPMspeed                PPM[2]  //Left Stick - Up - Down
#define PPMDuikenAskReal        PPM[3]  //...
#define PPMDuikenSchuifAskReal  PPM[5]  //...

#define ADXL345Calibration  20    //Calibration Gyro - Axel

#define VoltageMin          940
#define VoltageHysterese    950

/*Timer.h*/
Timer t;

/*PPM_INIT*/
#define inter 0       //Let op! int.0 staat verbonden op !pin 2! zie: http://arduino.cc/en/Reference/attachInterrupt .
#define channels 6    //In te stellen op het aantal in te lezen kanalen.
PPMdecode myPPMdecode = PPMdecode(inter, channels); //Aanmaken van een nieuw object.
short defaultValue[channels] = { 
  50, 50, 50, 50, 50, 0 };
int PPM[channels] = { 
  50, 50, 50, 50, 50, 0 };

/*PID*/
double PIDSetpoint, PIDInput, PIDOutput;
PID myPID(&PIDInput, &PIDOutput, &PIDSetpoint,2,5,1, DIRECT);


/*ADXL345_INIT (Hoek)*/
ADXL345 adxl = ADXL345();     //Aanmaken van een nieuw object.
int x, y, z;      //Sensor Val.
int xR, yR, zR;   //Real
int xO, yO, zO;   //Offset

/*DRUK_INIT*/
int diepteReal;
int drukCal;
#define drukPin 0
#define diepte (uint16_t)(abs((analogRead(drukPin)-drukCal)*2.5))
/* Berekening: vb: p = 200kPa, p0 = 101.325kPa, Vs = 5V,  ρ = 1027kg/m³
 VOUT = Vs* (0.004 x P-0.04) ± Error = 5 * (0.004 x 200.000-0.04) + 0 = 3.8V  (Zie DataSheet!)
 VOUT = Vs* (0.004 x P-0.04) ± Error = 5 * (0.004 x 101.325-0.04) + 0 = 1.8265V
 Devisions = (2^10 - 1) x ΔV / Vs = (2^10 - 1) x (3.8 - 1.8265) / 5 = 403.7772816div
 p=p0 + ρgh zodat h = (p - p0) / (ρ x g) = (200000 - 101325) / (9.81 * 1027) = 9.79m = 979.42cm
 C = 979.42/403.78 = 2.425636975 ~ 2.5
 */

/*Voltage_Check*/
int voltageReal;
#define voltagePin A1
boolean voltageAlarm = LOW;

/*Water_Check*/
#define waterCheckPin 4
boolean waterCheckAlarm = LOW;

/*Water_Koeling*/
#define waterKoelingPin A2
boolean waterKoeling = LOW;

/*RGBLed*/
rgbLed myLed = rgbLed(5, 5, 6);
#define outLedPin A3
//RED   = PIN 12
//GREEN = PIN 12
//BLUE  = PIN 13

/*Variabelen*/
int speedSetVal       = speedStop;            //Motoren
int speedSetValReal   = speedStop;
int directAngle       = directAngleStart;     //Servo richting links rechts.
int directAngleReal   = directAngleStart;
int directVBAngle     = directVBAngleStart;   //Servo richting achteraan, Omlaag - Omhoog.
int directVBAngleReal = directVBAngleStart;
int directVFAngle     = directVFAngleStart;   //Servo richting vooraan, Omlaag - Omhoog.
int directVFAngleReal = directVFAngleStart;

int PPMDuikenAsk;
int PPMDuikenSchuifAsk;

//int calAngle; //Draaihoek om de stabiliteit bij te sturen. (gyro afhankelijk)

int duikReal;               //Waarde van schuif pot. meter.
//int diepteAsk;              //Gevraagde diepte.
//int diepteDiff;             //Verschil met gevraagde en werkelijke diepte.
//int diepteLast;             //Vorige diepte.
//int duikDiff;               //Hoogte verschil per tijdseenheid.

boolean firstFlag = false;    //Duikprocedure starten.
boolean secondFlag = false;   //Snorkeldiepte bereikt met buitentank.
boolean tankInsFlag = false;  //Binnen Tank Pomp Verg.

enum HBrug { 
  off, left, right };
HBrug outTank = off;
HBrug inTank = off;

boolean blinkLed = false;     //Led pin 13 blink.

/*Servo*/
Servo direct;           //Richting Links Rechts
Servo directVB;         //Richting Achteraan, Omlaag - Omhoog
Servo directVF;         //Richting Vooran, Omlaag - Omhoog
Servo speedSet;         //ESC's Motoren

/*SETUP*/
void setup() {
  Serial.begin(112500);            //INIT_Serial.
  Serial.println("Welcome!");
  Serial.println("");

  adxl.autoPreset(1);              //INIT_ADXL345.
  xO = 0;  
  yO = 0;  
  zO = 0;
  delay(500);
  for (int i = 0; i < ADXL345Calibration; i++) { //Begrens alle waarden tussen 0 & 100
    adxl.readAccel(&x, &y, &z);
    xO += x;
    yO += y;
    zO += z;
    delay(50);
  }
  xO /= -ADXL345Calibration;
  yO /= -ADXL345Calibration;
  zO /= -ADXL345Calibration;

  drukCal = analogRead(drukPin);   //Pressure Calibration. (eventueel vast zetten)

  myPPMdecode.SetDefaultValues(defaultValue);

  //tell the PID to range between -150 to 150
  myPID.SetOutputLimits(-150, 150);

  //turn the PID on
  myPID.SetMode(AUTOMATIC);

  pinMode(7, OUTPUT);     //inTankValA
  pinMode(8, OUTPUT);     //inTankValB
  pinMode(9, OUTPUT);     //outTankValA
  pinMode(10, OUTPUT);    //outTankValB

  pinMode(waterKoelingPin, OUTPUT);
  pinMode(outLedPin, OUTPUT);

  pinMode(waterCheckPin, INPUT);

  //pinMode(12, OUTPUT);    //RED
  //pinMode(13, OUTPUT);    //BLUE

  myLed.transitionTime = 1000;

  direct.attach(3);                           //Richting Links Rechts
  directVB.attach(12);                         //Richting Achteraan, Omlaag - Omhoog
  directVF.attach(13);                         //Richting Vooraan, Omlaag - Omhoog
  speedSet.attach(11, 1000, 2000);            //ESC's Motoren

  delay(500);

  t.every(50, timer50);     //50ms timer
  t.every(1500, timer1500);   //1500ms timer
  t.every(1000, timer1000); //1000ms timer
}

/*******************************************************************************************************************
 * loop
 *******************************************************************************************************************/

void loop() {
  ppmRemap();           //Schaalt alle waardes tussen 0 en 100%.

  servoCalc();          //Berekent de hoek van de servo's en stuurt ze aan.

  escCalc();            //Berekent de snelheid van de motoren.

  tankCalc();           //Berekent de snelheid van de pompen en regelt hun werking.

  waterCheck();         //Controleert of er water in de boot is gekomen.

  voltageCheck();       //Controleert de spanning van de batterijen.

  rgbLed::updateLeds(); //Kleuren Led update.

  t.update();           //Timer update
}

/*******************************************************************************************************************
 * subroutines
 *******************************************************************************************************************/

void ppmRemap() {
  PPM[0] = map(myPPMdecode.channel[0], 4, 84, 0, 100); //Alles tussen 0 en 100 mappen
  PPM[1] = map(myPPMdecode.channel[1], 6, 87, 0, 100);
  PPM[2] = map(myPPMdecode.channel[2], 4, 84, 0, 100);
  PPM[3] = map(myPPMdecode.channel[3], 8, 89, 0, 100);
  PPM[4] = map(myPPMdecode.channel[4], 6, 87, 0, 100);
  PPM[5] = map(myPPMdecode.channel[5], 8, 87, 0, 100);

  for (int i; i < channels; i++) { //Begrens alle waarden tussen 0 & 100
    if (PPM[i] < 0) PPM[i] = 0;
    if (PPM[i] > 100) PPM[i] = 100;
  }

  if (myPPMdecode.error) {
    PPM[0] = 50;
    PPM[1] = 50;
    PPM[2] = 50;
    PPM[3] = 50;
    PPM[4] = 50;
    PPM[5] = 0;
  }
}

void servoCalc() {
  int directV = PPMdirectionVertical; //Channel 0, val: 0 to 100

  int centralFront = (directAngleVAngleFrontMin + directAngleVAngleFrontMax) / 2;
  int centralBack = (directAngleVAngleBackMin + directAngleVAngleBackMax) / 2;
  if (directV - 50 > servoOffset) {
    directVFAngle = map(directV, 50 + servoOffset, 100, centralFront, directAngleVAngleFrontMax);
    directVBAngle = map(directV, 0, 50 + servoOffset, directAngleVAngleBackMin, centralBack);
  }
  else {
    directVFAngle = map(directV, 0, 50 - servoOffset, directAngleVAngleFrontMin, centralFront);
    directVBAngle = map(directV, 50 - servoOffset, 100, centralBack, directAngleVAngleBackMax);
  }
  directAngle = map(PPMdirectionHorizontal, 0, 100, directAngleMax, directAngleMin);
}

void escCalc() {
  int speedSet = PPMspeed; //Channel 0, val: 0 to 100
  if (abs(speedSet - 50) < speedOffset){
    speedSetVal = speedStop; // map: 35 to 65 --> 90
    waterKoeling = LOW;
  }
  else if (speedSet - 50 > speedOffset)
  {
    speedSetVal = map(speedSet, 50 + speedOffset, 100, speedStop, speedMax); // map: 65 to 100 --> 90 to 165
    waterKoeling = HIGH;
  }
  else
  {
    speedSetVal = map(speedSet, 0, 50 - speedOffset, speedMin, speedStop); // map: 0 to 35 --> 15 to 90
    waterKoeling = HIGH;
  }
}

void tankCalc() {
  /*
   * boolean firstFlag = false;    //Duikprocedure starten.
   * boolean secondFlag = false;   //Snorkeldiepte bereikt met buitentank.
   *
   * int diepteReal;               //Diepte volgens druk pin.
   * int duikReal;                 //Waarde van schuif pot. meter.
   */
  //  if (abs(PPMDuikenAsk - 50) < 25)
  //  {
  //   // Stil
  //     tankInsFlag = false;
  //     inTank = off; 
  //  }
  //  else if (tankInsFlag)
  //  {
  //     inTank = off; 
  //  }
  //  else if ((PPMDuikenAsk - 50) > 24)  
  //  {// Beneden
  //      if (diepteReal < maxDiepteBuitenTank && (outTank == off && !secondFlag) || (outTank == right && secondFlag)) //Buiten Tank
  //      {
  //        outTank = left; //Inpompen
  //        t.after(delayTankOutDown, delayCallTankOutDown);
  //      }
  //      else if (outTank == off && secondFlag) //Binnen Tank
  //      {
  //        inTank = left;
  //        t.after(delayTankIns, delayCallTankInMax); 
  //      }
  //  }
  //  else
  //  {// Boven
  //     if (diepteReal < maxDiepteBuitenTank && (outTank == off && secondFlag) || (outTank == left  && !secondFlag)) //Buiten Tank
  //     {
  //        outTank = right; //Uitpompen
  //        t.after(delayTankOutDown, delayCallTankOutUp);
  //      }
  //      else if (outTank == off && secondFlag) //Binnen Tank
  //      {
  //        inTank = right;
  //        t.after(delayTankIns, delayCallTankInMax); 
  //      }
  //  }


  //Buiten Tank
  if (PPMDuikenSchuifAsk > 20 && !firstFlag) firstFlag = true;
  if (PPMDuikenSchuifAsk < 5 && firstFlag) firstFlag = false;

  if (diepteReal < maxDiepteBuitenTank) {
    if (firstFlag && ((outTank == off && !secondFlag) || (outTank == right && secondFlag)) ) {
      outTank = left; //Inpompen
      t.after(delayTankOutDown, delayCallTankOutDown);
    }
    if (!firstFlag && ((outTank == off && secondFlag) || (outTank == left  && !secondFlag)) ) {
      outTank = right;  //Uitpompen
      t.after(delayTankOutUp, delayCallTankOutUp);
    }
  }

  //Binnen Tank
  PIDInput = diepteReal;
  if (secondFlag) 
  {
    PIDSetpoint = map(PPMDuikenSchuifAsk, 20, 100, maxDiepteBuitenTank, maxDiepte);
  }
  else 
  {
    PIDSetpoint = diepteReal;
  }

  myPID.Compute();
  if (outTank == off && firstFlag && secondFlag) 
  {
    if (PIDOutput < -100 )
    {
      inTank = right;
    }
    else if (PIDOutput > 100 )
    {
      inTank = left;
    }
  }
  else 
  {
    inTank = off; 
  }

  //Binnen Tank H-Bridge
  //Later: als secondFlag && firstFlag, PID op insTank met Error = diepteReal - map(duikreal, 10, 100, maxDiepteBuitenTank, max diepte)
  //En de hysteresis van 10 naar 20
  //  int insTank = PPMbinnenTank; //Channel 3, val: 0 to 100
  //  if (abs(insTank - 50) < 25)
  //  {
  //    tankInsFlag = false; 
  //    inTank = off; 
  //  }
  //  else if (tankInsFlag)
  //  {
  //    inTank = off; 
  //  }
  //  else if ((insTank - 50) > 24)
  //  {
  //    inTank = left;
  //    t.after(delayTankIns, delayCallTankInMax);   
  //    
  //  }
  //  else
  //  {
  //    inTank = right;
  //    t.after(delayTankIns, delayCallTankInMax);
  //  }
}

void delayCallTankOutDown() {
  if (outTank != right) secondFlag = true;
  outTank = off;
}

void delayCallTankOutUp() {
  if (outTank != left) secondFlag = false;
  outTank = off;
}

void delayCallTankInMax() {
  tankInsFlag = true;
}

void voltageCheck() {
  if (fading(analogRead(voltagePin), &voltageReal, 1) < VoltageMin && !voltageAlarm) {
    voltageAlarm = true;
  }
  if (fading(analogRead(voltagePin), &voltageReal, 1) > VoltageHysterese && voltageAlarm) {
    voltageAlarm = false;
  }
}

void waterCheck() {
  waterCheckAlarm = !digitalRead(waterCheckPin);
}

int fading(int input, int *real, int val) {   //Berekent de fading.
  if (*real < input && *real + val < input) *real += val;
  else if (*real > input && *real - val > input) *real -= val;
  else *real = input;
  return *real;
}

void timer50() {  //50ms timer
  //hoek uitlezen + pieken weg werken
  adxl.readAccel(&x, &y, &z);
  fading(x + xO, &xR, 1);
  fading(y + yO, &yR, 1);
  fading(z + zO, &zR, 1);

  //fading ESC
  speedSet.write(fading(speedSetVal, &speedSetValReal, 10));
  direct.write(fading(directAngle, &directAngleReal, 15));
  directVB.write(fading(directVBAngle, &directVBAngleReal, 15));
  directVF.write(fading(directVFAngle, &directVFAngleReal, 15));

  fading(diepte, &diepteReal, 3);

  fading(PPMDuikenSchuifAskReal, &PPMDuikenSchuifAsk, 3);
  fading(PPMDuikenAskReal, &PPMDuikenAsk, 3); //Input Channel Fading

  //Inside Tank and Outside Tank
  //digitalWrite(7, inTankValA); //inTankValA
  //digitalWrite(8, inTankValB); //inTankValB
  //digitalWrite(9, outTankValA); //outTankValA
  //digitalWrite(10, outTankValB); //outTankValB
  if (outTank == off)
  {
    digitalWrite(9, HIGH); //outTankValA
    digitalWrite(10, HIGH); //outTankValB
  }
  else if (outTank == left)
  {
    digitalWrite(9, HIGH); //outTankValA
    digitalWrite(10, LOW); //outTankValB
  }
  else if (outTank == right)
  {
    digitalWrite(9, LOW); //outTankValA
    digitalWrite(10, HIGH); //outTankValB
  }

  if (inTank == off)
  {
    digitalWrite(7, HIGH); //inTankValA
    digitalWrite(8, HIGH); //inTankValB
  }
  else if (inTank == left)
  {
    digitalWrite(7, HIGH); //inTankValA
    digitalWrite(8, LOW); //inTankValB
  }
  else if (inTank == right)
  {
    digitalWrite(7, LOW); //inTankValA
    digitalWrite(8, HIGH); //inTankValB
  }

  digitalWrite(waterKoelingPin, waterKoeling);

  printValues(); //Log
}

void timer1000() {   //1000ms timer
  //blinkLed = !blinkLed;
  //digitalWrite(led, blinkLed);
}

void timer1500() {   //1500ms timer
  if (voltageAlarm || waterCheckAlarm) { //RED Blink
    blinkLed = !blinkLed;
    if (blinkLed) myLed.changeColor("#0000FF");
    else myLed.changeColor("#FFFFFF");

    digitalWrite(outLedPin, blinkLed);
  }
  else {    //Fade from blue to red
    short red = map(voltageReal, VoltageMin, 1023, 0, 255);
    short green = map(voltageReal, VoltageMin, 1023, 0, 255);
    short blue = map(voltageReal, VoltageMin, 1023, 255, 0);
    myLed.changeColor(red, green, blue);

    digitalWrite(outLedPin, LOW);
  }
  //voltageReal
}



