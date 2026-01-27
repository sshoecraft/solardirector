#!/usr/bin/env python3
"""
HVAC Daily Report
Generates statistics for HVAC system operation over a 24-hour period.
Reads from InfluxDB hvac database.
"""

import argparse
import json
import urllib.request
import urllib.parse
from datetime import datetime, timedelta, timezone
from collections import defaultdict
import os
import time

INFLUX_HOST = "http://192.168.1.168:8086"
INFLUX_DB = "hvac"

def get_system_timezone():
    """Get the system timezone name (e.g., 'America/Chicago')."""
    # Try to read from /etc/timezone first (Debian/Ubuntu)
    try:
        with open('/etc/timezone', 'r') as f:
            return f.read().strip()
    except:
        pass
    # Try the TZ environment variable
    tz = os.environ.get('TZ')
    if tz:
        return tz
    # Fall back to reading the localtime symlink
    try:
        link = os.readlink('/etc/localtime')
        # Extract timezone from path like /usr/share/zoneinfo/America/Chicago
        if 'zoneinfo/' in link:
            return link.split('zoneinfo/')[-1]
    except:
        pass
    return 'America/Chicago'  # Default fallback

LOCAL_TZ = get_system_timezone()

def influx_query(query, db=INFLUX_DB):
    """Execute InfluxDB query and return results."""
    params = urllib.parse.urlencode({'db': db, 'q': query})
    url = f"{INFLUX_HOST}/query?{params}"

    try:
        with urllib.request.urlopen(url) as response:
            data = json.loads(response.read().decode())
            if 'results' in data and len(data['results']) > 0:
                return data['results'][0]
    except Exception as e:
        print(f"Query error: {e}")
    return None

def get_state_changes(measurement, field, start_time, end_time, name_filter=None):
    """Get state changes for a measurement field."""
    where = f"time >= '{start_time}' and time <= '{end_time}'"
    if name_filter:
        where += f" and \"name\" = '{name_filter}'"

    query = f"SELECT time, {field}, \"name\" FROM {measurement} WHERE {where}"
    result = influx_query(query)

    if not result or 'series' not in result:
        return []

    changes = []
    for series in result['series']:
        cols = series['columns']
        time_idx = cols.index('time')
        field_idx = cols.index(field)
        name_idx = cols.index('name') if 'name' in cols else None

        prev_state = None
        for row in series['values']:
            state = row[field_idx]
            if state != prev_state:
                changes.append({
                    'time': datetime.fromisoformat(row[time_idx].replace('Z', '+00:00')),
                    'state': state,
                    'name': row[name_idx] if name_idx else None
                })
                prev_state = state

    return sorted(changes, key=lambda x: x['time'])

def calculate_runtime(changes, running_states=['Running', 'on', 1, 1.0]):
    """Calculate total runtime and cycles from state changes."""
    total_runtime = timedelta()
    cycles = 0
    start_time = None
    cycle_durations = []
    off_durations = []
    last_off_time = None

    for change in changes:
        if change['state'] in running_states:
            if start_time is None:
                start_time = change['time']
                cycles += 1
                if last_off_time:
                    off_durations.append(change['time'] - last_off_time)
        else:
            if start_time is not None:
                duration = change['time'] - start_time
                total_runtime += duration
                cycle_durations.append(duration)
                last_off_time = change['time']
                start_time = None

    # If still running at end of period
    still_running = start_time is not None

    return {
        'runtime': total_runtime,
        'cycles': cycles,
        'cycle_durations': cycle_durations,
        'off_durations': off_durations,
        'still_running': still_running,
        'last_start': start_time
    }

def get_temp_stats(measurement, field, start_time, end_time, name_filter=None):
    """Get min/max/avg temperature stats."""
    where = f"time >= '{start_time}' and time <= '{end_time}' and {field} > -100"
    if name_filter:
        where += f" and \"name\" = '{name_filter}'"

    query = f"SELECT min({field}), max({field}), mean({field}) FROM {measurement} WHERE {where}"
    result = influx_query(query)

    if not result or 'series' not in result:
        return None

    values = result['series'][0]['values'][0]
    return {
        'min': values[1],
        'max': values[2],
        'avg': values[3]
    }

