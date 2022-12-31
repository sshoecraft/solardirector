
#ifndef __SD_HTTP_H
#define __SD_HTTP_H

#define HTTP_SET_POSTFIELDS	0x0100

#define http_set_postfields(p,h,f) (p)->config(h,HTTP_SET_POSTFIELDS,f)

int http_request(void *handle, char *url, char *buf, int bufsize, char *post);

#endif
