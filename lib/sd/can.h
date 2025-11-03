
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
#elif defined(__APPLE__)
/* macOS doesn't have linux/can.h, so we define the structures ourselves */
typedef uint32_t canid_t;
#define CAN_MAX_DLEN 8
#define CAN_EFF_FLAG 0x80000000U /* EFF/SFF is set in the MSB */
#define CAN_RTR_FLAG 0x40000000U /* remote transmission request */
#define CAN_ERR_FLAG 0x20000000U /* error message frame */

/* Valid bits in CAN ID for frame formats */
#define CAN_SFF_MASK 0x000007FFU /* standard frame format (SFF) */
#define CAN_EFF_MASK 0x1FFFFFFFU /* extended frame format (EFF) */
#define CAN_ERR_MASK 0x1FFFFFFFU /* omit EFF, RTR, ERR flags */

struct can_frame {
	canid_t can_id;		/* 32 bit CAN_ID + EFF/RTR/ERR flags */
	uint8_t can_dlc;	/* frame payload length in byte (0 .. CAN_MAX_DLEN) */
	uint8_t __pad;		/* padding */
	uint8_t __res0;		/* reserved / padding */
	uint8_t __res1;		/* reserved / padding */
	uint8_t data[CAN_MAX_DLEN] __attribute__((aligned(8)));
};
#else
#include <linux/can.h>
#endif
typedef struct can_frame can_frame_t;

#if !defined(__WIN32) && !defined(__WIN64)
#include "driver.h"
extern solard_driver_t can_driver;
#endif

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

#ifdef JS
#include "jsengine.h"
JSObject *js_can_new(JSContext *cx, JSObject *parent, void *priv);
#endif

#endif
