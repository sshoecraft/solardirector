#ifdef MQTT

/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "common.h"

void solard_message_dump(solard_message_t *msg, int level) {
//	char temp[32];

	if (!msg) return;
//printf("level: %d, debug: %d\n", level, debug);
	if (level > debug) return;
//	snprintf(temp,sizeof(temp)-1,"%s",msg->data);

	printf("%-20.20s %s\n", "topic", msg->topic);
	printf("%-20.20s %d\n", "type", msg->type);
	if (msg->type == SOLARD_MESSAGE_TYPE_AGENT) printf("%-20.20s %s\n", "name", msg->name);
	else printf("%-20.20s %s\n", "id", msg->id);
	printf("%-20.20s %s\n", "func", msg->func);
	printf("%-20.20s %s\n", "replyto", msg->replyto);
	printf("%-20.20s %s\n", "data", msg->data);
	printf("%-20.20s %d\n", "size", msg->size);
#if 0
	switch(msg->type) {
	case SOLARD_MESSAGE_TYPE_AGENT:
		dprintf(level,"Agent msg: name: %s, func: %s, replyto: %s, data(%d): %s\n",
			msg->name,msg->func,msg->replyto,msg->size,temp);
		break;
	case SOLARD_MESSAGE_TYPE_CLIENT:
		dprintf(level,"Client msg: id: %s, replyto: %s, data(%d): %s\n",
			msg->id,msg->replyto,msg->size,temp);
		break;
	default:
		dprintf(level,"UNKNOWN: topic: %s, id/name: %s, func: %s, replyto: %s, data(%d): %s\n",
			msg->id,msg->name,msg->func,msg->replyto,msg->size,msg->data);
		break;
	}
#endif
}


/* Topic format:  SolarD/<id|name>/func (status,info,data,etc) */
int solard_getmsg(solard_message_t *msg, char *topic, char *message, int msglen, char *replyto) {
	char *root,*p;

	dprintf(4,"topic: %s, msglen: %d, replyto: %s\n", topic, msglen, replyto);

	/* Clear out dest before we start */
	memset(msg,0,sizeof(*msg));

	/* All messages must start with SOLARD_TOPIC_ROOT */
	root = strele(0,"/",topic);
	dprintf(4,"root: %s\n", root);
	if (strcmp(root,SOLARD_TOPIC_ROOT) != 0) return 1;
	strncpy(msg->topic,topic,sizeof(msg->topic)-1);

	/* Next must be agents or clients */
	p = strele(1,"/",topic);
	dprintf(4,"type: %s\n", p);
	if (strcmp(p,SOLARD_TOPIC_AGENTS) == 0) {
		msg->type = SOLARD_MESSAGE_TYPE_AGENT;
		/* Next is agent name */
		strncpy(msg->name,strele(2,"/",topic),sizeof(msg->name)-1);
		/* Next is agent func */
		strncpy(msg->func,strele(3,"/",topic),sizeof(msg->func)-1);
	} else if (strcmp(p,SOLARD_TOPIC_CLIENTS) == 0) {
		msg->type = SOLARD_MESSAGE_TYPE_CLIENT;
		/* Next is client id */
		strncpy(msg->id,strele(2,"/",topic),sizeof(msg->id)-1);
	} else {
		return 1;
	}

	if (message && msglen) {
		if (msglen >= SOLARD_MAX_PAYLOAD_SIZE) {
			log_warning("solard_getmsg: msglen(%d) > %d",msglen,SOLARD_MAX_PAYLOAD_SIZE);
			msg->size = SOLARD_MAX_PAYLOAD_SIZE-1;
		} else {
			msg->size = msglen;
		}
		memcpy(msg->data,message,msg->size);
		msg->data[msg->size] = 0;
	}
	if (replyto) strncpy(msg->replyto,replyto,sizeof(msg->replyto)-1);
//	dprintf(4,"replyto: %s\n", msg->replyto);
//	solard_message_dump(msg,0);
	return 0;
}

int solard_message_reply(mqtt_session_t *m, solard_message_t *msg, int status, char *message) {
	char *str;
	char topic[SOLARD_TOPIC_LEN];
	json_object_t *o;
	int r;

	/* If no replyto, dont bother */
	if (!*msg->replyto) return 0;

	o = json_create_object();
	json_object_set_number(o,"status",status);
	json_object_set_string(o,"message",message);
	str = json_dumps(json_object_value(o),0);
	json_destroy_object(o);

	/* Replyto is expected to be the UUID of the sender */
	sprintf(topic,"%s/%s",SOLARD_TOPIC_ROOT,msg->replyto);
	dprintf(1,"topic: %s\n", topic);
	r = mqtt_pub(m,topic,str,1,0);
	free(str);
	return r;
}

int solard_message_wait(list lp, int timeout) {
	time_t cur,upd;
	int count;

	while(1) {
		count = list_count(lp);
		if (count) break;
		if (timeout >= 0) {
			time(&cur);
			upd = list_updated(lp);
			dprintf(4,"cur: %ld, upd: %ld, diff: %ld\n", cur, upd, cur-upd);
			if ((cur - upd) >= timeout) break;
		}
		sleep(1);
	}
	return count;
}

