
dprintf(1,"%s started\n", script_name);

dprintf(1,"interval: %d\n", sd.interval);
if (sd.interval != 5) {
	dprintf(1,"setting interval to 5\n");
	sd.interval = 5;
}
exit(0);

if (typeof test1 == "undefined") test1 = 7;

while(0 == 1) {
	dprintf(1,"test1: %d\n", test1);
	if (test1 == 2) test1 = 7;
	else if (test1 == 7) test1 = 1;
	dprintf(1,"sleeping...\n");
	sleep(1);
}
