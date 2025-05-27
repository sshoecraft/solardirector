
function jconfig_conv_type(value,type) {

	let dlevel = 1;

	dprintf(dlevel,"value: %s(%s), type: %s\n", value, typeof(value), typestr(type));
	let newval;
	switch(type) {
	case DATA_TYPE_BOOL:
		newval = getBoolean(value);
		break;
	case DATA_TYPE_INT:
		newval = parseInt(value);
		break;
	case DATA_TYPE_FLOAT:
	case DATA_TYPE_DOUBLE:
		newval = parseFloat(value);
		break;
	case DATA_TYPE_STRING:
		newval = sprintf("%s",value);
		if (newval == "null" || newval == "undefined") newval = "";
		break;
	default:
		newval = value;
		break;
	}
	dprintf(dlevel,"newval: %s(%s)\n", newval, typeof(newval));
	return newval;
}

function jconfig_add_props(name,obj,props) {

	let dlevel = 1;

	// For each property
	let prop,ptype,ktype;
	let updated = false;
	for(let i=0; i < props.length; i++) {
		prop = props[i];

		dprintf(dlevel,"prop: name: %s, type: %s, value: %s\n", prop[0], typestr(prop[1]), prop[2]);

		// Make sure it's in obj
		let found = false;
		for(let key in obj) {
			dprintf(dlevel+2,"key: %s\n", key);
			if (key == prop[0]) {
				if (prop[1] == DATA_TYPE_BOOL)
					ptype = "boolean";
				else if (prop[1] == DATA_TYPE_STRING)
					ptype = "string";
				// XXX prob a bad assumption ... maybe
				else if (prop[1] >= DATA_TYPE_BOOL)
					ptype = "number";
				else
					ptype = "unknown";
				ktype = typeof(obj[key]);
				dprintf(dlevel+2,"ptype: %s, ktype: %s\n", ptype, ktype);
				if (ktype != ptype) obj[key] = jconfig_conv_type(obj[key],prop[1]);
				found = true;
				break;
			}
		}
		dprintf(dlevel,"found: %s\n", found);
		if (!found) {
			obj[prop[0]] = jconfig_conv_type(prop[2],prop[1]);
			updated = true;
		}
	}
	dprintf(dlevel,"updated: %s\n", updated);
	return updated;
}

function jconfig_set_opts(name,opts,obj,props,objs_name,verbose) {

	let dlevel = 1;

	if (typeof(verbose) == "undefined") verbose = false;
	// opts are key=value pair delimited by colon (:)
	let optarray = opts.esplit(":");
	dprintf(dlevel,"optarray.length: %d\n", optarray.length);
	for(let i=0; i < optarray.length; i++) {
		let opt = optarray[i];
		dprintf(dlevel,"opt: %s\n", opt);
		if (!opt.length) continue;
		let kv = opt.esplit("=");
		let key = kv[0];
		let value = kv[1];
		dprintf(dlevel+1,"key: %s, value: %s\n", key, value);
		if (typeof(key) == "undefined" || typeof(value) == "undefined") {
			config.errmsg = sprintf("error: invalid key/value pair: %s",opt);
			return 1;
		}
		// make sure key is in props
		let prop = undefined;
		dprintf(dlevel,"props.length: %d\n", props.length);
		for(let j=0; j < props.length; j++) {
//			dumpobj(props[j]);
			dprintf(dlevel,"props[%d][0]: %s, key: %s\n", j, props[j][0], key);
			if (props[j][0] == key) {
				dprintf(dlevel+1,"found\n");
				prop = props[j];
				break;
			}
		}
		dprintf(dlevel,"prop: %s\n", prop);
		if (!prop) {
			config.errmsg = "error: '" + key + "' not found.";
			return 1;
		}
		// value is a string; convert to property type
		newval = jconfig_conv_type(value,prop[1]);
		dprintf(dlevel,"verbose: %s\n", verbose);
		if (verbose) log_info("%ss[%s] %s = %s\n", objs_name, name, key, newval);
		obj[key] = newval;
	}
	return 0;
}

function jconfig_add(name,opts,label,objs,props,callback) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s\n", name);
	if (objs[name]) {
		config.errmsg = sprintf("add_%s: %s %s exists",label,label,name);
		log_error("%s\n",config.errmsg);
		return 1;
	}

	objs[name] = new Class(name);
	let obj = objs[name];
	obj.config = config;
	obj.key = name;
	/* not using config means no using triggers/etc */
//	config.add_props(obj,props);
	jconfig_add_props(name,obj,props);
	if (jconfig_set_opts(name,opts,obj,props,label,false)) {
		delete objs[name];
		return 1;
	}
	let r = (callback ? callback(name,obj) : 0);
	dprintf(dlevel,"r: %d\n", r);
	if (!r) {
//		log_info("Added %s %s\n", label, name);
		log_info("Added %s %s (%s)\n", label, name, opts);
		jconfig_save(label,objs,props);
	}
	return r;
}

function jconfig_del(name,label,objs,props) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s\n", name);
	if (!objs[name]) {
		config.errmsg = sprintf("del_%s: %s %s not found",label,label,name);
		log_error("%s\n",config.errmsg);
		return 1;
	}
	delete objs[name];
	jconfig_save(label,objs,props);
	log_info("Deleted %s %s\n", label, name);
	return 0;
}

