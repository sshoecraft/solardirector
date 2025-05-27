#!/opt/sd/bin/sdjs

function process_messages() {

        let dlevel = 4;

        dprintf(dlevel,"m: %s\n", m);
        dprintf(dlevel,"agents: %s\n", agents);
        dprintf(dlevel,"mq.length: %d\n", m.mq.length);
	let count = 0;
        for(let i=0; i < m.mq.length; i++) {
                let msg = m.mq[i];
//              dprintf(dlevel,"msg[%d]: %s\n", i, msg);
//              dumpobj(msg);
                dprintf(dlevel,"name: %s, msg.func: %s\n", msg.name, msg.func);
                let info = agents[msg.name];
                have_info = typeof(info) == "undefined" ? false : true;
                dprintf(dlevel,"have_info: %s\n", have_info);
                if (msg.func == "Info" && !have_info) {
                        let data = JSON.parse(msg.data);
                        dprintf(dlevel,"%s: agent_role: %s\n", msg.name, data.agent_role);
                        dprintf(dlevel,"adding: %s\n", msg.name);
                        agents[msg.name] = {};
                        agents[msg.name].agent_role = data.agent_role;
                } else if (msg.func == "Data" && have_info && info.agent_role == SOLARD_ROLE_BATTERY) {
                        dprintf(dlevel,"getting data for %s\n", msg.name);
                        agents[msg.name].data = JSON.parse(msg.data);
                        agents[msg.name].data.last_update = time();
			count++;
                }
        }
        m.purgemq();
	return count;
}

function cellmon_main(argv) {
	script_dir = dirname(script_name);
	lib_dir = (script_dir == "." ? "../../lib" : SOLARD_LIBDIR);
	sdlib_dir = lib_dir + "/sd";
	include(sdlib_dir+"/init.js");
	include(sdlib_dir+"/getopt.js");
//	include(script_dir+"/display.js");

        let opts = [
		[ "-N|dont clear screen", "no_clear", DATA_TYPE_BOOL, "false" ],
		[ "-r:#|refresh interval", "refresh", DATA_TYPE_INT, "1" ],
        ];

	let props = [
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision, trigger, ctx */
		[ "average", DATA_TYPE_BOOL, "yes", 0, ],
		[ "minmax", DATA_TYPE_BOOL, "yes", 0, ],
	];

	let client = new Client(argv,"cellmon","2.0",opts,props);

	agents = {};

	m = new MQTT(client.mqtt.config);
	m.sub(SOLARD_TOPIC_ROOT + "/" + SOLARD_TOPIC_AGENTS + "/#");
//	dumpobj(m);
	let count = 0;
	while(true) {
//		if (process_messages()) {
		process_messages();
		if (!client.opts.no_clear) {
			clear_screen();
			printf("**** %s ****\n",new Date());
		}
// uses a ton of memory
if (0) {
			na = [];
			for(let key in agents) { if (agents[key].data) na.push(agents[key].data); }
			na.sort(function(a,b) {
				let ua = a.name.toUpperCase();
				let ub = b.name.toUpperCase();
				if (ua < ub) return -1;
				else if (ua > ub) return 1
				else return 0;
			});
			display(na);
} else {
		display(agents);
}
		if (count % 60) gc();
		report_mem(true);
		sleep(client.opts.refresh);
	}

}

function gettd(start) {
	let end,ts,diff,mins;

//	diff = difftime(end,start);
	end = time();
	diff = end - start;
	dprintf(3,"start: %ld, end: %ld, diff: %d\n", start, end, diff);
	if (diff > 0) {
		mins = parseInt(diff / 60);
		if (mins > 0) diff -= parseInt(mins * 60);
		dprintf(3,"mins: %d, diff: %d\n", mins, diff);
	} else {
		mins = diff = 0;
	}
	if (mins >= 5) {
		ts = sprintf("--:--");
	} else {
		ts = sprintf("%02d:%02d",mins,diff);
	}
	dprintf(3,"ts: %s\n", ts);
	return ts;
}


function Create2DArray(rows) {
  let arr = [];

  for (let i=0;i<rows;i++) {
     arr[i] = [];
  }

  return arr;
}

