#include "Arduino.h"
#include "Porte.h"

Porte::Porte(unsigned int tours)
{
  _tours = tours;
  _running = false;

//  DDRB &= B11001111;
//  DDRB |= B00001111;
//  DDRD &= B11111111;
  
  // moteur
  DDRD |= B11110000;

  // fermee / ouverte
  DDRB |= B01000000;
  PORTB &= B10111111;
  DDRC &= B11111100;
  PORTC &= B11111100;
}

Porte::Porte()
{
  Porte(0);
}

void Porte::setTours(unsigned int tours)
{
	_tours = tours;
}

void Porte::fermer()
{
  if(!tourne() && !fermee())
  {
	delay(10);
	activerCapteurs();
    _cw = true;
    _start();
  }
}

void Porte::ouvrir()
{
  if(!tourne() && !ouverte())
  {
	activerCapteurs();
	delay(20);
    _cw = false;
    _start();
  }
}

void Porte::arreter()
{
  PORTD &= B00001111;
  Timer1.stop();
  _running = false;
  desactiverCapteurs();
}

void Porte::activerCapteurs()
{
  PORTB |= B01000000;
}

void Porte::desactiverCapteurs()
{
  PORTB &= B10111111;
}


boolean Porte::fermee()
{
  return PINC & B00000010;
}

boolean Porte::ouverte()
{
  return PINC & B00000001;
}

boolean Porte::tourne()
{
  return _running;
}

byte Porte::_shift(byte n)
{
  if(_cw) return ((n<<1) | (n>>3)) & B11110000;
  else return ((n>>1) | (n<<3)) & B11110000;
}

void Porte::tourner()
{   
  if(_tour >= _tours || _cw && fermee() || !_cw && ouverte())
  {
	arreter();
    return;
  }

  PORTD &= B00001111;
  if(_isFullStep)
  {
    PORTD |= _fullStep;
    _fullStep = _shift(_fullStep);
    _isFullStep = false;
  }
  else
  {
    PORTD |= _halfStep;
    _halfStep = _shift(_halfStep);
    _isFullStep = true;
  }

  _step++;
  if(_step == PORTE_UN_TOUR)
  {
    _tour++;
    _step = 0;
  }
  if(_period > PORTE_PERIOD_FIN)
  {
    _period -= PORTE_ACCEL;
    Timer1.setPeriod(_period);
  }
}

void Porte::_start()
{
  _isFullStep = !_cw;
  _halfStep = B00010000;
  _fullStep = B00110000;
  _step = 0;
  _tour = 0;
  _period = PORTE_PERIOD_INIT;
  _running = true;
  Timer1.setPeriod(PORTE_PERIOD_INIT);
  Timer1.start();
}


