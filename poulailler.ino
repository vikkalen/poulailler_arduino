  dssc#include "Porte.h"
#include "BH1750.h"
#include "Sleepy.h"
#include <RFM12B.h> // https://github.com/LowPowerLab/RFM12B
#include <EEPROM.h>

#define NODEID        31  //network ID used for this unit
#define NETWORKID    99  //the network ID we are on
#define GATEWAYID     1  //the node ID we're sending to
#define ACK_TIME     500  // # of ms to wait for an ack

typedef struct {
  int flag = 0;
} SendAckStruct;
typedef struct {
  int flag = 1;
  int ouvert;
  int ferme;
  int lux;
  int supplyV;  // Supply voltage
} SendStateStruct;
typedef struct {
  int flag = 2;
  int luxFermeture;
  int luxOuverture;
  int tours;
  int retry;
  int sleep;
} SendConfStruct;

SendAckStruct sendAckPayload;
SendStateStruct sendStatePayload;
SendConfStruct sendConfPayload;
SendConfStruct receiveConfPayload;

RFM12B radio;

Porte porte;
BH1750 lightMeter;

volatile boolean ouverture = false;
volatile boolean fermeture = false;
volatile boolean ouvertureForcee = false;
volatile boolean fermetureForcee = false;
volatile int ouvertureTentative = 0;
volatile int fermetureTentative = 0;

uint16_t luxFermeture = 0;
uint16_t luxOuverture = 5;
int tours = 1;
int retry = 1;
int sleep = 1;

uint16_t lux;
int ouvert = 0;
int ferme = 0;
int supplyV = 0;


//--------------------------------------------------------------------------------------------------
// Read current supply voltage
//--------------------------------------------------------------------------------------------------
long readVcc() {
   bitClear(PRR, PRADC); ADCSRA |= bit(ADEN); // Enable the ADC
   long result;
   // Read 1.1V reference against Vcc
   #if defined(__AVR_ATtiny84__) 
    ADMUX = _BV(MUX5) | _BV(MUX0); // For ATtiny84
   #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);  // For ATmega328
   #endif 
   delay(2); // Wait for Vref to settle
   ADCSRA |= _BV(ADSC); // Convert
   while (bit_is_set(ADCSRA,ADSC));
   result = ADCL;
   result |= ADCH<<8;
   result = 1126400L / result; // Back-calculate Vcc in mV
   ADCSRA &= ~ bit(ADEN); bitSet(PRR, PRADC); // Disable the ADC to save power
   return result;
} 
//########################################################################################################################

void porteTourner()
{
  porte.tourner();
}

void ouvrir() {
  if(!fermetureForcee) {
    if(ouvertureTentative < retry) {
      ouverture = true;
      ouvertureTentative++;    
    }
    ouvertureForcee = false; 
    fermetureForcee = false;
    fermetureTentative = 0;
  }
}

void fermer() {
  if(!ouvertureForcee) {
    if(fermetureTentative < retry) {
      fermeture = true;
      fermetureTentative++;    
    }
    ouvertureForcee = false; 
    fermetureForcee = false;
    ouvertureTentative = 0;
  }
}

void forceOuvrir() {
  ouverture = true;
  ouvertureForcee = true;
  ouvertureTentative = 0;
  fermeture = false;
  fermetureForcee = false;
  fermetureTentative = 0;
}

void forceFermer() {
  ouverture = false;
  ouvertureForcee = false;
  ouvertureTentative = 0;
  fermeture = true;
  fermetureForcee = true;
  fermetureTentative = 0;
}

void boutonPorte() {
  detachInterrupt(1);

  boolean boutonOuvrir = digitalRead(8);
  boolean boutonFermer = digitalRead(9);
  if(boutonOuvrir == LOW) forceOuvrir();
  if(boutonFermer == LOW) forceFermer();
  porte.arreter();
  
  attachInterrupt(1, boutonPorte, LOW);
}

void configure() {
  luxOuverture = receiveConfPayload.luxOuverture;
  luxFermeture = receiveConfPayload.luxFermeture;
  tours = receiveConfPayload.tours;
  retry = receiveConfPayload.retry;
  sleep = receiveConfPayload.sleep;
  writeConf();
  porte.setTours(tours);
}

