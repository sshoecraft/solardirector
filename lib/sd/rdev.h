
#ifndef __SD_RDEV_H
#define __SD_RDEV_H

#ifdef WINDOWS
typedef SOCKET socket_t;
#define SOCKET_CLOSE(s) closesocket(s);
#else
typedef int socket_t;
#define SOCKET_CLOSE(s) close(s)
#define INVALID_SOCKET -1
#endif

#if 0
struct __attribute__((packed, aligned(1))) rdev_header {
	uint8_t opcode;
	uint8_t unit;
	uint8_t whatlen;
	uint32_t len;
};
typedef struct rdev_header rdev_header_t;
#endif


#define RDEV_NAME_LEN 32
#define RDEV_TYPE_LEN 16

/* Functions */
enum RDEV_OPCODE {
	RDEV_OPCODE_NONE,
	RDEV_OPCODE_OPEN,
	RDEV_OPCODE_CLOSE,
	RDEV_OPCODE_READ,
	RDEV_OPCODE_WRITE,
};

#define RDEV_STATUS_SUCCESS 0
#define RDEV_STATUS_ERROR 1

#define RDEV_DEFAULT_PORT 3930

int rdev_send(socket_t fd, uint8_t opcode, uint8_t unit, uint32_t control, void *buf, uint16_t buflen);
int rdev_recv(socket_t fd, uint8_t *opcode, uint8_t *unit, uint32_t *control, void *buf, int buflen, int timeout);
int rdev_request(socket_t fd, uint8_t *opcode, uint8_t *unit, uint32_t *control, void *buf, uint16_t buflen, int timeout);

#endif /* __SD_RDEV_H */
