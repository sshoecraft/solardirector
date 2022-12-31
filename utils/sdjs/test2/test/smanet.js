var s = SMANET("rdev","192.168.1.7","serial0");
printf("connected: %s\n", s.connected);
if (s.connected) {
	if (!s.load_channels("/opt/sd/lib/SI6048UM.dat")) {
		var save_gd = s.get("GdManStr");
		printf("current val: %s\n", save_gd);
		if (save_gd == "Auto") {
			s.set("GdManStr","Start");
		} else {
			s.set("GdManStr","Auto");
		}
		printf("new val: %s\n", s.get("GdManStr"));
		exit(0);
	}
}
