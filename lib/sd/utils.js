
function getLocation(goInet) {

	let dlevel = 0;

	if (typeof(goinet) == "undefined") goinet = true;
	dprintf(dlevel,"goInet: %s\n", goInet);

	// See if the location file exists
	locfile = SOLARD_ETCDIR+"/location.conf";
	var f = new File(locfile,"text");
	let valid = false;
	let lat,lon;
	dprintf(dlevel,"exists: %s\n", f.exists);
	if (f.exists) {
		// Read it
		let props = [
			[ "lat", DATA_TYPE_DOUBLE, 0xDEADBEEF, 0 ],
			[ "lon", DATA_TYPE_DOUBLE, 0xDEADBEEF, 0 ]
		];
		var c = new Config(locfile);
		dprintf(dlevel,"c: %s\n", c);
		c.add_props(this,props,"location");
		c.read(locfile);
		let p = c.get_property("lat");
		lat = ((typeof(p) == "undefined") ? 0xDEADBEEF : p.value);
		p = c.get_property("lon");
		lon = ((typeof(p) == "undefined") ? 0xDEADBEEF : p.value);
		dprintf(dlevel,"lat: %f, lon: %f\n", lat, lon);
		if (lat != 0xDEADBEEF && lon != 0xDEADBEEF) valid = true;
	}
	dprintf(dlevel,"valid: %s\n", valid);
	if (!valid && goInet) {
		// Try and find our location
		printf("Trying geolocation...\n");
		let out = run_command("/usr/bin/curl -sSn http://ip-api.com 2>/dev/null | sed \"s,\x1B\[[0-9;]*[a-zA-Z],,g\"");
		dprintf(dlevel,"out: %s\n", out);
		if (typeof(out) != "undefined") {
			let str = "";
			for(let i=0; i < out.length; i++) str += out[i].toString();
			dprintf(dlevel,"str: %s\n", str);
			let j = JSON.parse(str);
			dprintf(dlevel,"j: %s\n", j);
			if (typeof(j) != "undefined") {
				if (dlevel > 0) dumpobj(j);
				if (typeof(j.lat) != "undefined" && typeof(j.lon) != "undefined") {
					lat = j.lat;
					lon = j.lon;
					valid = true;
				}
			}
		}
		dprintf(dlevel,"lat: %f, lon: %f\n", lat, lon);
		if (valid && !f.exists) {
			f.open("readWrite,create");
			f.writeln("[location]");
			f.writeln("lat="+lat);
			f.writeln("lon="+lon);
			f.close();
		}
	}
	dprintf(dlevel,"valid: %s\n", valid);
	if (!valid) {
		printf("error: unable to find location. create %s and put your location in it\n", locfile);
		printf("format:\n");
		printf("[location]\n");
		printf("lat=<latitude>\n");
		printf("lon=<longitude>\n");
		return undefined;
	}
	return { lat: lat, lon: lon };
}

function arrayUnique(array) {
    var a = array.concat();
    for(var i=0; i<a.length; ++i) {
        for(var j=i+1; j<a.length; ++j) {
            if(a[i] === a[j])
                a.splice(j--, 1);
        }
    }

    return a;
}

Array.prototype.unique = function() {
    var a = this.concat();
    for(var i=0; i<a.length; ++i) {
        for(var j=i+1; j<a.length; ++j) {
            if(a[i] === a[j])
                a.splice(j--, 1);
        }
    }

    return a;
};

String.prototype.esplit = function(sep) {
	var a = this.split(sep)
	if (a[0] == '' && a.length == 1) return [];
	return a;
};

function merge(target, source) {
    Object.keys(source).forEach(function (key) {
        if (source[key] && typeof source[key] === 'object') {
            merge(target[key] = target[key] || {}, source[key]);
            return;
        }
        target[key] = source[key];
    });
}

var STRIP_COMMENTS = /((\/\/.*$)|(\/\*[\s\S]*?\*\/))/mg;
var ARGUMENT_NAMES = /([^\s,]+)/g;
function getParamNames(func) {
  var fnStr = func.toString().replace(STRIP_COMMENTS, '');
  var result = fnStr.slice(fnStr.indexOf('(')+1, fnStr.indexOf(')')).match(ARGUMENT_NAMES);
  if(result === null)
     result = [];
  return result;
}

function isInt(value) {
  var x;
  return isNaN(value) ? !1 : (x = parseFloat(value), (0 | x) === x);
}

