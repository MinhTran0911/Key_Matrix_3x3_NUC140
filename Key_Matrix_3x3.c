#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "NUC100Series.h"

/*------------------------------MACROS Start--------------------------*/
// Define Clock Sources Enable Bit
#define  HXT_12M_EN 1UL << 0
#define  LXT_32K_EN 1UL << 1
#define HIRC_22M_EN 1UL << 2
#define LIRC_10K_EN 1UL << 3

// Define Clock Sources and PLL Stability Status
#define  HXT_12M_STABLE (CLK->CLKSTATUS & (1UL << 0))
#define  LXT_32K_STABLE (CLK->CLKSTATUS & (1UL << 1))
#define      PLL_STABLE (CLK->CLKSTATUS & (1UL << 2))
#define LIRC_10K_STABLE (CLK->CLKSTATUS & (1UL << 3))
#define HIRC_22M_STABLE (CLK->CLKSTATUS & (1UL << 4))

// Define Available Clock Sources for Timers
#define  HXT_12M 0x0UL
#define  LXT_32K 0x1UL
#define     HCLK 0x2UL
#define HIRC_22M 0x7UL

// Define Timers Operating Modes
#define   ONE_SHOT 0x0UL
#define   PERIODIC 0x1UL
#define     TOGGLE 0x2UL
#define CONTINUOUS 0x3UL

// Define Key Matrix Columns
#define COL_1 0
#define COL_2 1
#define COL_3 2

// Define Key Presses
#define K1_PRESSED (PA->ISRC & (1UL << 3)) && (current_column == COL_1)
#define K9_PRESSED (PA->ISRC & (1UL << 5)) && (current_column == COL_3)
/*------------------------------MACROS End--------------------------*/


/*------------------------------Global Variables Declarations Start--------------------------*/
// Currently Active Key Matrix Column
volatile static int current_column;

// This flag is used to debounce Key Matrix
// If true, waiting to accept a key press
// If false, no action is taken upon key press
volatile static bool receiving; 
/*------------------------------Global Variables Declarations End--------------------------*/


/*------------------------------Function Declarations Start----------------------------------*/
// Initializers
void CLKSRC_init(void);
void CPUCLK_init(void);
void TIMER0_init(uint8_t clksrc, uint8_t prescale, uint8_t mode, uint32_t cmp_val);
void TIMER1_init(uint8_t clksrc, uint8_t prescale, uint8_t mode, uint32_t cmp_val);
void Key_Matrix_init(void);
void LED5_init(void);
void LED8_init(void);

// Timer Control Functions
void TIMER0_start(void);
void TIMER0_stop(void);
void TIMER0_reset(void);

void TIMER1_start(void);
void TIMER1_stop(void);
void TIMER1_reset(void);
	
// LEDs Control Functions
void LED5_toggle(void);
void LED8_toggle(void);

// Interrupt Request Handlers
void TMR0_IRQHandler(void);
void TMR1_IRQHandler(void);
void GPAB_IRQHandler(void);

// Pending Interupt Request Clear Functions
void clear_pending_TMR0(void);
void clear_pending_TMR1(void);
void clear_pending_GPAB_INT(void);
/*------------------------------Function Declarations End---------------------------------*/


/*-------------------------Main Start--------------------------------*/
int main(void) {
	
	SYS_UnlockReg(); // Unlock Write-protected Registers
	
	CLKSRC_init(); // Enable all 4 Clock Sources
	CPUCLK_init(); // Generate 50 MHz CPUCLK
	
	
	/* Constraint: TIMER1 Compare Value must be significantly greater than TIMER0 Compare Value */
	
	// Timer 0 is used to Sweep the Key Matrix columns
	TIMER0_init(HXT_12M, 5, PERIODIC, 1000);
	
	// Timer 1 is used to Debounce the Key Matrix buttons
	TIMER1_init(HXT_12M, 5, ONE_SHOT, 1000000);
	
	// Set up GPIOs for Key Matrix, onboard LED5 and LED8
	Key_Matrix_init();
	LED5_init();
	LED8_init();
	
	// Reset and Start Timer 0 to start Sweeping columns
	TIMER0_stop();
	TIMER0_reset();
	TIMER0_start();
	
	SYS_LockReg(); // Lock Write-protected Registers
	
	current_column = COL_1; // First Column is Column 1 (K1, K4, K7)
	receiving = true; // Ready to receive first key press
	
	while (1) {
		/* 	
		 K1 Pressed -> LED5 Toggles
		 K9 Pressed -> LED8 Toggles
		 */
	} // end while (1)
		
} // end int main(void)
/*-------------------------Main End--------------------------------*/

