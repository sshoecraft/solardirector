#!/usr/bin/env python3
"""
Comprehensive daily/monthly billing report.
Python implementation of rep.go - queries InfluxDB for power usage data.
"""

import argparse
import json
import math
import os
import sys
import urllib.request
import urllib.parse
from datetime import datetime, timedelta

VERSION = "1.3.0"

# InfluxDB configuration defaults
DEFAULT_INFLUX_HOST = "http://localhost:8086"
INFLUX_DB = "power"
DEFAULT_TIMEZONE = "America/Chicago"

# Billing configuration
BILL_DAY = 11
COST_KWH = 0.16
FEED_RATE = 0.012

# Sun calculation constants
J1970 = 2440588.0
J2000 = 2451545.0
RAD = math.pi / 180.0
E = RAD * 23.4397  # obliquity of the Earth

# Default token path for InfluxDB 2.x
DEFAULT_TOKEN_FILE = os.path.expanduser("~/.influxdbv2/configs")

# Module-level state
influx_host = DEFAULT_INFLUX_HOST
influx_token = None
timezone = DEFAULT_TIMEZONE
tz_location = None  # will be set from timezone string


def normalize_host_url(host):
    if host.startswith("http://") or host.startswith("https://"):
        return host
    if ":" in host:
        return "http://" + host
    return "http://" + host + ":8086"


def load_influx_token():
    """Load InfluxDB API token from ~/.influxdbv2/configs."""
    try:
        with open(DEFAULT_TOKEN_FILE, "r") as f:
            for line in f:
                line = line.strip()
                if line.startswith("token"):
                    parts = line.split("=", 1)
                    if len(parts) == 2:
                        token = parts[1].strip().strip('"')
                        if token and token != "XXX":
                            return token
    except OSError:
        pass
    return None


def get_system_timezone():
    tz = os.environ.get("TZ")
    if tz:
        return tz
    try:
        link = os.readlink("/etc/localtime")
        if "/zoneinfo/" in link:
            parts = link.split("/zoneinfo/")
            if len(parts) == 2:
                return parts[1]
    except OSError:
        pass
    try:
        with open("/etc/timezone", "r") as f:
            tz = f.read().strip()
            if tz:
                return tz
    except OSError:
        pass
    try:
        with open("/var/db/zoneinfo", "r") as f:
            tz = f.read().strip()
            if tz:
                return tz
    except OSError:
        pass
    return DEFAULT_TIMEZONE


def make_influx_request(url):
    """Make an HTTP request to InfluxDB with optional token auth."""
    req = urllib.request.Request(url)
    if influx_token:
        req.add_header("Authorization", f"Token {influx_token}")
    with urllib.request.urlopen(req) as response:
        return json.loads(response.read().decode())


def influx_query(query, epoch=None):
    query_url = f"{influx_host}/query"
    params = {"db": INFLUX_DB, "q": query + f" tz('{timezone}')"}
    if epoch:
        params["epoch"] = epoch
    full_url = query_url + "?" + urllib.parse.urlencode(params)
    try:
        return make_influx_request(full_url)
    except Exception:
        return None


def influx_query_db(query, db):
    query_url = f"{influx_host}/query"
    params = {"db": db, "q": query + f" tz('{timezone}')"}
    full_url = query_url + "?" + urllib.parse.urlencode(params)
    try:
        return make_influx_request(full_url)
    except Exception:
        return None


def influx_single_value(query):
    resp = influx_query(query)
    if not resp or not resp.get("results"):
        return 0.0
    results = resp["results"]
    if not results or not results[0].get("series"):
        return 0.0
    series = results[0]["series"]
    if not series or not series[0].get("values"):
        return 0.0
    values = series[0]["values"]
    if not values or len(values[0]) < 2:
        return 0.0
    v = values[0][1]
    if v is None:
        return 0.0
    try:
        return float(v)
    except (TypeError, ValueError):
        return 0.0