def get_outside_temp_stats(start_time, end_time):
    """Get outside temperature stats (stored as 'temp' in 'ac' measurement)."""
    return get_temp_stats('ac', 'temp', start_time, end_time)

def get_water_temp_stats(start_time, end_time):
    """Get water temperature stats from ac measurement."""
    return get_temp_stats('ac', 'water_temp', start_time, end_time)

def get_unit_stats(start_time, end_time):
    """Get per-unit statistics."""
    unit_stats = {}

    # Get all unit data and extract unique names
    query = f"SELECT \"name\", state FROM units WHERE time >= '{start_time}' and time <= '{end_time}'"
    result = influx_query(query)

    if not result or 'series' not in result:
        return unit_stats

    # Extract unique unit names
    cols = result['series'][0]['columns']
    name_idx = cols.index('name')
    units_set = set()
    for row in result['series'][0]['values']:
        if row[name_idx]:
            units_set.add(row[name_idx])

    for unit in units_set:
        changes = get_state_changes('units', 'state', start_time, end_time, name_filter=unit)
        stats = calculate_runtime(changes, running_states=['Running'])

        # Get mode
        query = f"SELECT last(mode) FROM units WHERE time >= '{start_time}' and time <= '{end_time}' AND \"name\" = '{unit}'"
        result = influx_query(query)
        if result and 'series' in result:
            stats['mode'] = result['series'][0]['values'][0][1]
        else:
            stats['mode'] = 'Unknown'

        unit_stats[unit] = stats

    return unit_stats

def get_direct_stats(start_time, end_time):
    """Get direct mode statistics."""
    changes = get_state_changes('directs', 'state', start_time, end_time)
    stats = calculate_runtime(changes, running_states=['Running', 'RUNNING'])

    # Valve actuations = cycles * 2 (open + close)
    stats['valve_actuations'] = stats['cycles'] * 2
    if stats['still_running']:
        stats['valve_actuations'] -= 1  # Hasn't closed yet

    return stats

def get_storage_mode_changes(start_time, end_time):
    """Get storage mode changes during the period."""
    changes = get_state_changes('ac', 'mode', start_time, end_time)
    return changes

def get_direct_mode_at_time(t, start_time, end_time):
    """Get the direct mode (Heat/Cool) at a specific time."""
    # Query for mode value at or just after time t (when cycle starts, mode is set)
    t_str = t.strftime('%Y-%m-%dT%H:%M:%SZ')
    # Look for mode within a few seconds after cycle start
    query = f"SELECT mode FROM directs WHERE time >= '{t_str}' AND time <= '{end_time}' AND mode != 'None' ORDER BY time ASC LIMIT 1"
    result = influx_query(query)
    if result and 'series' in result:
        mode = result['series'][0]['values'][0][1]
        if mode and mode != 'None':
            return mode
    return None

def get_fan_stats(start_time, end_time):
    """Get per-fan statistics."""
    fan_stats = {}

    # Get all fan data and extract unique names
    query = f"SELECT \"name\", state FROM fans WHERE time >= '{start_time}' and time <= '{end_time}'"
    result = influx_query(query)

    if not result or 'series' not in result:
        return fan_stats

    # Extract unique fan names
    cols = result['series'][0]['columns']
    name_idx = cols.index('name')
    fans = set()
    for row in result['series'][0]['values']:
        if row[name_idx]:
            fans.add(row[name_idx])

    for fan in fans:
        changes = get_state_changes('fans', 'state', start_time, end_time, name_filter=fan)
        fan_stats[fan] = calculate_runtime(changes, running_states=['Running'])

    return fan_stats

