//var out = system("ls");
var f = new File("| /usr/bin/ls");
f.open("read")
var out = [];
var i = 0;
while(line = f.readln()) out[i++] = line;
f.close();
printf("out: %s\n", out);
exit(0);

include("../../core/utils.js");

//var out = run_command("/root/bin/rheem_get_mode 2>/dev/null");
var out = run_command("/root/bin/rheem_get_infox 2>/dev/null");
dumpobj(out);
