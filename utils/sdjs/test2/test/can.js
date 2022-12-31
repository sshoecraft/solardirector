var c = CAN("rdev","192.168.1.7","can0");
printf("connected: %s\n",c.connected);
var data = c.read(0x304,8);
for(var i=0; i < data.length; i++) printf("data[%d]: %02x\n", i, data[i]);
var mydata = [ 0x00, 0xFF, 0x00, 0xFF, 0x00 ];
c.write(0x399,mydata);