function display(agents) {
	let values = Create2DArray(32);
	let summ = Create2DArray(4);
	let bsum = Create2DArray(4);
	let lupd = [];
	let v, cell_total, cell_min, cell_max, cell_diff, cell_avg, cap, kwh;
	let x,y,npacks,cells,max_temps,pack_reported;
	let str = [];
	let slabels = [ "Min","Max","Avg","Diff" ];
	let tlabels = [ "Current","Voltage" ];
	let cdb;

	let dlevel = 1;

	const NSUMM = 4
	const NBSUM = 2
	const COLUMN_WIDTH = 7

	const BATTERY_STATE_UPDATED = 0x01;

	function solard_check_state(x,y) { return 1; }

	function FTEMP(v) { return ( ( ( parseFloat(v) * 9.0) / 5.0) + 32.0) }

if (0) {
	npacks = agents.length;
} else {
	npacks = 0;
	for(key in agents) {
		pp = agents[key].data;
		if (!pp) continue;
		npacks++;
	}
}
	dprintf(dlevel,"npacks: %d\n", npacks);
	if (!npacks) return;

	cells = 0;
	max_temps = 0;
	for(key in agents) {
		pp = agents[key].data;
//	for(let i = 0; i < agents.length; i++) {
//		pp = agents[i];
		if (!pp) continue;
//		printf("%s pp: %s\n", key, typeof(pp));
//		dprintf(dlevel,"updated: %d\n", solard_check_state(pp,BATTERY_STATE_UPDATED));
//		if (!solard_check_state(pp,BATTERY_STATE_UPDATED)) continue;
		dprintf(dlevel,"pp.ntemps: %d, max_temps: %d\n", pp.ntemps, max_temps);
		if (pp.ntemps > max_temps) max_temps = pp.ntemps;
		if (!cells) cells = pp.ncells;
	}
	dprintf(dlevel,"max_temps: %d\n", max_temps);
	let temps = Create2DArray(max_temps);
	dprintf(dlevel,"cells: %d\n",cells);
	if (!cells) return;
	x = 0;
	cap = 0.0;
	pack_reported = 0;
	let total_current = 0.0;
	for(key in agents) {
		pp = agents[key].data;
//	for(let i = 0; i < agents.length; i++) {
//		pp = agents[i];
		if (!pp) continue;
		if (!solard_check_state(pp,BATTERY_STATE_UPDATED)) {
			cell_min = cell_max = cell_avg = cell_total = 0;
		} else {
			cell_min = 9999999999.0;
			cell_max = 0.0;
			cell_total = 0;
			for(y=0; y < pp.ncells; y++) {
				v = pp.cellvolt[y];
				if (v < cell_min) cell_min = v;
				if (v > cell_max) cell_max = v;
				cell_total += v;
				values[y][x] = v;
//				dprintf(dlevel,"values[%d][%d]: %.3f\n", y, x, values[y][x]);
			}
			cap += pp.capacity;
			pack_reported++;
			for(y=0; y < pp.ntemps; y++) temps[y][x] = pp.temps[y];
			cell_avg = cell_total / pp.ncells;
		}
		cell_diff = cell_max - cell_min;
		dprintf(dlevel,"cells: total: %.3f, min: %.3f, max: %.3f, diff: %.3f, avg: %.3f\n",
			cell_total, cell_min, cell_max, cell_diff, cell_avg);
		summ[0][x] = cell_min;
		summ[1][x] = cell_max;
		summ[2][x] = cell_avg;
		summ[3][x] = cell_diff;
		bsum[0][x] = pp.current;
		bsum[1][x] = cell_total;
		total_current += parseFloat(pp.current);
		lupd[x] = gettd(pp.last_update);
		x++;
	}

//	if (!debug) system("clear; echo \"**** $(date) ****\"");

	let format = sprintf("%%-%d.%ds ",COLUMN_WIDTH,COLUMN_WIDTH);
	dprintf(dlevel,"format: %s\n", format);
	/* Header */
	printf(format,"");
	x = 1;
	for(key in agents) {
		pp = agents[key].data;
//	for(let i = 0; i < agents.length; i++) {
//		pp = agents[i];
		if (!pp) continue;
		if (pp.name.length > COLUMN_WIDTH) {
			let temp = pp.name.substring(pp.name.length - COLUMN_WIDTH);
			printf(format,temp);
		} else {
			printf(format,pp.name);
		}
	}
	printf("\n");
	/* Lines */
	printf(format,"");
	for(key in agents) {
		pp = agents[key].data;
//	for(let i = 0; i < agents.length; i++) {
//		pp = agents[i];
		if (!pp) continue;
		printf(format,"----------------------");
	}
	printf("\n");

	/* Charge/Discharge/Balance */
	printf(format,"CDB");
	for(key in agents) {
		pp = agents[key].data;
//	for(let i = 0; i < agents.length; i++) {
//		pp = agents[i];
		if (!pp) continue;
		cdb = ""
		cdb += pp.state.match(/charging/i) ? '*' : ' ';
		cdb += pp.state.match(/discharging/i) ? '*' : ' ';
		cdb += pp.state.match(/balancing/i) ? '*' : ' ';
		printf(format,cdb);
	}
	printf("\n");

	/* Temps */
	for(y=0; y < max_temps; y++) {
		str = sprintf("Temp%d",y+1);
		printf(format,str);
		for(x=0; x < npacks; x++) {
			str = sprintf("%2.1f",FTEMP(temps[y][x]));
//			str = sprintf("%2.1f",temps[y][x]);
			printf(format,str);
		}
		printf("\n");
	}
	if (max_temps) printf("\n");

	/* Cell values */
	for(y=0; y < cells; y++) {
		str = sprintf("Cell%02d",y+1);
		printf(format,str);
		for(x=0; x < npacks; x++) {
			str = sprintf("%.3f",values[y][x]);
			printf(format,str);
		}
		printf("\n");
	}
	printf("\n");

	/* Summ values */
	for(y=0; y < NSUMM; y++) {
		printf(format,slabels[y]);
		for(x=0; x < npacks; x++) {
			str = sprintf("%.3f",summ[y][x]);
			printf(format,str);
		}
		printf("\n");
	}
	printf("\n");

	/* Current/Total */
	for(y=0; y < NBSUM; y++) {
		printf(format,tlabels[y]);
		for(x=0; x < npacks; x++) {
			str = sprintf("%2.3f",bsum[y][x]);
			printf(format,str);
		}
		printf("\n");
	}

	/* Last update */
	printf("\n");
	printf(format,"updated");
	for(x=0; x < npacks; x++) printf(format,lupd[x]);
	printf("\n");

	printf("\n");
	printf("Total current: %.1f\n", total_current);
	printf("Total packs: %d\n", pack_reported);
	kwh = (cap * 48.0) / 1000.0;
//	printf("Capacity: %2.1f\n", cap);
	printf("kWh: %2.1f (80%%: %2.1f, 60%%: %2.1f)\n", kwh, (kwh * .8), (kwh * .6));
}
