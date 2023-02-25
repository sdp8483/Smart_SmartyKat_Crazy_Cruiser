/* Smart SmartyKat Crazy Cruiser
 * Target Device: PFS154-S08
 * 
 * Sam Perry 2023
 * github.com/sdp8483/Smart_SmartyKat_Crazy_Cruiser
 */

#include <stdint.h>
#include <pdk/device.h>
#include "auto_sysclock.h"

// Pin Defines - all pins are on port A
#define VIBE_PIN              0     /* vibration sensor input pin, used to wake from deep sleep */
#define MOTOR_PIN             4     /* motor control pin, controlled with a pmosfet */
#define LED_PIN               3     /* LED output pin, current source */

// Output Pin Fuction Defines
#define LED_ON()              PA |= (1 << LED_PIN)
#define LED_OFF()             PA &= ~(1 << LED_PIN)
#define MOTOR_ON()            PA &= ~(1 << MOTOR_PIN)
#define MOTOR_OFF()           PA |= (1 << MOTOR_PIN)

// State Machine
typedef enum {
  GOTO_SLEEP,                       /* prepare to sleep */
  SLEEP,                            /* toy is in deep sleep */
  WAKEUP,                           /* toy was awaken from deep sleep */
  ACTIVE,                           /* toy is active, LED and motor on */
} fsm_states_t;

fsm_states_t fsm_state = GOTO_SLEEP;

// Service Interrupt Requests
void interrupt(void) __interrupt(0) {
  /* Some notes and thoughts about interrupts on the PFS154
   *  Section 5.7 of the datasheet contains information about the interrupt controller.
   *  When an interrupt is triggered global interrupts are disabled, ie __disgint() is automaticaly called.
   *  CPU steps into ISR function below and executes code there. When done __engint() is automaticaly called and 
   *  code execution starts in the main loop where it left off. Confusingly the datasheet says that even if INTEN = 0 
   *  INTRQ can still be triggered by the interrupt source. So the peripheral or port should be further disabled to prevent
   *  triggering. */

  if (INTRQ & INTRQ_PA0) {        /* wake pin was pulled low */
    INTRQ &= ~INTRQ_PA0;          /* mark PA0 interrupt request serviced */
    fsm_state = WAKEUP;           /* change state */
  }

  if (INTRQ & INTRQ_T16) {        /* timer has expired */
    INTRQ &= ~INTRQ_T16;          /* mark T16 interrupt request serviced */
    fsm_state = GOTO_SLEEP;       /* change state */
  }
}

// Main program
void main() {
  MISC |= MISC_FAST_WAKEUP_ENABLE;  /* enable faster wakeup, 45 ILRC clocks instead of 3000 */

  // Initialize hardware
  PADIER = 0;                       /* on reset all pins are set as wake pins, setting register to 0 to disable */
  PBDIER = 0;                       /* there is no port B on the -S08 but without setting this to 0 the uC will wake unexpectedly */

  // Set Vibration Sensor pin as input
  PAC &= ~(1 << VIBE_PIN);   /* set as input (all pins are input by default, setting to make sure) */
  PAPH |= (1 << VIBE_PIN);   /* enable pullup resistor on pin */

  // Set output pins
  PAC |= (1 << MOTOR_PIN);          /* set motor control pin as output */
  PAC |= (1 << LED_PIN);            /* set led pin as output */
  LED_OFF();                        /* set initial LED state */
  MOTOR_OFF();                      /* set initial motor state */

  // Forever Loop
  while (1) {
    switch (fsm_state) {
      case GOTO_SLEEP:
        __disgint();                  /* disable global interrupts */
        
        LED_OFF();
        MOTOR_OFF();

        INTEN = 0;                    /* disable all interrupts */
        PADIER = (1 << VIBE_PIN);     /* enable only one wakeup pin */
        PBDIER = 0;                   /* make sure port B does not wake */
        INTEGS |= INTEGS_PA0_FALLING; /* trigger when switch closes and pulls pin to ground */
        INTEN |= INTEN_PA0;           /* enable interrupt on wake pin */
        INTRQ = 0;                    /* reset interrupts */

        fsm_state = SLEEP;            /* change state */

        break;

      case SLEEP:
        __engint();                   /* enable global interrupts */
        __stopsys();                  /* go to deep sleep */
        break;
      
      case WAKEUP:
        __disgint();                  /* disable global interrupts */
        INTEN = 0;                    /* disable all interrupts */
        PADIER = 0;                   /* disable wakeup pin */

        T16M = (uint8_t)(T16M_CLK_ILRC | T16M_CLK_DIV64 | T16M_INTSRC_13BIT);
                                      /* use 55kHz clock divided by 64, trigger when bit N goes from 0 to 1 */
        T16C = 0;                     /* set timer count to 0 */
        INTEN |= INTEN_T16;           /* enable T16 interrupt */
        INTRQ = 0;                    /* reset interrupts */

        LED_ON();
        MOTOR_ON();

        fsm_state = ACTIVE;

        break;
      
      case ACTIVE:
        __engint();                   /* enable global interrupts */
        __stopexe();                  /* light sleep, ILRC remains on */
        break;

      default:
        break;
    }
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
