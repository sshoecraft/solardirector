//const timezone = Intl.DateTimeFormat().resolvedOptions().timeZone;
//const currentTime = moment().tz(timezone).format();
console.log(new Date().toLocaleString());
//console.log(var d1 = Date((Date().setHours(var d2 = Date().getHours() - (var d3 = Date().getTimezoneOffset() / 60)))).toISOString())
function getLocaLTime() {
  // new Date().getTimezoneOffset() : getTimezoneOffset in minutes 
  //for GMT + 1 it is (-60) 
  //for GMT + 2 it is (-120) 
  //..
  var time_zone_offset_in_hours = new Date().getTimezoneOffset() / 60;
  //get current datetime hour
  var current_hour = new Date().getHours();
  //adjust current date hour 
  var local_datetime_in_milliseconds = new Date().setHours(current_hour - time_zone_offset_in_hours);
  //format date in milliseconds to ISO String
  var local_datetime = new Date(local_datetime_in_milliseconds).toISOString();
  return local_datetime;
}
console.log(getLocaLTime());
