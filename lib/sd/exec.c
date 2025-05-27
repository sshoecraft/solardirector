
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

#ifndef __WIN32
#include <sys/wait.h>
#include <signal.h>
#define OPTS (O_WRONLY | O_CREAT | O_TRUNC | O_SYNC)
#else
#include <windows.h>
#define OPTS (O_WRONLY | O_CREAT | O_TRUNC)
#endif
#include "common.h"

int solard_kill(int pid) {
	int r;

	dprintf(dlevel,"pid: %d\n", pid);

#ifndef __WIN32
	r = kill(pid,SIGTERM);
	dprintf(dlevel,"r: %d\n", r);
	return (r == 0 ? 0 : 1);
#else
	HANDLE h;

	h = OpenProcess(PROCESS_ALL_ACCESS,0,pid);
	r = TerminateProcess(h,1);
	CloseHandle(h);
	dprintf(dlevel,"r: %d\n", r);
	return (r == 0 ? 1 : 0);
#endif
}

int solard_wait(int pid) {
#ifndef __WIN32
	int status;

	dprintf(dlevel,"pid: %d\n", pid);

	/* Wait for it to finish */
	do {
		if (waitpid(pid,&status,0) < 0) return -1;
		dprintf(dlevel,"WIFEXITED: %d\n", WIFEXITED(status));
	} while(!WIFEXITED(status));

	/* Get exit status */
	status = WEXITSTATUS(status);
	dprintf(dlevel,"status: %d\n", status);
	return status;
#else
	DWORD ExitCode;
	HANDLE h; 

	dprintf(dlevel,"pid: %d\n", pid);

	/* Get process handle */
	h = OpenProcess(PROCESS_ALL_ACCESS,0,pid);

	/* Wait for it to complete */
	WaitForSingleObject( h, INFINITE );

	/* Get the status */
	if (GetExitCodeProcess(h, &ExitCode) == 0) return -1;

	/* Return status */
	return (int)ExitCode;
#endif
}

#ifdef __WIN32
LPTSTR GetLastErrorText( LPTSTR lpszBuf, DWORD dwSize ) {
	DWORD dwRet;
	LPTSTR lpszTemp = NULL;

	dwRet = FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_ARGUMENT_ARRAY,
			NULL,
			GetLastError(),
			LANG_NEUTRAL,
			(LPTSTR)&lpszTemp,
			0,
			NULL);

	// supplied buffer is not long enough
	if ( !dwRet || ( (long)dwSize < (long)dwRet+14 ) )
		lpszBuf[0] = TEXT('\0');
	else {
		lpszTemp[lstrlen(lpszTemp)-2] = TEXT('\0');  //remove cr and newline character
		sprintf( lpszBuf, TEXT("%s (0x%x)"), lpszTemp, (int)GetLastError() );
	}

	if ( lpszTemp ) LocalFree((HLOCAL) lpszTemp );
	return lpszBuf;
}

char *GetErrorText(void) {
	static char szErr[256];
	GetLastErrorText(szErr, sizeof(szErr));
	return szErr;
}
#endif

