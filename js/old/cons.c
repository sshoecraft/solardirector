

#include <string.h>
#include <stdio.h>
#include <jsapi.h>
#include "console.h"

JSClass js_Console_class = {
  "Console",
  0,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_EnumerateStub,
  JS_ResolveStub,
  JS_ConvertStub,
  JS_FinalizeStub,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

JSFunctionSpec Console_methods[] = {
  {"write", Console_write, 0, 0, 0},
  {"writeln", Console_writeln, 0, 0, 0},
  {"writeError", Console_writeError, 0, 0, 0},
  {"writeErrorln", Console_writeErrorln, 0, 0, 0},
  {"read", Console_read, 0, 0, 0},
  {"readInt", Console_readInt, 0, 0, 0},
  {"readFloat", Console_readFloat, 0, 0, 0},
  {"readChar", Console_readChar, 0, 0, 0},
  {NULL},
};

JSBool register_class_Console(JSContext *cx){
	JSObject *globalObj = JS_GetGlobalObject(cx);
	JSObject *obj=NULL;

    /* Define the file object. */
    obj = JS_InitClass(cx, globalObj, NULL, &js_Console_class, NULL, 0, NULL, Console_methods, NULL, NULL);
    if (!obj){
        return JS_FALSE;
    }
	return JS_TRUE;
}

/*****************************************/
/****** Begin Console object code. *******/
/*****************************************/

static JSBool Console_write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval){
  if (argc < 1){
    /* No arguments passed in, so do nothing. */
    /* We still want to return JS_TRUE though, other wise an exception will be thrown by the engine. */
    *rval = INT_TO_JSVAL(0); /* Send back a return value of 0. */
    return JS_TRUE;
  }
  else {
    /* "Hidden" feature.  The function will accept multiple arguments.  Each one is considered to be a string 
       and will be written to the console accordingly.  This makes it possible to avoid concatenation in the code 
       by using the Console.write function as so:
       Console.write("Welcome to my application", name, ". I hope you enjoy the ride!");
       */
    int i;
    size_t amountWritten=0;
    for (i=0; i<argc; i++){
      JSString *val = JS_ValueToString(cx, argv[i]); /* Convert the value to a javascript string. */
      char *str = JS_GetStringBytes(val); /* Then convert it to a C-style string. */
      size_t length = JS_GetStringLength(val); /* Get the length of the string, # of chars. */
      amountWritten = fwrite(str, sizeof(*str), length, stdout); /* write the string to stdout. */
    }
    *rval = INT_TO_JSVAL(amountWritten); /* Set the return value to be the number of bytes/chars written */
    return JS_TRUE;
  }
}

static JSBool Console_writeln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval){
  /* Just call the write function and then write out a \n */
  Console_write(cx, obj, argc, argv, rval);
  fwrite("\n", sizeof("\n"), 1, stdout);
  *rval = INT_TO_JSVAL(JSVAL_TO_INT(*rval)+1);
  return JS_TRUE;
}


static JSBool Console_writeError(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval){
  if (argc < 1){
    *rval = INT_TO_JSVAL(0);
    return JS_TRUE;
  }
  else {
    int i;
    size_t amountWritten=0;
    for (i=0; i<argc; i++){
      JSString *val = JS_ValueToString(cx, argv[i]);
      char *str = JS_GetStringBytes(val); 
          size_t length = JS_GetStringLength(val); 
          amountWritten = fwrite(str, sizeof(*str), length, stderr); 
    }
    *rval = INT_TO_JSVAL(amountWritten);
    return JS_TRUE;
  }
}

static JSBool Console_writeErrorln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval){
  /* Just call the writeError function and then write out a \n */
  Console_writeError(cx, obj, argc, argv, rval);
  fwrite("\n", sizeof("\n"), 1, stderr);
  *rval = INT_TO_JSVAL(JSVAL_TO_INT(*rval)+1);
  return JS_TRUE;
}