function jconfig_get(name,label,objs,props) {

	let dlevel = 1;

	let opts = "";
	if (name == "all") {
		let allnames = [];
		for(let objkey in objs) allnames.push(objkey);
		opts = allnames.toString();
	} else {
		let obj = objs[name];
		if (!obj) {
			config.errmsg = sprintf("get_%s: %s %s not found",label,label,name);
			log_error("%s\n",config.errmsg);
			return 1
		}
		let key,def,val;
		for(let i=0; i < props.length; i++) {
			key = props[i][0];
			def = jconfig_conv_type(props[i][2],props[i][1]);
			val = obj[key];
			if (typeof(val) == "undefined") continue;
			// dont list default values
			dprintf(dlevel+1,"val: %s, def: %s\n", val, def);
			if (val == def) continue;
			dprintf(dlevel,"key: %s, val: %s\n", key, val);
			opts += sprintf("%s%s=%s",(opts.length ? ":" : ""),key,val);
		}
	}
	return opts;
}

function jconfig_set(name,opt_str,label,objs,props) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, opt_str: %s\n", name, opt_str);

	let obj = objs[name];
	if (!obj) {
		config.errmsg = sprintf("set_%s: %s %s not found!",label,label,name);
		log_error("%s\n",config.errmsg);
		return 1;
	}
	if (jconfig_set_opts(name,opt_str,obj,props,label,true)) return 1;
	jconfig_save(label,objs,props);
	return 0;
}

function jconfig_get_config(name,label,objs,props) {

	let dlevel = 1;

	function get_obj(objs,name) {
		let obj = objs[name];
		if (!obj) return undefined;
		let newobj = {};
		let key,val;
		for(let i=0; i < props.length; i++) {
			key = props[i][0];
			val = obj[key];
			dprintf(dlevel+2,"key: %s, val: %s\n", key, val);
			newobj[key] = val;
		}
		return newobj;
	}

	let j;
	if (name == "all") {
		let allobj = {};
		for(let objkey in objs) allobj[objkey] = get_obj(objs,objkey);
//		dumpobj(allobj);
		j = JSON.stringify(allobj);
	} else {
		let info = get_obj(objs,name);
		if (!info) {
			config.errmsg = sprintf("get_%s: %s %s not found",label,label,name);
			log_error("%s\n",config.errmsg);
			return 1;
		}
		j = JSON.stringify(info);
	}
//	dprintf(-1,"j: %s\n", j);
	return j;
}

function jconfig_set_config(config,name,props,callback) {

	let dlevel = 1;

	dprintf(dlevel,"config: %s\n", config);
	/* if it has the all= prefix in front, strip it */
	let i = config.indexOf('all=');
	dprintf(dlevel,"i: %d\n", i);
	if (i >= 0) {
		config = config.substring(4);
		dprintf(dlevel,"NEW config: %s\n", config);
	}
	if (!test_json(config)) {
		config.errmsg = "unable to load config: invalid JSON";
		return 1;
	}
	let newobj = JSON.parse(config);
	jconfig_save(label,newobj,props)
	if (callback) callback(name,props,callback);
}

function jconfig_save(label,objs,props) {

	let dlevel = 2;

	let fields = [];

	let newobjs = {};
	for(key in objs) {
		dprintf(dlevel,"key: %s\n", key);
		let obj = objs[key];
		let newobj = {};
		isdef = false;
		for(let i=0; i < props.length; i++) {
			if (props[i][3] & CONFIG_FLAG_NOSAVE) continue;
			let prop_name = props[i][0];
			dprintf(dlevel+1,"prop_name: %s\n", prop_name);
			let def = jconfig_conv_type(props[i][2],props[i][1]);
			dprintf(dlevel+1,"def: %s\n", def);
			let val = obj[prop_name];
			dprintf(dlevel+1,"val: %s\n", val);
			if (val == def) continue;
			newobj[prop_name] = val;
			fields.push(prop_name);
		}
		newobjs[key] = newobj;
		fields.push(key);
	}

	fields = arrayUnique(fields);
	dprintf(dlevel,"fields: %s\n", fields);
//	dumpobj(fields);

	let j = JSON.stringify(newobjs,fields,4);
	dprintf(dlevel+2,"j: %s\n", j);
	let name = label + "s";
	let f = new File(SOLARD_ETCDIR + "/" + agent.name + "_" + name + ".json","text");
	dprintf(dlevel,"f: %s\n", f);
	f.open("write,truncate");
	f.write(j);
	f.close();
}

function jconfig_load(name,props,callback) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s\n", name);
	this[name] = read_json(SOLARD_ETCDIR + "/" + agent.name + "_" + name + ".json",true);

	// Init each obj
	let obj;
	for(let key in this[name]) {
		dprintf(dlevel,"key: %s\n", key);
		type = typeof(this[name][key]);
		dprintf(dlevel,"type: %s\n", type);
		if (type != "object") continue;
		jconfig_add_props(key,this[name][key],props);
		if (callback) callback(key,this[name][key]);
	}

//	return this[name];
}
