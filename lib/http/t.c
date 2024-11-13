
#include <stdio.h>
#include <string.h>
#include "auth.h"

void HTTPDigestCalcHA1(
                       IN int    nAlg,     /* 0 = MD5, 1 = MD5-Sess */
                       IN char * pszUserName,
                       IN char * pszRealm,
                       IN int    nRealmLength,
                       IN char * pszPassword,
                       IN char * pszNonce,
                       IN int    nNonceLength,
                       IN char * pszCNonce,
                       OUT HASHHEX SessionKey
                       )
{
    MD5_CTX Md5Ctx;
    HASH    HA1;
    HASHHEX HASess;
    HASH    HAll;

    HTTPMD5Init(&Md5Ctx);
    HTTPMD5Update(&Md5Ctx, (const unsigned char *)pszUserName, strlen(pszUserName)); //Daniel casting
    HTTPMD5Update(&Md5Ctx, (const unsigned char *)":", 1);
    HTTPMD5Update(&Md5Ctx, (const unsigned char *)pszRealm, nRealmLength); //Daniel
    HTTPMD5Update(&Md5Ctx, (const unsigned char *)":", 1);
    HTTPMD5Update(&Md5Ctx, (const unsigned char *)pszPassword, strlen(pszPassword)); //Daniel
    HTTPMD5Final((unsigned char *)HA1, &Md5Ctx);
}
