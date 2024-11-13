
#ifndef _HTTP_CLIENT_STRING
#define _HTTP_CLIENT_STRING

#include "wrapper.h" // Cross platform support
#include "client.h" 
#include "common.h"

///////////////////////////////////////////////////////////////////////////////
//
// Section      : HTTP Api global definitions
// Last updated : 01/09/2005
//
///////////////////////////////////////////////////////////////////////////////

BOOL                    HTTPStrInsensitiveCompare   (char *pSrc, char* pDest, uint32_t nLength);
BOOL                    HTTPStrSearch               (char *pSrc, char *pSearched, uint32_t nOffset, uint32_t nScope,HTTP_PARAM *HttpParam);
char                    HTTPStrExtract              (char *pParam,uint32_t nOffset,char Restore);
char                    *HTTPStrCaseStr              (char *pSrc, uint32_t nSrcLength, char *pFind); 
char*                   HTTPStrGetToken             (char *pSrc, uint32_t nSrcLength, char *pDest, uint32_t *nDestLength);
uint32_t                  HTTPStrGetDigestToken       (HTTP_PARAM pParamSrc, char *pSearched, HTTP_PARAM *pParamDest);
uint32_t                  HTTPStrHToL                 (char * s); 
char*                   HTTPStrLToH                 (char * dest,uint32_t nSrc);        
#endif