int solard_exec(char *prog, char *args[], char *log, int wait) {
#ifndef __WIN32
	int logfd;
	pid_t pid;
	int i,status;

	dprintf(dlevel,"prog: %s, args: %p, log: %s, wait: %d\n", prog, args, log, wait);
	if (log) {
		/* Open the logfile */
		logfd = open(log,OPTS, 0644);
		if (logfd < 0) {
			log_write(LOG_SYSERR,"solard_exec: open(%s)",log);
			return -1;
		}
	}

#if DEBUG
	for(i=0; args[i]; i++) dprintf(dlevel,"args[%d]: %s\n", i, args[i]);
#endif

	/* Fork the process */
	pid = fork();
	dprintf(dlevel,"pid: %d\n", pid);

	/* If pid < 0, error */
	if (pid < 0) {
		log_write(LOG_SYSERR,"fork");
		return -1;

#if 1
	/* Child */
	} else if (pid == 0) {
#if 0
		/* XXX causes MQTT connect to hang?? */
		/* Close stdin */
		close(0);
#endif

		if (log) {
			/* Make logfd stdout */
			dup2(logfd, STDOUT_FILENO);
			close(logfd);

			/* Redirect stderr to stdout */
			dup2(1, 2);
		}

		/* Run the program */
		execvp(prog,args);
		log_syserror("solard_exec: execvp");
		_exit(1);
#endif

	/* Parent */
	} else if (pid > 0) {
		/* Close the log in the parent */
		if (log) close(logfd);
		if (wait) {
			/* Wait for it to finish */
			do {
				if (waitpid(pid,&status,0) < 0) {
					kill(pid,SIGKILL);
					return -1;
				}
				dprintf(dlevel,"WIFEXITED: %d\n", WIFEXITED(status));
			} while(!WIFEXITED(status));

			/* Get exit status */
			status = WEXITSTATUS(status);
			dprintf(dlevel,"status: %d\n", status);

			/* Return status */
			return status;
		}
		return pid;
	}

#if 0
	/* Close stdin */
	close(0);

	if (log) {
		/* Make logfd stdout */
		dup2(logfd, STDOUT_FILENO);
		close(logfd);

		/* Redirect stderr to stdout */
		dup2(1, 2);
	}

	/* Run the program */
//	exit(execv(prog,args));
	_exit(execvp(prog,args));
#endif
	return 1;
#else
	HANDLE hLog;
	PROCESS_INFORMATION pi;
	STARTUPINFO StartupInfo = {0};
	SECURITY_ATTRIBUTES sa	= {0};
	char CommandLine[1024];
	register char **pp;
	DWORD dwPid;

	dprintf(dlevel,"prog: %s, args: %p, log: %s, wait: %d\n", prog, args, log, wait);

	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	if (log) {
		hLog = CreateFile ( log,
			  GENERIC_WRITE,	// access
			  FILE_SHARE_READ,	// share mode
			  &sa,				// security attrib
			  CREATE_ALWAYS,	// create mode
			  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
			  NULL);

		if (hLog == INVALID_HANDLE_VALUE) {
			dprintf(dlevel, "CreateFile(log): %s\n", GetErrorText());
			return (0);
		}
	} else hLog = INVALID_HANDLE_VALUE;

	*CommandLine = 0;
	for(pp = args; *pp; pp++) {
		if (strlen(CommandLine)) strcat(CommandLine," ");
		dprintf(dlevel,"adding: %s\n", *pp);
		strcat(CommandLine,*pp);
	}
//	replacevar(CommandLine);
	fixpath(CommandLine,sizeof(CommandLine)-1);
	dprintf(dlevel,"CommandLine: %s\n", CommandLine);

	memset(&StartupInfo.cb,0,sizeof(StartupInfo.cb));
	StartupInfo.cb = sizeof(STARTUPINFO);
	StartupInfo.dwFlags     = STARTF_USESTDHANDLES;
	StartupInfo.hStdInput	= INVALID_HANDLE_VALUE;
	StartupInfo.hStdOutput  = hLog;
	StartupInfo.hStdError   = hLog;
	dwPid = -1;
	if (CreateProcess(
			NULL,			// app. module name
			CommandLine,		// command line
			NULL,			// proc. sec. attrib.
			NULL,			// thread sec. attrib.
			TRUE,			// inherit handle
			0,			// creation flag
			NULL,			// env block
			NULL,			// current directory
			&StartupInfo,
			&pi)) {
		dprintf(dlevel, "procress created...\n");
  		dwPid = GetProcessId(pi.hProcess);
		CloseHandle(hLog);		// safe to get rid off log handle now
		if (wait) {
			DWORD ExitCode;

			/* Wait for it to complete */
			WaitForSingleObject( pi.hProcess, INFINITE );

			/* Get the status */
			if (GetExitCodeProcess(pi.hProcess, &ExitCode) == 0)
				return -1;

			/* Return status */
			return (int)ExitCode;
		}
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	} else {
		dprintf(dlevel, "CreateProcess: %s\n", GetErrorText());
		return -1;
	}

	return (int)dwPid;
#endif
}

