
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#ifndef _BSD_SOURCE
# define _BSD_SOURCE 1
#endif
#ifndef _DEFAULT_SOURCE
# define _DEFAULT_SOURCE 1
#endif

#include "common.h"
#ifdef WINDOWS
#include <wininet.h>
#else
#include <netdb.h>
extern int h_errno;
#include <arpa/inet.h>
#endif
#include <ctype.h>

#ifdef WINDOWS
#if 0
Syntax
C++

Copy
DNS_STATUS DnsQuery_A(
  [in]                PCSTR       pszName,
  [in]                WORD        wType,
  [in]                DWORD       Options,
  [in, out, optional] PVOID       pExtra,
  [out, optional]     PDNS_RECORD *ppQueryResults,
  [out, optional]     PVOID       *pReserved
);
Parameters
[in] pszName

A pointer to a string that represents the DNS name to query.

[in] wType

A value that represents the Resource Record (RR)DNS Record Type that is queried. wType determines the format of data pointed to by ppQueryResultsSet. For example, if the value of wType is DNS_TYPE_A, the format of data pointed to by ppQueryResultsSet is DNS_A_DATA.

[in] Options

A value that contains a bitmap of DNS Query Options to use in the DNS query. Options can be combined and all options override DNS_QUERY_STANDARD.

[in, out, optional] pExtra

This parameter is reserved for future use and must be set to NULL.

[out, optional] ppQueryResults

Optional. A pointer to a pointer that points to the list of RRs that comprise the response. For more information, see the Remarks section.

[out, optional] pReserved

This parameter is reserved for future use and must be set to NULL.

Return value
Returns success confirmation upon successful completion. Otherwise, returns the appropriate DNS-specific error code as defined in Winerror.h.

#include <winsock2.h> //winsock
#include <windns.h> //DNS api's
#include <stdio.h> //standard i/o

//Usage of the program
void Usage(char *progname) {
    fprintf(stderr,"Usage\n%s -n [HostName|IP Address] -t [Type] -s [DnsServerIp]\n",progname);
    fprintf(stderr,"Where:\n\t\"HostName|IP Address\" is the name or IP address of the computer ");
    fprintf(stderr,"of the record set being queried\n");
    fprintf(stderr,"\t\"Type\" is the type of record set to be queried A or PTR\n");
    fprintf(stderr,"\t\"DnsServerIp\"is the IP address of DNS server (in dotted decimal notation)");
    fprintf(stderr,"to which the query should be sent\n");
    exit(1);
}

void ReverseIP(char* pIP)
{
    char seps[] = ".";
    char *token;
    char pIPSec[4][4];
    int i=0;
    token = strtok( pIP, seps);
    while( token != NULL )
    {
        /* While there are "." characters in "string"*/
        sprintf(pIPSec[i],"%s", token);
        /* Get next "." character: */
        token = strtok( NULL, seps );
        i++;
    }
    sprintf(pIP,"%s.%s.%s.%s.%s", pIPSec[3],pIPSec[2],pIPSec[1],pIPSec[0],"IN-ADDR.ARPA");
}

