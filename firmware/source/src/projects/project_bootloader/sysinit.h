/*
 * File:        sysinit.h
 * Purpose:     Kinetis L Family Configuration
 *              Initializes clock, abort button, clock output, and UART to a default state
 *
 * Notes:
 *
 */

/********************************************************************/

extern int mcg_clk_hz;
extern int mcg_clk_khz;
extern int core_clk_khz;
extern int periph_clk_khz;
extern int pll_clk_khz;
extern int uart0_clk_khz;


// function prototypes
void sysinit (void);
void enable_abort_button(void);
void clk_out_init(void);
/********************************************************************/
