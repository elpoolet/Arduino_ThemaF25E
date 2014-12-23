#ifndef LM35LIBRARY_H
#define LM35LIBRARY_H

// Obligatoire
#include <WProgram.h> 

class LM35 {
public:
	LM35();
	~LM35();
	float lm35read(int lm35pin);
};

#endif
