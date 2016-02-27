

#ifndef __STRING_PARSER_H__
#define __STRING_PARSER_H__

#define STR_PARSER_SIZE	32
#define STR_PARSER_COUNT(x)   (sizeof (x) / sizeof (x[0]))

/*String Info  definitions structure. */
typedef struct  {
   char strInfo[STR_PARSER_SIZE];
   char (*func)(char c);
} STR_INFO_Type;

typedef enum{
	STR_NEW_STATE,
	STR_WAIT_FINISH,
	STR_FINISH
}STR_STATE_Type;


typedef struct  {
   STR_STATE_Type state;
	 uint16_t dataRecvLength;
	 uint16_t dataRecvMaxLength;
	 uint8_t strParserCntMax;
	 uint8_t strInProcess;
	 uint32_t timeout;
	 uint32_t timeoutSet;
} STR_PARSER_Type;

void StrParserInit(STR_PARSER_Type *process,uint8_t *numCharParsed,uint8_t strParserCntMax,uint16_t dataLengthMax);
void StrComnandParser(const STR_INFO_Type *info,STR_PARSER_Type *process,uint8_t *numCharParsed,uint8_t c);

#endif
