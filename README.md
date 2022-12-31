IMPORTANT: INSTALL THE FOLLOWING DEPS FIRST

Install dependant packages (example is for deb systems):

	apt-get install libbluetooth-dev
	apt-get install libcurl4-openssl-dev
	apt-get install libreadline-dev

paho.mqtt.c (https://github.com/eclipse/paho.mqtt.c.git)

	mkdir -p build && cd build
	cmake -DPAHO_BUILD_SHARED=TRUE -DPAHO_BUILD_STATIC=TRUE -DPAHO_ENABLE_TESTING=FALSE -DPAHO_BUILD_SAMPLES=FALSE ..
	make && make install
	
gattlib (https://github.com/labapart/gattlib.git)

	mkdir -p build && cd build
	cmake -DGATTLIB_BUILD_EXAMPLES=NO -DGATTLIB_SHARED_LIB=NO -DGATTLIB_BUILD_DOCS=NO -DGATTLIB_PYTHON_INTERFACE=NO ..
	make && make install

IF USING BLUETOOTH, YOU MUST PAIR THE DEVICE FIRST

	$ bluetoothctl 
	# scan on
	(look for your device)
	[NEW] Device XX:XX:XX:XX:XX:XX name
	# trust XX:XX:XX:XX:XX:XX
	# pair XX:XX:XX:XX:XX:XX
	(it may ask for your passkey)

Agents:
	bms/jbd - jbd bms agent
	bms/jk - jikong bms agent
	inverter/si - sma sunny island agent
	inverter/sb - sma sunny boy agent

agents will report to mqtt at regular interval - can set agent config with mqtt messages

util/sdconfig can be used to get/set params

	sdconfig pack_01 get BalanceWindow
	sdconfig pack_01 set BalanceWindow 0.15

	sdconfig si get GdManStr
	sdconfig si set GdManStr Start

	sdconfig -l sb
