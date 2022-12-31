#ifndef JS_CONSOLE_H
#define JS_CONSOLE_H

extern JSClass js_Console_class;
extern JSFunctionSpec Console_methods[];

JSBool register_class_Console(JSContext *cx);

static JSBool Console_write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool Console_writeln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool Console_writeError(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool Console_writeErrorln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool Console_read(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool Console_readInt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool Console_readFloat(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool Console_readChar(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);


#endif 

