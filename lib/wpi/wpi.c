
#ifdef WPI

#include <stdint.h>
#include <sys/time.h>
#include "debug.h"
#include "wpi.h"

#define dlevel 4

#ifdef JS
#include <jsapi.h>
#include <jsengine.h>

static JSBool js_pinMode(JSContext *cx, uintN argc, jsval *vp) {
	int pin,mode;

	if (argc != 2) {
		JS_ReportError(cx,"pinMode requires 2 arguments (pin: number, mode: number\n");
		return JS_FALSE;
	}

	/* Get the args*/
	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "u u", &pin, &mode)) return JS_FALSE;
	dprintf(dlevel,"pin: %d, mode: %d\n", pin, mode);

	pinMode(pin,mode);
	return JS_TRUE;
}

static JSBool js_digitalRead(JSContext *cx, uintN argc, jsval *vp) {
	int pin,r;

	if (argc != 1) {
		JS_ReportError(cx,"digitalRead requires 1 arguments (pin: number\n");
		return JS_FALSE;
	}

	/* Get the args*/
	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "u", &pin)) return JS_FALSE;
	dprintf(dlevel,"pin: %d\n", pin);

	r = digitalRead(pin);
	*vp = INT_TO_JSVAL(r);
	return JS_TRUE;
}

static JSBool js_digitalWrite(JSContext *cx, uintN argc, jsval *vp) {
	int pin,value;

	if (argc != 2) {
		JS_ReportError(cx,"digitalWrite requires 2 arguments (pin: number, value: number\n");
		return JS_FALSE;
	}

	/* Get the args*/
	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "u u", &pin, &value)) return JS_FALSE;
	dprintf(dlevel,"pin: %d, value: %d\n", pin, value);

	digitalWrite(pin,value);
	return JS_TRUE;
}

static JSBool js_analogRead(JSContext *cx, uintN argc, jsval *vp) {
	int pin,r;

	if (argc != 1) {
		JS_ReportError(cx,"analogRead requires 1 arguments (pin: number\n");
		return JS_FALSE;
	}

	/* Get the args*/
	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "u", &pin)) return JS_FALSE;
	dprintf(dlevel,"pin: %d\n", pin);

	r = analogRead(pin);
	*vp = INT_TO_JSVAL(r);
	return JS_TRUE;
}

static JSBool js_analogWrite(JSContext *cx, uintN argc, jsval *vp) {
	int pin,value;

	if (argc != 2) {
		JS_ReportError(cx,"analogWrite requires 2 arguments (pin: number, value: number\n");
		return JS_FALSE;
	}

	/* Get the args*/
	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "u u", &pin, &value)) return JS_FALSE;
	dprintf(dlevel,"pin: %d, value: %d\n", pin, value);

	analogWrite(pin,value);
	return JS_TRUE;
}

static JSBool js_readRHT03(JSContext *cx, uintN argc, jsval *vp) {
	int pin,r;
	int itemp,irh;
	JSObject *obj;
	jsval tval,rhval;

	if (argc != 1) {
		JS_ReportError(cx,"digitalRead requires 1 arguments (pin: number\n");
		return JS_FALSE;
	}

	/* Get the args*/
	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "u", &pin)) return JS_FALSE;
	dprintf(dlevel,"pin: %d\n", pin);

	r = readRHT03(pin, &itemp, &irh);
	dprintf(dlevel,"r: %d\n", r);
	obj = JS_NewObject(cx,0,0,0);
	dprintf(dlevel,"obj: %p\n", obj);
	if (!obj) {
		JS_ReportError(cx,"js_readRHT03: Error creating return object\n");
		return JS_FALSE;
	}
	JS_DefineProperty(cx,obj,"status",INT_TO_JSVAL(r),0,0,JSPROP_ENUMERATE|JSPROP_READONLY);
	JS_NewDoubleValue(cx, (double)itemp/10.0, &tval);
	JS_DefineProperty(cx,obj,"temp",tval,0,0,JSPROP_ENUMERATE|JSPROP_READONLY);
	JS_NewDoubleValue(cx, (double)irh/10.0, &rhval);
	JS_DefineProperty(cx,obj,"rh",rhval,0,0,JSPROP_ENUMERATE|JSPROP_READONLY);
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool js_readbme(JSContext *cx, uintN argc, jsval *vp) {
	int r,i;
	JSObject *obj;
	struct bme_data data;
	jsval val;
	struct _rdata {
		char *name;
		float *val;
	} rdata[] = {
		{ "temp", &data.c },
		{ "humidity", &data.h },
		{ "pressure", &data.p },
#if 0
		{ "c", &data.c },
		{ "h", &data.h },
		{ "f", &data.f },
		{ "i", &data.i },
		{ "w", &data.w },
		{ "p", &data.p },
		{ "a", &data.a },
#endif
	};
	#define rcount sizeof(rdata)/sizeof(struct _rdata)

	memset(&data,0,sizeof(data));
	r = readbme(&data);
	dprintf(dlevel,"r: %d\n", r);
	obj = JS_NewObject(cx,0,0,0);
	dprintf(dlevel,"obj: %p\n", obj);
	if (!obj) {
		JS_ReportError(cx,"js_readbme: Error creating return object\n");
		return JS_FALSE;
	}
	JS_DefineProperty(cx,obj,"status",INT_TO_JSVAL(r),0,0,JSPROP_ENUMERATE|JSPROP_READONLY);
	for(i=0; i < rcount; i++) {
		JS_NewDoubleValue(cx, (double)*rdata[i].val, &val);
		JS_DefineProperty(cx,obj,rdata[i].name,val,0,0,JSPROP_ENUMERATE|JSPROP_READONLY);
	}
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

int js_wpi_init(JSContext *cx, JSObject *parent, void *priv) {
	JSFunctionSpec funcs[] = {
		JS_FN("pinMode",js_pinMode,2,2,0),
		JS_FN("digitalRead",js_digitalRead,1,1,0),
		JS_FN("digitalWrite",js_digitalWrite,2,2,0),
		JS_FN("analogRead",js_analogRead,1,1,0),
		JS_FN("analogWrite",js_analogWrite,2,2,0),
		JS_FN("readRHT03",js_readRHT03,1,1,0),
		JS_FN("readbme",js_readbme,0,0,0),
		{ 0 }
	};
	JSConstantSpec constants[] = {
		JS_NUMCONST(HIGH),
		JS_NUMCONST(LOW),
		JS_NUMCONST(INPUT),
		JS_NUMCONST(OUTPUT),
		{ 0 }
	};

	dprintf(dlevel,"Defining wiringPi funcs...\n");
	if(!JS_DefineFunctions(cx, parent, funcs)) {
		return 1;
	}

	dprintf(dlevel,"Defining wiringPi constants...\n");
	if(!JS_DefineConstants(cx, parent, constants)) {
		return 1;
	}

	return 0;
}

int wpi_init(void *e) {
	int r;

	dprintf(dlevel,"e: %p\n", e);
	r = wiringPiSetup();
	dprintf(dlevel,"r: %d\n", r);
	JS_EngineAddInitFunc(e, "js_wpi_init", js_wpi_init, 0);
	dprintf(dlevel,"r: %d\n", r);
	return r;
}

#if 0
static JSClass js_driver_class = {
	"WiringPI",		/* Name */
	JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	driver_getprop,		/* getProperty */
	driver_setprop,		/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};
#endif

#else /* !JS */
int wpi_init(void) {
        return wiringPiSetup();
}
#endif /* JS */

#endif /* WPI */