def get_pump_stats(start_time, end_time):
    """Get per-pump statistics."""
    pump_stats = {}

    # Get all pump data and extract unique names
    query = f"SELECT \"name\", state FROM pumps WHERE time >= '{start_time}' and time <= '{end_time}'"
    result = influx_query(query)

    if not result or 'series' not in result:
        return pump_stats

    # Extract unique pump names
    cols = result['series'][0]['columns']
    name_idx = cols.index('name')
    pumps = set()
    for row in result['series'][0]['values']:
        if row[name_idx]:
            pumps.add(row[name_idx])

    for pump in pumps:
        changes = get_state_changes('pumps', 'state', start_time, end_time, name_filter=pump)
        stats = calculate_runtime(changes, running_states=['Running'])

        # Get temperature stats
        temp_in = get_temp_stats('pumps', 'temp_in', start_time, end_time, name_filter=pump)
        temp_out = get_temp_stats('pumps', 'temp_out', start_time, end_time, name_filter=pump)

        stats['temp_in'] = temp_in
        stats['temp_out'] = temp_out
        pump_stats[pump] = stats

    return pump_stats

def format_duration(td):
    """Format timedelta as H:MM:SS."""
    if td is None:
        return "-"
    total_seconds = int(td.total_seconds())
    hours = total_seconds // 3600
    minutes = (total_seconds % 3600) // 60
    seconds = total_seconds % 60
    return f"{hours}:{minutes:02d}:{seconds:02d}"

def format_duration_short(td):
    """Format timedelta as M:SS for shorter durations."""
    if td is None:
        return "-"
    total_seconds = int(td.total_seconds())
    minutes = total_seconds // 60
    seconds = total_seconds % 60
    return f"{minutes}:{seconds:02d}"

def format_temp(stats):
    """Format temperature stats."""
    if not stats or stats['min'] is None:
        return "-"
    return f"{stats['min']:.0f}-{stats['max']:.0f}°F (avg {stats['avg']:.0f}°F)"

def get_utc_offset():
    """Get current UTC offset in hours (handles DST)."""
    # time.timezone is seconds west of UTC, time.altzone is for DST
    # time.daylight tells us if DST is defined, time.localtime().tm_isdst tells if DST is active
    if time.daylight and time.localtime().tm_isdst:
        return -time.altzone / 3600
    return -time.timezone / 3600

def to_local_time(utc_dt):
    """Convert UTC datetime to local time."""
    offset = get_utc_offset()
    return utc_dt + timedelta(hours=offset)

def get_hourly_temps(start_time, end_time):
    """Get hourly average outside and water temperatures."""
    # Query outside temp and water temp separately to handle invalid values
    # Water temp: only include valid readings (> -100, as INVALID_TEMP is -200)
    # Outside temp: filter out invalid readings as well

    outside_query = f"SELECT mean(temp) FROM ac WHERE time >= '{start_time}' AND time <= '{end_time}' AND temp > -100 GROUP BY time(1h)"
    water_query = f"SELECT mean(water_temp) FROM ac WHERE time >= '{start_time}' AND time <= '{end_time}' AND water_temp > -100 GROUP BY time(1h)"

    outside_result = influx_query(outside_query)
    water_result = influx_query(water_query)

    # Build lookup for water temps by hour
    water_by_hour = {}
    if water_result and 'series' in water_result:
        for row in water_result['series'][0]['values']:
            if row[1] is not None:
                water_by_hour[row[0]] = row[1]

    hourly = []
    if outside_result and 'series' in outside_result:
        for row in outside_result['series'][0]['values']:
            t = datetime.fromisoformat(row[0].replace('Z', '+00:00'))
            hourly.append({
                'time': t,
                'outside': row[1],
                'water': water_by_hour.get(row[0])
            })
    return hourly

def print_storage_section(start_time, end_time):
    """Print storage mode section with water temp."""
    changes = get_storage_mode_changes(start_time, end_time)
    water = get_water_temp_stats(start_time, end_time)

    print(f"\n{'Storage:':<18}")

    # Water temp summary
    if water and water['min'] is not None:
        print(f"  Water Temp: {water['min']:.0f}-{water['max']:.0f}°F (avg {water['avg']:.0f}°F)")
    else:
        print(f"  Water Temp: -")

    # Mode info
    if not changes:
        print(f"  Mode: No data")
        return

    modes = [c['state'] for c in changes]
    unique_modes = list(dict.fromkeys(modes))

    if len(unique_modes) == 1:
        print(f"  Mode: {unique_modes[0]} (constant)")
    else:
        print(f"  Mode changes:")
        for change in changes:
            local_time = to_local_time(change['time'])
            print(f"    {local_time.strftime('%H:%M:%S')} -> {change['state']}")

