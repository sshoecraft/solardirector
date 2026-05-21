#!/usr/bin/env python3

"""
Build EMA Checkpoint from InfluxDB Historical Data

Finds the most recent complete charge and discharge cycles,
applies EMA formula iteratively, and creates ema_checkpoint.dat
to seed the EMA system with learned values.

Usage:
    ./build_ema_checkpoint.py                    # Use last 2 years, 3 cycles
    ./build_ema_checkpoint.py --lookback-years 1 # Use last 1 year
    ./build_ema_checkpoint.py --cycles 5         # Use 5 recent cycles
"""

from influxdb import InfluxDBClient
from datetime import datetime, timedelta
from dateutil import parser as dateparser
import json
import argparse
import sys


def parse_timestamp(ts_str):
    """Parse ISO timestamp to datetime"""
    try:
        dt = dateparser.parse(ts_str)
        if dt.tzinfo is not None:
            dt = dt.replace(tzinfo=None)
        return dt
    except:
        return None


def run_influx_query(client, query):
    """Execute InfluxDB query and return list of results"""
    try:
        result = client.query(query)
        return list(result.get_points())
    except Exception as e:
        print(f"Query error: {e}", file=sys.stderr)
        return []


def find_charge_cycles(events, max_hours=12):
    """Find Empty→Full charge cycles within max_hours"""
    cycles = []
    prev_event = None

    for event in events:
        action = event.get('action')
        time_str = event.get('time')

        if prev_event:
            prev_action = prev_event.get('action')
            prev_time = prev_event.get('time')

            if prev_action == 'Empty' and action == 'Full':
                try:
                    t1 = parse_timestamp(prev_time)
                    t2 = parse_timestamp(time_str)
                    if t1 and t2:
                        hours = (t2 - t1).total_seconds() / 3600.0
                        if hours > 0 and hours <= max_hours:
                            cycles.append({
                                'start_time': prev_time,
                                'end_time': time_str,
                                'duration_hours': hours
                            })
                except:
                    pass

        prev_event = event

    return cycles


def find_discharge_cycles(events, max_hours=24):
    """Find Full→Empty discharge cycles within max_hours"""
    cycles = []
    prev_event = None

    for event in events:
        action = event.get('action')
        time_str = event.get('time')

        if prev_event:
            prev_action = prev_event.get('action')
            prev_time = prev_event.get('time')

            if prev_action == 'Full' and action == 'Empty':
                try:
                    t1 = parse_timestamp(prev_time)
                    t2 = parse_timestamp(time_str)
                    if t1 and t2:
                        hours = (t2 - t1).total_seconds() / 3600.0
                        if hours > 0 and hours <= max_hours:
                            cycles.append({
                                'start_time': prev_time,
                                'end_time': time_str,
                                'duration_hours': hours
                            })
                except:
                    pass

        prev_event = event

    return cycles


def process_cycle(client, cycle, cycle_type):
    """Process a cycle to get total Ah. Returns (total_ah, skip_reason)"""
    query = f"""
        SELECT battery_voltage, battery_power
        FROM inverter
        WHERE time >= '{cycle['start_time']}' AND time <= '{cycle['end_time']}'
    """
    data = run_influx_query(client, query)

    if len(data) < 10:
        return 0, f"only {len(data)} data points"

    total_ah = 0.0
    prev_time = None

    for row in data:
        try:
            voltage = float(row.get('battery_voltage', 0))
            power = float(row.get('battery_power', 0))
            current_time = parse_timestamp(row.get('time'))

            if voltage <= 0 or current_time is None:
                continue

            if prev_time is not None:
                dt_hours = (current_time - prev_time).total_seconds() / 3600.0

                if dt_hours > 0 and dt_hours < 1:
                    if cycle_type == 'charge' and power < 0:
                        # Charging: negative power = INTO battery
                        current = abs(power) / voltage
                        total_ah += current * dt_hours
                    elif cycle_type == 'discharge' and power > 0:
                        # Discharging: positive power = OUT of battery
                        current = power / voltage
                        total_ah += current * dt_hours

            prev_time = current_time
        except (ValueError, TypeError):
            continue

    if total_ah < 50:
        return total_ah, f"{total_ah:.1f} Ah (need 50+)"

    return total_ah, None


def calculate_ema(values, alpha=0.15):
    """
    Calculate EMA from a list of values.
    
    EMA formula:
        EMA[0] = values[0]
        EMA[i] = alpha * values[i] + (1-alpha) * EMA[i-1]
    
    Returns final EMA value.
    """
    if not values:
        return None
    
    ema = values[0]
    for value in values[1:]:
        ema = alpha * value + (1 - alpha) * ema
    
    return ema


