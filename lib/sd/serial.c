#ifdef __APPLE__
/* macOS version of serial driver */
#include "transports.h"
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "debug.h"

#define dlevel 7
#define DEFAULT_SPEED 9600

struct serial_session {
	int fd;
	char target[256];
	int speed,data,stop,parity,vmin,vtime;
};
typedef struct serial_session serial_session_t;

static int serial_read_direct(void *handle, uint32_t *control, void *buf, int buflen) {
	serial_session_t *s = handle;
	int bytes;
	struct timeval tv;
	fd_set rfds;
	int num;

	if (!buf || !buflen) return 0;

	/* See if there's anything to read */
	FD_ZERO(&rfds);
	FD_SET(s->fd,&rfds);
	tv.tv_usec = 500000;
	tv.tv_sec = 0;
	num = select(s->fd+1,&rfds,0,0,&tv);
	if (num < 1) {
		bytes = 0;
		goto serial_read_done;
	}

	/* Read */
	bytes = read(s->fd, buf, buflen);
	if (bytes < 0) bytes = 0;

serial_read_done:
	return bytes;
}

static void *serial_new(void *target, void *topts) {
	serial_session_t *s;
	char *p;

	s = calloc(1,sizeof(*s));
	if (!s) return 0;

	s->fd = -1;
	strncat(s->target,target ? (char*)target : "",sizeof(s->target)-1);

	/* TOPTS:  Baud, data, parity, stop, vmin, vtime */
	/* Defaults: 9600, 8, N, 1, 0, 5 */

	/* Baud rate */
	p = topts ? strele(0,",",(char*)topts) : "";
	s->speed = atoi(p);
	if (!s->speed) s->speed = DEFAULT_SPEED;

	/* Data bits */
	p = topts ? strele(1,",",(char*)topts) : "";
	s->data = atoi(p);
	if (!s->data) s->data = 8;

	/* Parity */
	p = topts ? strele(2,",",(char*)topts) : "";
	if (strlen(p)) {
		switch(p[0]) {
		case 'N':
		default:
			s->parity = 0;
			break;
		case 'E':
			s->parity = 1;
			break;
		case 'O':
			s->parity = 2;
			break;
		}
	}

	/* Stop */
	p = topts ? strele(3,",",(char*)topts) : "";
	s->stop = atoi(p);
	if (!s->stop) s->stop = 1;

	/* vmin */
	p = topts ? strele(4,",",(char*)topts) : "";
	s->vmin = atoi(p);
	if (!s->vmin) s->vmin = 0;

	/* vtime */
	p = topts ? strele(5,",",(char*)topts) : "";
	s->vtime = atoi(p);
	if (!s->vtime) s->vtime = 5;

	return s;
}

static int set_interface_attribs(int fd, int speed, int data, int parity, int stop, int vmin, int vtime) {
	struct termios tty;
	int rate;

	// Don't block
	fcntl(fd, F_SETFL, FNDELAY);

	// Get current device tty
	tcgetattr(fd, &tty);

	/* Use raw mode */
	cfmakeraw(&tty);

	tty.c_cc[VMIN]  = vmin;
	tty.c_cc[VTIME] = vtime;

	/* Baud */
	switch(speed) {
	case 9600:      rate = B9600;    break;
	case 19200:     rate = B19200;   break;
	case 1200:      rate = B1200;    break;
	case 2400:      rate = B2400;    break;
	case 4800:      rate = B4800;    break;
	case 38400:     rate = B38400;   break;
	case 57600:     rate = B57600;   break;
	case 115200:	rate = B115200;   break;
	case 230400:	rate = B230400;   break;
	default:
		rate = B9600;
		break;
	}
	cfsetispeed(&tty, rate);
	cfsetospeed(&tty, rate);

	/* Data bits */
	switch(data) {
		case 5: tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS5; break;
		case 6: tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS6; break;
		case 7: tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS7; break;
		case 8: default: tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; break;
	}

	/* Stop bits (2 = 2, any other value = 1) */
	tty.c_cflag &= ~CSTOPB;
	if (stop == 2) tty.c_cflag |= CSTOPB;
	else tty.c_cflag &= ~CSTOPB;

	/* Parity (0=none, 1=even, 2=odd */
        tty.c_cflag &= ~(PARENB | PARODD);
	switch(parity) {
		case 1: tty.c_cflag |= PARENB; break;
		case 2: tty.c_cflag |= PARODD; break;
		default: break;
	}

	// do it
	tcsetattr(fd, TCSANOW, &tty);
	tcflush(fd,TCIOFLUSH);
	return 0;
}

