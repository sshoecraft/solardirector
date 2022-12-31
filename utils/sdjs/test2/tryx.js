
include("jslint.js");

var myResult = JSLINT("test.js", null);

function main() {
	print("try in main");
	return 4
}
print("try out main");