function strstr(haystack, needle, bool) {
    // Finds first occurrence of a string within another
    //
    // version: 1103.1210
    // discuss at: http://phpjs.org/functions/strstr    // +   original by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // +   bugfixed by: Onno Marsman
    // +   improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // *     example 1: strstr(.Kevin van Zonneveld., .van.);
    // *     returns 1: .van Zonneveld.    // *     example 2: strstr(.Kevin van Zonneveld., .van., true);
    // *     returns 2: .Kevin .
    // *     example 3: strstr(.name@example.com., .@.);
    // *     returns 3: .@example.com.
    // *     example 4: strstr(.name@example.com., .@., true);    // *     returns 4: .name.
    var pos = 0;

    haystack += "";
    pos = haystack.indexOf(needle); if (pos == -1) {
        return false;
    } else {
        if (bool) {
            return haystack.substr(0, pos);
        } else {
            return haystack.slice(pos);
        }
    }
}

//const clone = JSON.parse(JSON.stringify(orig));
function clone(obj) {
    var copy;

    // Handle the 3 simple types, and null or undefined
    if (null == obj || "object" != typeof obj) return obj;

    // Handle Date
    if (obj instanceof Date) {
        copy = new Date();
        copy.setTime(obj.getTime());
        return copy;
    }

    // Handle Array
    if (obj instanceof Array) {
        copy = [];
        for (var i = 0, len = obj.length; i < len; i++) {
            copy[i] = clone(obj[i]);
        }
        return copy;
    }

    // Handle Object
    if (obj instanceof Object) {
        copy = {};
        for (var attr in obj) {
            if (obj.hasOwnProperty(attr)) copy[attr] = clone(obj[attr]);
        }
        return copy;
    }

    throw new Error("Unable to copy obj! Its type isn't supported.");
}

function strrchr (haystack, needle) {
  //  discuss at: https://locutus.io/php/strrchr/
  // original by: Brett Zamir (https://brett-zamir.me)
  //    input by: Jason Wong (https://carrot.org/)
  // bugfixed by: Brett Zamir (https://brett-zamir.me)
  //   example 1: strrchr("Line 1\nLine 2\nLine 3", 10).substr(1)
  //   returns 1: 'Line 3'
  var pos = 0;
  if (typeof needle !== 'string') {
    needle = String.fromCharCode(parseInt(needle, 10));
  }
  needle = needle.charAt(0);
  pos = haystack.lastIndexOf(needle);
  if (pos === -1) {
    return false;
  }
  return haystack.substr(pos);
}

//function objname(obj) { return obj.toString().match(/ (\w+)/)[1]; };

function chkconf(sec,prop,type,def,flags) {
	if (typeof(sec) == "undefined") return;
	if (typeof(sec[prop]) != "undefined") return;
	config.add(objname(sec),prop,type,def,flags);
}

function pct(val1, val2) {
//	printf("val1: %f, val2: %f\n", val1,val2);
	var lv1 = val1;
	if (lv1 < 0) lv1 *= (-1);
	var lv2 = val2;
	if (lv2 < 0) lv2 *= (-1);
//	printf("lv1: %f, lv2: %f\n", lv1, lv2);
	if (lv1 > lv2)
		var d = lv2/lv1;
	else
		var d = lv1/lv2;
//	printf("d: %f\n", d);
	return 100 - (d * 100.0);
}

function rtest(obj) { return JSON.parse(JSON.stringify(obj)); }

function delobj(obj) {
	for(key in obj) delete obj[key];
	delete obj;
}

function dumpobj(obj) {
        if (0 == 1) {
        printf("%s keys:\n",objname(obj));
//        for(key in obj) printf("key: %s, value: %s\n",key,obj[key]);
        for(key in obj) {
		print("key: " + key + "\n");
		print("value: " + obj[key] + "\n");
	}
        printf("\n");
        return;
        }
        for(key in obj) {
		val = (obj[key] && typeof obj[key] === 'function' ? '[function]' : obj[key]);
		printf("%-30.30s %s\n", (key + ":"), val);
        }
}

if (!String.prototype.startsWith) {
	String.prototype.startsWith = function(searchString, position){
		position = position || 0;
		return ((this.substr(position, searchString.length) === searchString) ? true : false);
	};
}

function unsigned(arg) { return arg >>> 0; }

time = function() { return systime() >>> 0; }

function get_random(min, max) {
	return Math.floor(Math.random() * (max - min + 1) + min)
}

function run_command(str) {
	var cmd = "| " + str;
//	printf("cmd: %s\n", cmd);
	var f = new File(cmd);
	var out = [];
	if (!f.open("read")) return undefined;
	var i = 0;
	line = f.readln();
	if (line == null) return undefined;
	else out[i++] = line;
	while(line = f.readln()) out[i++] = line;
	f.close();
	return out;
}