static int serial_open(void *handle) {
	serial_session_t *s = handle;
	char path[256];

	strcpy(path,s->target);
	if ((access(path,0)) != 0) {
		if (strncmp(path,"/dev",4) != 0) {
			sprintf(path,"/dev/%s",s->target);
		}
	}
	s->fd = open(path, O_RDWR | O_NOCTTY);
	if (s->fd < 0) {
		return 1;
	}
	usleep(1000);
	set_interface_attribs(s->fd, s->speed, s->data, s->parity, s->stop, s->vmin, s->vtime);

	return 0;
}

static int serial_read(void *handle, uint32_t *control, void *buf, int buflen) {
	return serial_read_direct(handle,control,buf,buflen);
}

static int serial_write(void *handle, uint32_t *control, void *buf, int buflen) {
	serial_session_t *s = handle;
	int bytes;
	int bytes_left = buflen;
	do {
		bytes = write(s->fd,buf,bytes_left);
		if (bytes > 0) bytes_left -= bytes;
	} while(bytes >= 0 && bytes_left);
	return bytes;
}

static int serial_close(void *handle) {
	serial_session_t *s = handle;
	if (s->fd >= 0) {
		tcflush(s->fd, TCIFLUSH);
		close(s->fd);
		s->fd = -1;
	}
	return 0;
}

static int serial_config(void *h, int func, ...) {
	return 1;
}

static int serial_destroy(void *handle) {
	serial_session_t *s = handle;
        serial_close(s);
        free(s);
        return 0;
}

solard_driver_t serial_driver = {
	"serial",
	serial_new,
	serial_destroy,
	serial_open,
	serial_close,
	serial_read,
	serial_write,
	serial_config
};

#else
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 7
#include "debug.h"

/* Use the buffering functions */
#define USE_BUFFER 0

#include <fcntl.h> 
#include <time.h>
#ifdef WINDOWS
#include <windows.h>
#else
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/fcntl.h>
#endif
#include "buffer.h"
#include "transports.h"

#define DEFAULT_SPEED 9600

struct serial_session {
#ifdef WINDOWS
	HANDLE h;
#else /* !WINDOWS */
	int fd;
#endif /* WINDOWS */
	char target[SOLARD_TARGET_LEN+1];
	int speed,data,stop,parity,vmin,vtime;
#if USE_BUFFER
	buffer_t *buffer;
#endif /* USE_BUFFER */
};
typedef struct serial_session serial_session_t;

static int serial_read_direct(void *handle, uint32_t *control, void *buf, int buflen) {
	serial_session_t *s = handle;
	int bytes;

	dprintf(dlevel+1,"buf: %p, buflen: %d\n", buf, buflen);
	if (!buf || !buflen) return 0;

#ifdef WINDOWS
	ReadFile(s->h, buf, buflen, (LPDWORD)&bytes, 0);
#else /* !WINDOWS */
	struct timeval tv;
	fd_set rfds;
	int num;

	/* See if there's anything to read */
	FD_ZERO(&rfds);
	FD_SET(s->fd,&rfds);
//	dprintf(dlevel,"waiting...\n");
	tv.tv_usec = 500000;
	tv.tv_sec = 0;
	num = select(s->fd+1,&rfds,0,0,&tv);
//	dprintf(dlevel,"num: %d\n", num);
	if (num < 1) {
		bytes = 0;
		goto serial_read_done;
	}

	/* Read */
	bytes = read(s->fd, buf, buflen);
	if (bytes < 0) log_write(LOG_SYSERR|LOG_DEBUG,"serial_read: read");

serial_read_done:
#endif /* WINDOWS */
	dprintf(dlevel,"bytes: %d\n", bytes);
	return bytes;
}

