#!/usr/bin/python3
import paho.mqtt.client as mqtt
import time;
import json;

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe("SolarD/si/Info")
#    client.subscribe("SolarD/Inverter/Sunny Island/Info")

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
#    print(msg.topic+" "+str(msg.payload))
	data = json.loads(msg.payload)
#	print(type(data))
#	print(data);
	for item in data['configuration']:
		if type(item) == dict:
			keys = list(item.keys());
#			print(type(keys))
			for key in keys:
#				print("key: " + key)
#				continue
				for i2 in item[key]:
#					print("i2: " + str(i2))
#					print(type(i2))
					if type(i2) == dict:
						k2 = i2.keys()
						for k3 in k2:
							print(k3)
					else:
						print(i2)

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("localhost", 1883, 60)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
client.loop_start()
time.sleep(.1);