def parse_influx_value(v):
    if v is None:
        return 0.0
    try:
        return float(v)
    except (TypeError, ValueError):
        return 0.0


def get_first_year():
    query = "select first(input_power) from inverter"
    resp = influx_query(query)
    if not resp or not resp.get("results"):
        return datetime.now().year
    results = resp["results"]
    if not results or not results[0].get("series"):
        return datetime.now().year
    series = results[0]["series"]
    if not series or not series[0].get("values"):
        return datetime.now().year
    values = series[0]["values"]
    if not values or not values[0]:
        return datetime.now().year
    timestamp = values[0][0]
    if not isinstance(timestamp, str):
        return datetime.now().year
    try:
        t = datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
        return t.year
    except (ValueError, TypeError):
        return datetime.now().year


def collect_data(start_date, end_date):
    query = (
        f"select sum(grid_consumed) from inverter "
        f"where time >= '{start_date.strftime('%Y-%m-%d')} 00:00:00' "
        f"and time < '{end_date.strftime('%Y-%m-%d')} 23:59:59' "
        f"group by time(1d) fill(null)"
    )
    resp = influx_query(query)
    data = []
    if not resp or not resp.get("results"):
        return data
    results = resp["results"]
    if not results or not results[0].get("series"):
        return data
    for series in results[0]["series"]:
        for value in series.get("values", []):
            if len(value) >= 2:
                timestamp = value[0]
                if not isinstance(timestamp, str):
                    continue
                try:
                    t = datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
                except (ValueError, TypeError):
                    continue
                usage = parse_influx_value(value[1])
                data.append({"date": t.strftime("%Y-%m-%d"), "usage": usage})
    return data


def get_usage_for_date(data, date_str):
    for d in data:
        if d["date"] == date_str:
            return d["usage"]
    return 0.0


def get_feed_for_range(start_date, end_date):
    total = 0.0
    current = start_date
    while current < end_date:
        query = (
            f"select sum(input_power) from inverter "
            f"where input_power > 0 and time > '{current.strftime('%Y-%m-%d')} 00:00:00' "
            f"and time < '{current.strftime('%Y-%m-%d')} 23:59:59'"
        )
        feed = influx_single_value(query)
        if feed > 0:
            total += feed
        current += timedelta(days=1)
    return total


# --- Sun calculation functions ---

def to_julian(date):
    epoch = datetime(1970, 1, 1)
    delta = date - epoch
    millis = delta.total_seconds() * 1000
    return millis / 86400000.0 - 0.5 + J1970


def to_days(date):
    return to_julian(date) - J2000


def solar_mean_anomaly(d):
    return RAD * (357.5291 + 0.98560028 * d)


def ecliptic_longitude(M):
    C = RAD * (1.9148 * math.sin(M) + 0.02 * math.sin(2 * M) + 0.0003 * math.sin(3 * M))
    P = RAD * 102.9372
    return M + C + P + math.pi


def sun_declination(l, b):
    return math.asin(math.sin(b) * math.cos(E) + math.cos(b) * math.sin(E) * math.sin(l))


def julian_cycle(d, lw):
    return round(d - 0.0009 - lw / (2 * math.pi))


def approx_transit(Ht, lw, n):
    return 0.0009 + (Ht + lw) / (2 * math.pi) + n


def solar_transit_j(ds, M, L):
    return J2000 + ds + 0.0053 * math.sin(M) - 0.0069 * math.sin(2 * L)


def hour_angle(h, phi, dec):
    cos_h = (math.sin(h) - math.sin(phi) * math.sin(dec)) / (math.cos(phi) * math.cos(dec))
    if cos_h > 1:
        cos_h = 1
    elif cos_h < -1:
        cos_h = -1
    return math.acos(cos_h)


def get_set_j(h, lw, phi, dec, n, M, L):
    w = hour_angle(h, phi, dec)
    a = approx_transit(w, lw, n)
    return solar_transit_j(a, M, L)


