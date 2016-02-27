/*!
 * \file    llwu_common.h
 * \brief   common LLWU defines
 *
 * This file defines the functions/interrupt handlers/macros used for LLWU.
 * And some common function prototypes and initializations.
 *
 * \version $Revision: 1.1 $
 * \author  Philip Drake[rxaa60]
 */

#ifndef __LLWU_COMMON_H__
#define __LLWU_COMMON_H__

/* 
 * Misc. Defines
 */
#define LLWU_LPTMR_ME   0x01
#define LLWU_CMP0_ME   0x02
#define LLWU_CMP1_ME   0x04
#define LLWU_CMP2_ME   0x08
#define LLWU_TSI_ME   0x10
#define LLWU_RTCA_ME   0x20
#define LLWU_RESRV_ME   0x40
#define LLWU_RTCS_ME   0x80

#define LLWU_PIN_DIS 0
#define LLWU_PIN_RISING 1
#define LLWU_PIN_FALLING 2
#define LLWU_PIN_ANY 3

#define LLWU_WUF5		0x00000001
#define LLWU_WUF6		0x00000002
#define LLWU_WUF7		0x00000004
#define LLWU_WUF8		0x00000008
#define LLWU_WUF9		0x00000010
#define LLWU_WUF10	0x00000020
#define LLWU_WUF11	0x00000040
#define LLWU_WUF12	0x00000080
#define LLWU_WUF13	0x00000100
#define LLWU_WUF14	0x00000200
#define LLWU_WUF15	0x00000400
#define LLWU_MWUF0	0x00000800

extern uint32_t llwuSource;
/* 
 * Function prototypes
 */

void llwu_configure(unsigned int pin_en, unsigned char rise_fall, unsigned char module_en);
void llwu_isr(void);
void llwu_configure_filter(unsigned int wu_pin_num, unsigned char filter_en, unsigned char rise_fall ); 

#endif /* __LLWU_COMMON_H__ */