#if USE_BUFFER
static int serial_get(void *handle, uint8_t *buffer, int buflen) {
#ifdef WINDOWS
	serial_session_t *s = handle;
	int bytes;

	dprintf(dlevel,"bytes: %d\n", bytes);
	return bytes;
#else /* !WINDOWS */
	return serial_read_direct(handle,0,buffer,buflen);
#endif /* WINDOWS */
}
#endif /* USE_BUFFER */

static void *serial_new(void *target, void *topts) {
	serial_session_t *s;
	char *p;

	s = calloc(1,sizeof(*s));
	if (!s) {
		log_write(LOG_SYSERR,"serial_new: calloc(%d)",sizeof(*s));
		return 0;
	}
#if USE_BUFFER
	s->buffer = buffer_init(1024,serial_get,s);
	if (!s->buffer) return 0;
#endif

#ifdef WINDOWS
	s->h = INVALID_HANDLE_VALUE;
#else
	s->fd = -1;
#endif
	strncat(s->target,strele(0,",",(char *)target),sizeof(s->target)-1);

	/* TOPTS:  Baud, data, parity, stop, vmin, vtime */
	/* Defaults: 9600, 8, N, 1, 0, 5 */

	/* Baud rate */
	p = strele(0,",",topts);
	s->speed = atoi(p);
	if (!s->speed) s->speed = DEFAULT_SPEED;

	/* Data bits */
	p = strele(1,",",topts);
	s->data = atoi(p);
	if (!s->data) s->data = 8;

	/* Parity */
	p = strele(2,",",topts);
	if (strlen(p)) {
		switch(p[0]) {
		case 'N':
		default:
			s->parity = 0;
			break;
		case 'E':
			s->parity = 1;
			break;
		case 'O':
			s->parity = 2;
			break;
		}
	}

	/* Stop */
	p = strele(3,",",topts);
	s->stop = atoi(p);
	if (!s->stop) s->stop = 1;

	/* vmin */
	p = strele(4,",",topts);
	s->vmin = atoi(p);
	if (!s->vmin) s->vmin = 0;

	/* vtime */
	p = strele(5,",",topts);
	s->vtime = atoi(p);
	if (!s->vtime) s->vtime = 5;

	dprintf(dlevel,"target: %s, speed: %d, data: %d, parity: %c, stop: %d, vmin: %d, vtime: %d\n",
		s->target, s->speed, s->data, s->parity == 0 ? 'N' : s->parity == 2 ? 'O' : 'E', s->stop, s->vmin, s->vtime);

	return s;
}

#ifndef WINDOWS
static int set_interface_attribs (int fd, int speed, int data, int parity, int stop, int vmin, int vtime) {
	struct termios tty;
	int rate;

	dprintf(dlevel,"fd: %d, speed: %d, data: %d, parity: %d, stop: %d, vmin: %d, vtime: %d\n",
		fd, speed, data, parity, stop, vmin, vtime);

	// Don't block
	fcntl(fd, F_SETFL, FNDELAY);

	// Get current device tty
	tcgetattr(fd, &tty);

#if 0
	// Set the transport port in RAW Mode (non cooked)
	tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	tty.c_cflag |= (CLOCAL | CREAD);
	tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	tty.c_oflag &= ~OPOST;
#else
	/* Or we can just use this :) */
	cfmakeraw(&tty);
#endif

	tty.c_cc[VMIN]  = vmin;
	tty.c_cc[VTIME] = vtime;

	/* Baud */
	switch(speed) {
	case 9600:      rate = B9600;    break;
	case 19200:     rate = B19200;   break;
	case 1200:      rate = B1200;    break;
	case 2400:      rate = B2400;    break;
	case 4800:      rate = B4800;    break;
	case 38400:     rate = B38400;   break;
	case 57600:     rate = B57600;   break;
	case 115200:	rate = B115200;   break;
	case 230400:	rate = B230400;   break;
	case 460800:	rate = B460800;   break;
	case 500000:	rate = B500000;   break;
	case 576000:	rate = B576000;   break;
	case 921600:	rate = B921600;   break;
	case 600:       rate = B600;      break;
	case 300:       rate = B300;      break;
	case 150:       rate = B150;      break;
	case 110:       rate = B110;      break;
	default:
		tty.c_cflag &= ~CBAUD;
		tty.c_cflag |= CBAUDEX;
		rate = speed;
		break;
	}
	cfsetispeed(&tty, rate);
	cfsetospeed(&tty, rate);

	/* Data bits */
	switch(data) {
		case 5: tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS5; break;
		case 6: tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS6; break;
		case 7: tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS7; break;
		case 8: default: tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; break;
	}

	/* Stop bits (2 = 2, any other value = 1) */
	tty.c_cflag &= ~CSTOPB;
	if (stop == 2) tty.c_cflag |= CSTOPB;
	else tty.c_cflag &= ~CSTOPB;

	/* Parity (0=none, 1=even, 2=odd */
        tty.c_cflag &= ~(PARENB | PARODD);
	switch(parity) {
		case 1: tty.c_cflag |= PARENB; break;
		case 2: tty.c_cflag |= PARODD; break;
		default: break;
	}

	// No flow control
//	tty.c_cflag &= ~CRTSCTS;

	// do it
	tcsetattr(fd, TCSANOW, &tty);
	tcflush(fd,TCIOFLUSH);
	return 0;
}

