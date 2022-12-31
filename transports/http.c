
#ifdef CURL
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ctype.h>
#include <netdb.h>
#include <curl/curl.h>
#include "transports.h"
#include "http.h"

#define DEFAULT_PORT 23

#define HTTP_ENDPOINT_SIZE 128

struct http_session {
	char endpoint[HTTP_ENDPOINT_SIZE];
	char username[32];
	char password[64];
	CURL *curl;
	CURLcode res;
	char *buf;
	int bufsize;
	int index;
};
typedef struct http_session http_session_t;

static int curl_init = 0;
static size_t wrfunc(char *ptr, size_t size, size_t nmemb, void *ctx);

static void *http_new(void *target, void *topts) {
	http_session_t *s;
	char temp[128];
	bool verify,verbose;
	int i;
	struct curl_slist *headers = NULL;

	if (!curl_init) {
		/* In windows, this will init the winsock stuff */
		curl_global_init(CURL_GLOBAL_ALL);
		curl_init = 1;
	}

	s = calloc(sizeof(*s),1);
	if (!s) {
		perror("http_new: malloc");
		return 0;
	}

	/* Create the endpoint */
	if (strncmp((char *)target,"http://",7) != 0) strcpy(s->endpoint,"http://");
	strncat(s->endpoint,(char *)target,sizeof(s->endpoint)-1);
	dprintf(5,"endpoint: %s\n", s->endpoint);

	verify = verbose = false;
	i = 0;
	while(1) {
		strcpy(temp,strele(i++,",",(char *)topts));
		trim(temp);
		dprintf(1,"temp: %s\n", temp);
		if (!strlen(temp)) break;
		if (strncmp(temp,"user=",5)==0)
			strcpy(s->username,strele(1,"=",temp));
		else if (strncmp(temp,"username=",9)==0)
			strcpy(s->username,strele(1,"=",temp));
		else if (strncmp(temp,"pass=",5)==0)
			strcpy(s->password,strele(1,"=",temp));
		else if (strncmp(temp,"password=",9)==0)
			strcpy(s->password,strele(1,"=",temp));
		else if (strcmp(temp,"ssl_verify=")==0) {
			char *p = strele(1,"=",temp);
			conv_type(DATA_TYPE_BOOLEAN,&verify,0,DATA_TYPE_STRING,p,strlen(p));
		}
		else if (strcmp(temp,"verbose=")==0) {
			char *p = strele(1,"=",temp);
			conv_type(DATA_TYPE_BOOLEAN,&verbose,0,DATA_TYPE_STRING,p,strlen(p));
		}
	}
	dprintf(1,"username: %s, password: %s\n", s->username, s->password);

	/* get a curl handle */
	s->curl = curl_easy_init();
	if(!s->curl) {
		free(s);
		return 0;
	}
	curl_easy_setopt(s->curl, CURLOPT_VERBOSE, verbose);
	curl_easy_setopt(s->curl, CURLOPT_SSL_VERIFYPEER, verify);
	curl_easy_setopt(s->curl, CURLOPT_WRITEFUNCTION, wrfunc);
	curl_easy_setopt(s->curl, CURLOPT_WRITEDATA, s);

//	headers = curl_slist_append(headers, "Expect:");
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(s->curl, CURLOPT_HTTPHEADER, headers);

	return s;
}

static void *https_new(void *target, void *topts) {
	char endpoint[HTTP_ENDPOINT_SIZE];

	*endpoint = 0;
	if (strncmp((char *)target,"https://",8) != 0) strcpy(endpoint,"https://");
	strncat(endpoint,(char *)target,sizeof(endpoint) - strlen(endpoint));
	return http_new;
}

static int http_open(void *handle) {
//	http_session_t *s = handle;
	return 0;
}

static int http_close(void *handle) {
//	http_session_t *s = handle;

	return 0;
}

static size_t wrfunc(char *ptr, size_t size, size_t nmemb, void *ctx) {
	http_session_t *s = ctx;

	int sz = size*nmemb;
	dprintf(1,"index: %d, sz: %d, bufsize: %d\n", s->index, sz, s->bufsize);
	if ((s->index + sz) > s->bufsize) {
		void *newb;

		newb = realloc(s->buf, s->index+sz);
		if (!newb) return 0;
		s->bufsize = s->index+sz;
	}
	s->index += sz;
	return size*nmemb;
}

static int http_read(void *handle, uint32_t *control, void *buf, int buflen) {
//	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	return 1;
}

static int http_write(void *handle, uint32_t *control, void *buf, int buflen) {
#if 0
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 0L);



	http_session_t *s = handle;
//	int retcode = 0;
//	CURLcode res = CURLE_FAILED_INIT;
	char errbuf[CURL_ERROR_SIZE] = { 0, };
	char agent[1024] = { 0, };
	char *url = "none";

#if 0
                                'User-Agent': 'Mozilla/5.0 (Windows NT 6.0; WOW64; rv:24.0) Gecko/20100101 Firefox/24.0',
                                'Accept': 'application/json, text/plain, */*',
                                'Accept-Language': 'fr,fr-FR;q=0.8,en-US;q=0.5,en;q=0.3',
                                'Accept-Encoding': 'gzip, deflate',
                                'Referer': self.__url + '/',
                                'Content-Type': 'application/json',
                                'Content-Length': str(len(params)),