static JSBool Console_read(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval){
  if (argc < 1){
    /* Read a full line off the console.  */
    char *finalBuff = NULL;
    char *newBuff = NULL;
    char tmpBuff[100];
    size_t totalLen = 0;
    int len = 0;
    do {
      memset(tmpBuff, 0, sizeof(tmpBuff));
      fgets(tmpBuff, sizeof(tmpBuff), stdin);
      len = strlen(tmpBuff);
      

      if (len > 0){
        char lastChar = tmpBuff[len-1];
        
        newBuff = JS_realloc(cx, finalBuff, totalLen+len+1);      
        if (newBuff == NULL){
          JS_free(cx, finalBuff);
          *rval = BOOLEAN_TO_JSVAL(JS_FALSE);
          return JS_TRUE;
        }
        else {
          finalBuff = newBuff;
          memset(finalBuff+totalLen, 0, len);
        }
        
        strncat(finalBuff+totalLen, tmpBuff, len);
        totalLen += len;
        if (lastChar == '\n' || lastChar == '\r' || feof(stdin)){
          JSString *str = JS_NewString(cx, finalBuff, totalLen);
          *rval = STRING_TO_JSVAL(str);
          return JS_TRUE;
        }
      }
      else if (feof(stdin) && totalLen == 0){
        *rval = BOOLEAN_TO_JSVAL(JS_FALSE);
        return JS_TRUE;
      }
      else if (totalLen > 0){
        JSString *str = JS_NewString(cx, finalBuff, totalLen);
        *rval = STRING_TO_JSVAL(str);
        return JS_TRUE;
      }
    } while (1);
  }
  else {
    int32 maxlen=0;
    if (JS_ValueToInt32(cx, argv[0], &maxlen) == JS_TRUE){
      JSString *ret=NULL;
      size_t amountRead = 0;
      char *newPointer = NULL;
      char *cstring = JS_malloc(cx, sizeof(*cstring)*(maxlen+1));
      if (cstring == NULL){
        *rval = BOOLEAN_TO_JSVAL(JS_FALSE);
        return JS_TRUE;
      }
      memset(cstring, 0, sizeof(*cstring)*(maxlen+1));
      amountRead = fread(cstring, sizeof(*cstring), maxlen, stdin);
      newPointer = JS_realloc(cx, cstring, amountRead);
      if (newPointer == NULL){
        JS_free(cx, cstring);
        *rval = BOOLEAN_TO_JSVAL(JS_FALSE);
        return JS_TRUE;
      }
      else {
        cstring = newPointer;
      }
      
      ret = JS_NewString(cx, cstring, sizeof(*cstring)*amountRead);
      *rval = STRING_TO_JSVAL(ret);
      return JS_TRUE;
    }
    else {
      *rval = BOOLEAN_TO_JSVAL(JS_FALSE);
      return JS_TRUE;
    }
  }
}

static JSBool Console_readInt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval){
  int32 readinteger = 0;
  if (fscanf(stdin, "%d", &readinteger) == 1){
    if (JS_NewNumberValue(cx, readinteger, rval) == JS_TRUE){
      return JS_TRUE;
    }
  }
  *rval = BOOLEAN_TO_JSVAL(JS_FALSE);
  return JS_TRUE;
}

static JSBool Console_readFloat(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval){
  jsdouble readfloat = 0;
  if (fscanf(stdin, "%lf", &readfloat) == 1){
    if (JS_NewDoubleValue(cx, readfloat, rval) == JS_TRUE){
      return JS_TRUE;
    }
  }
 *rval = BOOLEAN_TO_JSVAL(JS_FALSE);
  return JS_TRUE;
}


static JSBool Console_readChar(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval){
  JSString *str=NULL;
  char ch, *ptr=NULL;
  if (feof(stdin)){
    *rval = BOOLEAN_TO_JSVAL(JS_FALSE);
    return JS_TRUE;
  }
  
  ch=fgetc(stdin);
  if (ch == EOF){
    *rval = BOOLEAN_TO_JSVAL(JS_FALSE);
    return JS_TRUE;
  }
  
  ptr = JS_malloc(cx, sizeof(char)*1);
  if (ptr == NULL){
    *rval = BOOLEAN_TO_JSVAL(JS_FALSE);
    return JS_TRUE;
  }
  
  *ptr = ch;
  str = JS_NewString(cx, ptr, sizeof(char)*1);
  *rval = STRING_TO_JSVAL(str);
  return JS_TRUE;
}