void readConf() {
  int eeAddress = 0;
  EEPROM.get(eeAddress, luxOuverture);
  eeAddress += 2;
  EEPROM.get(eeAddress, luxFermeture);
  eeAddress += 2;
  EEPROM.get(eeAddress, tours);
  eeAddress += 2;
  EEPROM.get(eeAddress, retry);
  eeAddress += 2;
  EEPROM.get(eeAddress, sleep);
}

void writeConf() {
  int eeAddress = 0;
  EEPROM.put(eeAddress, luxOuverture);
  eeAddress += 2;
  EEPROM.put(eeAddress, luxFermeture);
  eeAddress += 2;
  EEPROM.put(eeAddress, tours);
  eeAddress += 2;
  EEPROM.put(eeAddress, retry);
  eeAddress += 2;
  EEPROM.put(eeAddress, sleep);
}

void updateState() {
  lightMeter.begin(BH1750_ONE_TIME_HIGH_RES_MODE);
  delay(180);
  lux = lightMeter.readLightLevel();

  porte.activerCapteurs();
  delay(20);
    
  if(porte.ouverte()) ouvert = 1;
  else ouvert = 0;

  if(porte.fermee()) ferme = 1;
  else ferme = 0;

  porte.desactiverCapteurs();

  supplyV = readVcc(); // Get supply voltage
}

void sendAck() {
    radio.Send(GATEWAYID, &sendAckPayload, sizeof sendAckPayload, false);
}

void sendState() {
    
    sendStatePayload.ouvert = ouvert;
    sendStatePayload.ferme = ferme;
    sendStatePayload.lux = lux;
    sendStatePayload.supplyV = supplyV;

    radio.Send(GATEWAYID, &sendStatePayload, sizeof sendStatePayload, false);
}

void sendConf() {
    sendConfPayload.luxOuverture = luxOuverture;
    sendConfPayload.luxFermeture = luxFermeture;
    sendConfPayload.tours = tours;
    sendConfPayload.retry = retry;
    sendConfPayload.sleep = sleep;
    
    radio.Send(GATEWAYID, &sendConfPayload, sizeof sendConfPayload, false);
}

boolean receive() {
  long now = millis();
  int flag = 0;
  while (millis() - now <= ACK_TIME)
    if (radio.ReceiveComplete() && radio.CRCPass() && RF12_SOURCEID == GATEWAYID) {
      flag = *(int*) (radio.Data);
      break;
    }

  if(flag == 0) // fin de communication
    return false;
  else if(flag == 1)
    sendState();
  else if(flag == 2)
    sendConf();
  else if(flag == 3)  {
    receiveConfPayload = *(SendConfStruct*) radio.Data;
    configure();
    sendConf();
  }
  else if(flag == 4) {
    forceOuvrir();
    sendAck();
  }
  else if(flag == 5) {
    forceFermer();
    sendAck();
  }

  return true;
}

ISR(WDT_vect) { Sleepy::watchdogEvent(); }

void setup() {

  ADCSRA &= ~ bit(ADEN); bitSet(PRR, PRADC); // Disable the ADC to save power

  radio.Initialize(NODEID, RF12_433MHZ, NETWORKID, 0, 20);
  radio.Sleep(); //sleep right away to save power

  //lightMeter.begin(BH1750_ONE_TIME_HIGH_RES_MODE);

  Timer1.initialize(PORTE_PERIOD_INIT);
  Timer1.attachInterrupt(porteTourner);
  Timer1.stop();

  // force fermeture / ouverture
  pinMode(8, INPUT);
  pinMode(9, INPUT);
  digitalWrite(8, HIGH);
  digitalWrite(9, HIGH);

  digitalWrite(3, HIGH);
  attachInterrupt(1, boutonPorte, LOW);

  readConf();

  porte.setTours(tours);
}

void loop() {

  if(!porte.tourne()) {
    updateState();

    radio.Wakeup();
    sendState();
    while(receive());
    radio.Sleep();

    if(ouvert == 1) ouvertureTentative = 0;
    if(ferme == 1) fermetureTentative = 0;

    if(lux < luxFermeture) fermer();
    else if(lux > luxOuverture) ouvrir();

    if(ouverture) {
      ouverture = false;
      porte.ouvrir();
    }

    if(fermeture) {
      fermeture = false;
      porte.fermer();
    }
  }

  if(!porte.tourne()) {
    int iteration = 0;
    while(iteration++ < sleep) Sleepy::loseSomeTime(1024);
  }
}
