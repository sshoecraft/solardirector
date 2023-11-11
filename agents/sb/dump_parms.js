#!./sb -X

//
//// This script dumps the parameters in the order it gets them back from the Sunny Boy
//

include("dump.js")

// Open the sunny boy
if (sb.open()) {
	log_error("unable to connect\n");
	exit(1);
}
// Ask for all values
var results = sb.request(SB_ALLPAR,sb.mkfields([]));
if (!results) {
	log_error("invalid request\n");
	sb.close();
	exit(1);
}
dump_results(results);

// Close it
sb.close();