def get_state_at_start(measurement, field, start_time):
    """Check if measurement was in a running state at start of period."""
    query = f"SELECT {field} FROM {measurement} WHERE time < '{start_time}' ORDER BY time DESC LIMIT 1"
    result = influx_query(query)
    if result and 'series' in result:
        return result['series'][0]['values'][0][1]
    return None

def get_mode_at_time(measurement, t, start_time, end_time, name_filter=None):
    """Get the mode at a specific time for a measurement."""
    t_str = t.strftime('%Y-%m-%dT%H:%M:%SZ')
    where = f"time >= '{t_str}' AND time <= '{end_time}'"
    if name_filter:
        where += f" AND \"name\" = '{name_filter}'"
    query = f"SELECT mode FROM {measurement} WHERE {where} AND mode != 'None' ORDER BY time ASC LIMIT 1"
    result = influx_query(query)
    if result and 'series' in result:
        mode = result['series'][0]['values'][0][1]
        if mode and mode != 'None':
            return mode
    return None

def print_generic_cycle_table(measurement, name, start_time, end_time, title=None):
    """Print detailed cycle table for a unit or fan with continuous timeline."""
    changes = get_state_changes(measurement, 'state', start_time, end_time, name_filter=name)

    # Check if already running at start of period
    start_dt = datetime.fromisoformat(start_time.replace('Z', '+00:00'))
    end_dt = datetime.fromisoformat(end_time.replace('Z', '+00:00'))
    query = f"SELECT state FROM {measurement} WHERE time < '{start_time}' AND \"name\" = '{name}' ORDER BY time DESC LIMIT 1"
    result = influx_query(query)
    initial_state = result['series'][0]['values'][0][1] if result and 'series' in result else None
    already_running = initial_state in ['Running', 'RUNNING']

    if not changes and not already_running:
        return  # No cycles to print

    if title:
        print(f"\n{title}:")
    else:
        print(f"\n{name} Cycles:")
    print(f"{'Date':<12} {'Start':<10} {'End':<10} {'Duration':>8} {'Mode':<6}")
    print(f"{'-'*12} {'-'*10} {'-'*10} {'-'*8} {'-'*6}")

    # Build list of periods (both ON and OFF)
    periods = []
    current_time = start_dt
    is_running = already_running
    current_mode = None

    if already_running:
        current_mode = get_mode_at_time(measurement, start_dt, start_time, end_time, name_filter=name)

    for change in changes:
        change_time = change['time']

        if change_time > current_time:
            # Add period from current_time to change_time
            if is_running:
                mode = current_mode if current_mode else "Heat"
            else:
                mode = "Off"
            periods.append((current_time, change_time, mode))

        # Update state
        if change['state'] in ['Running', 'RUNNING']:
            is_running = True
            current_mode = get_mode_at_time(measurement, change_time, start_time, end_time, name_filter=name)
        else:
            is_running = False
            current_mode = None

        current_time = change_time

    # Add final period to end of day
    if current_time < end_dt:
        if is_running:
            mode = current_mode if current_mode else "Heat"
        else:
            mode = "Off"
        periods.append((current_time, end_dt, mode))

    # Consolidate consecutive periods with same mode
    consolidated = []
    for period in periods:
        if consolidated and consolidated[-1][2] == period[2]:
            # Extend previous period
            consolidated[-1] = (consolidated[-1][0], period[1], period[2])
        else:
            consolidated.append(period)

    # Print only Heat/Cool periods (skip Off)
    for period_start, period_end, mode in consolidated:
        if mode == "Off":
            continue
        local_start = to_local_time(period_start)
        local_end = to_local_time(period_end)
        dur = period_end - period_start
        duration = format_duration_short(dur)
        print(f"{local_start.strftime('%Y-%m-%d'):<12} {local_start.strftime('%H:%M:%S'):<10} {local_end.strftime('%H:%M:%S'):<10} {duration:>8} {mode:<6}")