/*-------------------------Interrupt Service Routines Start--------------------------------*/
void TMR0_IRQHandler(void) {
	// Periodically Sweep Through all 3 Key Matrix Columns
	if (current_column == COL_3) current_column = COL_1;
	else current_column += 1;
	
	// Activate current column by pulling it LOW
	// Other columns are HIGH
	// Rows are Quasi-Bidir, setting them HIGH allow them to act as Inputs
	switch (current_column) {
		case COL_1:
			PA->DOUT &= ~(1UL << 2);
			PA->DOUT |= (1UL << 1);
			PA->DOUT |= (1UL << 0);
			
			PA->DOUT |= (1UL << 3);
			PA->DOUT |= (1UL << 4);
			PA->DOUT |= (1UL << 5);
			
			break;
		
		case COL_2:
			PA->DOUT |= (1UL << 2);
			PA->DOUT &= ~(1UL << 1);
			PA->DOUT |= (1UL << 0);
			
			PA->DOUT |= (1UL << 3);
			PA->DOUT |= (1UL << 4);
			PA->DOUT |= (1UL << 5);
			
			break;
		
		case COL_3:
			PA->DOUT |= (1UL << 2);
			PA->DOUT |= (1UL << 1);
			PA->DOUT &= ~(1UL << 0);
			
			PA->DOUT |= (1UL << 3);
			PA->DOUT |= (1UL << 4);
			PA->DOUT |= (1UL << 5);
			
			break;
		
	} // end switch (current column)
	
	
	clear_pending_TMR0();
} // end TMR0_IRQHandler(void)

void TMR1_IRQHandler(void) {
	// After a short amount of time, reset the receiving flag to true allow another key press to be detected
	receiving = true; 
	clear_pending_TMR1();
} // end void TMR1_IRQHandler(void)

void GPAB_IRQHandler(void) {
	// If receiving flag is true, check which Key in the Key Matrix is pressed, otherwise ignore.
	if (receiving) {
		// K1 Detection
		if (K1_PRESSED) {
			LED5_toggle();
		} // end if ((PA->ISRC & (1UL << 3)) && (current_column == COL_1))
		// K9 Detection
		else if (K9_PRESSED) {
			LED8_toggle();
		} // end else if ((PA->ISRC & (1UL << 5)) && (current_column == COL_3))
		
		receiving = false; // Not receiving any key press for a short amount of time (debounce)
		TIMER1_start(); // Start Debounce Timer
	} // end if (receving)
	clear_pending_GPAB_INT();
} // end GPAB_IRQHandler(void)
/*-------------------------Interrupt Service Routines End--------------------------------*/


/*-------------------------Function Definitions Start--------------------------------*/
// Initializers
void CLKSRC_init(void) {
	/* This function enables all 4 Clock Sources of the NUC140 microcontroller:
	 * 12 MHz High-speed Crystal (HXT)
	 * 32.768 kHz Low-speed Crystal (LXT)
	 * 22.1184 MHz High-speed RC Oscillator (HIRC)
	 * 10 kHz Low-speed RC Oscillator (LIRC)
	 * @param: None
	 * @return: None
	 */
	
	// Turn On and Wait until 12 MHz HXT is Stable
	CLK->PWRCON |= HXT_12M_EN;
	while (!HXT_12M_STABLE);
	
	// Turn On and Wait until 32 kHz LXT is Stable
	CLK->PWRCON |= LXT_32K_EN;
	while (!LXT_32K_STABLE);
	
	// Turn On and Wait until 22 MHz HIRC is Stable
	CLK->PWRCON |= HIRC_22M_EN;
	while (!HIRC_22M_STABLE);
	
	// Turn On and Wait until 12 MHz HXT is Stable
	CLK->PWRCON |= LIRC_10K_EN;
	while (!LIRC_10K_STABLE);
} // end void CLKSRC_init(void)

