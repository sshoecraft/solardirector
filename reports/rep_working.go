package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"net/http"
	"net/url"
	"os"
	"strconv"
	"strings"
	"time"
)

const (
	// InfluxDB configuration defaults
	DEFAULT_INFLUX_HOST = "http://localhost:8086"
	INFLUX_DB           = "power"
	DEFAULT_TIMEZONE    = "America/Chicago"
)

var (
	influxHost string
	timezone   string
)

type InfluxResponse struct {
	Results []struct {
		StatementID int `json:"statement_id"`
		Series      []struct {
			Name    string          `json:"name"`
			Columns []string        `json:"columns"`
			Values  [][]interface{} `json:"values"`
		} `json:"series"`
	} `json:"results"`
}

type DailyUsage struct {
	Date  string
	Usage float64
	Valid bool
}

func influxQuery(query string) (*InfluxResponse, error) {
	// Build query URL
	queryURL := fmt.Sprintf("%s/query", influxHost)
	params := url.Values{}
	params.Add("db", INFLUX_DB)
	params.Add("q", query+fmt.Sprintf(" tz('%s')", timezone))

	fullURL := queryURL + "?" + params.Encode()

	// Make HTTP request
	resp, err := http.Get(fullURL)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	// Read response
	body, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	// Parse JSON
	var influxResp InfluxResponse
	err = json.Unmarshal(body, &influxResp)
	if err != nil {
		return nil, err
	}

	return &influxResp, nil
}

func getFirstYear() int {
	query := "select first(input_power) from inverter where input_power < 0"
	resp, err := influxQuery(query)
	if err != nil || len(resp.Results) == 0 || len(resp.Results[0].Series) == 0 {
		return time.Now().Year()
	}

	values := resp.Results[0].Series[0].Values
	if len(values) == 0 || len(values[0]) < 1 {
		return time.Now().Year()
	}

	// Parse timestamp
	timestamp, ok := values[0][0].(string)
	if !ok {
		return time.Now().Year()
	}

	t, err := time.Parse(time.RFC3339, timestamp)
	if err != nil {
		return time.Now().Year()
	}

	return t.Year()
}

func collectData(startDate, endDate string) (map[string]float64, error) {
	// Query to get daily sum of negative input power (converted to kWh)
	query := fmt.Sprintf(
		"select sum(input_power)*-0.0000027778 from inverter where input_power < 0 and time >= '%s 00:00:00' and time < '%s 00:00:00' group by time(1d) fill(null)",
		startDate, endDate,
	)

	resp, err := influxQuery(query)
	if err != nil {
		return nil, err
	}

	data := make(map[string]float64)

	if len(resp.Results) == 0 || len(resp.Results[0].Series) == 0 {
		return data, nil
	}

	for _, series := range resp.Results[0].Series {
		for _, value := range series.Values {
			if len(value) >= 2 {
				// Parse timestamp
				timestamp, ok := value[0].(string)
				if !ok {
					continue
				}

				t, err := time.Parse(time.RFC3339, timestamp)
				if err != nil {
					continue
				}

				// Parse value
				var usage float64
				if value[1] != nil {
					switch v := value[1].(type) {
					case float64:
						usage = v
					case int:
						usage = float64(v)
					case string:
						usage, _ = strconv.ParseFloat(v, 64)
					}
					data[t.Format("2006-01-02")] = usage
				}
			}
		}
	}

	return data, nil
}