def from_julian(j):
    millis = (j + 0.5 - J1970) * 86400000
    epoch = datetime(1970, 1, 1)
    return epoch + timedelta(milliseconds=millis)


def get_sun_times(date, lat, lon):
    lw = RAD * -lon
    phi = RAD * lat

    d = to_days(date)
    n = julian_cycle(d, lw)
    ds = approx_transit(0, lw, n)

    M = solar_mean_anomaly(ds)
    L = ecliptic_longitude(M)
    dec = sun_declination(L, 0)

    jnoon = solar_transit_j(ds, M, L)

    h0 = -0.833 * RAD
    jset = get_set_j(h0, lw, phi, dec, n, M, L)
    jrise = jnoon - (jset - jnoon)

    sunrise = from_julian(jrise)
    sunset = from_julian(jset)
    return sunrise, sunset


def get_coordinates():
    try:
        with open("/opt/sd/etc/location.conf", "r") as f:
            lat = None
            lon = None
            for line in f:
                line = line.strip()
                if line.startswith("lat="):
                    try:
                        lat = float(line[4:])
                    except ValueError:
                        pass
                elif line.startswith("lon="):
                    try:
                        lon = float(line[4:])
                    except ValueError:
                        pass
            if lat is not None and lon is not None:
                return lat, lon
    except OSError:
        pass
    return None


def get_hourly_temp_map(date):
    start_time = date.replace(hour=0, minute=0, second=0, microsecond=0)
    end_time = start_time + timedelta(days=1)

    query = (
        f"select mean(heat_index_outdoor_f) from ws2000 "
        f"where time >= '{start_time.strftime('%Y-%m-%d %H:%M:%S')}' "
        f"and time < '{end_time.strftime('%Y-%m-%d %H:%M:%S')}' "
        f"group by time(1h) fill(null)"
    )

    resp = influx_query_db(query, "weather")
    temp_map = {}

    if not resp or not resp.get("results"):
        return temp_map
    results = resp["results"]
    if not results or not results[0].get("series"):
        return temp_map

    for series in results[0]["series"]:
        for value in series.get("values", []):
            if len(value) >= 2 and value[1] is not None:
                timestamp = value[0]
                if isinstance(timestamp, str):
                    try:
                        t = datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
                        # Convert to local hour
                        # We use the hour from the timestamp directly since tz() was applied
                        hour = t.hour
                        temp_map[hour] = parse_influx_value(value[1])
                    except (ValueError, TypeError):
                        pass
    return temp_map


def query_daytime_temp(start_time, end_time):
    query = (
        f"select mean(heat_index_outdoor_f) from ws2000 "
        f"where time >= '{start_time.strftime('%Y-%m-%d %H:%M:%S')}' "
        f"and time <= '{end_time.strftime('%Y-%m-%d %H:%M:%S')}'"
    )

    resp = influx_query_db(query, "weather")
    if not resp or not resp.get("results"):
        return 0.0
    results = resp["results"]
    if not results or not results[0].get("series"):
        return 0.0
    series = results[0]["series"]
    if not series or not series[0].get("values"):
        return 0.0
    values = series[0]["values"]
    if not values or len(values[0]) < 2 or values[0][1] is None:
        return 0.0
    return parse_influx_value(values[0][1])


def get_daytime_temp(date):
    coords = get_coordinates()
    if coords is None:
        # Fallback to 8am-8pm
        start_time = date.replace(hour=8, minute=0, second=0, microsecond=0)
        end_time = date.replace(hour=20, minute=0, second=0, microsecond=0)
        return query_daytime_temp(start_time, end_time)

    lat, lon = coords
    sunrise, sunset = get_sun_times(date, lat, lon)

    start_time = sunrise + timedelta(hours=2)
    end_time = sunset + timedelta(hours=2)
    return query_daytime_temp(start_time, end_time)


