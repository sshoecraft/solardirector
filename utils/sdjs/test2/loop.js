while(1) {
        if (typeof(last_sysmemused) == "undefined") last_sysmemused = -1;
        if (sysmemused() != last_sysmemused) {
                printf("sysmem: %d\n", sysmemused());
                last_sysmemused = sysmemused();
        }
	run("sleep.js");
}
