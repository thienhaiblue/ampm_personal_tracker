 #include "common.h"


void SpSub(void)
{
		/* Launch command */
		FTFA_FSTAT |= FTFA_FSTAT_CCIF_MASK;    
		/* wait for command completion */
		while (!(FTFA_FSTAT & FTFA_FSTAT_CCIF_MASK)) {};
}
		