# --- Display functions ---

def display_usage(report_date, data, first_year):
    report_date_str = report_date.strftime("%Y-%m-%d")
    report_day_usage = get_usage_for_date(data, report_date_str)

    previous_year_date = report_date.replace(year=report_date.year - 1)
    previous_year_date_str = previous_year_date.strftime("%Y-%m-%d")
    previous_year_usage = get_usage_for_date(data, previous_year_date_str)

    print(f"{'Usage:':<30s} {report_day_usage:.2f} kWh")

    report_day_temp = get_daytime_temp(report_date)
    if report_day_temp > 0:
        print(f"{'Temperature:':<30s} {report_day_temp:.1f}\u00b0F")

    print(f"{'Previous year:':<30s} {previous_year_usage:.2f} kWh")

    previous_year_temp = get_daytime_temp(previous_year_date)
    if previous_year_temp > 0:
        print(f"{'Previous year temp:':<30s} {previous_year_temp:.1f}\u00b0F")

    diff = 0.0
    text = ""

    if report_day_usage == 0:
        if previous_year_usage == 0:
            diff = 0.0
        else:
            diff = 100.0
            text = "less"
    else:
        if previous_year_usage == 0:
            diff = 100.0
            text = "more"
        else:
            if previous_year_usage > report_day_usage:
                pct = (report_day_usage / previous_year_usage) * 100
                diff = 100.0 - pct
                text = "less"
            else:
                pct = (previous_year_usage / report_day_usage) * 100
                diff = 100.0 - pct
                text = "more"

    print(f"{'Usage vs Previous year:':<30s} {diff:.1f}% {text}")

    # Historical average for this day
    total = 0.0
    count = 0
    month_day = report_date.strftime("%m-%d")

    for year in range(first_year, report_date.year):
        date_str = f"{year}-{month_day}"
        usage = get_usage_for_date(data, date_str)
        if usage > 0:
            total += usage
            count += 1

    avg = total / count if count > 0 else 0.0

    print(f"{'Historical average:':<30s} {avg:.2f} kWh")

    # Historical average daytime temperature
    temp_total = 0.0
    temp_count = 0

    for year in range(first_year, report_date.year):
        hist_date = report_date.replace(year=year)
        day_temp = get_daytime_temp(hist_date)
        if day_temp > 0:
            temp_total += day_temp
            temp_count += 1

    temp_avg = temp_total / temp_count if temp_count > 0 else 0.0

    if temp_avg > 0:
        print(f"{'Historical temp:':<30s} {temp_avg:.1f}\u00b0F")

    if avg > 0:
        if avg > report_day_usage:
            pct = (report_day_usage / avg) * 100
            diff = 100.0 - pct
            text = "less"
        else:
            pct = (avg / report_day_usage) * 100
            diff = 100.0 - pct
            text = "more"
        print(f"{'Usage vs Average:':<30s} {diff:.1f}% {text}")


def display_cycle(start_date, end_date, today, data):
    total = 0.0
    count = 0

    current = start_date
    while current <= today and current < end_date:
        date_str = current.strftime("%Y-%m-%d")
        usage = get_usage_for_date(data, date_str)
        if usage > 0:
            total += usage
            count += 1
        current += timedelta(days=1)

    cycle_days = (end_date - start_date).days

    avg = total / count if count > 0 else 0.0
    projected = avg * cycle_days
    if projected < total:
        projected = total

    cost = projected * COST_KWH

    print()
    print(f"{'Cycle usage:':<30s} {total:.2f} kWh")
    print(f"{'Daily average:':<30s} {avg:.2f} kWh")
    print(f"{'Projected total:':<30s} {projected:.2f} kWh")
    print(f"{'Projected cost:':<30s} ${cost:.2f}")

    return projected, cost


