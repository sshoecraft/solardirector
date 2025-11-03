# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This is the reports subdirectory of the Solar Dashboard (SD) system. It contains Bash scripts that generate power usage reports, billing analysis, and automated email notifications based on solar inverter data stored in InfluxDB.

### Key Scripts

**hist.sh** - Historical power usage report generator (Original Bash version)
- Queries InfluxDB for inverter input power data using curl
- Generates year-by-year grid layout reports
- Calculates daily usage totals from negative input power values
- Shows historical comparisons across multiple years

**hist** - Historical power usage report generator (Go implementation)
- Pure Go implementation using InfluxDB HTTP API
- Flexible host specification: hostname, hostname:port, or full URL
- Auto-detects system timezone or use -tz flag to override
- Same functionality as bash version but more portable
- No external Go dependencies (jq, curl, dc)
- Built from hist.go source file

**rep.sh** - Comprehensive daily/monthly billing report (Original Bash version)
- Tracks power usage within billing cycles (starts day 11 of month)
- Calculates costs at $0.16/kWh and feed-in credits at $0.012/kWh
- Provides hourly usage breakdowns and grid feed-back tracking
- Projects monthly costs and compares with previous years
- Generates detailed analysis of peak usage times

**rep** - Comprehensive daily/monthly billing report (Go implementation)
- Pure Go implementation using InfluxDB HTTP API
- Same functionality as bash version with better performance
- Flexible host specification: hostname, hostname:port, or full URL
- Auto-detects system timezone or use -tz flag to override
- All calculations and report formatting match bash version
- Built from rep.go source file

**solar_report** - Automated email report dispatcher
- Sends daily HTML-formatted power reports via email
- Integrates with `mkpdf` to generate PDF attachments
- Runs the `rep` script and emails results to configured recipients

## Architecture

### Data Flow
1. Solar inverter logs power data to InfluxDB (`power` database)
2. Report scripts query InfluxDB using HTTP API with timezone handling
3. Data processing uses shell utilities (`dc`, `bc`, `jq`) for calculations
4. Email reports distributed via `sendmail` with MIME attachments

### Common Development Commands

```bash
# Build Go programs
make                         # Build all Go programs
make hist                    # Build just hist
make rep                     # Build just rep
make clean                   # Clean build artifacts

# Generate historical report (Go version)
./hist                       # All years from first data
./hist 2020                  # From 2020 to current
./hist 2020 2023             # Specific year range
./hist 2020-03-15 2023-03-15 # Specific date range

# Use custom InfluxDB host (various formats supported)
./hist -host solardirector 2020 2023           # Hostname only (adds :8086)
./hist -H influxdb:8087 2020                   # Hostname with custom port
./hist -host http://192.168.1.100:8086 2020 2023  # Full URL
./hist -H https://influxdb.example.com 2020    # HTTPS URL

# Use custom timezone
./hist -tz America/Chicago 2020 2023
./hist -tz UTC 2020 2023

# Combine options
./hist -H solardirector -tz America/New_York 2020 2023

# Generate historical report (Original Bash version)
./hist.sh                    # All years
./hist.sh 2020 2023          # Specific year range

# Generate daily report (Go version)
./rep                        # Report for yesterday
./rep 2024-01-15             # Report for specific date
./rep -H solardirector       # Use specific InfluxDB host
./rep -tz UTC 2024-01-15     # Use specific timezone

# Generate daily report (Original Bash version)
./rep.sh                     # Report for yesterday
./rep.sh 2024-01-15          # Report for specific date

# Send email report (typically run from cron)
./solar_report

# Test InfluxDB queries
curl -s "http://localhost:8086/query?db=power&q=select+last(input_power)+from+inverter+tz('America/Chicago')"
```

## Configuration

### Key Parameters
- **InfluxDB**: `localhost:8086`, database: `power`
- **Timezone**: Auto-detected from system (Go version) or `America/Chicago` (Bash scripts)
- **Billing cycle**: Starts on day 11 of each month
- **Electricity rates**: $0.16/kWh consumption, $0.012/kWh feed-in
- **Email recipient**: `spshoecraft@gmail.com`

### Data Tables
- `inverter`: Main power measurement table
  - `input_power`: Negative values indicate grid consumption
  - `output_power`: Positive values indicate solar generation
  - `daily_energy`: Daily energy totals

## Testing Approach

```bash
# Build Go programs before testing
make

# Dry run with specific date
./rep 2024-01-01 | less

# Check InfluxDB connectivity
curl -s http://localhost:8086/ping

# Validate calculations for known date (Go version)
./hist 2023 2023 | grep "07-15"

# Validate calculations for known date (Bash version)
./hist.sh 2023 2023 | grep "2023-07-15"

# Test email formatting without sending
./solar_report | sed 's/^sendmail.*/cat/' | sh > test_email.txt

# Compare output between bash and Go versions
./rep.sh 2024-01-15 > bash_output.txt
./rep 2024-01-15 > go_output.txt
diff bash_output.txt go_output.txt
```

## Dependencies

### Bash Scripts (hist.sh, rep, solar_report)
- **InfluxDB 1.x**: Time-series database for power metrics
- **curl**: HTTP client for InfluxDB queries
- **jq**: JSON parsing for InfluxDB responses
- **dc/bc**: Precision arithmetic calculations
- **sendmail**: Email delivery
- **mkpdf**: PDF generation (external utility)

### Go Implementation (hist)
- **Go 1.11+**: Go compiler/runtime (uses ioutil for older Go compatibility)
- **InfluxDB 1.x**: Time-series database (accessed via HTTP API)
- No external Go dependencies - uses only standard library

## Common Issues and Solutions

- **No data returned**: Check InfluxDB retention policies and data presence
- **Incorrect totals**: Verify timezone settings match data collection timezone
- **Email not sending**: Check sendmail configuration and mail queue
- **Billing cycle mismatch**: Adjust `bill_day` variable in `rep` script