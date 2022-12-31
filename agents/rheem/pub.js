
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

function rheem_publish() {
	let fields = [ "id", "enabled", "running", "status", "mode", "temp", "level" ];
	let j = JSON.stringify(rheem.devices,fields,4);
	printf("%s\n", j);
	mqtt.pub(SOLARD_TOPIC_ROOT+"/Agents/"+rheem.agent.name+"/"+SOLARD_FUNC_DATA,j,0);
}

function pub_main() { rheem_publish(); }
