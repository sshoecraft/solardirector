
include("../../core/utils.js");

function set_val(arg) {
	printf("setting aval!\n");
}

var props = [
	[ "aval", DATA_TYPE_STRING, "def", 0, set_val ],
];

dummy = {};
config.add_props(dummy,props);
dumpobj(dummy);
p = config.get_property("aval");
dumpobj(p);
abort(0);

//dumpobj(this);
p = config.get_property("parm1");
dumpobj(p);
p.value = "yes";
//dumpobj(p);
printf("p.value: %s\n", p.value);
abort(0);
