
function sdconfig(instance,agent_name,func_name) {

	let dlevel = 1;

	dprintf(dlevel,"caller: %s, agent_name: %s, func_name: %s\n", getCallerFunctionName(), agent_name, func_name);

	let agent_topic = sprintf("%s/%s/%s",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_AGENTS,agent_name);
	dprintf(dlevel,"agent_topic: %s\n", agent_topic);

	let info_topic = agent_topic + "/" + SOLARD_FUNC_INFO;
	dprintf(dlevel,"info_topic: %s\n", info_topic);

	function _mkresults(status,message,data) {
		let reply = {};
		reply.status = status;
		reply.message = message;
		reply.results = data;
		return reply;
	}
	function _mkerror(msg) {
		return _mkresults(1,msg,{});
	}

	if (typeof(instance.__sdconfig) == 'undefined') instance.__sdconfig = {};
	if (typeof(instance.__sdconfig.mqtt_instance) == 'undefined') {
		dprintf(dlevel,"creating info instance ...\n");
		instance.__sdconfig.mqtt_instance = new MQTT(instance.mqtt.config);
		let my_id = instance.__sdconfig.mqtt_instance.clientid;
		dprintf(dlevel,"my clientid: %s\n", my_id);
		instance.__sdconfig.mqtt_instance.sub(sprintf("%s/%s/%s/#",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_CLIENTS,my_id));
	}
	let sdm = instance.__sdconfig.mqtt_instance;

	// Do we have this info already?
	if (typeof(instance.__sdconfig.info) == 'undefined') instance.__sdconfig.info = {}
	let info = instance.__sdconfig.info[agent_name];
	dprintf(dlevel,"info: %s\n", info);
	if (typeof(info) == 'undefined') {
		// sub to the agent info topic
		sdm.sub(info_topic);
		delay(500);
		dprintf(dlevel,"sdm.mq.length: %d\n", sdm.mq.length);
		if (!sdm.mq.length) return _mkerror("unable to get info for agent "+agent_name);
		for(let i=sdm.mq.length-1; i >= 0; i--) {
			dprintf(dlevel,"i: %d\n", i);
			let msg = sdm.mq[i];
			if (msg.func == "Info") instance.__sdconfig.info[msg.name] = JSON.parse(msg.data);
		}
		sdm.unsub(info_topic);
		sdm.purgemq();
		info = instance.__sdconfig.info[agent_name];
		dprintf(dlevel,"info: %s\n", info);
	}

	if (typeof(info) == "undefined") return _mkerror("unable to get info for agent "+agent_name);
	if (func_name == "***VARS***") return _mkresults(0,"success",{ variables: info.configuration[0] });
	if (func_name == "***FUNCS***") return _mkresults(0,"success",{ functions: info.functions });
	let funcs = info.functions;
	let func;
	for(let i=0; i < funcs.length; i++) {
		dprintf(dlevel,"func: %s\n", funcs[i].name);
		if (funcs[i].name == func_name) {
			func = funcs[i];
			break;
		}
	}
	if (!func) return _mkerror(sprintf("agent %s does not support function %s",agent_name,func_name));
	dprintf(dlevel,"nargs: %d\n", func.nargs);

	let req = "{\"" + func_name + "\":["
	let n = arguments.length - 3;
	if (n != func.nargs) return _mkerror(sprintf("sdconfig func %s requires %d arguments but %d passed", func_name, func.nargs, n));
	if (func.nargs > 1) req += "[";
	for(let i = 3; i < arguments.length; i++) {
		dprintf(dlevel,"arg[%d]: %s\n", i, arguments[i]);
		if (i > 3) req += ",";
		req += "\"" + arguments[i] + "\"";
	}
	if (func.nargs > 1) req += "]";
	req += "]}"
	dprintf(dlevel,"req: %s\n", req);
	sdm.pub(agent_topic,req);
	let retries = 10;
	while(retries--) {
		if (sdm.mq.length) break;
		delay(500);
	}
	dprintf(dlevel,"retries: %d\n", retries);
	if (retries < 0) return _mkerror("agent "+agent_name+" not responding");;
	dprintf(dlevel,"sdm.mq.length: %d\n", sdm.mq.length);
	let data = "";
	for(let i=sdm.mq.length-1; i >= 0; i--) {
		msg = sdm.mq[i];
		dprintf(dlevel,"func: %s\n", msg.func);
		if (msg.func.length) continue;
		data = msg.data;
	}
	if (!data.length) _mkerror("internal error: no data received");
	// JSON.parse returns FALSE and stops execution if json string is not valid - use test_json instead
	if (!test_json(data)) return _mkerror("error parsing reply from agent "+agent_name);
	let r = JSON.parse(data);
	sdm.purgemq();
	return r;
}
