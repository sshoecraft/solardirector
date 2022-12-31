
include("funcs.js")

function main() {
	print("in s.js main");
	return 12;
}
print("starting main");
var r = main();
print("s main r: "+r);

