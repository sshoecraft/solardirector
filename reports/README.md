# AC Performance Analysis Tool

This Go program analyzes AC unit water temperature differentials from InfluxDB data.

## Usage

```bash
# Basic usage - defaults to AC1 unit and InfluxDB at 192.168.1.164
go run acperf.go

# Specify different AC unit
go run acperf.go ac2

# Specify different InfluxDB server
go run acperf.go -server 192.168.1.100

# Specify both unit and server
go run acperf.go -unit ac2 -server 192.168.1.100

# Using flags
go run acperf.go -unit ac1 -server 192.168.1.164
```

## Command Line Arguments

- `unit` (positional or flag): AC unit name (default: "ac1")
- `-server`: InfluxDB server address (default: "192.168.1.164")
- `-unit`: AC unit name (alternative to positional argument)

## Examples

```bash
# Analyze AC1 performance (default)
go run acperf.go

# Analyze AC2 performance
go run acperf.go ac2

# Use different InfluxDB server
go run acperf.go -server 192.168.1.100 ac1

# Full specification
go run acperf.go -unit ac2 -server 192.168.1.200
```

## Output

The program provides:
- Average, median, min, max water temperature differentials
- Heat transfer analysis (assuming 20.2 GPM flow rate)
- Water temperature ranges (inlet/outlet)
- System operation mode (cooling/heating)
- Operation time period

## Building

```bash
# Build executable
go build -o acperf acperf.go

# Run built executable
./acperf ac1
```

## Requirements

- Go 1.21+
- Access to InfluxDB server on port 8086
- HVAC database with `units` and `pumps` measurements
