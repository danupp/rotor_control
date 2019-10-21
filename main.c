#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#define F_CPU 3333333
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LED_pin PIN4_bm
#define TXD_pin PIN2_bm
#define RXD_pin PIN3_bm
#define COMP_pin PIN7_bm
#define PULSE PIN1_bm

#define CW 0x01
#define CCW 0x02
#define MAX_POS 450
#define MIN_POS -90

#define POSITION_EEADDR 0

volatile uint8_t step_flag;
volatile uint8_t process_usart_flag;
volatile uint8_t timeout_flag;
volatile uint8_t rot_flag;
volatile uint8_t poweroff_flag;
volatile uint8_t rx_ptr;
volatile uint8_t timeout;
volatile char rx_buff[30];

void RotStop() {
  rot_flag = 0x00;
  PORTA.OUT &= ~LED_pin;
}

void RotStart(uint8_t dir) {
  rot_flag = 0x01;
  timeout = 0;
  PORTA.OUT |= LED_pin;
}


void USART_Transmit_String(char *buffer) {
  uint8_t n = 0;
  while(buffer[n]) {
    while (!(USART0.STATUS & USART_DREIF_bm));
    USART0.TXDATAL = buffer[n];
    n++;
  }
}

uint16_t measure_temperature() {
  int8_t sigrow_offset = SIGROW.TEMPSENSE1;
  uint8_t sigrow_gain = SIGROW.TEMPSENSE0;
  uint32_t temp;
  VREF.CTRLA = 0x10; // 1.1V ref
  ADC0.MUXPOS = 0x1e; // tempsense
  _delay_ms(1);
  ADC0.COMMAND = 0x01;
  while (ADC0.COMMAND);
  // Now convert as per datasheet
  temp = ADC0.RES - sigrow_offset;
  temp *= sigrow_gain;
  temp += 0x80;
  temp >>= 8;
  return temp;
}

ISR(BOD_VLM_vect) {

  RotStop(); // just in case
  poweroff_flag = 0x01;
}

ISR(RTC_PIT_vect) { // 1 Hz
  RTC.PITINTFLAGS = 0x01; // clear flag
  //PORTA.OUTTGL = LED_pin;
  if (rot_flag) {
    timeout ++;
    if(timeout>3) {
      RotStop();
      timeout_flag = 0x01;
    }
  }

}


ISR(PORTA_PORT_vect) {
  
  if(PORTA.INTFLAGS & PULSE) {
    PORTA.INTFLAGS |= PULSE; // Clear flag
    step_flag = 0x01;
  }  
}

/*
ISR(TCA0_OVF_vect) { // 192.0 kHz @ 19.2 MHz 
  TCA0.SINGLE.INTFLAGS = 0x01; // clear flag
}
*/

ISR (USART0_RXC_vect) {

  unsigned char udr_rx;

  /* Get and return received data from buffer */
  if (USART0.RXDATAH & USART_FERR_bm)
    udr_rx = USART0.RXDATAL;
  else {
    udr_rx = USART0.RXDATAL;
    USART0.TXDATAL = udr_rx;
    
    if (udr_rx=='\n') {
      process_usart_flag = 1;
    }
    else if (rx_ptr < 28) {
      rx_buff[rx_ptr]=udr_rx;
      rx_ptr++;
    }
  }
}

