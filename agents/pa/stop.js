
function stop_main() {
	let dlevel = 1;
	dprintf(dlevel,"*** PA STOP ***\n");
	
	// Agent shutdown tasks
	log_info("PA Agent shutting down\n");
	
	// Clean up reservations on shutdown
	if (pa.reservations && pa.reservations.length > 0) {
		log_info("Releasing %d reservations on shutdown\n", pa.reservations.length);
		pa.reservations = [];
		pa.reserved = 0;
	}
}
