
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SUNNYBOY_H
#define __SUNNYBOY_H

#include "agent.h"
#include <curl/curl.h>

#define INT_TAGS 1

struct sb_string {
#if INT_TAGS
	int tag;
#else
	char *tag;
#endif
	char *string;
};
typedef struct sb_string sb_string_t;

struct sb_session;
struct sb_object {
	char *key;
	int labeltag;
	char *label;
	int evtmsgtag;
	char *evtmsg;
	int hiertags[8];
	int hiercount;
	char *hier[8];
	int unittag;
	char *unit;
	double mult;
	int format;
	bool min;
	bool max;
	bool sum;
	bool avg;
	bool cnt;
	bool sumd;
	struct sb_session *s;				/* Session backptr for JS */
#ifdef JS
	jsval path_val;
#endif
};
typedef struct sb_object sb_object_t;

struct sb_value {
	int type;
	void *data;
	int len;
};
typedef struct sb_value sb_value_t;

struct sb_result {
	sb_object_t *obj;
	int low;
	int high;
	list selects;
	list values;
#ifdef JS
	jsval obj_val;
	jsval selects_val;
	jsval values_val;
#endif
};
typedef struct sb_result sb_result_t;

#define SB_ENDPOINT_SIZE 128
#define SB_USER_SIZE 16
#define SB_PASSWORD_SIZE 64
#define SB_SESSION_ID_SIZE 64
#define SB_LANG_SIZE 32

struct sb_session {
	solard_agent_t *ap;		/* Agent config pointer */
	char endpoint[SB_ENDPOINT_SIZE];
	char user[SB_USER_SIZE];
	char password[SB_PASSWORD_SIZE];
	char lang[SB_LANG_SIZE];
	uint16_t state;			/* Pack state */
	CURL *curl;
	char session_id[SB_SESSION_ID_SIZE];
	char *buffer;			/* Read buffer */
	int bufsize;			/* Buffer size */
	int bufidx;			/* Current write pos */
	char *login_fields;
	char *read_fields;
	char *config_fields;
	int errcode;			/* error indicator */
	char errmsg[256];		/* Error message if errcode !0 */
	int connected;			/* Login succesful, we are connected */
	char strings_filename[256];
	sb_string_t *strings;
	int strsize;
	int strings_count;
	char objects_filename[256];
	sb_object_t *objects;
	int objsize;
	int objects_count;
	bool write_flag;
	bool norun_flag;
	bool inc_flag;
	bool use_internal_strings;
	bool pub_warned;
	bool flatten;
	double last_power;
	bool log_power;
#ifdef JS
	jsval agent_val;
	jsval results_val;
	JSPropertySpec *props;
#endif
};
typedef struct sb_session sb_session_t;

/* States */
#define SB_STATE_OPEN		0x01

#define SB_LOGIN "dyn/login.json"
#define SB_LOGOUT "dyn/logout.json"
#define SB_GETVAL "dyn/getValues.json"
#define SB_LOGGER "dyn/getLogger.json"
#define SB_ALLVAL "dyn/getAllOnlValues.json"
#define SB_ALLPAR "dyn/getAllParamValues.json"
#define SB_SETPAR "dyn/setParamValues.json"

#if 0
# login: '/login.json',
# logout: '/logout.json',
# getTime: '/getTime.json',
# getValues: '/getValues.json',
# getAllOnlValues: '/getAllOnlValues.json',
# getAllParamValues: '/getAllParamValues.json',
# setParamValues: '/setParamValues.json',
# getWebSrvConf: '/getWebSrvConf.json',
# getEventsModal: '/getEvents.json',
# getEvents: '/getEvents.json',
# getLogger: '/getLogger.json',
# getBackup: '/getConfigFile.json',
# loginGridGuard: '/loginSR.json',
# sessionCheck: '/sessionCheck.json',
# setTime: '/setTime.json',
# backupUpdate: 'input_file_backup.htm',
# fwUpdate: 'input_file_update.htm',
# SslCertUpdate: 'input_file_ssl.htm'
#endif

extern solard_driver_t sb_driver;

/* info */
json_value_t *sb_get_info(sb_session_t *s);

/* Config */
int sb_agent_init(sb_session_t *s, int argc, char **argv);
int sb_config(void *h, int req, ...);

/* Utils */
//char *sb_mkfields(char **keys, int count);
//int sb_read_file(sb_session_t *s, char *filename, char *what);

/* Driver */
char *sb_mkfields(char **keys, int count);
json_value_t *sb_request(sb_session_t *s, char *func, char *fields);

/* Results */
list sb_get_results(sb_session_t *s, json_value_t *v);
int sb_destroy_results(list results);
sb_value_t *sb_get_result_value(list results, char *key);

/* JSfuncs */
int sb_jsinit(sb_session_t *s);

/* Strings */
int sb_get_strings(sb_session_t *s);
int sb_read_strings(sb_session_t *s, char *filename);
int sb_write_strings(sb_session_t *s, char *filename);
char *sb_get_string(sb_session_t *s, int tag);

/* Objects */
int sb_get_objects(sb_session_t *s);
int sb_read_objects(sb_session_t *s, char *filename);
int sb_write_objects(sb_session_t *s, char *filename);
sb_object_t *sb_get_object(sb_session_t *s, char *key);
int sb_get_object_path(sb_session_t *s, char *dest, int destsize, sb_object_t *o);

#endif
