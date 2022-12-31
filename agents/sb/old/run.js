
function run_main() {
        if (typeof(last_memused) == "undefined") last_memused = -1;
        if (memused() != last_memused) {
                printf("mem: %d\n", memused());
                last_memused = memused();
        }
}
