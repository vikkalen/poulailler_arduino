#include "Porte.h"
#include "BH1750.h"
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
  int fermetureDelay;
} SendConfStruct;

SendAckStruct sendAckPayload;
SendStateStruct sendStatePayload;
SendConfStruct sendConfPayload;
SendConfStruct receiveConfPayload;

RFM12B radio;

Porte porte;
BH1750 lightMeter;

volatile boolean jour = false;
volatile boolean nuit = false;
volatile boolean ouverture = false;
volatile boolean fermeture = false;
volatile boolean ouvertureForcee = false;
volatile boolean fermetureForcee = false;
volatile int ouvertureTentative = 0;
volatile int fermetureTentative = 0;
volatile int fermetureDelayInc = 0;
volatile boolean fermetureEnAttente = false;
volatile boolean lumiere = false;
volatile boolean lumiereForcee = false;

uint16_t luxFermeture = 1;
uint16_t luxOuverture = 5;
int tours = 1;
int retry = 1;
int sleep = 5;
int fermetureDelay = 10;

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
  while (bit_is_set(ADCSRA, ADSC));
  result = ADCL;
  result |= ADCH << 8;
  result = 1126400L / result; // Back-calculate Vcc in mV
  ADCSRA &= ~ bit(ADEN); bitSet(PRR, PRADC); // Disable the ADC to save power
  return result;
}
//########################################################################################################################

void porteTourner()
{
  porte.tourner();
}

void ilFaitJour() {
  jour = true;
  nuit = false;
  if (!fermetureForcee) {
    if (ouvertureTentative < retry) {
      ouverture = true;
      ouvertureTentative++;
    }
    ouvertureForcee = false;
    fermetureForcee = false;
    fermetureTentative = 0;
  }
}

void ilFaitNuit() {
  if (jour) {
    fermetureEnAttente = true;
    fermetureDelayInc = 0;
    allumerLumiere();
    lumiereForcee = false;
  }
  jour = false;
  nuit = true;
  if (fermetureEnAttente && (fermetureDelayInc++ >= fermetureDelay)) {
    fermetureEnAttente = false;
  }
  
  if (!fermetureEnAttente && !ouvertureForcee) {
    if (fermetureTentative < retry) {
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

void allumerLumiere() {
  lumiere = true;
  digitalWrite(A2, HIGH);
}

void eteindreLumiere() {
  lumiere = false;
  digitalWrite(A2, LOW);
}

void basculerLumiere() {
  if (lumiere) eteindreLumiere();
  else allumerLumiere();
  lumiereForcee = true;
}

void boutonPorte() {
  detachInterrupt(1);

  doSleep(3, 1);
  
  boolean boutonOuvrir = digitalRead(8) == LOW;
  boolean boutonFermer = digitalRead(9) == LOW;

  if (boutonOuvrir && boutonFermer) basculerLumiere();
  else if (boutonOuvrir) forceOuvrir();
  else if (boutonFermer) forceFermer();
  
  porte.arreter();

  doSleep(6, 1);

  attachInterrupt(1, boutonPorte, LOW);
}

void configure() {
  luxOuverture = receiveConfPayload.luxOuverture;
  luxFermeture = receiveConfPayload.luxFermeture;
  tours = receiveConfPayload.tours;
  retry = receiveConfPayload.retry;
  sleep = receiveConfPayload.sleep;
  fermetureDelay = receiveConfPayload.fermetureDelay;
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
  eeAddress += 2;
  EEPROM.get(eeAddress, fermetureDelay);
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
  eeAddress += 2;
  EEPROM.put(eeAddress, fermetureDelay);
}

void updateState() {
  lightMeter.begin(BH1750_ONE_TIME_HIGH_RES_MODE);
  delay(180);
  lux = lightMeter.readLightLevel();

  porte.activerCapteurs();
  delay(20);

  if (porte.ouverte()) ouvert = 1;
  else ouvert = 0;

  if (porte.fermee()) ferme = 1;
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
  sendConfPayload.fermetureDelay = fermetureDelay;

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

  if (flag == 0) // fin de communication
    return false;
  else if (flag == 1)
    sendState();
  else if (flag == 2)
    sendConf();
  else if (flag == 3)  {
    receiveConfPayload = *(SendConfStruct*) radio.Data;
    configure();
    sendConf();
  }
  else if (flag == 4) {
    forceOuvrir();
    sendAck();
  }
  else if (flag == 5) {
    forceFermer();
    sendAck();
  }
  else if (flag == 6) {
    basculerLumiere();
    sendAck();
  }

  return true;
}

ISR(WDT_vect) {
  //lumiere = !lumiere;
  //digitalWrite(A2, lumiere ? HIGH : LOW);
}

// function to configure the watchdog: let it sleep 8 seconds before firing
// when firing, configure it for resuming program execution
void configure_wdt(char mode)
{
  cli();                           // disable interrupts for changing the registers
  MCUSR = 0;                       // reset status register flags
                                   // Put timer in interrupt-only mode:                                       
  WDTCSR |= 0b00011000;            // Set WDCE (5th from left) and WDE (4th from left) to enter config mode,
  WDTCSR =  0b01000000 | mode  ; // set WDIE: interrupt enabled
                                   // clr WDE: reset disabled
                                   // and set delay interval (right side of bar) to 8 seconds

  sei();                           // re-enable interrupts

  // reminder of the definitions for the time before firing
  // delay interval patterns:
  //  16 ms:     0b000000
  //  500 ms:    0b000101
  //  1 second:  0b000110
  //  2 seconds: 0b000111
  //  4 seconds: 0b100000
  //  8 seconds: 0b100001
 
}

// Put the Arduino to deep sleep. Only an interrupt can wake it up.
void doSleep(char mode, int ncycles)
{  
  configure_wdt(0b000111 & mode);

  int nbr_remaining = ncycles;
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  while (nbr_remaining-- > 0) {
    sleep_mode();
    sleep_disable();
  }
}

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

  // lumière
  pinMode(A2, OUTPUT);
  digitalWrite(A2, LOW);

  //writeConf();
  readConf();

  porte.setTours(tours);

  eteindreLumiere();
}

void loop() {

  updateState();

  radio.Wakeup();
  sendState();
  while (receive());
  radio.Sleep();

  if (!fermetureEnAttente && !lumiereForcee) eteindreLumiere();

  if (ouvert == 1) ouvertureTentative = 0;
  if (ferme == 1) fermetureTentative = 0;

  if (lux < luxFermeture) ilFaitNuit();
  else if (lux > luxOuverture) ilFaitJour();

  if (ouverture) {
    ouverture = false;
    porte.ouvrir();
  }
  else if (fermeture) {
    fermeture = false;
    porte.fermer();
  }

  while (porte.tourne());
  
  doSleep(6, sleep);
}
