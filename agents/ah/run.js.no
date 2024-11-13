
function run_main() {
if (1 == 1) {
        if (typeof(last_memused) == "undefined") last_memused = -1;
        last_memused = 0;
        if (memused() != last_memused) {
                printf("mem: %d\n", memused());
                last_memused = memused();
        }
}

if (1 == 1) {
        if (typeof(last_sysmemused) == "undefined") last_sysmemused = -1;
        if (sysmemused() != last_sysmemused) {
                printf("sysmem: %d\n", sysmemused());
                last_sysmemused = sysmemused();
        }
}
}
