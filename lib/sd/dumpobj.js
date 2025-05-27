function dumpobj(obj,label,types) {

	let dlevel = 4;

	if (typeof(types) == "undefined") types = false;

	function _doit(obj,level,types) {
		for(key in obj) {
			dprintf(dlevel,"key: %s\n", key);
			val = (obj[key] && typeof obj[key] === 'function' ? '[function]' : obj[key]);
			let vtype = typeof(obj[key]);
			dprintf(dlevel,"vtype: %s\n", vtype);
			va =  (obj[key] && vtype == "object") ? obj[key].valueOf().toString() : "";
			dprintf(dlevel,"va: %s\n", va);
			let xp = (vtype != "string" && va.indexOf("ConfigProperty") == -1 && va.indexOf("ConfigSection") == -1 && va.indexOf("Config") == -1);
			dprintf(dlevel,"xp: %d\n", xp);
			for(let i=0; i < level; i++) printf(" ");
			if (key == "global" || !xp) {
				if (types) printf("%-30.30s %s\n", key + "(" + vtype + "):", val);
				else printf("%-30.30s %s\n", (key + ":"), val);
			} else {
				if (vtype == "object") {
					if (types) printf("%s\n", key + "(" + vtype + "):");
					else printf("%s\n", (key + ":"));
				}
				else {
					if (types) printf("%-30.30s %s\n", key + "(" + vtype + "):", val);
					else printf("%-30.30s %s\n", (key + ":"), val);
				}
				_doit(obj[key],level+1);
			}
		}
	}
	let start = 0;
	if (typeof(label) != "undefined") {
		printf("%s:\n",label);
		start++;
	}
//	printf("%s:\n", objname(obj));
	_doit(obj,start,types);
}