void CPUCLK_init(void) {
	/* This function initializes the CPU Clock Frequency to 50 MHz
	 * using the Phase Lock Loop (PLL)
	 * @param: None
	 * @return: None
	 */
	
	// Clear PLLCON register
	// PLL in Normal Mode, PLL FOUT enabled, PLL Clock Source from 12 MHz HXT
	CLK->PLLCON &= ~(0xFFFFFUL << 0);
	
	// Set PLL Values
	// Fin = 12 MHz, Fout = 50 MHz
	// Fout/Fin = 50/12 = 25/6 = NF/(NR*NO)
	// Choose NO = 2  => OUT_DV = 1
	// =>     NR = 3  =>  IN_DV = 1
	// =>     NF = 25 =>  FB_DV = 23
	CLK->PLLCON |= (1UL << 14);
	CLK->PLLCON |= (1UL << 9);
	CLK->PLLCON |= (23UL << 0);
	
	// Wait until PLL is Stable
	while (!PLL_STABLE);
	
	// Clear CLKSEL0[2:0]
	CLK->CLKSEL0 &= ~(0x7UL << 0);
	
	// Select PLL Clock as Clock Source
	CLK->CLKSEL0 |= (0x2UL << 0);
	
	// Clear CLKDIV[3:0], Clock Divider Ratio is 1
	CLK->CLKDIV &= ~(0xFUL << 0);
	
} // end void CPUCLK_init(void)

void TIMER0_init(uint8_t clksrc, uint8_t prescale, uint8_t mode, uint32_t cmp_val) {
	/* This function initializes Timer 0 from the NUC140 microcontroller
	 * and enables its Interrupt
	 * @param clksrc: clock source for Timer 0 [12MHz HXT, 32.768 kHz LXT, HCLK, 22.1184 MHz HIRC]
	 * @param prescale: Timer 0 8-bit prescaler value [3 ~ 255]
	 * @param mode: Timer 0 operating mode [ONE-SHOT, PERIODIC, TOGGLE, CONTINUOUS]
	 * @param cmp_val: Timer 0 24-bit compare value [2 ~ 16,777,215]
	 * @return: None
	 */
	
	// Select Clock Source for Timer 0
	// Clear CLKSEL1[10:8]
	CLK->CLKSEL1 &= ~(0x7UL << 8);

	if (clksrc != HXT_12M && clksrc != LXT_32K && clksrc != HCLK && clksrc != HIRC_22M)
		// If Timer 0 Input Clock is different than 4 available Clock Sources,
		// default Timer 0 Input Clock to 12 MHz HXT.
		CLK->CLKSEL1 |= (HXT_12M << 8);
	else CLK->CLKSEL1 |= ((unsigned) clksrc << 8);
	
	
	// Enable Clock Source for Timer 0
	CLK->APBCLK |= (1UL << 2);
	
	
	// Set Timer 0 Compare Value
	// If Compare Value is less than 2 or greater than 2^25 - 1 (16,777,215),
	// default Compare Value to 1000.
	// Compare Value less than 2 will make Timer goes into unknown state (according to the Technical Manual)
	if (cmp_val < 2 || cmp_val > 16777215)
		TIMER0->TCMPR = 1000;
	else TIMER0->TCMPR = cmp_val;
	
	// Set 8-bit Prescale for Timer 0
	// Clear TCSR0[7:0] (PRESCALE) to 0
	TIMER0->TCSR &= ~(0xFFUL << 0);
	if (prescale < 3)
		// If Prescale value less than 3, default Prescale to 5.
		// From experiment, Prescale value less than 3 results in unstable Timer output.
		TIMER0->TCSR |= (0x05UL << 0);
	else TIMER0->TCSR |= ((unsigned) prescale << 0);
	
	
	// Enable Data Load to update Timer Data Register (TDR)
	TIMER0->TCSR |= (1UL << 16);
	
	
	// Disable Counter Mode
	TIMER0->TCSR &= ~(1UL << 24);
	
	
	// Stop Timer, Reset current Prescale Counter and Up Timer
	TIMER0->TCSR |= (1UL << 26);
	
	
	// Set Timer 0 Operating Mode
	// Clear TCSR0[28:27] (MODE) to 00 (ONE-SHOT MODE)
	TIMER0->TCSR &= ~(0x3UL << 27);
	if (mode != ONE_SHOT && mode != PERIODIC && mode != TOGGLE && mode != CONTINUOUS)
		// If Timer Mode is different than 4 available Modes,
		// default Timer Mode to PERIODIC
		TIMER0->TCSR |= (PERIODIC << 27);
	else TIMER0->TCSR |= ((unsigned) mode << 27);
	
	
	// Enable Timer 0 Interrupt on Compare Value match
	TIMER0->TCSR |= (1UL << 29);
	
	
	// Enable System-level Interrupt for Timer 0
	NVIC->ISER[0] |= (1UL << 8);
	// Set Timer 0 Interrupt Priority to 1 (2nd-highest)
	NVIC->IP[2] &= ~(0x3UL << 6); // Clear IPR2[7:6]
	NVIC->IP[2] |= (0x1UL << 6);
	
} // end void TIMER0_init(uint8_t clksrc, uint8_t prescale, uint8_t mode, uint32_t cmp_val)

