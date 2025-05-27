
function pub_main() {
	let pub = {};
	pub.agents = agents;
	if (mqtt && mqtt.enabled && mqtt.connected) mqtt.pub(data_topic,JSON.stringify(pub));
}