def display_last_cycle(start_date, data, projected, cost):
    last_year_start = start_date.replace(year=start_date.year - 1)
    last_year_end = add_months(last_year_start, 1)

    total = 0.0
    current = last_year_start
    while current < last_year_end:
        date_str = current.strftime("%Y-%m-%d")
        usage = get_usage_for_date(data, date_str)
        if usage > 0:
            total += usage
        current += timedelta(days=1)

    print()
    print(f"{'Last year cycle usage:':<30s} {total:.2f} kWh")

    if projected > 0 and total > 0:
        if total > projected:
            pct = (projected / total) * 100
            diff = 100.0 - pct
            text = "less"
        else:
            pct = (total / projected) * 100
            diff = 100.0 - pct
            text = "more"

        print(f"{'Projected vs last year:':<30s} {diff:.1f}% {text}")

        if text == "less":
            last_cost = total * COST_KWH
            savings = last_cost - cost
            print(f"{'Savings vs last year:':<30s} ${savings:.2f}")


def display_cycle_average(start_date, data, first_year, today, projected, cost):
    total = 0.0
    count = 0

    last_year = (today - timedelta(days=365)).year

    for year in range(first_year, last_year + 1):
        year_start = datetime(year, start_date.month, start_date.day)
        year_end = add_months(year_start, 1)

        cycle_total = 0.0
        current = year_start
        while current < year_end:
            date_str = current.strftime("%Y-%m-%d")
            usage = get_usage_for_date(data, date_str)
            if usage > 0:
                cycle_total += usage
            current += timedelta(days=1)

        if cycle_total > 0:
            total += cycle_total
            count += 1

    avg = total / count if count > 0 else 0.0
    avg_cost = avg * COST_KWH

    print()
    print(f"{'Cycle average(all years):':<30s} {avg:.2f} kWh")
    print(f"{'Cycle average cost:':<30s} ${avg_cost:.2f}")

    if projected > 0 and avg > 0:
        if avg > projected:
            pct = (projected / avg) * 100
            diff = 100.0 - pct
            text = "less"
        else:
            pct = (avg / projected) * 100
            diff = 100.0 - pct
            text = "more"

        print(f"{'Projected vs average:':<30s} {diff:.1f}% {text}")

        if text == "less":
            savings = avg_cost - cost
            print(f"{'Savings vs average:':<30s} ${savings:.2f}")


def display_feed(start_date, end_date, cost):
    total = get_feed_for_range(start_date, end_date)

    cycle_days = (end_date - start_date).days
    avg = total / cycle_days if cycle_days > 0 else 0.0
    projected = avg * cycle_days
    if projected < total:
        projected = total

    returns = projected * FEED_RATE
    overall_cost = cost - returns

    print()
    print(f"{'Total power fed back to grid:':<30s} {total:.2f} kWh")
    print(f"{'Daily average of total:':<30s} {avg:.2f} kWh")
    print(f"{'Projected monthly total:':<30s} {projected:.2f} kWh")
    print(f"{'Projected monthly return:':<30s} ${returns:.2f}")

    if cost > 0:
        print()
        print(f"{'>>> Overall projected cost:':<30s} ${overall_cost:.2f}")


def display_hourly(report_date):
    query = (
        f"select sum(grid_consumed) from inverter "
        f"where time > '{report_date.strftime('%Y-%m-%d')} 00:00:00' "
        f"and time < '{report_date.strftime('%Y-%m-%d')} 23:59:59' "
        f"group by time(1h) fill(0)"
    )

    resp = influx_query(query, epoch="ns")
    if not resp or not resp.get("results"):
        return
    results = resp["results"]
    if not results or not results[0].get("series"):
        return

    temp_map = get_hourly_temp_map(report_date)

    print()
    print("Hourly usage:")
    print(f"{'Timestamp':<25s} {'kWh':>6s} {'Temp':>8s}")

    total = 0.0
    for series in results[0]["series"]:
        for value in series.get("values", []):
            if len(value) >= 2:
                ts = parse_timestamp(value[0])
                if ts is None:
                    continue

                usage = parse_influx_value(value[1])
                if -0.005 < usage < 0.005:
                    usage = 0.0

                hour = ts.hour
                if hour in temp_map and temp_map[hour] > 0:
                    print(f"{ts.strftime('%Y-%m-%d %H:%M:%S'):<25s} {usage:6.2f} {temp_map[hour]:7.1f}\u00b0F")
                else:
                    print(f"{ts.strftime('%Y-%m-%d %H:%M:%S'):<25s} {usage:6.2f}")
                total += usage

    print(f"{'Total:':<25s} {total:6.2f}")