void TIMER1_init(uint8_t clksrc, uint8_t prescale, uint8_t mode, uint32_t cmp_val) {
	/* This function initializes Timer 1 from the NUC140 microcontroller
	 * and enables its Interrupt
	 * @param clksrc: clock source for Timer 1 [12MHz HXT, 32.768 kHz LXT, HCLK, 22.1184 MHz HIRC]
	 * @param prescale: Timer 1 8-bit prescaler value [3 ~ 255]
	 * @param mode: Timer 1 operating mode [ONE-SHOT, PERIODIC, TOGGLE, CONTINUOUS]
	 * @param cmp_val: Timer 1 24-bit compare value [2 ~ 16,777,215]
	 * @return: None
	 */
	
	// Select Clock Source for Timer 1
	// Clear CLKSEL1[10:8]
	CLK->CLKSEL1 &= ~(0x7UL << 12);

	if (clksrc != HXT_12M && clksrc != LXT_32K && clksrc != HCLK && clksrc != HIRC_22M)
		// If Timer 1 Input Clock is different than 4 available Clock Sources,
		// default Timer 1 Input Clock to 12 MHz HXT.
		CLK->CLKSEL1 |= (HXT_12M << 12);
	else CLK->CLKSEL1 |= ((unsigned) clksrc << 12);
	
	
	// Enable Clock Source for Timer 1
	CLK->APBCLK |= (1UL << 3);
	
	
	// Set Timer 1 Compare Value
	// If Compare Value is less than 2 or greater than 2^25 - 1 (16,777,215),
	// default Compare Value to 1000.
	// Compare Value less than 2 will make Timer goes into unknown state (according to the Technical Manual)
	if (cmp_val < 2 || cmp_val > 16777215)
		TIMER1->TCMPR = 1000;
	else TIMER1->TCMPR = cmp_val;
	
	// Set 8-bit Prescale for Timer 1
	// Clear TCSR0[7:0] (PRESCALE) to 1
	TIMER1->TCSR &= ~(0xFFUL << 0);
	if (prescale < 3)
		// If Prescale value less than 3, default Prescale to 5.
		// From experiment, Prescale value less than 3 results in unstable Timer output.
		TIMER1->TCSR |= (0x05UL << 0);
	else TIMER1->TCSR |= ((unsigned) prescale << 0);
	
	
	// Enable Data Load to update Timer Data Register (TDR)
	TIMER1->TCSR |= (1UL << 16);
	
	
	// Disable Counter Mode
	TIMER1->TCSR &= ~(1UL << 24);
	
	
	// Stop Timer, Reset current Prescale Counter and Up Timer
	TIMER1->TCSR |= (1UL << 26);
	
	
	// Set Timer 1 Operating Mode
	// Clear TCSR0[28:27] (MODE) to 00 (ONE-SHOT MODE)
	TIMER1->TCSR &= ~(0x3UL << 27);
	if (mode != ONE_SHOT && mode != PERIODIC && mode != TOGGLE && mode != CONTINUOUS)
		// If Timer Mode is different than 4 available Modes,
		// default Timer Mode to PERIODIC
		TIMER1->TCSR |= (PERIODIC << 27);
	else TIMER1->TCSR |= ((unsigned) mode << 27);
	
	
	// Enable Timer 1 Interrupt on Compare Value match
	TIMER1->TCSR |= (1UL << 29);
	
	
	// Enable System-level Interrupt for Timer 0
	NVIC->ISER[0] |= (1UL << 9);
	// Set Timer 0 Interrupt Priority to 1 (2nd-highest)
	NVIC->IP[2] &= ~(0x3UL << 14); // Clear IPR2[7:6]
	NVIC->IP[2] |= (0x1UL << 14);
	
} // end void TIMER1_init(uint8_t clksrc, uint8_t prescale, uint8_t mode, uint32_t cmp_val)