def print_cycle_table(direct_stats, start_time, end_time):
    """Print detailed cycle table with mode information."""
    changes = get_state_changes('directs', 'state', start_time, end_time)

    # Check if already running at start of period
    start_dt = datetime.fromisoformat(start_time.replace('Z', '+00:00'))
    initial_state = get_state_at_start('directs', 'state', start_time)
    already_running = initial_state in ['Running', 'RUNNING']

    if not changes and not already_running:
        print("\nNo direct mode cycles recorded.")
        return

    print("\nDirect Mode Cycles:")
    print(f"{'Date':<12} {'Start':<10} {'End':<10} {'Duration':>8} {'Mode':<6}")
    print(f"{'-'*12} {'-'*10} {'-'*10} {'-'*8} {'-'*6}")

    start = None
    cycle_mode = None

    # If already running at start of period, treat midnight as the start
    if already_running:
        start = start_dt
        cycle_mode = get_direct_mode_at_time(start, start_time, end_time)

    started_at_midnight = already_running

    for change in changes:
        if change['state'] in ['Running', 'RUNNING']:
            start = change['time']
            cycle_mode = get_direct_mode_at_time(start, start_time, end_time)
            started_at_midnight = False
        elif change['state'] == 'Stopped' and start:
            duration = format_duration_short(change['time'] - start)
            local_start = to_local_time(start)
            local_end = to_local_time(change['time'])
            mode_str = cycle_mode if cycle_mode else "-"
            # Show "(started)" if this cycle was already running at midnight
            start_str = "(started)" if started_at_midnight else local_start.strftime('%H:%M:%S')
            print(f"{local_start.strftime('%Y-%m-%d'):<12} {start_str:<10} {local_end.strftime('%H:%M:%S'):<10} {duration:>8} {mode_str:<6}")
            start = None
            cycle_mode = None
            started_at_midnight = False

    # If still running at end of period
    if start:
        # Calculate duration to end of reporting period, not to now
        end_dt = datetime.fromisoformat(end_time.replace('Z', '+00:00'))
        duration = format_duration_short(end_dt - start)
        local_start = to_local_time(start)
        mode_str = cycle_mode if cycle_mode else "-"
        # Show "(started)" if this cycle was already running at midnight
        start_str = "(started)" if started_at_midnight else local_start.strftime('%H:%M:%S')
        print(f"{local_start.strftime('%Y-%m-%d'):<12} {start_str:<10} {'-':<10} {duration:>8} {mode_str:<6}")