function toJSONString(obj,fields,numspaces) {

	if (!numspaces) numspaces = 0;
	let str = "{";
	if (numspaces) str += "\n";
	let first = true;
	let sformat = "%" + sprintf("%d",numspaces) + "s";
	let spaces = sprintf(sformat,"");

	addkey = function() {
		if (!first) str += ",";
		else first = false;
		if (numspaces) str += "\n";
		str += spaces+"\"" + key + "\":";
		var type = typeof(obj[key]);
		switch(type) {
		case 'undefined':
		case 'number':
			if (isNaN(obj[key])) str += 0;
			else str += obj[key];
			break;
		case 'string':
			str += "\""+obj[key]+"\"";
			break;
		case 'boolean':
			str += sprintf("%s",obj[key]);
			break;
		default:
			log_error("toJSON: unhandled type: %s\n", type);
			break;
		}
	}

	var key;
	if (fields) {
		for(var index in fields) {
			key = fields[index];
			if (key in obj) addkey();
		}
	} else {
		for(let key in obj)
			addkey();
	}
	if (numspaces) str += "\n";
	str += "}";
	return str;
}

function timestamp() {
	var tzoffset = (new Date()).getTimezoneOffset() * 60000; //offset in milliseconds
	return (new Date(Date.now() - tzoffset)).toISOString().slice(0, -1);
}

function parseBoolean(str) {
	return /true/i.test(str);
}
// location: lat,lon
// timespec format: [sunrise|sunset|0-23:0-59][[+|-]# of seconds]
// what: timespec name
// next: if date < current, use next day
function get_date(location,timespec,what,nextday) {

	var dlevel = 3;

	dprintf(dlevel,"location: %s, timespec: %s, what: %s, nextday: %s\n", location, timespec, what, nextday);

	function get_offset(d,s) {

		dprintf(dlevel,"d: %s, s: %s\n", d.toString(), s);

		let i = s.indexOf('-');
		dprintf(dlevel,"minus i: %d\n", i);
		let add = 1;
		if (i < 0) i = s.indexOf('+');
		else add = 0;
		dprintf(dlevel,"plus i: %d\n", i);
		if (i >= 0) {
			let a = parseInt(s.substr(i+1));
			dprintf(dlevel,"a: %s\n", a);
			if (a <= 0) {
				log_error("get_date: invalid time string(offset seconds): %s\n", timespec);
				return null;
			}
			dprintf(dlevel,"add: %d\n", add);
			if (add) d.setMinutes(d.getMinutes() + a);
			else d.setMinutes(d.getMinutes() - a);
		}
		return d;
	}

	var d;
	let i = timespec.indexOf(':');
	if (i >= 0) {
		let t,h,m;
		t = timespec.substr(0,i);
		dprintf(dlevel,"t: %s\n", t);
		if (t.length < 2) {
			log_error("get_date: invalid time string(hours length): %s\n", timespec);
			return null;
		}
		h = parseInt(t);
		if (h < 0 || h > 23) {
			log_error("get_date: invalid time string(hours): %s\n", timespec);
			return null;
		}
		t = timespec.substr(3,2);
		dprintf(dlevel,"t: %s\n", t);
		if (t.length < 2) {
			log_error("get_date: invalid time string(minutes length): %s\n", timespec);
			return null;
		}
		m = parseInt(t);
		if (m < 0 || m > 59) {
			log_error("get_date: invalid time string(minutes): %s\n", timespec);
			return null;
		}
		d = new Date();
		d.setHours(h);
		d.setMinutes(m);
	} else {
		var p = timespec.match(/sunrise|sunset/i);
		dprintf(dlevel,"p: %s(%s)\n", p,typeof(p));
		if (p) {
			let m = p[p.index];
			dprintf(dlevel,"m: %s(%s)\n", m,typeof(m));
			// Must have location for sunrise/set
			if (typeof(location) != "string") {
				log_error("get_date: location must be a string");
				return null;
			}
			if (location.length < 1) {
				log_error("get_date: unable to calculate time for %s (%s), location is empty\n",what,timespec);
				return null;
			}
			let locs = location.split(",");
			let lat = parseFloat(locs[0]);
			let lon = parseFloat(locs[1]);
			if (isNaN(lat)) {
				log_error("get_date: latitude specified in location (%s) is invalid\n", locs[0]);
				return null;
			}
			if (isNaN(lon)) {
				log_error("get_date: longitude specified in location (%s) is invalid\n", locs[1]);
				return null;
			}
			dprintf(dlevel,"lat: %s, lon: %s\n", lat, lon);
			let times = SunCalc.getTimes(new Date(), lat, lon);
			if (!times) {
				log_error("get_date: unable to calculate sunrise/sunset: bad location?\n");
				return null;
			}
			if (m.match(/sunrise/i)) {
				d = times.sunrise;
			} else if (m.match(/sunset/i)) {
				d = times.sunset;
			}
		} else {
			d = Date.parse(timespec);
		}
	}
	if (!d) return d;
	dprintf(dlevel,"d: %s\n", d.toString());
	d = get_offset(d,timespec);
	dprintf(dlevel,"d: %s\n", d.toString());
	var cur = new Date();
	dprintf(dlevel,"%s: nextday: %s, d: %s, cur: %s, diff: %s\n", what, nextday, d.getTime(), cur.getTime(), cur.getTime() - d.getTime());
	if (nextday && d.getTime() <= cur.getTime()) {
		d.setDate(d.getDate() + 1);
		dprintf(dlevel,"NEW d: %s\n", d.toString());
	}
	return d;
}