// the main function 
void __cdecl main(int argc, char* argv[])
{
    DNS_STATUS status; //Return value of DnsQuery_A() function.
    PDNS_RECORD pDnsRecord; //Pointer to DNS_RECORD structure.
    PIP4_ARRAY pSrvList = NULL; //Pointer to IP4_ARRAY structure.
    WORD wType; //Type of the record to be queried.
    char* pOwnerName; //Owner name to be queried.
    char pReversedIP[255];//Reversed IP address.
    char DnsServIp[255]; //DNS server ip address.
    DNS_FREE_TYPE freetype;
    freetype = DnsFreeRecordListDeep;
    IN_ADDR ipaddr;

    if (argc > 4)
    {
        for (int i = 1; i < argc; i++)
        {
            if ((argv[i][0] == '-') || (argv[i][0] == '/'))
            {
                switch (tolower(argv[i][1]))
                {
                    case 'n':
                        pOwnerName = argv[++i];
                        break;
                    case 't':
                        if (!stricmp(argv[i + 1], "A"))
                            wType = DNS_TYPE_A; //Query host records to resolve a name.
                        else if (!stricmp(argv[i + 1], "PTR"))
                        {
                            //pOwnerName should be in "xxx.xxx.xxx.xxx" format
                            if (strlen(pOwnerName) <= 15)
                            {
                                //You must reverse the IP address to request a Reverse Lookup 
                                //of a host name.
                                sprintf(pReversedIP, "%s", pOwnerName);
                                ReverseIP(pReversedIP);
                                pOwnerName = pReversedIP;
                                wType = DNS_TYPE_PTR; //Query PTR records to resolve an IP address
                            }
                            else
                            {
                                Usage(argv[0]);
                            }
                        }
                        else
                            Usage(argv[0]);
                        i++;
                        break;

                    case 's':
                        // Allocate memory for IP4_ARRAY structure.
                        pSrvList = (PIP4_ARRAY)LocalAlloc(LPTR, sizeof(IP4_ARRAY));
                        if (!pSrvList)
                        {
                            printf("Memory allocation failed \n");
                            exit(1);
                        }
                        if (argv[++i])
                        {
                            strcpy(DnsServIp, argv[i]);
                            pSrvList->AddrCount = 1;
                            pSrvList->AddrArray[0] = inet_addr(DnsServIp); //DNS server IP address
                            break;
                        }

                    default:
                        Usage(argv[0]);
                        break;
                }
            }
            else
                Usage(argv[0]);
        }
    }
    else
        Usage(argv[0]);

    // Calling function DnsQuery to query Host or PTR records 
    status = DnsQuery(pOwnerName, //Pointer to OwnerName. 
    wType, //Type of the record to be queried.
    DNS_QUERY_BYPASS_CACHE, // Bypasses the resolver cache on the lookup. 
    pSrvList, //Contains DNS server IP address.
    &pDnsRecord, //Resource record that contains the response.
    NULL); //Reserved for future use.

    if (status)
    {
        if (wType == DNS_TYPE_A)
            printf("Failed to query the host record for %s and the error is %d \n", pOwnerName, status);
        else
            printf("Failed to query the PTR record and the error is %d \n", status);
    }
    else
    {
        if (wType == DNS_TYPE_A)
        {
            //convert the Internet network address into a string
            //in Internet standard dotted format.
            ipaddr.S_un.S_addr = (pDnsRecord->Data.A.IpAddress);
            printf("The IP address of the host %s is %s \n", pOwnerName, inet_ntoa(ipaddr));

            // Free memory allocated for DNS records. 
            DnsRecordListFree(pDnsRecord, freetype);
        }
        else
        {
            printf("The host name is %s \n", (pDnsRecord->Data.PTR.pNameHost));

            // Free memory allocated for DNS records. 
            DnsRecordListFree(pDnsRecord, freetype);
        }
    }
    LocalFree(pSrvList);
}
#endif
int os_gethostbyname(char *dest, int destsize, char *name) {
	struct hostent *he;
	unsigned char *ptr;
	int r;

	r = 0;
	he = gethostbyname(name);
	if (!he) {
		DWORD dwError = WSAGetLastError();
		r = 1;
		if (dwError != 0) {
			if (dwError == WSAHOST_NOT_FOUND) {
				r = 2;
			} else if (dwError == WSANO_DATA) {
				r = 3;
			}
		}
		return r;
	}

	dprintf(1,"he->h_addr: %p\n", he->h_addr);
	if (!he->h_addr) return 2;

	/* XXX.XXX.XXX.XXX + NULL */
	if (destsize < 16) return 1;

	ptr = (unsigned char *) he->h_addr;
	sprintf(dest,"%d.%d.%d.%d",ptr[0],ptr[1],ptr[2],ptr[3]);
	return 0;
}
#else
#if 1
static int _os_gethostbyname(char *addr, int addrsize, char *name) {
	struct addrinfo *result;
	struct addrinfo *res;
	int error;

	*addr = 0;

	/* resolve the domain name into a list of addresses */
	error = getaddrinfo(name, NULL, NULL, &result);
	if (error != 0) {   
		if (error == EAI_SYSTEM) log_syserror("_os_gethostbyname: getaddrinfo");
		else log_error("_os_gethostbyname: getaddrinfo: %s\n", gai_strerror(error));
		return 1;
	}

	/* loop over all returned results */
	for (res = result; res!= NULL; res = res->ai_next) {
		if (res->ai_family != AF_INET) continue;
		if (!inet_ntop(res->ai_family,  &((struct sockaddr_in *)res->ai_addr)->sin_addr, addr, addrsize)) continue;
		break;
	}

	freeaddrinfo(result);
	return 0;
}
#else
int _os_gethostbyname(char *dest, int destsize, char *name) {
	struct hostent *he;
	unsigned char *ptr;
	int r;

	r = 0;
	he = gethostbyname(name);
	if (!he) {
		r = 1;
		return r;
	}

	dprintf(1,"he->h_addr: %p\n", he->h_addr);
	if (!he->h_addr) return 2;

	/* XXX.XXX.XXX.XXX + NULL */
	if (destsize < 16) return 1;

	ptr = (unsigned char *) he->h_addr;
	sprintf(dest,"%d.%d.%d.%d",ptr[0],ptr[1],ptr[2],ptr[3]);
	return 0;
}
#endif

