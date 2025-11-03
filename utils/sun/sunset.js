#!/usr/bin/env sdjs
include(SOLARD_LIBDIR+"/sd/utils.js");
include(SOLARD_LIBDIR+"/sd/suncalc.js");

// Get date from command line argument, or use current date
var date;
if (argv.length > 0) {
    // Parse date from argument (YYYY-MM-DD format)
    date = new Date(argv[0]);
} else {
    date = new Date();
}

let loc = getLocation(true);
if (loc) var times = SunCalc.getTimes(date, loc.lat, loc.lon);
if (times) printf("%s\n", times.sunset);
