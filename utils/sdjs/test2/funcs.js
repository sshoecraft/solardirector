
var today = new Date();
var lastYear = new Date();
lastYear.setFullYear(today.getFullYear()-1);

/* Takes the difference in seconds between two times and returns an
object that describes how many years, days, hours, minutes, and seconds have
elapsed.
*/
function CalculateTimeDifference(difference){

    var years = Math.floor(difference/(60*60*24*365));
    difference = difference % (60*60*24*365);

    var days = Math.floor(difference/(60*60*24));
    difference = difference % (60*60*24);

    var hours = Math.floor(difference/(60*60));
    difference = difference % (60*60);

    var minutes = Math.floor(difference/60);
    difference = difference % 60;

    var seconds = difference;

    return {years: years, days: days, hours: hours, minutes: minutes, seconds: seconds};
}

var difference = today.getTime() - lastYear.getTime();
//Convert to seconds difference.
difference /= 1000;

//Add in some random amount of time.
difference += Math.random()*today.getFullYear();

var breakdown = CalculateTimeDifference(difference);

var num = 3;

function test1() {
	print("test1: returning 4!");
	return 4;
}
