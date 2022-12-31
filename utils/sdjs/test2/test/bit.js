
var ONE = 1
var TWO = 2

function dec2bin(dec){
	return (dec >>> 0).toString(2);
//	return parseInt(dec,2).toString(10);
}

function bin2dec(bin){
	return parseInt(bin, 2);
}

var num = ONE;
printf("num: %d\n", num);
num |= TWO;
printf("num: %d\n", num);
var bits = dec2bin(num);
printf("bits: %s\n", bits);
var n = bin2dec(bits);
printf("n: %d\n", n);