void Key_Matrix_init(void) {
	// GPA.0-2 to Inputs
	PA->PMD &= ~((0x3UL << 0) | (0x3UL << 2) | (0x3UL << 4));
	
	// GPA.0-2 (Columns) to Push-Pull Outputs
	PA->PMD |= (0x1UL << 0) | (0x1UL << 2) | (0x1UL << 4);
	
	// GPA.0-2 (Columns) HIGH
	PA->DOUT |= (1UL << 0) | (1UL << 1) | (1UL << 2);
	
	// Set GPA.3-5 (Rows) Edge-triggered Interrupt
	PA->IMD &= ~((1UL << 3) | (1UL << 4) | (1UL << 5));
	
	// Enable GPA.3-5 (Rows) Falling Edge trigger Interrupt
	PA->IEN |= ((1UL << 3) | (1UL << 4) | (1UL << 5));
	
	// Disable GPA.3-5 (Rows) Rising Edge trigger Interrupt
	PA->IEN &= ~((1UL << 19) | (1UL << 20) | (1UL << 21));
	
	// Clear any pending GPA.3-5 (Rows) Interrupt flag
	PA->ISRC |= (1UL << 3) | (1UL << 4) | (1UL << 5);
	
	// Enable System-level Interrupt for GPAB_INT
	NVIC->ISER[0] |= (1UL << 4);
	
	// Set GPAB_INT priority to 00 (highest)
	NVIC->IP[1] &= ~(0x3UL << 14);
	NVIC->IP[1] |= (0x1UL << 14);
} // end void Key_Matrix_init(void)

void LED5_init(void) {
	/* This function initializes GPC.12 to work with onboard LED5
	 * @param: None
	 * @return: None
	 */
	
	// Clear GPC.12 Pin Mode Bits PC->PMD[25:24]
	PC->PMD &= ~(0x3UL << 24);
	// Set GPC.12 Pin Mode to Push-Pull Output
	PC->PMD |= (0x1UL << 24);
} // end LED5_init(void)

void LED8_init(void) {
	/* This function initializes GPC.12 to work with onboard LED5
	 * @param: None
	 * @return: None
	 */
	
	// Clear GPC.15 Pin Mode Bits PC->PMD[31:30]
	PC->PMD &= ~(0x3UL << 30);
	// Set GPC.15 Pin Mode to Push-Pull Output
	PC->PMD |= (0x1UL << 30);
} // end LED8_init(void)

// Timer Control Functions
void TIMER0_start(void) {
	/* This function starts Timer 0
	 * @param: None
	 * @return: None
	 */
	
	// Start Counting
	TIMER0->TCSR |= (1UL << 30);
} // end TIMER0_start(void)

void TIMER0_stop(void) {
	/* This function stops Timer 0
	 * @param: None
	 * @return: None
	 */
	
	// Stop Counting
	TIMER0->TCSR &= ~(1UL << 30);
} // end void TIMER0_stop(void)

void TIMER0_reset(void) {
	/* This function resets Timer 0
	 * @param: None
	 * @return: None
	 */
	
	// Stop Counting, Reset Prescale Count and Timer Count to 0
	TIMER0->TCSR |= (1UL << 26);
} // end TIMER0_reset(void)

void TIMER1_start(void) {
	/* This function starts Timer 1
	 * @param: None
	 * @return: None
	 */
	TIMER1->TCSR |= (1UL << 30);
} // end TIMER1_start(void)

void TIMER1_stop(void) {
	/* This function stops Timer 1
	 * @param: None
	 * @return: None
	 */
	
	// Stop Counting
	TIMER1->TCSR &= ~(1UL << 30);
} // end void TIMER1_stop(void)

void TIMER1_reset(void) {
	/* This function resets Timer 1
	 * @param: None
	 * @return: None
	 */
	
	// Stop Counting, Reset Prescale Count and Timer Count to 0
	TIMER1->TCSR |= (1UL << 26);
} // end TIMER1_reset(void)

void LED5_toggle(void) {
	/* This function toggles onboard LED5
	 * @param: None
	 * @return: None
	 */
	
	PC->DOUT ^= (1UL << 12);
} // end LED5_toggle(void) 

void LED8_toggle(void) {
	/* This function toggles onboard LED8
	 * @param: None
	 * @return: None
	 */
	
	PC->DOUT ^= (1UL << 15);
} // end LED8_toggle(void) 

void clear_pending_TMR0(void) {
	/* This function clear the Interrupt Request from TIMER 0
	 * after the Request has been handled
	 * @param: None
	 * @return: None
	 */
	
	TIMER0->TISR |= (1UL << 0);
} // end void clear_pending_TMR0(void)

void clear_pending_TMR1(void) {
	/* This function clear the Interrupt Request from TIMER 1
	 * after the Request has been handled
	 * @param: None
	 * @return: None
	 */
	
	TIMER1->TISR |= (1UL << 0);
} // end void clear_pending_TMR1(void)

void clear_pending_GPAB_INT(void) {
	PA->ISRC |= (0xFFFFUL << 0); // Clear all Interrupt Source Flags from GPA
	PB->ISRC |= (0xFFFFUL << 0); // Clear all Interrupt Source Flags from GPB
} // end clear_pending_GPAB_INT(void)
/*-------------------------Function Definitions End--------------------------------*/
