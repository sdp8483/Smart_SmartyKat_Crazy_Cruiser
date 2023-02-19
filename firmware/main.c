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
#define MOTOR_PIN             4     /* motor control pin */
#define LED_PIN               3     /* LED output pin */

// Output Pin Fuction Defines
#define LED_ON()              PA |= (1 << LED_PIN)
#define LED_OFF()             PA &= ~(1 << LED_PIN)
#define MOTOR_ON()            PA &= ~(1 << MOTOR_PIN)
#define MOTOR_OFF()           PA |= (1 << MOTOR_PIN)

void toy_active(void);
void toy_sleep(void);

// Service Interrupt Requests
void interrupt(void) __interrupt(0) {
  if (INTRQ & INTRQ_T16) {
    INTRQ &= ~INTRQ_T16;          /* mark T16 interrupt request serviced */
    INTEN &= ~INTRQ_T16;          /* disable T16 interrupt */
    T16M = T16M_CLK_DISABLE;      /* disable T16 timer */
    toy_sleep();
  }

  if (INTRQ & INTRQ_PA0) {
    INTRQ &= ~INTRQ_PA0;          /* mark PA0 interrupt request serviced */
    INTEN &= ~INTEN_PA0;          /* disable pin wakeup interrupt */
    PADIER &= ~(1 << VIBE_PIN);   /* disable pin wake function */
    toy_active();
  }
}

void toy_active() {
  LED_ON();
  MOTOR_ON();
  T16M = (uint8_t)(T16M_CLK_ILRC | T16M_CLK_DIV64 | T16M_INTSRC_13BIT);
                                  /* use 55kHz clock divided by 64, trigger when bit 13 goes from 0 to 1 */
  T16C = 0;                       /* set timer count to 0 */
  INTEN |= INTEN_T16;             /* enable T16 interrupt */
  INTRQ = 0;                      /* reset interrupts */
  __stopexe();                    /* light sleep, ILRC remains on */
}

void toy_sleep() {
  LED_OFF();
  MOTOR_OFF();
  PADIER |= (1 << VIBE_PIN);      /* enable pin wake function */
  INTEN |= INTEN_PA0;             /* enable wakeup pin */
  INTRQ = 0;                      /* reset interrupts */
  __stopsys();                    /* go to deep sleep */
}

// Main program
void main() {
  MISC |= MISC_FAST_WAKEUP_ENABLE;  /* enable faster wakeup, 45 ILRC clocks instead of 3000 */

  // Initialize hardware
  PADIER = 0;                       /* on reset all pins are set as wake pins, setting register to 0 to disable */

  // Set Vibration Sensor pin as input
  PAC &= ~(1 << VIBE_PIN);   /* set as input (all pins are input by default, setting to make sure) */
  PAPH |= (1 << VIBE_PIN);   /* enable pullup resistor on pin */

  // Set output pins
  PAC |= (1 << MOTOR_PIN);          /* set motor control pin as output */
  PAC |= (1 << LED_PIN);            /* set led pin as output */

  // setup interrupts
  INTEN = 0;                        /* disable all interrupts, on reset state is not defined in datasheet */
  INTEGS |= INTEGS_PA0_FALLING;     /* trigger when switch closes and pulls pin to ground */
  __engint();                       /* enable global interrupts */
  /* Some notes and thoughts about interrupts on the PFS154
   *  Section 5.7 of the datasheet contains information about the interrupt controller.
   *  When an interrupt is triggered global interrupts are disabled, ie __disgint() is automaticaly called.
   *  CPU steps into ISR function above and executes code there. When done __engint() is automaticaly called and 
   *  code execution starts in the main loop where it left off. Confusingly the datasheet says that even if INTEN = 0 
   *  INTRQ can still be triggered by the interrupt source. So the peripheral or port should be further disabled to prevent
   *  triggering. */
  
  toy_sleep();
  // Main processing loop
  while (1) {
    // Setup Timer16, when timer interrupts CPU goes to sleep
    /* Timer16 is setup to count up using ILRC clock diveded down to get the desired toy run time .
     * The LED and motor are turned on, cpu goes into light sleep where main code execution is stopped but ILRC is active.
     * When timer bit changes from 0 to 1 an interrupt is called and the CPU wakes, executes code below call to light sleep.
     * T16 is disabled, motor and LED are turned off. PA0 is set to wake the CPU from deep no clock sleep. 
     * CPU is set to sleep, ILCR is off. A falling edge on PA0 will wake CPU and start main loop execution at beginning. */

    // LED_ON();
    // MOTOR_ON();
    // T16M = (uint8_t)(T16M_CLK_ILRC | T16M_CLK_DIV64 | T16M_INTSRC_13BIT);
    //                                 /* use 55kHz clock divided by 64, trigger when bit 13 goes from 0 to 1 */
    // T16C = 0;                       /* set timer count to 0 */
    // INTEN |= INTEN_T16;             /* enable T16 interrupt */
    // INTRQ = 0;                      /* reset interrupts */
    // __stopexe();                    /* light sleep, ILRC remains on */
    /* Toy is active while T16 counts up. On interrupt code resumes execution below */

    // LED_OFF();
    // MOTOR_OFF();
    // PADIER |= (1 << VIBE_PIN);      /* enable pin wake function */
    // INTEN |= INTEN_PA0;             /* enable wakeup pin */
    // INTRQ = 0;                      /* reset interrupts */
    // __stopsys();                    /* go to deep sleep */
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