def display_last_hourly(report_date):
    last_year = report_date.replace(year=report_date.year - 1)

    query = (
        f"select sum(grid_consumed) from inverter "
        f"where time > '{last_year.strftime('%Y-%m-%d')} 00:00:00' "
        f"and time < '{last_year.strftime('%Y-%m-%d')} 23:59:59' "
        f"group by time(1h) fill(0)"
    )

    resp = influx_query(query, epoch="ns")
    if not resp or not resp.get("results"):
        return
    results = resp["results"]
    if not results or not results[0].get("series"):
        return

    temp_map = get_hourly_temp_map(last_year)

    print()
    print("Previous Year hourly usage:")
    print(f"{'Timestamp':<25s} {'kWh':>6s} {'Temp':>8s}")

    total = 0.0
    for series in results[0]["series"]:
        for value in series.get("values", []):
            if len(value) >= 2:
                ts = parse_timestamp(value[0])
                if ts is None:
                    continue

                usage = parse_influx_value(value[1])
                if -0.005 < usage < 0.005:
                    usage = 0.0

                hour = ts.hour
                if hour in temp_map and temp_map[hour] > 0:
                    print(f"{ts.strftime('%Y-%m-%d %H:%M:%S'):<25s} {usage:6.2f} {temp_map[hour]:7.1f}\u00b0F")
                else:
                    print(f"{ts.strftime('%Y-%m-%d %H:%M:%S'):<25s} {usage:6.2f}")
                total += usage

    print(f"{'Total:':<25s} {total:6.2f}")


