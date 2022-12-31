
include("jslint.js");

var c = " \"mqtt\": { \"host\": \"localhost\", \"port\": 1883, \"clientid\": \"d1214e7f-b5e0-4829-a16f-f6cf9ae4556b\", \"username\": \"\", \"password\": \"\" }" 

var result = JSLINT(c, null);
print("result: ",result);
printf("warnings: %s\n",result.warnings);
//result.warnings.forEach(function({formatted_message}) {
//	console.log(formatted_message);
//});                 
print("errors: ",JSLINT.errors);
//JSLINT.errors.forEach(function({formatted_message}) {
//	console.log(formatted_message);
//});                 
JSLINT.errors.forEach(printf);
