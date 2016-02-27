/******************************************************************************
* File:    isr.h
* Purpose: Define interrupt service routines referenced by the vector table.
* Note: Only "vectors.c" should include this header file.
******************************************************************************/

#ifndef __ISR_H
#define __ISR_H 1


/* Example */
/*
#undef  VECTOR_101
#define VECTOR_101 lpt_isr


// ISR(s) are defined in your project directory.
extern void lpt_isr(void);
*/
extern void uart0_isr(void);

#undef  VECTOR_028
#define VECTOR_028 uart0_isr

#endif  //__ISR_H

/* End of "isr.h" */