static int serial_open(void *handle) {
	serial_session_t *s = handle;
	char path[256];
	struct flock fl;

	dprintf(dlevel,"target: %s\n", s->target);
	strcpy(path,s->target);
	if ((access(path,0)) == 0) {
		if (strncmp(path,"/dev",4) != 0) {
			sprintf(path,"/dev/%s",s->target);
			dprintf(dlevel,"path: %s\n", path);
		}
	}
	dprintf(dlevel,"path: %s\n", path);
	s->fd = open(path, O_RDWR | O_EXCL | O_NOCTTY);
	if (s->fd < 0) {
		log_write(LOG_SYSERR,"open %s\n", path);
		return 1;
	}
	fl.l_type = F_RDLCK | F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;
	if (fcntl(s->fd,F_SETLK,&fl) < 0) {
		close(s->fd);
		s->fd = 0;
		return 1;
	}
	usleep(1000);
	set_interface_attribs(s->fd, s->speed, s->data, s->parity, s->stop, s->vmin, s->vtime);

	dprintf(dlevel,"done!\n");
	return 0;
}
#else
static int set_interface_attribs (HANDLE h, int speed, int data, int parity, int stop, int vmin, int vtime) {
	DCB Dcb;
	COMMTIMEOUTS timeout;

	GetCommState (h, &Dcb); 

	/* Baud */
	Dcb.BaudRate        = speed;

	/* Stop bits (2 = 2, any other value = 1) */
	if (stop == 2) Dcb.StopBits = TWOSTOPBITS;
	else Dcb.StopBits = ONESTOPBIT;

	/* Data bits */
	Dcb.ByteSize        = data;

	/* Parity (0=none, 1=even, 2=odd */
	switch(parity) {
		case 1: Dcb.Parity = EVENPARITY; Dcb.fParity = 1; break;
		case 2: Dcb.Parity = ODDPARITY; Dcb.fParity = 1; break;
		default: Dcb.Parity = NOPARITY; Dcb.fParity = 0; break;
	}

	Dcb.fBinary = 1;
	Dcb.fOutxCtsFlow    = 0;
	Dcb.fOutxDsrFlow    = 0;
	Dcb.fDsrSensitivity = 0;
	Dcb.fTXContinueOnXoff = TRUE;
	Dcb.fOutX           = 0;
	Dcb.fInX            = 0;
	Dcb.fNull           = 0;
	Dcb.fErrorChar      = 0;
	Dcb.fAbortOnError   = 0;
	Dcb.fRtsControl     = RTS_CONTROL_DISABLE;
	Dcb.fDtrControl     = DTR_CONTROL_DISABLE;

	SetCommState (h, &Dcb); 

	//Set read timeouts
	if(vtime > 0) {
		if(vmin > 0) {
			timeout.ReadIntervalTimeout = vtime * 100;
		} else {				//Total timeout
			timeout.ReadTotalTimeoutConstant = vtime * 100;
		}
	} else {
		if(vmin > 0) {	//Counted read
			//Handled by length parameter of serialRead(); timeouts remain 0 (unused)
		} else {				//"Nonblocking" read
			timeout.ReadTotalTimeoutConstant = 1;	//Wait as little as possible for a blocking read (1 millisecond)
		}
	}
	if(!SetCommTimeouts(h, &timeout)) {
		log_write(LOG_ERROR,"Unable to setting serial port timeout: %lu\n", GetLastError());
	}
	return 0;
}

