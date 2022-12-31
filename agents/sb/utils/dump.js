
function dump_results() {
	// Get the results
	var results = sb.results;

	// For each result ...
	dprintf(1,"results.length: %d\n", results.length);
	for(i=0; i < results.length; i++) {
		var o = results[i].object;
		dprintf(1,"object[%03d]: key: %s, label: %s, path: %s, unit: %s, mult: %f\n", i, o.key, o.label, o.path, o.unit, o.mult);
		var v = results[i].values;
		dprintf(1,"values[%03d]: length: %d, data: %s\n", i, v.length, v);
		var out = sprintf("[%s] ", o.key);
		dprintf(1,"o.path.length: %d\n", o.path.length);
		if (o.path.length) {
			for(j=0; j < o.path.length; j++) out += sprintf("%s%s",(j ? " -> " : ""),o.path[j]);
			out += sprintf(" -> ");
		}
		out += sprintf("%s: ", o.label);
		dprintf(1,"v.type: %s\n", typeof(v));
		var u = (typeof(o.unit) != "undefined" ? " "+o.unit : "");
		dprintf(1,"v.length: %d\n", v.length);
		if (v.length == 1) {
			var val = (v[0] == null ? "" : v[0]);
			if (typeof(val) == "number") {
				out += sprintf("%.1f%s",v[0],u);
			} else if (val.length) {
				out += sprintf("%s%s",v[0],u);
			}
		} else {
			dprintf(1,"v.length: %d\n", v.length);
			var outsave = out;
			for(j=0; j < v.length; j++) {
//				printf("v[%d]: %s\n", j, v[j]);
				out += sprintf("[%s] %s%s",String.fromCharCode("A".charCodeAt(0)+j),v[j],u);
				printf("%s\n",out);
				out = outsave;
			}
			continue;
		}
		printf("%s\n",out);
	}
}
