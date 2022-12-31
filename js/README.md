
This is a version of Mozilla's Spidermonkey 1.8-RC1 library (before they went to C++)

	Additions(p=partial,x=done):

	x File object
	x Dir[ectory] object (part of file)
	x Global functions
		printf
		sprintf
		dprintf
		putstr
		print
		error
		load
		include
		system
		run
		sleep
		exit
		abort
		quit
		readline
		timestamp
		systime
		dirname
		basename
		log_info
		log_warning
		log_error
		log_syserror
		gc
		version
		memused (within JS)
		sysmemused
	p console object 
	x JSON object
	x Socket object
	x CAN oject (Linux only - SocketCAN)
	x SMANET object
	x Influx object
	window object
	Serial object
	Socket object
	Websocket object
	Bluetooth object (linux only)
	MQTT object (via paho.mqtt.c lib)
	HTTP object (uses CURL)
	SMTP object
