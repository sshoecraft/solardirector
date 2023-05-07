#!/opt/sd/bin/sdjs
include(SOLARD_LIBDIR+"/sd/suncalc.js");
var si = {};
config.read("/opt/sd/etc/si.json");
var props = [ [ "location", DATA_TYPE_STRING, "none", 0 ], ];
config.add_props(si,props);
var loc = si.location.split(",");
if (loc) var times = SunCalc.getTimes(new Date(), loc[0], loc[1]);
if (times) printf("%s\n", times.sunset);
