/*
 * Service Controller JavaScript Functions
 * Minimal C functions exposed to JavaScript runtime
 */

#include "sc.h"
#include "jsapi.h"

// JavaScript function: system_exec(command)
static JSBool js_system_exec(JSContext *cx, uintN argc, jsval *argv) {
    char *command;
    FILE *fp;
    char buffer[16384];
    char *result = NULL;
    size_t result_len = 0;
    size_t total_len = 0;
    JSString *str;
    
    if (argc != 1) {
        JS_ReportError(cx, "system_exec requires 1 argument (command)");
        return JS_FALSE;
    }
    
    command = js_GetStringBytes(JS_ValueToString(cx, argv[0]));
    if (!command) {
        JS_ReportError(cx, "system_exec: invalid command");
        return JS_FALSE;
    }
    
    dprintf(3, "system_exec: %s\n", command);
    
    fp = popen(command, "r");
    if (!fp) {
        *argv = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, ""));
        return JS_TRUE;
    }
    
    // Read output in chunks
    while (fgets(buffer, sizeof(buffer), fp)) {
        size_t chunk_len = strlen(buffer);
        char *new_result = realloc(result, total_len + chunk_len + 1);
        if (!new_result) {
            if (result) free(result);
            pclose(fp);
            JS_ReportError(cx, "system_exec: memory allocation failed");
            return JS_FALSE;
        }
        result = new_result;
        strcpy(result + total_len, buffer);
        total_len += chunk_len;
    }
    
    pclose(fp);
    
    if (result) {
        // Remove trailing newline if present
        if (total_len > 0 && result[total_len - 1] == '\n') {
            result[total_len - 1] = '\0';
        }
        str = JS_NewStringCopyZ(cx, result);
        free(result);
    } else {
        str = JS_NewStringCopyZ(cx, "");
    }
    
    *argv = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

// JavaScript function: read_file(filename)
static JSBool js_read_file(JSContext *cx, uintN argc, jsval *argv) {
    char *filename;
    FILE *fp;
    char *content = NULL;
    long file_size;
    JSString *str;
    
    if (argc != 1) {
        JS_ReportError(cx, "read_file requires 1 argument (filename)");
        return JS_FALSE;
    }
    
    filename = js_GetStringBytes(JS_ValueToString(cx, argv[0]));
    if (!filename) {
        JS_ReportError(cx, "read_file: invalid filename");
        return JS_FALSE;
    }
    
    fp = fopen(filename, "r");
    if (!fp) {
        *argv = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, ""));
        return JS_TRUE;
    }
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (file_size > 0) {
        content = malloc(file_size + 1);
        if (content) {
            size_t bytes_read = fread(content, 1, file_size, fp);
            content[bytes_read] = '\0';
        }
    }
    
    fclose(fp);
    
    if (content) {
        str = JS_NewStringCopyZ(cx, content);
        free(content);
    } else {
        str = JS_NewStringCopyZ(cx, "");
    }
    
    *argv = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

// JavaScript function: write_file(filename, content)
static JSBool js_write_file(JSContext *cx, uintN argc, jsval *argv) {
    char *filename, *content;
    FILE *fp;
    
    if (argc != 2) {
        JS_ReportError(cx, "write_file requires 2 arguments (filename, content)");
        return JS_FALSE;
    }
    
    filename = js_GetStringBytes(JS_ValueToString(cx, argv[0]));
    content = js_GetStringBytes(JS_ValueToString(cx, argv[1]));
    
    if (!filename || !content) {
        JS_ReportError(cx, "write_file: invalid arguments");
        return JS_FALSE;
    }
    
    fp = fopen(filename, "w");
    if (!fp) {
        *argv = BOOLEAN_TO_JSVAL(JS_FALSE);
        return JS_TRUE;
    }
    
    fprintf(fp, "%s", content);
    fclose(fp);
    
    *argv = BOOLEAN_TO_JSVAL(JS_TRUE);
    return JS_TRUE;
}

// JavaScript function: time()
static JSBool js_time(JSContext *cx, uintN argc, jsval *argv) {
    time_t now = time(NULL);
    *argv = INT_TO_JSVAL((int32)now);
    return JS_TRUE;
}

// JavaScript function: file_exists(filename)
static JSBool js_file_exists(JSContext *cx, uintN argc, jsval *argv) {
    char *filename;
    struct stat st;
    
    if (argc != 1) {
        JS_ReportError(cx, "file_exists requires 1 argument (filename)");
        return JS_FALSE;
    }
    
    filename = js_GetStringBytes(JS_ValueToString(cx, argv[0]));
    if (!filename) {
        JS_ReportError(cx, "file_exists: invalid filename");
        return JS_FALSE;
    }
    
    *argv = BOOLEAN_TO_JSVAL(stat(filename, &st) == 0);
    return JS_TRUE;
}

// Function registration table
static JSFunctionSpec sc_functions[] = {
    { "system_exec", js_system_exec, 1, 0, 0 },
    { "read_file", js_read_file, 1, 0, 0 },
    { "write_file", js_write_file, 2, 0, 0 },
    { "time", js_time, 0, 0, 0 },
    { "file_exists", js_file_exists, 1, 0, 0 },
    { 0, 0, 0, 0, 0 }
};

// JavaScript initialization function for the SC agent
static JSBool js_sc_init(JSContext *cx, JSObject *parent, void *private) {
    if (!JS_DefineFunctions(cx, parent, sc_functions)) {
        log_error("JS_DefineFunctions(sc_functions) failed!\n");
        return JS_FALSE;
    }
    return JS_TRUE;
}

// Initialize JavaScript functions using SD framework
int sc_jsfunc_init(void *js_engine) {
    return JS_EngineAddInitFunc(js_engine, "sc", js_sc_init, 0);
}