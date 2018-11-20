#ifndef Porte_h
#define Porte_h

#include "Arduino.h"
#include "TimerOne.h"

#define PORTE_UN_TOUR 4096
#define PORTE_PERIOD_INIT 3000
#define PORTE_PERIOD_FIN 2000
#define PORTE_ACCEL 1

class Porte
{
  public:
    Porte(unsigned int tours);
    Porte();
    void fermer();
    void ouvrir();
    void tourner();
    void arreter();
    void activerCapteurs();
    void desactiverCapteurs();
    void setTours(unsigned int tours);
    boolean fermee();
    boolean ouverte();
    boolean tourne();
  private:
    unsigned int _tours;
    byte _halfStep;
    byte _fullStep;
    boolean _cw;
    boolean _isFullStep;
    unsigned int _nbTours;
    unsigned int _step;
    unsigned int _tour;
    boolean _running;
    unsigned int _period;
    byte _shift(byte n);
    void _start();
};

#endif
