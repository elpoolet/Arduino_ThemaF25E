#include "lm35library.h" 

//const byte LED_PIN = 13; //use the LED @ Arduino pin 13
// déclaration des variables globales de classe
// -- diviseur de base
float divider = 0.48828125 ;
// -- offset de base pour les lectures. C'est une température à rajouter
float offset = 0.8 ;
// -- tableau des 20 dernieres mesures afin de faire une moyenne
int lastsens[20] ;
int nbmesures = 20;
int maxindex = 0;


//<<constructor>>
LM35::LM35(){
  float templm35 ;
  maxindex = nbmesures-1;
  templm35 = analogRead(pin_LM35) * divider ;
  for (int i = 0 ; i < nbmesures ; i++) {
    lastsens[i] = templm35 ; 
  }
}


//<<destructor>>
LM35::~LM35(){/*nothing to destruct*/}


// lecture de la valeur en cours
void LM35::lm35read(int lm35pin){


}
