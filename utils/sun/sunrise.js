#!/opt/sd/bin/sdjs
include(SOLARD_LIBDIR+"/sd/suncalc.js");
loc = getLocation(true);
if (loc) var times = SunCalc.getTimes(new Date(), loc.lat, loc.lon);
if (times) printf("%s\n", times.sunrise);