func displayHistoricalReport(startYear, startMonth, startDay, endYear, endMonth, endDay int, data map[string]float64) {
	fmt.Println("\nHistorical Usage:")

	// Determine the actual end year for display
	histEndYear := endYear
	if endMonth == 1 && endDay == 1 {
		histEndYear = endYear - 1
	}

	// Build header
	fmt.Printf("%-7s ", "date")
	for year := startYear; year <= histEndYear; year++ {
		fmt.Printf("  %8d", year)
	}
	fmt.Println()

	// Header separator
	fmt.Printf("%-7s ", "=====")
	for year := startYear; year <= histEndYear; year++ {
		fmt.Printf("  %8s", "========")
	}
	fmt.Println()

	// Initialize tracking variables
	totals := make(map[int]float64)
	mins := make(map[int]float64)
	maxs := make(map[int]float64)
	counts := make(map[int]int)

	for year := startYear; year <= histEndYear; year++ {
		mins[year] = 99999999.99
	}

	// Use a leap year for iteration
	iterYear := 2020
	if endYear != startYear {
		// Different years, iterate from start month/day to end month/day
	} else {
		// Same year
	}

	// Create date for iteration
	startDate := time.Date(iterYear, time.Month(startMonth), startDay, 0, 0, 0, 0, time.UTC)
	endIterYear := iterYear
	if endYear != startYear {
		endIterYear = iterYear + 1
	}
	endDate := time.Date(endIterYear, time.Month(endMonth), endDay, 0, 0, 0, 0, time.UTC)

	// Iterate through dates and display data
	currentDate := startDate
	for currentDate.Before(endDate) {
		month := currentDate.Month()
		day := currentDate.Day()
		fmt.Printf("%02d-%02d   ", month, day)

		for year := startYear; year <= histEndYear; year++ {
			dateKey := fmt.Sprintf("%d-%02d-%02d", year, month, day)
			usage, exists := data[dateKey]
			
			if exists && usage > 0 {
				totals[year] += usage
				counts[year]++
				if usage < mins[year] {
					mins[year] = usage
				}
				if usage > maxs[year] {
					maxs[year] = usage
				}
				fmt.Printf("  %8.2f", usage)
			} else {
				fmt.Printf("  %8s", "NULL")
			}
		}
		fmt.Println()

		// Handle leap year for February
		if month == 2 && day == 28 {
			currentDate = currentDate.AddDate(0, 0, 2) // Skip to March 1
		} else if month == 2 && day == 29 {
			currentDate = currentDate.AddDate(0, 0, 1)
		} else {
			currentDate = currentDate.AddDate(0, 0, 1)
		}
	}

	// Footer separator
	fmt.Printf("%-7s ", "=====")
	for year := startYear; year <= histEndYear; year++ {
		fmt.Printf("  %8s", "========")
	}
	fmt.Println()

	// Totals
	fmt.Printf("%-7s ", "Total")
	for year := startYear; year <= histEndYear; year++ {
		fmt.Printf("  %8.2f", totals[year])
	}
	fmt.Println()

	// Minimums
	fmt.Printf("%-7s ", "Min")
	for year := startYear; year <= histEndYear; year++ {
		if counts[year] > 0 {
			fmt.Printf("  %8.2f", mins[year])
		} else {
			fmt.Printf("  %8s", "NULL")
		}
	}
	fmt.Println()

	// Maximums
	fmt.Printf("%-7s ", "Max")
	for year := startYear; year <= histEndYear; year++ {
		if counts[year] > 0 {
			fmt.Printf("  %8.2f", maxs[year])
		} else {
			fmt.Printf("  %8s", "NULL")
		}
	}
	fmt.Println()

	// Averages
	fmt.Printf("%-7s ", "Average")
	for year := startYear; year <= histEndYear; year++ {
		if counts[year] > 0 {
			avg := totals[year] / float64(counts[year])
			fmt.Printf("  %8.2f", avg)
		} else {
			fmt.Printf("  %8s", "NULL")
		}
	}
	fmt.Println()

	// Year Projection
	fmt.Printf("%-7s ", "YearPro")
	for year := startYear; year <= histEndYear; year++ {
		if counts[year] > 0 {
			avg := totals[year] / float64(counts[year])
			projection := avg * 366 // Always use 366 for consistency
			fmt.Printf("  %8.2f", projection)
		} else {
			fmt.Printf("  %8s", "NULL")
		}
	}
	fmt.Println()
}