function double_equals(a,b) {

	let dlevel = 7;

	dprintf(dlevel,"a: %.10f, b: %.10f\n", a, b);
	let v = Math.abs(a - b);
	dprintf(dlevel,"v: %.10f\n", v);
	let r = v < 10e-7;
	dprintf(dlevel,"r: %.10f\n", r);
	return r;
//	return ((Math.abs(a) - Math.abs(b)) < 10e-7);
}

// Return the 1st result as an object
function influx2obj(results,include_time) {
	if (!results) return undefined;
//	dumpobj(results);
	if (typeof("include_time") == "undefined") include_time = false;
	let cols = results[0].columns;
	let vals = results[0].values;
	if (!cols || !vals) return undefined;
	// If every name begins with "last_" strip it
	let remove_last = true;
	for(let i=0; i < cols.length; i++) {
//		dprintf(0,"col[%d]: %s\n", i, cols[i]);
		if (cols[i] != "time" && !cols[i].startsWith("last_")) remove_last = false;
	}	
//	dprintf(0,"remove_last: %s\n", remove_last);
	var obj = {};
	for(let i=0; i < cols.length; i++) {
		let name = cols[i];
//		dprintf(0,"name: %s\n", name);
		if (name == "time" && !include_time) continue;
		if (remove_last) name = name.substr(5);
//		dprintf(0,"name: %s\n", name);
		if (vals[0][i] == null) obj[name] = 0;
		else obj[name] = vals[0][i];
	}
//	dumpobj(obj);
	return obj;
}

// return results as array of objects
// XXX influx should do this??
function influx2arr(r,include_time) {

	let dlevel = 2;

	if (typeof("include_time") == "undefined") include_time = false;

	// only do the 1st series of the 1st result
	dprintf(dlevel,"r: %s\n", r);
	let results = r.results;
	dprintf(dlevel,"results.length: %d\n", results.length);
	if (!results.length) return undefined;
	series = results[0].series;
	dprintf(dlevel,"series.length: %d\n", series.length);
	if (!series.length) return undefined;
	let cols = series[0].columns;
	let vals = series[0].values
	dprintf(dlevel,"cols: %s, vals: %s\n", cols, vals);
        if (!cols || !vals) return undefined;

        // If every name begins with "last_" strip it
        let remove_last = true;
        for(let i=0; i < cols.length; i++) {
                dprintf(dlevel,"col[%d]: %s\n", i, cols[i]);
                if (cols[i] != "time" && !cols[i].startsWith("last_")) remove_last = false;
        }
        dprintf(dlevel,"remove_last: %s\n", remove_last);

	var arr = [];
	dprintf(dlevel,"vals.length: %d\n", vals.length);
	for(let j=0; j < vals.length; j++) {
		var obj = {};
		for(let i=0; i < cols.length; i++) {
			let name = cols[i];
			dprintf(dlevel,"name: %s\n", name);
			if (name == "time" && !include_time) continue;
			if (remove_last && name != "time") name = name.substr(5);
			dprintf(dlevel,"name: %s\n", name);
			if (vals[j][i] == null) obj[name] = 0;
			else obj[name] = vals[j][i];
		}
		arr[j] = obj;
	}
	return arr;
}

function report_mem() {
	if (typeof(last_memused) == "undefined") last_memused = -1;
	if (memused() != last_memused) {
		var udiff = memused() - last_memused;
		printf("mem: %d (%s%d)\n", memused(), (udiff > 0 ? "+" : ""), udiff);
		last_memused = memused();
	}

if (0) {
	if (typeof(last_sysmemused) == "undefined") last_sysmemused = 0;
	if (sysmemused() != last_sysmemused) {
		var udiff = sysmemused() - last_sysmemused;
		printf("sysmem: %d (%s%d)\n", sysmemused(), (udiff > 0 ? "+" : ""), udiff);
		last_sysmemused = sysmemused();
	}
}
}

function set_trigger(name,func,arg) {

	let dlevel = 3;

	dprintf(dlevel,"name: %s\n", name);
	let p = config.get_property(name);
	dprintf(dlevel,"p: %s\n", p);
	if (p) {
		p.trigger = func;
		p.arg = arg;
	}
}