#endif


	snprintf(agent, sizeof(agent), "libcurl/%s", curl_version_info(CURLVERSION_NOW)->version);
	agent[sizeof(agent) - 1] = 0;
	dprintf(1,"agent: %s\n", agent);
	curl_easy_setopt(s->curl, CURLOPT_USERAGENT, agent);

//	curl_easy_setopt(s->curl, CURLOPT_POSTFIELDS, json);
//	curl_easy_setopt(s->curl, CURLOPT_POSTFIELDSIZE, -1L);

	/* This is a test server, it fakes a reply as if the json object were created */
	curl_easy_setopt(s->curl, CURLOPT_URL, url);

	curl_easy_setopt(s->curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(s->curl, CURLOPT_ERRORBUFFER, errbuf);

#endif
	return 1;
}

static int http_config(void *h, int func, ...) {
	va_list ap;
	int r;

	r = 1;
	va_start(ap,func);
	switch(func) {
#if 0
	case HTTP_SET_POSTFIELDS:
		curl_easy_setopt(s->curl, CURLOPT_POSTFIELDS, post);
		break;	
	case HTTP_GET_RESULTS:
		break;	
#endif
	default:
		dprintf(1,"error: unhandled func: %d\n", func);
		break;
	}
	return r;
}

static int http_destroy(void *handle) {
	http_session_t *s = handle;

        http_close(s);
        free(s);
        return 0;
}

solard_driver_t http_driver = {
	"http",
	http_new,
	http_destroy,
	http_open,
	http_close,
	http_read,
	http_write,
	http_config
};

solard_driver_t https_driver = {
	"https",
	https_new,
	http_destroy,
	http_open,
	http_close,
	http_read,
	http_write,
	http_config
};

/* Helper function */
int http_request(void *handle, char *url, char *buf, int bufsize, char *post) {
	http_session_t *s = handle;

	strncpy(buf,url,bufsize);
	if (post) {
		http_config(s, HTTP_SET_POSTFIELDS, post);
		return http_write(s,0,buf,bufsize);
	} else {
		return http_read(s,0,buf,bufsize);
	}
}

#if 0
void *sb_new(void *conf, void *driver, void *driver_handle) {
	sb_session_t *s;
	struct curl_slist *hs = curl_slist_append(0, "Content-Type: application/json");

	s = calloc(1,sizeof(*s));
	if (!s) {
		log_syserror("sb_new: calloc");
		return 0;
	}
	s->ap = conf;
	s->curl = curl_easy_init();
	if (!s->curl) {
		log_syserror("sb_new: curl_init");
		free(s);
		return 0;
	}

	return s;
}

int sb_open(void *handle) {
	sb_session_t *s = handle;

	if (!s) return 1;

	dprintf(3,"open: %d\n", check_state(s,SB_STATE_OPEN));
	if (check_state(s,SB_STATE_OPEN)) return 0;

	/* Create the login info */
	if (!s->login_fields) {
		json_object_t *o;

		o = json_create_object();
		json_object_set_string(o,"right","usr");
		json_object_set_string(o,"pass",s->password);
		s->login_fields = json_dumps(json_object_value(o),0);
		if (!s->login_fields) {
			log_syserror("sb_open: create login fields: %s\n", strerror(errno));
			return 1;
		}
		json_destroy_object(o);
	}

	*s->session_id = 0;
	if (sb_request(s,SB_LOGIN,s->login_fields)) return 1;
	if (!*s->session_id) {
		log_error("sb_open: error getting session_id .. bad password?\n");
		return 1;
	}

	dprintf(1,"session_id: %s\n", s->session_id);
	set_state(s,SB_STATE_OPEN);
	return 0;
}

int sb_close(void *handle) {
	sb_session_t *s = handle;

	if (!s) return 1;
	dprintf(1,"open: %d\n", check_state(s,SB_STATE_OPEN));
	if (!check_state(s,SB_STATE_OPEN)) return 0;

	sb_request(s,SB_LOGOUT,"{}");
	curl_easy_cleanup(s->curl);
	s->curl = 0;
	return 0;
}

static int sb_read(void *handle, void *buf, int buflen) {
	sb_session_t *s = handle;

	/* Open if not already open */
        if (sb_driver.open(s)) return 1;

	if (!s->read_fields) {
		json_object_t *o;
		json_array_t *a;

		o = json_create_object();
		a = json_create_array();
		json_object_set_array(o,"destDev",a);
		s->read_fields = json_dumps(json_object_value(o),0);
		if (!s->read_fields) {
			log_syserror("mkfields: json_dumps");
			return 1;
		}
		dprintf(1,"fields: %s\n", s->read_fields);
	}


	s->results = list_create();
	if (sb_request(s,SB_ALLVAL,s->read_fields)) {
		list_destroy(s->results);
		return 1;
	}
	list_destroy(s->results);
	return 0;
}

solard_driver_t http_driver = {
	SOLARD_DRIVER_AGENT,
	"sb",
	sb_new,				/* New */
	0,				/* Destroy */
	sb_open,			/* Open */
	sb_close,			/* Close */
	sb_read,			/* Read */
	0,				/* Write */
	sb_config			/* Config */
};
#endif
#endif
