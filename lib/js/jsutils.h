
#ifndef __JS_UTILS_H
#define __JS_UTILS_H

struct JSConstSpec {
	const char *name;
	jsval value;
};
typedef struct JSConstSpec JSConstSpec;
	
char *js_string(JSContext *cx, jsval val);

#endif
