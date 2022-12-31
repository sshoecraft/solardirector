
#ifndef __SD_CAN_H
#define __SD_CAN_H

#include <sys/types.h>
#include <stdint.h>
#ifdef WINDOWS
typedef uint32_t canid_t;
#define CAN_MAX_DLEN 8
struct can_frame {
	canid_t can_id;		/* 32 bit CAN_ID + EFF/RTR/ERR flags */
	uint8_t can_dlc;	/* frame payload length in byte (0 .. CAN_MAX_DLEN) */
	uint8_t data[CAN_MAX_DLEN] __attribute__((aligned(8)));
};
#else
#include <linux/can.h>
#endif
typedef struct can_frame can_frame_t;

struct can_config_filter {
	uint32_t *id;
	uint32_t count;
};

enum CAN_CONFIG_FUNCS {
	CAN_CONFIG_SET_RANGE=100,
	CAN_CONFIG_SET_FILTER,
	CAN_CONFIG_CLEAR_FILTER,
	CAN_CONFIG_PARSE_FILTER,
	CAN_CONFIG_START_BUFFER,
	CAN_CONFIG_STOP_BUFFER,
	CAN_CONFIG_GET_FD,
};

#define CAN_ID_ANY 0xFFFF

#endif
