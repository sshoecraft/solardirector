
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#ifdef WINDOWS
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#define USE_THREADS 0
#else
#include <sys/signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define USE_THREADS 0
#endif
#include "rdevserver.h"

#if USE_THREADS
struct rdevctx {
	int sock;
	devserver_config_t *conf;
};

static void *call_rdevserver(void *ctx) {
	struct rdevctx *r = ctx;
	devserver_config_t *newconf;

	dprintf(1,"copying conf...\n");
	newconf = malloc(sizeof(*newconf));
	if (!newconf) {
		log_syserror("call_rdevserver: malloc");
		return 0;
	}
	memcpy(newconf,r->conf,sizeof(*newconf));
	dprintf(1,"calling devserver...\n");
	devserver(newconf, r->sock);
	dprintf(1,"freeing newconf...\n");
	free(newconf);
	return 0;
}
#endif

int server(rdev_config_t *conf) {
	struct sockaddr_in sin;
	socklen_t sin_size;
	int c,status;
	socket_t s;
	char addr[32];
	unsigned char *ptr;
#if USE_THREADS
	struct rdevctx ctx;
	pthread_attr_t attr;
	pthread_t th;
#else
	pid_t pid;
#endif
#ifndef __WIN32
	sigset_t set;

	/* Ignore SIGPIPE */
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	sigprocmask(SIG_BLOCK, &set, NULL);
	signal(SIGCHLD, SIG_IGN);
#endif

	dprintf(1,"opening socket...\n");
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)  {
		log_write(LOG_SYSERR,"socket");
		goto rdev_server_error;
	}
	status = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const void *)&status, sizeof(status)) < 0) {
		log_write(LOG_SYSERR,"setsockopt");
		goto rdev_server_error;
	}
	sin_size = sizeof(sin);
	memset(&sin,0,sin_size);

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	dprintf(1,"port: %d\n", conf->port);
	sin.sin_port = htons((unsigned short)conf->port);

	dprintf(1,"binding...\n");
	if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0)  {
		log_write(LOG_SYSERR,"bind");
		goto rdev_server_error;
	}
	dprintf(1,"listening...\n");
	if (listen(s, 5) < 0) {
		log_write(LOG_SYSERR,"listen");
		goto rdev_server_error;
	}
	while(check_state(conf,RDEVSERVER_RUNNING)) {
		dprintf(1,"accepting...\n");
		c = accept(s,(struct sockaddr *)&sin,&sin_size);
		if (c < 0) {
			log_write(LOG_SYSERR,"accept");
			goto rdev_server_error;
		}
		dprintf(1,"c: %d\n", c);
		ptr = (unsigned char *) &sin.sin_addr.s_addr;
		sprintf(addr,"%d.%d.%d.%d", ptr[0],ptr[1],ptr[2],ptr[3]);
		log_info("Connection from %s\n", addr);
#if USE_THREADS
		/* Create a detached thread */
		dprintf(1,"init attr...\n");
		if (pthread_attr_init(&attr)) {
			log_error("pthread_attr_init: %s",strerror(errno));
			continue;
		}
		dprintf(1,"setting attr...\n");
		if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) {
			log_error("pthread_attr_setdetachstate: %s",strerror(errno));
			pthread_attr_destroy(&attr);
			continue;
		}
		dprintf(1,"creating thread...\n");
		ctx.sock = c;
		ctx.conf = &conf->ds;
		if (pthread_create(&th,&attr,&call_rdevserver,&ctx)) {
			log_error("pthread_create: %s",strerror(errno));
			pthread_attr_destroy(&attr);
			continue;
		}
		dprintf(1,"thread created\n");
		pthread_attr_destroy(&attr);
#else
		pid = fork();
		if (pid < 0) {
			log_write(LOG_SYSERR,"fork");
			goto rdev_server_error;
		} else if (pid == 0) {
			dprintf(1,"pid: %d\n", pid);
			dprintf(1,"calling devserver...\n");
			_exit(devserver(conf,c));
		} else {
			dprintf(1,"pid: %d\n", pid);
			close(c);
#if 0
			dprintf(1,"waiting on pid...\n");
			waitpid(pid,&status,0);
			dprintf(1,"WIFEXITED: %d\n", WIFEXITED(status));
			if (WIFEXITED(status)) dprintf(1,"WEXITSTATUS: %d\n", WEXITSTATUS(status));
			status = (WIFEXITED(status) ? WEXITSTATUS(status) : 1);
			dprintf(1,"status: %d\n", status);
#endif
		}
#endif
	}
	return 0;
rdev_server_error:
	return 1;
}
