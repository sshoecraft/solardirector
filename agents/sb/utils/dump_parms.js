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
if (sb.request(SB_ALLPAR,sb.mkfields([]))) {
	log_error("invalid request\n");
	sb.close();
	exit(1);
}

dump_results();

// Close it
sb.close();