void main () {
  char buffer[50];
  int16_t position, target_position, ang;
  uint8_t direction;

  static const char string_start[] PROGMEM = "\n*******\nRotor control by SM6VFZ\n\n";
  static const char string_timeout[] PROGMEM = "\nTimeout, No feedback!\n";
  static const char string_temperature[] PROGMEM = "\nTemperature: ";
  static const char string_step_too_small[] PROGMEM = "\nStep too small.";
  static const char string_pos[] PROGMEM = "Position: ";
  static const char string_forcing[] PROGMEM = "-> Forcing pos to ";
  static const char string_degrees[] PROGMEM = " deg";
  static const char string_stopping[] PROGMEM = "Stopping at ";
  static const char string_save[] PROGMEM = "Saving position to EEPROM.";
  static const char string_load[] PROGMEM = "(Re)loading position from EEPROM.";
  static const char string_quit[] PROGMEM = "\n***** Good night. *****";
  
  PORTA.DIR = LED_pin; // outputs
  PORTB.DIR = TXD_pin;
  PORTA.PIN7CTRL = 0x04; // digital input disable for AINP0/PA7

  BOD.VLMCTRLA = 0x02; // VLM 25% over BOD
  BOD.INTCTRL = 0x01;  // VLM INT when voltage crosses from above
  
  /*
  VREF.CTRLA = 0b00000010; // 2.5V for DAC0 and AC0
  AC0.MUXCTRLA = 0b00000010; // Use VREF for neg input
  AC0.CTRLA = 0b011100011; // OUTEN (PA5), pos edge, 10mV hyst, AC enable
  */
  
  /*
  TCA0.SINGLE.PER = 0x0063;
  TCA0.SINGLE.INTCTRL = 0x01;
  TCA0.SINGLE.CTRLA = 0x01; // Div 1, enable
  */

  ADC0.CTRLA = 0b00000001; // ADC enable at 10 bits resolution
  ADC0.CTRLC = 0x01; // int ref, ./4 prescaler
  ADC0.CTRLD = 0b10000000; // 128 CLK_ADC INITDLY
  ADC0.SAMPCTRL = 0b00001000; // 32 samplen????
  VREF.CTRLA = 0b00110000; // 4.3V ref

  USART0.BAUD = 1389;  // 9600 baud for 3.33 MHz and normal mode
  USART0.CTRLA = 0b10000000; // RXCIE
  USART0.CTRLB = 0b11000000; // RX+TX enable

  while(RTC.STATUS);
  RTC.CLKSEL = 0x00; // Use 32 kHz from OSC32K
  while(RTC.PITSTATUS);
  RTC.PITINTCTRL = 0x01; // enable interrupt
  while(RTC.PITSTATUS);
  RTC.PITCTRLA = 0b01110001; // Enable, interrupt after 32768 cycles => 1Hz

  rx_ptr=0;
  position = eeprom_read_word(POSITION_EEADDR);
  
  strcpy_P(buffer, string_start);
  USART_Transmit_String(buffer);
  sprintf(buffer,"\nStart at %d deg.\n> ", position);
  USART_Transmit_String(buffer);

  PORTA.OUT |= LED_pin;
  _delay_ms(200);
  PORTA.OUT &= ~LED_pin;
  _delay_ms(200);
  PORTA.OUT |= LED_pin;
  _delay_ms(200);
  PORTA.OUT &= ~LED_pin;
  _delay_ms(200);
  PORTA.OUT |= LED_pin;
  _delay_ms(200);
  PORTA.OUT &= ~LED_pin;

  step_flag = 0x00;
  process_usart_flag = 0x00;
  timeout_flag = 0x00;
  poweroff_flag = 0x00;
  
  sei();
   
  while(1) {
    if(poweroff_flag) {
      USART_Transmit_String("aaaa");
      cli();
      //eeprom_write_word(POSITION_EEADDR,position);
      strcpy_P(buffer, string_quit);
      USART_Transmit_String(buffer);
      _delay_ms(2000);
      sei();
    }
    else if(step_flag) {
      step_flag = 0x00;
      _delay_ms(1);
      if ((direction == CW) && (PORTA.IN & PULSE)) { // Low-to-high
	timeout = 0;
	position++;
	if((position > MAX_POS) || (position>(target_position-2)))
	  RotStop();
      }
      else if ((direction == CCW) && !(PORTA.IN & PULSE)) { //High-to-low
	position--;
	timeout = 0;
	if ((position < MIN_POS) || (position<(target_position+2)))
	  RotStop();
      }
    }
    else if (process_usart_flag) {
      rx_ptr=0;
      
      if (strncmp((const char *)rx_buff,"?",1)==0) {
         strcpy_P(buffer, string_start);
	 USART_Transmit_String(buffer);
      }
      else if (strncmp((const char *)rx_buff,"temp",4)==0) {
         strcpy_P(buffer, string_temperature);
	 USART_Transmit_String(buffer);
	 sprintf(buffer,"%d C",measure_temperature()-273);
	 USART_Transmit_String(buffer);
      }
      else if (strncmp((const char *)rx_buff,"r+ ",3)==0) {
         if(sscanf((const char *)rx_buff+3,"%d",&ang)) {
            if (ang > 3) {
	      target_position=position+ang;
	      RotStart(CW);
	    }
	    else {
	       strcpy_P(buffer, string_step_too_small);
	       USART_Transmit_String(buffer);
            }
	 }
      }
      else if (strncmp((const char *)rx_buff,"r- ",3)==0) {
         if(sscanf((const char *)rx_buff+3,"%d",&ang)) {
            if (ang > 3) {
	      target_position=position-ang;
	      RotStart(CCW);
	    }
	    else {
	       strcpy_P(buffer, string_step_too_small);
	       USART_Transmit_String(buffer);
            }
	 }
      }
      else if (strncmp((const char *)rx_buff,"r ",2)==0) {
	if(sscanf((const char *)rx_buff+2,"%d",&ang)) {
	  if ((ang > (position - 3)) && (ang < (position + 3))) {
 	    strcpy_P(buffer, string_step_too_small);
	    USART_Transmit_String(buffer);
	  }
	  else {
	    if (ang > position) {
	      target_position = ang;
	      RotStart(CW);
	    }
	    else if (ang < position) {
	      target_position = ang;
	      RotStart(CCW);
	    }
	  }
	}
      }
      else if (strncmp((const char *)rx_buff,"cal ",4)==0) {
	if(sscanf((const char *)rx_buff+4,"%d",&ang)) {
	  position = ang;
	  strcpy_P(buffer, string_forcing);
	  USART_Transmit_String(buffer);
	  sprintf(buffer,"%d",position);
	  USART_Transmit_String(buffer);
	  strcpy_P(buffer, string_degrees);
	  USART_Transmit_String(buffer);
	}
      }
      else if (strncmp((const char *)rx_buff,"load",4)==0) {
	RotStop(); // just in case
	strcpy_P(buffer, string_load);
	USART_Transmit_String(buffer);
	position=eeprom_read_word(POSITION_EEADDR);
      }
      else if (strncmp((const char *)rx_buff,"save",4)==0) {
	RotStop(); // just in case
	strcpy_P(buffer, string_save);
	USART_Transmit_String(buffer);
	eeprom_write_word(POSITION_EEADDR,position);
      }
      else if (strncmp((const char *)rx_buff,"d",1)==0) {
	strcpy_P(buffer, string_pos);
	USART_Transmit_String(buffer);
	sprintf(buffer,"%d",position);
	USART_Transmit_String(buffer);
	strcpy_P(buffer, string_degrees);
	USART_Transmit_String(buffer);
      }
      else if (strncmp((const char *)rx_buff,"s",1)==0) {
        RotStop();
	strcpy_P(buffer, string_stopping);
	USART_Transmit_String(buffer);
	sprintf(buffer,"%d",position);
	USART_Transmit_String(buffer);
	strcpy_P(buffer, string_degrees);
	USART_Transmit_String(buffer);
      }
      else {
	USART_Transmit_String("?");
      }
      USART_Transmit_String("\n> ");
      memset((char *)rx_buff,'\0',30); 
      process_usart_flag = 0;
    }
    if (timeout_flag) {
      timeout_flag = 0x00;
      strcpy_P(buffer, string_timeout);
      USART_Transmit_String(buffer);
      USART_Transmit_String("\n> ");      
    }
    sleep_mode();
  }
  
}