
#include "solard.h"

float pct(float val1, float val2) {
	float val;

	dprintf(5,"val1: %.3f, val2: %.3f\n", val1, val2);
	val = 100 - ((val2 / val1)  * 100.0);
	dprintf(5,"val(1): %.3f\n", val);
	if (val < 0.0) val *= -1.0;
	dprintf(5,"val(2): %.3f\n", val);
	dprintf(5,"returning: %f\n", val);
	return val;
}

static int ncount = 0;

void do_notify(solard_config_t *conf, list lp) {
	char logfile[256],*args[32],*p;
	int i,status;
	pid_t pid;

	if (!strlen(conf->notify_path)) {
		log_error("no notify path set, unable to send message!\n");
		while((p = list_get_next(lp)) != 0) log_error(p);
		return;
	}

	pid = getpid();
	sprintf(logfile,"%s/notify_%d.log",SOLARD_LOGDIR,ncount++);
	dprintf(1,"logfile: %s\n", logfile);

	/* Exec the agent */
	i = 0;
	args[i++] = conf->notify_path;
	list_reset(lp);
	while((p = list_get_next(lp)) != 0) args[i++] = p;
	args[i++] = 0;
	pid = solard_exec(conf->notify_path,args,logfile,0);
	dprintf(1,"pid: %d\n", pid);
	if (pid > 0) status = solard_wait(pid);
	else status = 1;
	if (status != 0) {
		log_write(LOG_SYSERR,"unable to send message: see %s for detais",logfile);
		return;
	}
	unlink(logfile);
}

void solard_monitor(solard_config_t *conf) {
	solard_agentinfo_t *info;
	float total,count,avg,p;
	char reason[128],*name;
	solard_battery_t *bp;
	solard_inverter_t *inv;
	list lp,notify_list;
	time_t now;
	int i,last,diff;

	time(&now);
	notify_list = list_create();

	lp = list_create();
	total = count = 0.0;
	list_reset(conf->agents);
	while((info = list_get_next(conf->agents)) != 0) {
//		dprintf(1,"info->data: %p\n", info->data);
		if (!info->data) continue;
		if (strcmp(info->role,SOLARD_ROLE_BATTERY) == 0) {
			bp = info->data;
			if (!check_state(bp,BATTERY_STATE_UPDATED)) continue;
			name = bp->name;
			last = bp->last_update;
			inv = 0;
		} else if (strcmp(info->role,SOLARD_ROLE_INVERTER) == 0) {
			inv = info->data;
			if (!check_state(inv,SOLARD_INVERTER_STATE_UPDATED)) continue;
			name = inv->name;
			last = inv->last_update;
			bp = 0;
		} else {
			dprintf(2,"unknown role: %s\n", info->role);
			continue;
		}
		diff = now - last;
		dprintf(2,"%s: last: %d, now: %d, diff: %d, notify: %d\n", name, last, now, diff, conf->agent_notify);
		if (conf->agent_notify > 0) {
			if (diff >= conf->agent_notify) {
				dprintf(5,"notify_gone state: %d\n", check_state(info,AGENTINFO_NOTIFY_GONE));
				if (!check_state(info,AGENTINFO_NOTIFY_GONE)) {
					sprintf(reason,"%s %s has not reported for %d seconds",info->role,name,diff);
					list_add(notify_list,reason,strlen(reason)+1);
					set_state(info,AGENTINFO_NOTIFY_GONE);
				}
				continue;
			} else {
				clear_state(info,AGENTINFO_NOTIFY_GONE);
			}
		} else if (diff >= conf->agent_warning) {
			continue;
		}
		if (bp) {
			dprintf(2,"bp->voltage: %.1f\n", bp->voltage);
			if (bp->voltage < 1.0 || bp->voltage > 100.0) {
				if (!check_state(info,AGENTINFO_NOTIFY_VOLT)) {
					sprintf(reason,"Battery %s has an invalid voltage: %f",bp->name,bp->voltage);
					list_add(notify_list,reason,strlen(reason)+1);
					set_state(info,AGENTINFO_NOTIFY_VOLT);
				}
			} else {
				list_add(lp,info,sizeof(*info));
				total += bp->voltage;
				count++;
			}
		} else if (inv) {
			dprintf(2,"inv->soc: %.1f\n", inv->soc);
			if (inv->soc <= 35.0) {
				if (!check_state(info,AGENTINFO_NOTIFY_VOLT)) {
					sprintf(reason,"Inverter %s capacity is %.1f%%!",inv->name,inv->soc);
					list_add(notify_list,reason,strlen(reason)+1);
					set_state(info,AGENTINFO_NOTIFY_VOLT);
				}
			} else {
				clear_state(info,AGENTINFO_NOTIFY_VOLT);
			}
		}
	}
	if (count) {
		avg = total / count;
		dprintf(5,"avg: %f\n", avg);

		/* Check voltage against average */
		list_reset(lp);
		while((info = list_get_next(lp)) != 0) {
			bp = info->data;
			dprintf(5,"%s: now: %ld, last_update: %ld, diff: %ld\n", bp->name, now, bp->last_update, (now - bp->last_update));
			if ((now - bp->last_update) > 30) continue;
			dprintf(5,"voltage: %f, average: %f\n", bp->voltage, avg);
			p = pct(bp->voltage,avg);
			if (p >= 2.0) {
				if (!check_state(info,AGENTINFO_NOTIFY_VOLT)) {
					sprintf(reason,"Battery %s voltage (%.1f) differs from average (%.1f) by %.2f%%",
						bp->name,bp->voltage,avg,p);
					list_add(notify_list,reason,strlen(reason)+1);
					set_state(info,AGENTINFO_NOTIFY_VOLT);
				}
			} else {
				clear_state(info,AGENTINFO_NOTIFY_VOLT);
			}
		}
		list_destroy(lp);
	}

	/* Temps */
	list_reset(conf->agents);
	while((info = list_get_next(conf->agents)) != 0) {
		dprintf(5,"name: %s, role: %s\n", info->name, info->role);
		if (strcmp(info->role,SOLARD_ROLE_BATTERY) != 0) continue;
		bp = info->data;
		dprintf(1,"bp: %p\n", bp);
		if (!bp) continue;
		if (!check_state(bp,BATTERY_STATE_UPDATED)) continue;
		dprintf(5,"ntemps: %d\n", bp->ntemps);
		for(i=0; i < bp->ntemps; i++) {
			dprintf(5,"%s: temp[%d]: %.1f\n",bp->name,i,bp->temps[i]);
			if (bp->temps[i] <= 5 || bp->temps[i] >= 30) {
				if (!check_state(info,AGENTINFO_NOTIFY_TEMP)) {
					sprintf(reason,"Battery %s %s temp: %.1f",bp->name,(bp->temps[i] < 5 ? "low" : "high"),bp->temps[i]);
					list_add(notify_list,reason,strlen(reason)+1);
					set_state(info,AGENTINFO_NOTIFY_TEMP);
				}
				break;
			} else {
				clear_state(info,AGENTINFO_NOTIFY_TEMP);
			}
		}
	}

	/* bat charging/discharging */
	/* inverter voltage */

	if (list_count(notify_list) > 0) do_notify(conf,notify_list);
}