def main():
    parser = argparse.ArgumentParser(
        description='Build EMA checkpoint from InfluxDB historical cycles',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)

    parser.add_argument('--host', default='solardirector.localdomain')
    parser.add_argument('--port', type=int, default=8086)
    parser.add_argument('--database', default='power')
    parser.add_argument('--lookback-years', type=int, default=2)
    parser.add_argument('--cycles', type=int, default=3,
                        help='Number of recent cycles for EMA (default: 3)')
    parser.add_argument('--alpha', type=float, default=0.15,
                        help='EMA alpha parameter (default: 0.15)')
    parser.add_argument('--output', default='ema_checkpoint.dat')

    args = parser.parse_args()

    print("="*60)
    print("EMA CHECKPOINT BUILDER")
    print("="*60)
    print(f"InfluxDB: {args.host}:{args.port}/{args.database}")
    print(f"Lookback: {args.lookback_years} years")
    print(f"Cycles: {args.cycles}, Alpha: {args.alpha}")
    print()

    # Connect and get events
    client = InfluxDBClient(host=args.host, port=args.port, database=args.database)

    # Use InfluxDB relative time syntax (InfluxDB uses 'd' for days, not 'y' for years)
    lookback_days = args.lookback_years * 365
    query = f"""
        SELECT * FROM events
        WHERE module='Battery' AND (action='Empty' OR action='Full')
        AND time >= now() - {lookback_days}d
        ORDER BY time ASC
    """
    
    print("Fetching events...")
    events = run_influx_query(client, query)
    
    if not events:
        print("ERROR: No events found!", file=sys.stderr)
        sys.exit(1)
    
    print(f"Found {len(events)} events\n")

    # Find cycles
    charge_cycles = find_charge_cycles(events)
    discharge_cycles = find_discharge_cycles(events)
    
    print(f"Found {len(charge_cycles)} charge cycles")
    print(f"Found {len(discharge_cycles)} discharge cycles\n")

    # Process charge cycles
    print("="*60)
    print("PROCESSING RECENT CHARGE CYCLES")
    print("="*60)
    
    charge_capacities = []
    for cycle in reversed(charge_cycles[-20:]):
        total_ah, skip_reason = process_cycle(client, cycle, 'charge')
        print(f"  {cycle['duration_hours']:.1f}h: {total_ah:.1f} Ah", end="")
        
        if skip_reason is None:
            charge_capacities.append(total_ah)
            print(" ✓")
            if len(charge_capacities) >= args.cycles:
                break
        else:
            print(f" ✗ ({skip_reason})")

    # Process discharge cycles
    print("\n" + "="*60)
    print("PROCESSING RECENT DISCHARGE CYCLES")
    print("="*60)
    
    discharge_capacities = []
    for cycle in reversed(discharge_cycles[-20:]):
        total_ah, skip_reason = process_cycle(client, cycle, 'discharge')
        print(f"  {cycle['duration_hours']:.1f}h: {total_ah:.1f} Ah", end="")
        
        if skip_reason is None:
            discharge_capacities.append(total_ah)
            print(" ✓")
            if len(discharge_capacities) >= args.cycles:
                break
        else:
            print(f" ✗ ({skip_reason})")

    # Calculate EMAs
    print("\n" + "="*60)
    print("CALCULATING EMA")
    print("="*60)
    
    charge_ema = calculate_ema(charge_capacities, args.alpha) if charge_capacities else None
    discharge_ema = calculate_ema(discharge_capacities, args.alpha) if discharge_capacities else None
    
    if charge_ema:
        print(f"\n✓ Charge EMA: {charge_ema:.1f} Ah (from {len(charge_capacities)} cycles)")
        for i, ah in enumerate(charge_capacities, 1):
            print(f"    Cycle {i}: {ah:.1f} Ah")
    
    if discharge_ema:
        print(f"\n✓ Discharge EMA: {discharge_ema:.1f} Ah (from {len(discharge_capacities)} cycles)")
        for i, ah in enumerate(discharge_capacities, 1):
            print(f"    Cycle {i}: {ah:.1f} Ah")

    if not charge_ema and not discharge_ema:
        print("\nERROR: No valid cycles found!", file=sys.stderr)
        sys.exit(1)

    # Write checkpoint
    ema_state = {
        "charge_capacity_ema": charge_ema,
        "discharge_capacity_ema": discharge_ema,
        "alpha": args.alpha,
        "min_samples": 3,
        "charge_cycles": len(charge_capacities) if charge_capacities else 0,
        "discharge_cycles": len(discharge_capacities) if discharge_capacities else 0,
        "bootstrap_charge": [],
        "bootstrap_discharge": [],
        "last_updated": datetime.now().isoformat(),
        "battery_capacity_config": 1050
    }

    print(f"\nWriting {args.output}...")
    with open(args.output, 'w') as f:
        json.dump(ema_state, f, indent=2)

    print("\n" + "="*60)
    print("SUCCESS!")
    print("="*60)
    print(f"✓ EMA checkpoint created: {args.output}")
    print(f"✓ 100% seeded from historical data")
    print(f"✓ Drift correction active immediately on deployment")
    print()
    print("Deploy:")
    print(f"  sudo cp {args.output} /opt/sd/lib/agents/si/")
    print("  sudo systemctl restart si")

    client.close()


if __name__ == '__main__':
    main()