int solard_checkpid(int pid, int *exitcode) {
#ifndef __WIN32
	int status;

	dprintf(dlevel,"pid: %d\n", pid);

	if (waitpid(pid, &status, WNOHANG) != 0) {
		/* Get exit status */
		dprintf(dlevel,"WIFEXITED: %d\n", WIFEXITED(status));
		if (WIFEXITED(status)) dprintf(dlevel,"WEXITSTATUS: %d\n", WEXITSTATUS(status));
		status = (WIFEXITED(status) ? WEXITSTATUS(status) : 1);
		dprintf(dlevel,"status: %d\n", status);
		*exitcode = status;
		return 1;
	}
	return 0;
#else
	HANDLE h;
	int r;
	DWORD Status;

	h = OpenProcess(PROCESS_ALL_ACCESS,0,pid);
	r = GetExitCodeProcess(h, &Status);
	*exitcode = Status;
	CloseHandle(h);
	dprintf(dlevel,"r: %d\n", r);
	return (r == 0 ? 1 : 0);
#endif
}

int solard_pidofname(char *name) {
	DIR *dirp;
	struct dirent *ent;
	int pid = -1;
	FILE *fp;
	char status_path[292],line[256],*pname;

	dprintf(dlevel,"name: %s\n", name);

	// Open the /proc directory to scan for processes
	dirp = opendir("/proc");
	dprintf(dlevel+1,"dirp: %p\n", dirp);
	if (!dirp) {
		log_syserror("solard_getpid: unable to opendir /proc");
		return -1;
	}

	// Loop through all entries in /proc
	while ((ent = readdir(dirp)) != 0) {
		// Check if the entry is a number (i.e., a process directory)
		if (ent->d_type == DT_DIR && isdigit(ent->d_name[0])) {
			// Construct the path to the status file
			snprintf(status_path, sizeof(status_path), "/proc/%s/status", ent->d_name);
			dprintf(dlevel+1,"status_path: %s\n", status_path);

			// Open the status file for this process
			fp = fopen(status_path, "r");
			dprintf(dlevel+1,"fp: %p\n", fp);
			if (!fp) continue;

			// Read the process name from the status file
			if (fgets(line, sizeof(line), fp) != 0) {
				dprintf(dlevel+1,"line: %s\n", line);
				// The process name is the second token in the first line
				if (strncmp(line, "Name:", 5) == 0) {
					// Compare the process name with the binary name
					pname = trim(strele(1," ",line));
					dprintf(dlevel+1,"pname: %s\n", pname);
					if (strcmp(pname,name) == 0) {
						dprintf(dlevel+1,"found!\n");
						pid = atoi(ent->d_name);
					}
				}
			}
			fclose(fp);
		}
		if (pid >= 0) break;
	}

	closedir(dirp);
	dprintf(dlevel,"pid: %d\n", pid);
	return pid;
}

int solard_pidofpath(char *path) {
	DIR *dirp;
	struct dirent *ent;
	int pid = -1;
	char link_path[292],process_path[1024];
	ssize_t len;

	dprintf(dlevel,"path: %s\n", path);

	// Open the /proc directory to scan for processes
	dirp = opendir("/proc");
	dprintf(dlevel+1,"dirp: %p\n", dirp);
	if (!dirp) {
		log_syserror("solard_getpid: unable to opendir /proc");
		return -1;
	}

	// Loop through all entries in /proc
	while ((ent = readdir(dirp)) != 0) {
		// Check if the entry is a number (i.e., a process directory)
		if (ent->d_type == DT_DIR && isdigit(ent->d_name[0])) {
			// Construct the path to the /proc/[pid]/exe symlink
			snprintf(link_path, sizeof(link_path), "/proc/%s/exe", ent->d_name);
			dprintf(dlevel+1,"link_path: %s\n", link_path);

			// Read the symlink to get the full executable path
			len = readlink(link_path, process_path, sizeof(process_path) - 1);
			if (len < 0) continue;

			// Null-terminate the path
			process_path[len] = '\0';
			dprintf(dlevel+1,"process_path: %s\n", process_path);

			// Compare the process path with the desired path
			if (strcmp(process_path, path) == 0) {
				dprintf(dlevel,"found!\n");
				pid = atoi(ent->d_name);
			}
		}
		if (pid >= 0) break;
	}

	closedir(dirp);
	dprintf(dlevel,"pid: %d\n", pid);
	return pid;
}
