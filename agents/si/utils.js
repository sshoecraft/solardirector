
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

function getshort(data,index) {
	var s = (data[index] & 0xF0) << 8 | data[index+1] & 0x0F;
	return s;
}

function putshort(data,index,num) {
	var hex = sprintf("%04x",num);
	data[index++] = "0x" + hex.slice(2,4);
	data[index++] = "0x" + hex.slice(0,2);
	return data;
};

function putlong(data,index,num) {
	var hex = sprintf("%08x",num);
//	dprintf(1,"hex: %s\n", hex);
	data[index++] = "0x" + hex.slice(6,8);
	data[index++] = "0x" + hex.slice(4,6);
	data[index++] = "0x" + hex.slice(2,4);
	data[index++] = "0x" + hex.slice(0,2);
	return data;
};

function si_isvrange(v) { return ((v >= SI_VOLTAGE_MIN) && (v  <= SI_VOLTAGE_MAX)) }
function HOURS(n) { return(n * 3600); }
function MINUTES(n) { return(n * 60); }
//function float_equals(n1,n2) { return(n1 === n2); }

function addstat(str,val,text) {
	if (val) str += "[" + text + "]";
	return str;
}

function si_smanet_get_value(name,timeout) {
	var start,now,diff,val;

	var dlevel = 1;

	dprintf(dlevel,"connected: %d\n", smanet.connected);
	if (!smanet.connected && smanet.connect()) return undefined;

	dprintf(dlevel,"name: %s, timeout: %d\n", name, timeout);
	start = time();
	while(true) {
		dprintf(dlevel,"trying get...\n");
		val = smanet.get(name);
		dprintf(dlevel,"val: %s\n", val);
		if (typeof(val) != "undefined") break;
		now = time();
		diff = now - start;
		dprintf(dlevel,"diff: %d, timeout: %d\n", diff, timeout);
		if (diff >= timeout-1) return undefined;
		sleep(1);
	}
	return val;
}

function si_smanet_set_value(name,value,timeout) {
	var start,now,diff,val;

	var dlevel = 1;

	if (si.readonly) return 0;

	dprintf(dlevel,"name: %s, value: %s, timeout: %d\n", name, value, timeout);

	dprintf(dlevel,"connected: %d\n", smanet.connected);
	if (!smanet.connected) {
		smanet.errmsg = "SMANET is not connected";
		return 1;
	}
	dprintf(dlevel,"name: %s, value: %s\n", name, value);
	start = time();
	while(true) {
		dprintf(dlevel,"trying set...\n");
		if (!smanet.set(name,value)) {
			val = smanet.get(name);
			dprintf(dlevel,"val: %s\n", val);
			if (val == value)
				break;
		}
		now = time();
		diff = now - start;
		dprintf(dlevel,"diff: %d, timeout: %d\n", diff, timeout);
		if (diff >= timeout-1) return 1;
		sleep(1);
	}
	return 0;
}

function si_smanet_getset(name,value,timeout) {
	var old_val;

	var dlevel = 1;

	dprintf(dlevel,"name: %s, value: %s\n", name, value);

	old_val = si_smanet_get_value(name,2);
	dprintf(dlevel,"old_val: %s\n", old_val);
	if (!old_val) return undefined;
	if (old_val == value) return old_val;

	dprintf(dlevel,"Setting...\n");
	if (si_smanet_set_value(name,value,2)) return undefined;

	return old_val;
}

function setcleanval(name,value,format) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, value: %s, format: %s\n", name, value, format);

	if (typeof(format) != "undefined") log_verbose("Setting %s to: " + format + "\n",name,value);
	var p = config.get_property(name);
	dprintf(dlevel,"p: %s\n", p);
	let has_val = false;
	let old_dirty = false;
	if (p) {
		has_val = check_bit(p.flags,CONFIG_FLAG_VALUE);
		old_dirty = p.dirty;
	}
	si[name] = value;
	if (p) {
		dprintf(dlevel,"has_val: %s, old_dirty: %s\n", has_val, old_dirty);
		if (!has_val) p.flags = clear_bit(p.flags,CONFIG_FLAG_VALUE);
		p.dirty = old_dirty;
	}
}

function si_start_grid() {

	var dlevel = 1;

	if (!smanet.connected) return 0;

	dprintf(dlevel,"Starting grid...\n");
	if (si_smanet_set_value("GdManStr","Start",3)) {
		si.errmsg = "unable to set GdManStr to Start: " + smanet.errmsg;
		log_error("%s\n",si.errmsg);
		return 1;
	}
	dprintf(dlevel,"Done!\n");
	si.grid_started = true;
	return 0;
}

function si_stop_grid(force) {

	var dlevel = 1;

	if (!smanet.connected) return 0;

	// Dont turn off if still charging unless forced
	if (typeof(force) == "undefined") force = false;
	dprintf(dlevel,"charge_mode: %s, force: %s\n",si.charge_mode,force);
	if (si.charge_mode && !force) return 1;

	dprintf(dlevel,"Stopping grid...\n");
	if (si_smanet_set_value("GdManStr","Auto",3)) {
		si.errmsg = "unable to set GdManStr to Auto: " + smanet.errmsg;
		log_error("%s\n",si.errmsg);
		return 1;
	}
	dprintf(dlevel,"Done!\n");
	si.grid_started = false;
	return 0;
}

function si_start_gen() {

	var dlevel = 1;

	if (!smanet.connected) return 0;

	dprintf(dlevel,"Starting gen...\n");
	if (si_smanet_set_value("GnManStr","Start",3)) {
		si.errmsg = "unable to set GnManStr to Start: " + smanet.errmsg;
		log_error("%s\n",si.errmsg);
		return 1;
	}
	dprintf(dlevel,"Done!\n");
	si.gen_started = true;
	si.gen_feed_save = si.feed_enabled;
	if (si.feed_enabled) feed_stop(true);
	return 0;
}

function si_stop_gen(force) {

	var dlevel = 1;

	if (!smanet.connected) return 0;

	// Dont turn off if still charging unless forced
	if (typeof(force) == "undefined") force = false;
	dprintf(dlevel,"charge_mode: %s, force: %s\n",si.charge_mode,force);
	if (si.charge_mode && !force) return 1;

	dprintf(dlevel,"Stopping gen...\n");
	if (si_smanet_set_value("GnManStr","Auto",3)) {
		si.errmsg = "unable to set GnManStr to Auto: " + smanet.errmsg;
		log_error("%s\n",si.errmsg);
		return 1;
	}
	dprintf(dlevel,"Done!\n");
	si.gen_started = false;
	if (si.gen_feed_save) feed_start(false);
	return 0;
}
