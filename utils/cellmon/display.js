include(dirname(script_name)+"/../../core/utils.js");

COLUMN_WIDTH = 7

function FTEMP(v) { return ( ( ( parseFloat(v) * 9.0) / 5.0) + 32.0) }

function gettd(start) {
	var end,ts,diff,mins;

//	diff = difftime(end,start);
	end = time();
	diff = end - start;
	dprintf(0,"start: %ld, end: %ld, diff: %d\n", start, end, diff);
	if (diff > 0) {
		mins = parseInt(diff / 60);
		if (mins > 0) diff -= parseInt(mins * 60);
		dprintf(0,"mins: %d, diff: %d\n", mins, diff);
	} else {
		mins = diff = 0;
	}
	if (mins >= 5) {
		ts = sprintf("--:--");
	} else {
		ts = sprintf("%02d:%02d",mins,diff);
	}
	dprintf(0,"ts: %s\n", ts);
	return ts;
}

NSUMM = 4
NBSUM = 2

BATTERY_STATE_UPDATED = 0x01;
function solard_check_state(x,y) { return 1; }

function Create2DArray(rows) {
  var arr = [];

  for (var i=0;i<rows;i++) {
     arr[i] = [];
  }

  return arr;
}

function display(agents) {
	var values = Create2DArray(32);
	var summ = Create2DArray(4);
	var bsum = Create2DArray(4);
	var lupd = [];
	var v, cell_total, cell_min, cell_max, cell_diff, cell_avg, cap, kwh;
	var x,y,npacks,cells,max_temps,pack_reported;
	var str = [];
	var slabels = [ "Min","Max","Avg","Diff" ];
	var tlabels = [ "Current","Voltage" ];
	var cdb;

	npacks = 0;
	for(key in agents) {
		pp = agents[key].data;
		if (!pp) continue;
		npacks++;
	}
	dprintf(0,"npacks: %d\n", npacks);
	if (!npacks) return;

	cells = 0;
	max_temps = 0;
	for(key in agents) {
		pp = agents[key].data;
		if (!pp) continue;
//		printf("%s pp: %s\n", key, typeof(pp));
//		dprintf(0,"updated: %d\n", solard_check_state(pp,BATTERY_STATE_UPDATED));
//		if (!solard_check_state(pp,BATTERY_STATE_UPDATED)) continue;
		printf("pp.ntemps: %d, max_temps: %d\n", pp.ntemps, max_temps);
		if (pp.ntemps > max_temps) max_temps = pp.ntemps;
		if (!cells) cells = pp.ncells;
	}
	printf("max_temps: %d\n", max_temps);
	var temps = Create2DArray(max_temps);
	dprintf(0,"cells: %d\n",cells);
	if (!cells) return;
	x = 0;
	cap = 0.0;
	pack_reported = 0;
	for(key in agents) {
		pp = agents[key].data;
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
//				dprintf(0,"values[%d][%d]: %.3f\n", y, x, values[y][x]);
			}
			cap += pp.capacity;
			pack_reported++;
			for(y=0; y < pp.ntemps; y++) temps[y][x] = pp.temps[y];
			cell_avg = cell_total / pp.ncells;
		}
		cell_diff = cell_max - cell_min;
		dprintf(0,"cells: total: %.3f, min: %.3f, max: %.3f, diff: %.3f, avg: %.3f\n",
			cell_total, cell_min, cell_max, cell_diff, cell_avg);
		summ[0][x] = cell_min;
		summ[1][x] = cell_max;
		summ[2][x] = cell_avg;
		summ[3][x] = cell_diff;
		bsum[0][x] = pp.current;
		bsum[1][x] = cell_total;
		lupd[x] = gettd(pp.last_update);
		x++;
	}

//	if (!debug) system("clear; echo \"**** $(date) ****\"");

	var format = sprintf("%%-%d.%ds ",COLUMN_WIDTH,COLUMN_WIDTH);
	printf("format: %s\n", format);
	/* Header */
	printf(format,"");
	x = 1;
	for(key in agents) {
		pp = agents[key].data;
		if (!pp) continue;
		if (pp.name.length > COLUMN_WIDTH) {
			var temp = pp.name.substring(pp.name.length - COLUMN_WIDTH);
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
		if (!pp) continue;
		printf(format,"----------------------");
	}
	printf("\n");

	/* Charge/Discharge/Balance */
	printf(format,"CDB");
	for(key in agents) {
		pp = agents[key].data;
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
	printf("Total packs: %d\n", pack_reported);
	kwh = (cap * 48.0) / 1000.0;
//	printf("Capacity: %2.1f\n", cap);
	printf("kWh: %2.1f (80%%: %2.1f, 60%%: %2.1f)\n", kwh, (kwh * .8), (kwh * .6));
}
