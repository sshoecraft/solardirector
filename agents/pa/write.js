
function write_main() {
	let dlevel = 1;
	dprintf(dlevel,"*** PA WRITE ***\n");
	
	// Publish current power reservation status
	mqtt.pub(pa.pub_topic, { 
		reserved: pa.reserved, 
		avail: pa.avail 
	});
}