def generate_report(report_date=None):
    """Generate the full HVAC daily report."""

    if report_date is None:
        report_date = datetime.now().date() - timedelta(days=1)

    # Convert local date to UTC time range
    # Local midnight = UTC time - offset hours
    offset_hours = get_utc_offset()
    start_dt = datetime(report_date.year, report_date.month, report_date.day) - timedelta(hours=offset_hours)
    end_dt = start_dt + timedelta(days=1) - timedelta(seconds=1)

    start_time = start_dt.strftime('%Y-%m-%dT%H:%M:%SZ')
    end_time = end_dt.strftime('%Y-%m-%dT%H:%M:%SZ')

    print(f"HVAC Daily Report - {report_date}")
    print("=" * 40)

    # Outside temperature
    outside = get_outside_temp_stats(start_time, end_time)
    print(f"\n{'Outside Temp:':<18} {format_temp(outside)}")

    # Storage mode (includes water temp)
    print_storage_section(start_time, end_time)

    # ===== SUMMARY SECTION =====

    # Units (compressors)
    unit_stats = get_unit_stats(start_time, end_time)
    if unit_stats:
        print(f"\n{'Units:':<18}")
        for unit, stats in sorted(unit_stats.items()):
            status = ""
            avg_cycle = "-"
            if stats['cycles'] > 0 and stats['cycle_durations']:
                avg_td = sum(stats['cycle_durations'], timedelta()) / len(stats['cycle_durations'])
                avg_cycle = format_duration_short(avg_td)
            print(f"  {unit:<16} Runtime: {format_duration(stats['runtime'])}{status}  Starts: {stats['cycles']}  Avg: {avg_cycle}  Mode: {stats['mode']}")

    # Fan stats
    fan_stats = get_fan_stats(start_time, end_time)
    if fan_stats:
        print(f"\n{'Fans:':<18}")
        for fan, stats in sorted(fan_stats.items()):
            status = ""
            print(f"  {fan:<16} Runtime: {format_duration(stats['runtime'])}{status}  Cycles: {stats['cycles']}")

    # Pump stats
    pump_stats = get_pump_stats(start_time, end_time)
    if pump_stats:
        print(f"\n{'Pumps:':<18}")
        for pump, stats in sorted(pump_stats.items()):
            status = ""
            temp_str = ""
            if stats['temp_in'] and stats['temp_in']['min']:
                temp_str = f"  Temp: {stats['temp_in']['min']:.0f}-{stats['temp_in']['max']:.0f}°F"
            print(f"  {pump:<16} Runtime: {format_duration(stats['runtime'])}{status}{temp_str}")

    # Direct mode summary
    direct_stats = get_direct_stats(start_time, end_time)
    status = ""
    print(f"\n{'Direct Mode:':<18} Runtime: {format_duration(direct_stats['runtime'])}{status}")
    print(f"{'':18} Cycles: {direct_stats['cycles']}  Valve actuations: {direct_stats['valve_actuations']}")

    # ===== DETAILS SECTION =====

    # Unit cycle tables
    if unit_stats:
        for unit in sorted(unit_stats.keys()):
            if unit_stats[unit]['cycles'] > 0:
                print_generic_cycle_table('units', unit, start_time, end_time, title=f"{unit} Unit Cycles")

    # Fan cycle tables
    if fan_stats:
        for fan in sorted(fan_stats.keys()):
            if fan_stats[fan]['cycles'] > 0:
                print_generic_cycle_table('fans', fan, start_time, end_time, title=f"{fan} Fan Cycles")

    # Direct mode cycle table
    print_cycle_table(direct_stats, start_time, end_time)

    # Direct mode averages
    if direct_stats['cycle_durations']:
        avg_on = sum(direct_stats['cycle_durations'], timedelta()) / len(direct_stats['cycle_durations'])
        print(f"\nAvg direct cycle duration: {format_duration_short(avg_on)}")
    if direct_stats['off_durations']:
        avg_off = sum(direct_stats['off_durations'], timedelta()) / len(direct_stats['off_durations'])
        print(f"Avg gap between direct cycles: {format_duration_short(avg_off)}")

    # Hourly temperature table
    hourly = get_hourly_temps(start_time, end_time)
    if hourly:
        print(f"\nHourly Temperatures:")
        print(f"{'Hour':<8} {'Storage':>10} {'Outside':>10}")
        print(f"{'-'*8} {'-'*10} {'-'*10}")
        for h in hourly:
            local_time = to_local_time(h['time'])
            water_str = f"{h['water']:.1f}°F" if h['water'] is not None else "-"
            outside_str = f"{h['outside']:.1f}°F" if h['outside'] is not None else "-"
            print(f"{local_time.strftime('%H:00'):<8} {water_str:>10} {outside_str:>10}")

def main():
    global INFLUX_HOST

    parser = argparse.ArgumentParser(description='HVAC Daily Report')
    parser.add_argument('date', nargs='?', help='Report date (YYYY-MM-DD), defaults to yesterday')
    parser.add_argument('-H', '--host', default=INFLUX_HOST, help='InfluxDB host')
    args = parser.parse_args()

    INFLUX_HOST = args.host

    report_date = None
    if args.date:
        report_date = datetime.strptime(args.date, '%Y-%m-%d').date()

    generate_report(report_date)

if __name__ == '__main__':
    main()