static int serial_open(void *handle) {
	serial_session_t *s = handle;
	char path[256];

	dprintf(dlevel,"target: %s\n", s->target);
	sprintf(path,"\\\\.\\%s",s->target);
	dprintf(dlevel,"path: %s\n", path);
	s->h = CreateFileA(path,                //port name
                      GENERIC_READ | GENERIC_WRITE, //Read/Write
                      0,                            // No Sharing
                      NULL,                         // No Security
                      OPEN_EXISTING,// Open existing port only
                      0,            // Non Overlapped I/O
                      NULL);        // Null for Comm Devices

	dprintf(dlevel,"h: %p\n", s->h);
	if (s->h == INVALID_HANDLE_VALUE) return 1;

	set_interface_attribs(s->h, s->speed, s->data, s->parity, s->stop, s->vmin, s->vtime);

	dprintf(dlevel,"done!\n");
	return 0;
}
#endif

static int serial_read(void *handle, uint32_t *control, void *buf, int buflen) {
#if USE_BUFFER
	return buffer_get(s->buffer,buf,buflen);
#else /* !USE_BUFFER */
	return serial_read_direct(handle,control,buf,buflen);
#endif /* USE_BUFFER */
#if 0
	serial_session_t *s = handle;

#ifdef WINDOWS
	int bytes;
	ReadFile(s->h, buf, buflen, (LPDWORD)&bytes, 0);
	dprintf(dlevel+1,"bytes: %d\n", bytes);
	return bytes;
#else /* !WINDOWS */
#endif /* WINDOWS */
#endif
}

static int serial_write(void *handle, uint32_t *control, void *buf, int buflen) {
	serial_session_t *s = handle;
	int bytes;

	dprintf(dlevel+1,"buf: %p, buflen: %d\n", buf, buflen);

#ifdef WINDOWS
	WriteFile(s->h,buf,buflen,(LPDWORD)&bytes,0);
#else
	int bytes_left = buflen;
	do {
		dprintf(dlevel,"bytes_left: %d\n", bytes_left);
		bytes = write(s->fd,buf,bytes_left);
		dprintf(dlevel,"bytes: %d\n", bytes);
		bytes_left -= bytes;
	} while(bytes >= 0 && bytes_left);
#endif
#ifdef DEBUG
	if (debug >= 8 && bytes > 0) bindump("TO DEVICE",buf,buflen);
#endif
//	usleep ((buflen + 25) * 10000);
	return bytes;
}

static int serial_close(void *handle) {
	serial_session_t *s = handle;

#ifdef WINDOWS
	CloseHandle(s->h);
	s->h = INVALID_HANDLE_VALUE;
#else
	dprintf(dlevel,"fd: %d\n",s->fd);
	if (s->fd >= 0) {
		dprintf(dlevel,"flushing...\n");
		tcflush(s->fd, TCIFLUSH);
		dprintf(dlevel,"closing...\n");
		close(s->fd);
		s->fd = -1;
	}
#endif
	return 0;
}

static int serial_config(void *h, int func, ...) {
	va_list ap;
	int r;

	r = 1;
	va_start(ap,func);
	switch(func) {
	default:
		dprintf(dlevel,"error: unhandled func: %d\n", func);
		break;
	}
	return r;
}

static int serial_destroy(void *handle) {
	serial_session_t *s = handle;

        serial_close(s);
        free(s);
        return 0;
}

solard_driver_t serial_driver = {
	"serial",
	serial_new,
	serial_destroy,
	serial_open,
	serial_close,
	serial_read,
	serial_write,
	serial_config
};
#endif /* !__APPLE_ */
