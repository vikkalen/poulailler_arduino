#include "TimerOne.h"

long temps = 0;         //Durée de rotation pour un tour

byte motorHalfstep = B00000001;
byte motorFullstep = B00000011;
boolean motorCW = true;
boolean motorIsFullStep = false;
unsigned int motorNbTours;
const unsigned int motorUnTour = 4096;
unsigned int motorStep;
unsigned int motorTour;
boolean motorRunning = false;
unsigned int motorPeriod;
unsigned int motorInitialPeriod = 2000;
unsigned int motorFinalPeriod = 1000;
unsigned int motorAccel = 1;

char buffer[32];
int cnt = 0;
boolean ready = false;

byte motorShiftPhase (byte n)
{
  if(motorCW) return ((n<<1) | (n>>3)) & B00001111;
  else return ((n>>1) | (n<<3)) & B00001111;
}

void motorTurn()
{   
  if(motorTour >= motorNbTours)
  {
    PORTB = PORTB & B11110000;
    Timer1.stop();
    motorRunning = false;
    temps =  millis() - temps ; //Chronomètre un rour complet  6.236 sec par tour à vitesse 200
    Serial.println(temps);      //Affiche le temps (en ms) pour un tour complet
    return;
  }

  PORTB = PORTB & B11110000;
  if(motorIsFullStep)
  {
    PORTB = PORTB | motorFullstep;
    motorFullstep = motorShiftPhase(motorFullstep);
    motorIsFullStep = false;
  }
  else
  {
    PORTB = PORTB | motorHalfstep;
    motorHalfstep = motorShiftPhase(motorHalfstep);
    motorIsFullStep = true;
  }

//  delay(20000);
  motorStep++;
  if(motorStep == motorUnTour)
  {
    motorTour++;
    motorStep = 0;
  }
  if(motorPeriod > motorFinalPeriod)
  {
    motorPeriod -= motorAccel;
    Timer1.setPeriod(motorPeriod);
  }
}

void motorStart()
{
  motorIsFullStep = !motorCW;
  motorHalfstep = B00000001;
  motorFullstep = B00000011;
  motorStep = 0;
  motorTour = 0;
  motorPeriod = motorInitialPeriod;
  motorRunning = true;
  Serial.println("Moteur en marche ");
  temps = millis();
  Timer1.setPeriod(motorInitialPeriod);
  Timer1.start();
}

void setup()
{
  Serial.begin(9600);     // 9600 bps
  Serial.println("Test de moteur pas a pas");

  Timer1.initialize(motorInitialPeriod);
  Timer1.attachInterrupt(motorTurn);
  Timer1.stop();

  DDRB = DDRB | B00001111;
}

void loop()
{
  Serial.flush();
  ready = false;
  int tours;
  while (Serial.available())
  {
    char c = Serial.read();
    buffer[cnt++] = c;
    if ((c == '\n') || (cnt == sizeof(buffer) - 1))
    {
      buffer[cnt] = '\0';
      cnt = 0;
      String s = buffer;
      tours = s.toInt();
      ready = true;
      break;
    }
  }
  
  if(ready && !motorRunning)
  {
    delay(2000);

    motorCW = tours > 0;
    motorNbTours = tours > 0 ? tours : -tours;
    motorStart();
  }
}