static int _os_gethostbyname_init = 0;
static list nameservers;
static list domains;

static int do_init(void) {
	char line[4096],*s;
	FILE *fp;
	int i;
	register char *p;
	register int len;

	nameservers = list_create();
	domains = list_create();
	fp = fopen("/etc/resolv.conf","r");
	if (!fp) return 1;
	while(fgets(line,sizeof(line),fp)) {
		len = strlen(line);
//		if (len > 1) len--;
		p = line + len;
//		dprintf(1,"p: %c\n", *p);
		while(isspace(*p)) p--;
		if (!strlen(line)) continue;
//		dprintf(dlevel+1,"line: %s\n", line);
		p = strchr(line,' ');
		if (!p) continue;
		else *p = 0;
		dprintf(dlevel,"line: %s\n", line);
		if (strcasecmp(line,"search") == 0 || strcmp(line,"domain") == 0) {
			s = p + 1;
			i = 0;
			while(1) {
				p = strele(i++," ",s);
				if (!strlen(p)) break;
				dprintf(dlevel,"adding: %s\n", p);
				list_add(domains,p,strlen(p)+1);
			}
		}
	}
	fclose(fp);
	_os_gethostbyname_init = 1;
	return 0;
}

int os_gethostbyname(char *addr, int addrsize, char *name) {
	char temp[128];
	register char *p;

	dprintf(dlevel,"name: %s\n", name);
	if (strcmp(name,"localhost") == 0) {
		strncpy(addr,"127.0.0.1",addrsize);
		return 0;
	}

	if (is_ip(name)) {
		strncpy(addr,name,addrsize);
		return 0;
	}

	if (!_os_gethostbyname_init) do_init();
	p = name + strlen(name);
	while(*p == 0 && p > name) p--;
	dprintf(dlevel,"p: %c\n", *p);
	if (*p == '.') return _os_gethostbyname(addr,addrsize,name);
	list_reset(domains);
	while((p = list_get_next(domains)) != 0) {
		snprintf(temp,sizeof(temp)-1,"%s.%s",name,p);
		if (_os_gethostbyname(addr,addrsize,temp) == 0) return 0;
	}
	return 1;
}
#endif

#if 0
remoteHost = gethostbyname(host_name);
    
    if (remoteHost == NULL) {
        dwError = WSAGetLastError();
        if (dwError != 0) {
            if (dwError == WSAHOST_NOT_FOUND) {
                printf("Host not found\n");
                return 1;
            } else if (dwError == WSANO_DATA) {
                printf("No data record found\n");
                return 1;
            } else {
                printf("Function failed with error: %ld\n", dwError);
                return 1;
            }
        }
    } else {
        printf("Function returned:\n");
        printf("\tOfficial name: %s\n", remoteHost->h_name);
        for (pAlias = remoteHost->h_aliases; *pAlias != 0; pAlias++) {
            printf("\tAlternate name #%d: %s\n", ++i, *pAlias);
        }
        printf("\tAddress type: ");
        switch (remoteHost->h_addrtype) {
        case AF_INET:
            printf("AF_INET\n");
            break;
        case AF_NETBIOS:
            printf("AF_NETBIOS\n");
            break;
        default:
            printf(" %d\n", remoteHost->h_addrtype);
            break;
        }
        printf("\tAddress length: %d\n", remoteHost->h_length);

        i = 0;
        if (remoteHost->h_addrtype == AF_INET)
        {
            while (remoteHost->h_addr_list[i] != 0) {
                addr.s_addr = *(u_long *) remoteHost->h_addr_list[i++];
                printf("\tIP Address #%d: %s\n", i, inet_ntoa(addr));
            }
        }
        else if (remoteHost->h_addrtype == AF_NETBIOS)
        {   
            printf("NETBIOS address was returned\n");
        }   
    }
}
#endif
