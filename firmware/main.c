/* Smart SmartyKat Crazy Cruiser
 * Target Device: PFS154-S08
 *
 */

#include <stdint.h>
#include <pdk/device.h>
#include "auto_sysclock.h"

// Pin Defines - all pins are on port A
#define VIBE_SENSOR_PIN       0     /* vibration sensor input pin, used to wake from deep sleep */
#define MOTOR_PIN             4     /* motor control pin */
#define LED_PIN               3     /* LED output pin */

// Output Pin Fuction Defines
#define LED_ON()              PA |= (1 << LED_PIN)
#define LED_OFF()             PA &= ~(1 << LED_PIN)
#define MOTOR_ON()            PA &= ~(1 << MOTOR_PIN)
#define MOTOR_OFF()           PA |= (1 << MOTOR_PIN)

// Service Interrupt Requests
void interrupt(void) __interrupt(0) {
  if (INTRQ & INTRQ_T16) {
    INTRQ &= ~INTRQ_T16;          /* mark T16 interrupt request serviced */
    INTEN &= ~INTRQ_T16;          /* disable T16 interrupt */
  }

  if (INTRQ & INTRQ_PA0) {
    INTRQ &= ~INTRQ_PA0;          /* mark PA0 interrupt request serviced */
    INTEN &= ~INTEN_PA0;          /* disable pin wakeup interrupt */
  }
}

// Main program
void main() {
  MISC |= MISC_FAST_WAKEUP_ENABLE;  /* enable faster wakeup, 45 ILRC clocks instead of 3000 */

  // Initialize hardware
  PADIER = 0;                       /* on reset all pins are set as wake pins, setting register to 0 to disable */

  // Set Vibration Sensor pin as input and wakeup
  PAC &= ~(1 << VIBE_SENSOR_PIN);   /* set as input (all pins are input by default, setting to make sure) */
  PADIER |= (1 << VIBE_SENSOR_PIN); /* enable as wakeup/interrupt pin */
  PAPH |= (1 << VIBE_SENSOR_PIN);   /* enable pullup resistor on pin */

  // Set output pins
  PAC |= (1 << MOTOR_PIN);          /* set motor control pin as output */
  PAC |= (1 << LED_PIN);            /* set led pin as output */

  __engint();                       /* enable global interrupts */
 
  // Main processing loop
  while (1) {
    // Setup Timer16, when timer interrupts CPU goes to sleep
    // __disgint();                    /* disable global interrupts */
    T16M = (uint8_t)(T16M_CLK_ILRC | T16M_CLK_DIV64 | T16M_INTSRC_13BIT);
                                    /* use 55kHz clock divided by 64, trigger when bit 13 goes from 0 to 1 */
    T16C = 0;                       /* set timer count to 0 */
    LED_ON();
    MOTOR_ON();
    INTEN |= INTEN_T16;             /* enable T16 interrupt */
    INTRQ = 0;                      /* reset interrupts */
    // __engint();                     /* enable global interrupts */
    __stopexe();                    /* light sleep, ILRC remains on */
    /* Toy is active while T16 counts up. On interrupt code resumes execution below */
    // __disgint();                    /* disable global interrupts */
    T16M = T16M_CLK_DISABLE;        /* disable T16 timer */
    LED_OFF();
    MOTOR_OFF();
    INTEN |= INTEN_PA0;             /* enable wakeup pin */
    INTRQ = 0;                      /* reset interrupts */
    // __engint();                     /* enable global interrupts */
    __stopsys();                    /* go to deep sleep */
    /* Toy is in deep sleep. Only wakes on interrupt from PA0 triggered by vibration switch, code resumes execution below */
  }
}

// Startup code - Setup/calibrate system clock
unsigned char _sdcc_external_startup(void) {
  /* Set the system clock 
   * note it is necessary to enable IHRC clock while updating clock settings or CPU will hang  */
  PDK_USE_ILRC_SYSCLOCK();          /* use ILRC 55kHz clock as sysclock */
  PDK_DISABLE_IHRC();               /* disable IHRC to save power */
  EASY_PDK_CALIBRATE_ILRC(F_CPU, TARGET_VDD_MV);

  return 0;   // Return 0 to inform SDCC to continue with normal initialization.
}
