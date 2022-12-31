var s = "SPS     "
var k = s.split('');
for(i=0; i < k.length; i++) { printf("%s0x%02x", (i ? "," : ""), k[i].charCodeAt(0)); }
while(i < 8) { printf("%s0x00", (i++ ? "," : "")); }
