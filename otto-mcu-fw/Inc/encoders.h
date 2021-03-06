/**
  ******************************************************************************
  * File Name          : encoders.h
  * Description        : This file is the header file for the encoder code.
  * Author             : Steven Hang (github.com/adorbs)
  ******************************************************************************
  */
#ifndef __encoders_H
#define __encoders_H
#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include "otto.h"

typedef struct encoder_t
{
	pindef_t pinA;
 	pindef_t pinB;
 	uint8_t value;
}encoder_t;

extern encoder_t gEncoders[NUM_ENCODERS];

void initEncoder(encoder_t* enc, pindef_t pin_a, pindef_t pin_b);
void updateEncoder(encoder_t* encoder);

#ifdef __cplusplus
}
#endif
#endif /*__encoders_H */
