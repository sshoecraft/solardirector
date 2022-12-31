
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "utils.h"

#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#ifndef __WIN32
#include <pwd.h>
#else
#include <windows.h>
#include <shlobj.h>
#endif

int gethomedir(char *dest, int dest_len) {
#ifndef __WIN32
	struct passwd *pw;

	pw = getpwuid(getuid());
	if (!pw) return 1;

	*dest = 0;
	strncat(dest,pw->pw_dir,dest_len);
	return 0;
#else
#if 0
HRESULT SHGetKnownFolderPath(
  REFKNOWNFOLDERID rfid,
  DWORD            dwFlags,
  HANDLE           hToken,
  PWSTR            *ppszPath
);
#endif
//	WCHAR path[MAX_PATH];
//	if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, path))) {
	char path[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, 0, path))) {
		strncpy(dest,path,dest_len);
		return 0;
	} else {
		return 1;
	}
#endif
}

#ifdef GEHOMEDIR_DIRECT
int gethomedir(long uid, char *dest, int destlen) {
	FILE *fp;
	char line[256],*p,*s;
	int len,id,i;

	fp = fopen("/etc/passwd","r");
	if (!fp) {
		perror("fopen /etc/passwd");
		return 1;
	}
	while(fgets(line,sizeof(line)-1,fp)) {
		len = strlen(line);
		if (!len) continue;
		while(len > 1 && isspace(line[len-1])) len--;
		dprintf(1,"line[%d]: %d\n", len, line[len]);
		line[len] = 0;
		dprintf(1,"line: %s\n", line);
		p = line;
		for(i=0; i < 2; i++) {
			p = strchr(p+1,':');
			if (!p) goto getmyhomedir_error;
			dprintf(1,"p(%d): %s\n", i, p);
		}
		s = p+1;
		p = strchr(s,':');
		if (p) *p = 0;
		dprintf(1,"s: %s\n", s);
		id = atoi(s);
		if (id == uid) {
			for(i=0; i < 2; i++) {
				p = strchr(p+1,':');
				if (!p) goto getmyhomedir_error;
				dprintf(1,"p(%d): %s\n", i, p);
			}
			s = p+1;
			p = strchr(s,':');
			if (p) *p = 0;
			dprintf(1,"s: %s\n", s);
			dest[0] = 0;
			strncat(dest,s,destlen);
			len = strlen(dest);
			if (dest[len] != '/') strcat(dest,"/");
			fclose(fp);
			return 0;
		}
	}
getmyhomedir_error:
	fclose(fp);
	return 1;
}
#endif