def display_historical(start_date, end_date, data, first_year, today):
    print()
    print("Historical Usage:")
    print()

    this_year = today.year
    hist_end_year = end_date.year
    if start_date.year != end_date.year:
        hist_end_year = end_date.year - 1

    # Split years: prior years, then avg column, then current year
    prior_years = list(range(first_year, hist_end_year))
    current_year_col = hist_end_year
    show_avg = len(prior_years) > 0

    # Header
    header = f"{'date':<5s} "
    for year in prior_years:
        header += f" {year:>9d}"
    if show_avg:
        header += f" {'Avg':>9s}"
    header += f" {current_year_col:>9d}"
    print(header)

    # Separator
    sep = f"{'='*5:<5s} "
    for year in prior_years:
        sep += f" {'='*9:>9s}"
    if show_avg:
        sep += f" {'='*9:>9s}"
    sep += f" {'='*9:>9s}"
    print(sep)

    # Initialize totals
    totals = {}
    for year in range(first_year, hist_end_year + 1):
        totals[year] = 0.0
    avg_total = 0.0

    # Display data
    current = start_date
    while current < end_date:
        month_day = current.strftime("%m-%d")
        line = f"{month_day} "

        # Collect prior year values for averaging
        day_values = []

        for year in prior_years:
            actual_year = year
            if start_date.year != end_date.year and current.month < start_date.month:
                actual_year = year + 1

            date_str = f"{actual_year}-{month_day}"
            usage = get_usage_for_date(data, date_str)

            if actual_year == this_year and date_str == today.strftime("%Y-%m-%d"):
                line += f" {'NULL':>9s}"
            elif usage > 0:
                totals[year] += usage
                day_values.append(usage)
                line += f" {usage:9.2f}"
            else:
                line += f" {'NULL':>9s}"

        # Avg column
        if show_avg:
            if day_values:
                day_avg = sum(day_values) / len(day_values)
                avg_total += day_avg
                line += f" {day_avg:9.2f}"
            else:
                line += f" {'NULL':>9s}"

        # Current year column
        actual_year = current_year_col
        if start_date.year != end_date.year and current.month < start_date.month:
            actual_year = current_year_col + 1

        date_str = f"{actual_year}-{month_day}"
        usage = get_usage_for_date(data, date_str)

        if actual_year == this_year and date_str == today.strftime("%Y-%m-%d"):
            line += f" {'NULL':>9s}"
        elif usage > 0:
            totals[current_year_col] += usage
            line += f" {usage:9.2f}"
        else:
            line += f" {'NULL':>9s}"

        print(line)

        # After Feb 28, always show Feb 29 row for leap year data
        if month_day == "02-28":
            leap_day = "02-29"
            line = f"{leap_day} "
            day_values = []

            for year in prior_years:
                actual_year = year
                if start_date.year != end_date.year and current.month < start_date.month:
                    actual_year = year + 1

                date_str = f"{actual_year}-{leap_day}"
                usage = get_usage_for_date(data, date_str)

                if actual_year == this_year and date_str == today.strftime("%Y-%m-%d"):
                    line += f" {'NULL':>9s}"
                elif usage > 0:
                    totals[year] += usage
                    day_values.append(usage)
                    line += f" {usage:9.2f}"
                else:
                    line += f" {'NULL':>9s}"

            if show_avg:
                if day_values:
                    day_avg = sum(day_values) / len(day_values)
                    avg_total += day_avg
                    line += f" {day_avg:9.2f}"
                else:
                    line += f" {'NULL':>9s}"

            actual_year = current_year_col
            if start_date.year != end_date.year and current.month < start_date.month:
                actual_year = current_year_col + 1

            date_str = f"{actual_year}-{leap_day}"
            usage = get_usage_for_date(data, date_str)

            if actual_year == this_year and date_str == today.strftime("%Y-%m-%d"):
                line += f" {'NULL':>9s}"
            elif usage > 0:
                totals[current_year_col] += usage
                line += f" {usage:9.2f}"
            else:
                line += f" {'NULL':>9s}"

            print(line)

            # Skip to March 1
            next_day = current + timedelta(days=1)
            if next_day.day == 29:
                current = next_day + timedelta(days=1)
            else:
                current = next_day
        else:
            current += timedelta(days=1)

    # Footer separator
    sep = f"{'='*5:<5s} "
    for year in prior_years:
        sep += f" {'='*9:>9s}"
    if show_avg:
        sep += f" {'='*9:>9s}"
    sep += f" {'='*9:>9s}"
    print(sep)

    # Totals
    cycle_days = (end_date - start_date).days
    line = f"{'Total':<5s} "
    for year in prior_years:
        line += f" {totals[year]:9.2f}"
    if show_avg:
        line += f" {avg_total:9.2f}"
    line += f" {totals[current_year_col]:9.2f}"
    print(line)

    # Daily averages
    line = f"{'Avg':<5s} "
    for year in prior_years:
        avg = totals[year] / cycle_days if cycle_days > 0 else 0.0
        line += f" {avg:9.2f}"
    if show_avg:
        avg_daily = avg_total / cycle_days if cycle_days > 0 else 0.0
        line += f" {avg_daily:9.2f}"
    cur_avg = totals[current_year_col] / cycle_days if cycle_days > 0 else 0.0
    line += f" {cur_avg:9.2f}"
    print(line)


# --- Helpers ---

