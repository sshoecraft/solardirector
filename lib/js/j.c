#include <jsapi.h>
#include <stdio.h>
#include <stdlib.h>

JSBool ObjectToJson(JSContext *cx, JSObject *obj, JSString **json_str);

// Helper function to recursively build JSON string
JSBool ObjectToJsonRecursive(JSContext *cx, JSObject *obj, char **json_out) {
    JSBool rval = JS_TRUE;
    JSString *json_str = NULL;

    // This creates an indented JSON output
    JS_ConvertValue(cx, OBJECT_TO_JSVAL(obj), JSTYPE_OBJECT, &json_str);
    
    if (json_str) {
        *json_out = JS_EncodeString(cx, json_str);
    } else {
        *json_out = NULL;
    }

    return rval;
}

// Entry point for converting object to json string.
JSBool ObjectToJson(JSContext *cx, JSObject *obj, JSString **json_out) {
    // Recursively construct the JSON string
    return ObjectToJsonRecursive(cx, obj, json_out);
}

int main() {
	JSRuntime *rt = JS_NewRuntime(1048576);

    JSContext *cx = JS_NewContext(rt, 8192);  // Size of the context stack
    JSObject *global = JS_NewObject(cx, NULL, NULL, NULL);

    // Create an example JS object
    JSObject *obj = JS_NewObject(cx, NULL, NULL, NULL);

    // Example to add to JS object
#define str "Testing"
    JS_DefineProperty(cx, obj, "name", JS_NewString(cx, str, strlen(str)), NULL, NULL, JSPROP_ENUMERATE);

    char *json_str = NULL;

    // Call to get JSON representation
    if (ObjectToJson(cx, obj, &json_str)) {
        printf("Converted JSON: %s\n", json_str);
        free(json_str);
    } else {
        printf("Failed to convert object to JSON.\n");
    }

    JS_DestroyContext(cx);
    JS_ShutDown();

    return 0;
}

