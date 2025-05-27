
char *js_toJSON(JSContext *cx, jsval val) {
	JSObject *obj,*rec;
	JSIdArray *ida;
	jsval *ids;
	char *name;
	influx_session_t *s;
	influx_response_t *r;
	jsval *argv = vp + 2;
	int i;
	char *type,*key;
	jsval val;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);

	/* Get the measurement name */
	if (strcmp(jstypestr(cx,argv[0]),"string") != 0) goto js_influx_write_usage;
	name = (char *)JS_EncodeString(cx,JSVAL_TO_STRING(argv[0]));
	dprintf(dlevel,"name: %s\n", name);

	/* If the 2nd arg is a string, send it direct */
	type = jstypestr(cx,argv[1]);
	dprintf(dlevel,"arg 2 type: %s\n", type);
	if (strcmp(type,"string") == 0) {
		char *value = (char *)JS_EncodeString(cx,JSVAL_TO_STRING(argv[1]));
		dprintf(dlevel,"value: %s\n", value);
		r = influx_write(s,name,value);
		JS_free(cx,value);

	/* Otherwise it's an object - gets the keys and values */
	} else if (strcmp(type,"object") == 0) {
		char *string,temp[2048],value[1536];
		int strsize,stridx,newidx,len;

		rec = JSVAL_TO_OBJECT(argv[1]);
		ida = JS_Enumerate(cx, rec);
		dprintf(dlevel,"ida: %p, count: %d\n", ida, ida->length);
		ids = &ida->vector[0];
		strsize = INFLUX_INIT_BUFSIZE;
		string = malloc(strsize);
//		stridx = sprintf(string,"%s",name);
		stridx = 0;
		for(i=0; i< ida->length; i++) {
	//		dprintf(dlevel+1,"ids[%d]: %s\n", i, jstypestr(cx,ids[i]));
			key = js_string(cx,ids[i]);
			if (!key) continue;
	//		dprintf(dlevel+1,"key: %s\n", key);
			val = JSVAL_VOID;
			if (!JS_GetProperty(cx,rec,key,&val)) {
				JS_free(cx,key);
				continue;
			}
			jsval_to_type(DATA_TYPE_STRING,&value,sizeof(value)-1,cx,val);
			dprintf(dlevel,"key: %s, type: %s, value: %s\n", key, jstypestr(cx,val), value);
			if (JSVAL_IS_NUMBER(val)) {
				jsdouble d;
				if (JSVAL_IS_INT(val)) {
					d = (jsdouble)JSVAL_TO_INT(val);
					if (JSDOUBLE_IS_NaN(d)) strcpy(value,"0");
				} else {
					d = *JSVAL_TO_DOUBLE(val);
					if (JSDOUBLE_IS_NaN(d)) strcpy(value,"0.0");
				}
				len = snprintf(temp,sizeof(temp)-1,"%s=%s",key,value);
			} else if (JSVAL_IS_STRING(val)) {
				len = snprintf(temp,sizeof(temp)-1,"%s=\"%s\"",key,value);
			} else {
				dprintf(dlevel,"ignoring field %s: unhandled type\n", key);
				continue;
			}
			dprintf(dlevel,"temp: %s\n", temp);
			/* XXX include comma */
			newidx = stridx + len + 1;
			dprintf(dlevel,"newidx: %d, size: %d\n", newidx, strsize);
			if (newidx > strsize) {
				char *newstr;

				dprintf(dlevel,"string: %p\n", string);
				newstr = realloc(string,newidx);
				dprintf(dlevel,"newstr: %p\n", newstr);
				if (!newstr) {
					JS_ReportError(cx,"Influx.write: memory allocation error");
					JS_free(cx,key);
					JS_free(cx,value);
					free(string);
					return JS_FALSE;
				}
				string = newstr;
				strsize = newidx;
			}
			sprintf(string + stridx,"%s%s",(i ? "," : " "),temp);
			stridx = newidx;
			JS_free(cx,key);
//			JS_free(cx,value);
		}
		JS_DestroyIdArray(cx, ida);
		string[stridx] = 0;
		dprintf(dlevel,"string: %s\n", string);
		r = influx_write(s,name,string);
		free(string);
	} else {
		JS_free(cx,name);
		goto js_influx_write_usage;
	}
	JS_free(cx,name);
	if (!r) *vp = JSVAL_VOID;
	else *vp = OBJECT_TO_JSVAL(js_influx_response_new(cx, obj, r));
    	return JS_TRUE;
js_influx_write_usage:
	JS_ReportError(cx,"Influx.write requires 2 arguments (measurement:string, rec:object OR rec:string)");
	return JS_FALSE;
}