solard_message_t *solard_message_wait_id(list lp, char *id, int timeout) {
	solard_message_t *msg;
	time_t cur,upd;

	dprintf(1,"==> want id: %s\n", id);

	while(1) {
		list_reset(lp);
		while((msg = list_get_next(lp)) != 0) {
			dprintf(1,"msg->id: %s\n", msg->id);
			if (strcmp(msg->replyto,id) == 0) {
				dprintf(1,"found\n");
				return msg;
			}
		}
		if (timeout >= 0) {
			time(&cur);
			upd = list_updated(lp);
			dprintf(4,"cur: %ld, upd: %ld, diff: %ld\n", cur, upd, cur-upd);
			if ((cur - upd) >= timeout) break;
		}
		sleep(1);
	}
	dprintf(1,"NOT found\n");
	return 0;
}

solard_message_t *solard_message_wait_target(list lp, char *target, int timeout) {
	solard_message_t *msg;
	time_t cur,upd;

	while(1) {
		list_reset(lp);
		while((msg = list_get_next(lp)) != 0) {
			if (strcmp(msg->name, target) == 0)
				return msg;
		}
		if (timeout >= 0) {
			time(&cur);
			upd = list_updated(lp);
			dprintf(4,"cur: %ld, upd: %ld, diff: %ld\n", cur, upd, cur-upd);
			if ((cur - upd) >= timeout) break;
		}
		sleep(1);
	}
	return 0;
}

int solard_message_delete(list lp, solard_message_t *msg) {
	return list_delete(lp, msg);
}

#ifdef JS
enum MESSAGE_PROPERTY_ID {
	MESSAGE_PROPERTY_ID_TOPIC=1,
	MESSAGE_PROPERTY_ID_NAME,
	MESSAGE_PROPERTY_ID_FUNC,
	MESSAGE_PROPERTY_ID_DATA,
};

static JSBool message_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	solard_message_t *msg;
	int prop_id;

	msg = JS_GetPrivate(cx,obj);
	dprintf(dlevel,"msg: %p\n", msg);
	if (!msg) {
		JS_ReportError(cx, "message_getprop: internal error: private is null!");
		return JS_FALSE;
	}
	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case MESSAGE_PROPERTY_ID_TOPIC:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,msg->topic,strlen(msg->topic));
			break;
		case MESSAGE_PROPERTY_ID_NAME:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,msg->name,strlen(msg->name));
			break;
		case MESSAGE_PROPERTY_ID_FUNC:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,msg->func,strlen(msg->func));
			break;
		case MESSAGE_PROPERTY_ID_DATA:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,msg->data,strlen(msg->data));
			break;
		}
	}
	return JS_TRUE;
}

static JSClass js_message_class = {
	"Message",		/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	message_getprop,	/* getProperty */
	JS_PropertyStub,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *js_InitMessageClass(JSContext *cx, JSObject *parent) {
	JSPropertySpec message_props[] = {
		{ "topic", MESSAGE_PROPERTY_ID_TOPIC, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "name", MESSAGE_PROPERTY_ID_NAME, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "func", MESSAGE_PROPERTY_ID_FUNC, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "data", MESSAGE_PROPERTY_ID_DATA, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ 0 }
	};
	JSFunctionSpec message_funcs[] = {
//		JS_FN("delete",js_message_delete,0,0,0),
		{ 0 }
	};
	JSObject *obj;

	dprintf(2,"creating %s class\n", js_message_class.name);
	obj = JS_InitClass(cx, parent, 0, &js_message_class, 0, 0, message_props, message_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class", js_message_class.name);
		return 0;
	}
	dprintf(dlevel,"done!\n");
	return obj;
}

JSObject *js_message_new(JSContext *cx, JSObject *parent, solard_message_t *m) {
	JSObject *newobj;

	/* Create the new object */
	dprintf(2,"Creating %s object...\n", js_message_class.name);
	newobj = JS_NewObject(cx, &js_message_class, 0, parent);
	if (!newobj) return 0;

	JS_SetPrivate(cx,newobj,m);

	dprintf(dlevel,"newobj: %p\n", newobj);
	return newobj;
}

JSObject *js_create_messages_array(JSContext *cx, JSObject *parent, list l) {
	JSObject *rows,*mobj;
	jsval val;
	solard_message_t *msg;
	int i;

	dprintf(2,"count: %d\n", list_count(l));
	rows = JS_NewArrayObject(cx, list_count(l), NULL);
	i = 0;
	list_reset(l);
	while((msg = list_get_next(l)) != 0) {
		mobj = js_message_new(cx,rows,msg);
//		dprintf(0,"mobj: %p\n", mobj);
		if (!mobj) continue;
		val = OBJECT_TO_JSVAL(mobj);
		JS_SetElement(cx, rows, i++, &val);
	}
	return rows;
}
#endif
#endif
