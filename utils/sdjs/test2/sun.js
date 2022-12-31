
include("../../core/init.js");

lat=31.80872918
lon=-95.14473718

//include("suncalc.js");

var times = SunCalc.getTimes(new Date(), lat, lon);
var data = {};
for(key in times) data[key] = sprintf(times[key]);
//dumpobj(times);
var j = JSON.stringify(data,null,4);
printf("j: %s\n", j);
