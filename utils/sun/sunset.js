#!/opt/sd/bin/sdjs
include(SOLARD_LIBDIR+"/sd/utils.js");
include(SOLARD_LIBDIR+"/sd/suncalc.js");
let loc = getLocation(true);
if (loc) var times = SunCalc.getTimes(new Date(), loc.lat, loc.lon);
if (times) printf("%s\n", times.sunset);