def parse_timestamp(value):
    """Parse a timestamp from InfluxDB (can be nanoseconds int/float or RFC3339 string)."""
    if isinstance(value, (int, float)):
        ns = int(value)
        epoch = datetime(1970, 1, 1)
        return epoch + timedelta(microseconds=ns // 1000)
    if isinstance(value, str):
        try:
            ns = int(value)
            epoch = datetime(1970, 1, 1)
            return epoch + timedelta(microseconds=ns // 1000)
        except ValueError:
            pass
        try:
            return datetime.fromisoformat(value.replace("Z", "+00:00")).replace(tzinfo=None)
        except (ValueError, TypeError):
            pass
    return None


def add_months(date, months):
    """Add months to a date, handling month overflow."""
    month = date.month - 1 + months
    year = date.year + month // 12
    month = month % 12 + 1
    # Clamp day to max days in target month
    import calendar
    max_day = calendar.monthrange(year, month)[1]
    day = min(date.day, max_day)
    return date.replace(year=year, month=month, day=day)


def main():
    global influx_host, influx_token, timezone

    timezone = get_system_timezone()

    parser = argparse.ArgumentParser(description="Comprehensive daily/monthly billing report")
    parser.add_argument("date", nargs="?", help="Report date (YYYY-MM-DD), defaults to yesterday")
    parser.add_argument("-H", "--host", default=DEFAULT_INFLUX_HOST, help="InfluxDB host (hostname, hostname:port, or full URL)")
    parser.add_argument("-t", "--token", default=None, help="InfluxDB API token (default: read from ~/.influxdbv2/configs)")
    parser.add_argument("-tz", "--timezone", default=timezone, help="Timezone for queries (default: system timezone)")
    parser.add_argument("--hist", action="store_true", help="Display only historical usage table")
    parser.add_argument("--full", action="store_true", help="Show full year (Jan-Dec) history with all years")
    args = parser.parse_args()

    influx_host = normalize_host_url(args.host)
    timezone = args.timezone

    if args.token:
        influx_token = args.token
    else:
        influx_token = load_influx_token()

    if args.date:
        try:
            today = datetime.strptime(args.date, "%Y-%m-%d")
        except ValueError:
            print(f"Error parsing date: {args.date}", file=sys.stderr)
            sys.exit(1)
    else:
        today = datetime.now()

    report_date = today - timedelta(days=1)

    # Calculate billing cycle dates
    if report_date.day < BILL_DAY:
        # Previous month's billing cycle
        start_date = add_months(report_date.replace(day=BILL_DAY), -1)
        end_date = report_date.replace(day=BILL_DAY)
    else:
        # Current month's billing cycle
        start_date = report_date.replace(day=BILL_DAY)
        end_date = add_months(report_date.replace(day=BILL_DAY), 1)

    first_year = get_first_year()

    # Limit to 5 years back unless --full
    if not args.full:
        five_years_ago = today.year - 5
        if first_year < five_years_ago:
            first_year = five_years_ago

    # For --full, use Jan 1 to Dec 31 for historical display
    if args.full:
        hist_start = datetime(report_date.year, 1, 1)
        hist_end = datetime(report_date.year + 1, 1, 1)
    else:
        hist_start = start_date
        hist_end = end_date

    # Collect all data
    data_start_date = datetime(first_year, 1, 1)
    all_data = collect_data(data_start_date, hist_end + timedelta(days=365))

    if report_date.year < first_year:
        first_year = report_date.year

    # Display reports
    if args.hist or args.full:
        display_historical(hist_start, hist_end, all_data, first_year, today)
    else:
        display_usage(report_date, all_data, first_year)
        projected, cost = display_cycle(start_date, end_date, today, all_data)
        display_last_cycle(start_date, all_data, projected, cost)
        display_cycle_average(start_date, all_data, first_year, today, projected, cost)
        display_feed(start_date, end_date, cost)
        display_hourly(report_date)
        display_last_hourly(report_date)
        display_historical(start_date, end_date, all_data, first_year, today)


if __name__ == "__main__":
    main()