func getSystemTimezone() string {
	// Try to get timezone from TZ environment variable first
	if tz := os.Getenv("TZ"); tz != "" {
		return tz
	}
	
	// Try to read /etc/localtime symlink (POSIX method)
	if link, err := os.Readlink("/etc/localtime"); err == nil {
		// Extract timezone from symlink path like /usr/share/zoneinfo/America/Chicago
		if strings.Contains(link, "/zoneinfo/") {
			parts := strings.Split(link, "/zoneinfo/")
			if len(parts) == 2 {
				return parts[1]
			}
		}
	}
	
	// Try to read /etc/timezone file (Debian/Ubuntu)
	if data, err := ioutil.ReadFile("/etc/timezone"); err == nil {
		tz := strings.TrimSpace(string(data))
		if tz != "" {
			return tz
		}
	}
	
	// Try to read /var/db/zoneinfo (macOS)
	if data, err := ioutil.ReadFile("/var/db/zoneinfo"); err == nil {
		tz := strings.TrimSpace(string(data))
		if tz != "" {
			return tz
		}
	}
	
	// Fall back to Go's location detection
	loc := time.Now().Location()
	name := loc.String()
	
	// If still "Local", default to America/Chicago to match bash script
	if name == "Local" {
		return "America/Chicago"
	}
	
	return name
}

func parseDate(dateStr string) (int, int, int, error) {
	parts := strings.Split(dateStr, "-")
	if len(parts) < 1 {
		return 0, 0, 0, fmt.Errorf("invalid date format")
	}

	year, err := strconv.Atoi(parts[0])
	if err != nil {
		return 0, 0, 0, err
	}

	month := 1
	if len(parts) > 1 {
		month, err = strconv.Atoi(parts[1])
		if err != nil {
			return 0, 0, 0, err
		}
	}

	day := 1
	if len(parts) > 2 {
		day, err = strconv.Atoi(parts[2])
		if err != nil {
			return 0, 0, 0, err
		}
	}

	return year, month, day, nil
}

func normalizeHostURL(host string) string {
	// If already a full URL, return as-is
	if strings.HasPrefix(host, "http://") || strings.HasPrefix(host, "https://") {
		return host
	}
	
	// If it contains a port, add http://
	if strings.Contains(host, ":") {
		return "http://" + host
	}
	
	// Just a hostname, add http:// and default port 8086
	return "http://" + host + ":8086"
}

func main() {
	// Get system timezone
	timezone = getSystemTimezone()
	
	// Define command line flags
	flag.StringVar(&influxHost, "host", DEFAULT_INFLUX_HOST, "InfluxDB host (hostname, hostname:port, or full URL)")
	flag.StringVar(&influxHost, "H", DEFAULT_INFLUX_HOST, "InfluxDB host (shorthand)")
	flag.StringVar(&timezone, "tz", timezone, "Timezone for queries (default: system timezone)")
	
	// Parse command line arguments
	flag.Parse()
	args := flag.Args()
	
	// Normalize the host URL
	influxHost = normalizeHostURL(influxHost)

	var startYear, startMonth, startDay int
	var endYear, endMonth, endDay int

	// Determine start date
	if len(args) >= 1 {
		var err error
		startYear, startMonth, startDay, err = parseDate(args[0])
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error parsing start date: %v\n", err)
			os.Exit(1)
		}
	} else {
		// Get first year from database
		startYear = getFirstYear()
		startMonth = 1
		startDay = 1
	}

	// Determine end date
	if len(args) >= 2 {
		var err error
		endYear, endMonth, endDay, err = parseDate(args[1])
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error parsing end date: %v\n", err)
			os.Exit(1)
		}
	} else {
		// Default to next year January 1st
		endYear = time.Now().Year() + 1
		endMonth = 1
		endDay = 1
	}

	// Validate dates
	startDate := time.Date(startYear, time.Month(startMonth), startDay, 0, 0, 0, 0, time.UTC)
	endDate := time.Date(endYear, time.Month(endMonth), endDay, 0, 0, 0, 0, time.UTC)
	
	if endDate.Before(startDate) {
		fmt.Fprintf(os.Stderr, "error: end cannot be before start\n")
		os.Exit(1)
	}

	// Format dates for query
	startStr := fmt.Sprintf("%04d-%02d-%02d", startYear, startMonth, startDay)
	endStr := fmt.Sprintf("%04d-%02d-%02d", endYear, endMonth, endDay)

	// Collect data
	data, err := collectData(startStr, endStr)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error collecting data: %v\n", err)
		os.Exit(1)
	}

	// Display historical report
	displayHistoricalReport(startYear, startMonth, startDay, endYear, endMonth, endDay, data)
}