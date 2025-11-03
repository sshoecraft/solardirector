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
	
	// Billing configuration
	BILL_DAY   = 11
	COST_KWH   = 0.16
	FEED_RATE  = 0.012
)

var (
	influxHost string
	timezone   string
	location   *time.Location
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
	Date  time.Time
	Usage float64
	Valid bool
}

type UsageData struct {
	Date  string
	Usage float64
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
	
	// Default to America/Chicago to match bash script
	return DEFAULT_TIMEZONE
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

func influxQueryWithEpoch(query string, epoch string) (*InfluxResponse, error) {
	// Build query URL
	queryURL := fmt.Sprintf("%s/query", influxHost)
	params := url.Values{}
	params.Add("db", INFLUX_DB)
	params.Add("q", query+fmt.Sprintf(" tz('%s')", timezone))
	if epoch != "" {
		params.Add("epoch", epoch)
	}

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

func influxSingleValue(query string) float64 {
	resp, err := influxQuery(query)
	if err != nil || len(resp.Results) == 0 || len(resp.Results[0].Series) == 0 {
		return 0.0
	}

	values := resp.Results[0].Series[0].Values
	if len(values) == 0 || len(values[0]) < 2 {
		return 0.0
	}

	// Parse value
	switch v := values[0][1].(type) {
	case float64:
		return v
	case int:
		return float64(v)
	case string:
		val, _ := strconv.ParseFloat(v, 64)
		return val
	default:
		return 0.0
	}
}

func getFirstYear() int {
	query := "select first(input_power) from inverter"
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

func collectData(startDate, endDate time.Time) ([]UsageData, error) {
	// Query to get daily sum of negative input power (converted to kWh)
	query := fmt.Sprintf(
		"select sum(input_power)*-0.0000027778 from inverter where input_power < 0 and time >= '%s 00:00:00' and time < '%s 23:59:59' group by time(1d) fill(null)",
		startDate.Format("2006-01-02"), endDate.Format("2006-01-02"),
	)

	resp, err := influxQuery(query)
	if err != nil {
		return nil, err
	}

	var data []UsageData

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
				}
				
				data = append(data, UsageData{
					Date:  t.Format("2006-01-02"),
					Usage: usage,
				})
			}
		}
	}

	return data, nil
}

func getUsageForDate(data []UsageData, date string) float64 {
	for _, d := range data {
		if d.Date == date {
			return d.Usage
		}
	}
	return 0.0
}

func getFeedForRange(startDate, endDate time.Time) float64 {
	total := 0.0
	current := startDate
	
	for current.Before(endDate) {
		query := fmt.Sprintf(
			"select sum(input_power)*0.0000027778 from inverter where input_power > 0 and time > '%s 00:00:00' and time < '%s 23:59:59'",
			current.Format("2006-01-02"), current.Format("2006-01-02"),
		)
		
		feed := influxSingleValue(query)
		if feed > 0 {
			total += feed
		}
		
		current = current.AddDate(0, 0, 1)
	}
	
	return total
}

func displayUsage(reportDate time.Time, data []UsageData, firstYear int) {
	reportDateStr := reportDate.Format("2006-01-02")
	reportDayUsage := getUsageForDate(data, reportDateStr)
	
	previousYearDate := reportDate.AddDate(-1, 0, 0)
	previousYearDateStr := previousYearDate.Format("2006-01-02")
	previousYearUsage := getUsageForDate(data, previousYearDateStr)
	
	fmt.Printf("%-30s %.2f kWh\n", "Usage:", reportDayUsage)
	fmt.Printf("%-30s %.2f kWh\n", "Previous year:", previousYearUsage)
	
	// Calculate percentage difference
	var diff float64
	var text string
	
	if reportDayUsage == 0 {
		if previousYearUsage == 0 {
			diff = 0.0
		} else {
			diff = 100.0
			text = "less"
		}
	} else {
		if previousYearUsage == 0 {
			diff = 100.0
			text = "more"
		} else {
			if previousYearUsage > reportDayUsage {
				pct := (reportDayUsage / previousYearUsage) * 100
				diff = 100.0 - pct
				text = "less"
			} else {
				pct := (previousYearUsage / reportDayUsage) * 100
				diff = 100.0 - pct
				text = "more"
			}
		}
	}
	
	fmt.Printf("%-30s %.1f%% %s\n", "Usage vs Previous year:", diff, text)
	
	// Calculate historical average for this day
	total := 0.0
	count := 0
	monthDay := reportDate.Format("01-02")
	
	for year := firstYear; year < reportDate.Year(); year++ {
		dateStr := fmt.Sprintf("%d-%s", year, monthDay)
		usage := getUsageForDate(data, dateStr)
		if usage > 0 {
			total += usage
			count++
		}
	}
	
	var avg float64
	if count > 0 {
		avg = total / float64(count)
	}
	
	fmt.Printf("%-30s %.2f kWh\n", "Historical average:", avg)
	
	if avg > 0 {
		if avg > reportDayUsage {
			pct := (reportDayUsage / avg) * 100
			diff = 100.0 - pct
			text = "less"
		} else {
			pct := (avg / reportDayUsage) * 100
			diff = 100.0 - pct
			text = "more"
		}
		fmt.Printf("%-30s %.1f%% %s\n", "Usage vs Average:", diff, text)
	}
}

func displayCycle(startDate, endDate, today time.Time, data []UsageData) (float64, float64) {
	total := 0.0
	count := 0
	
	current := startDate
	for !current.After(today) && current.Before(endDate) {
		dateStr := current.Format("2006-01-02")
		usage := getUsageForDate(data, dateStr)
		if usage > 0 {
			total += usage
			count++
		}
		current = current.AddDate(0, 0, 1)
	}
	
	// Calculate cycle days
	cycleDays := int(endDate.Sub(startDate).Hours() / 24)
	
	avg := 0.0
	if count > 0 {
		avg = total / float64(count)
	}
	
	projected := avg * float64(cycleDays)
	if projected < total {
		projected = total
	}
	
	cost := projected * COST_KWH
	
	fmt.Println()
	fmt.Printf("%-30s %.2f kWh\n", "Cycle usage:", total)
	fmt.Printf("%-30s %.2f kWh\n", "Daily average:", avg)
	fmt.Printf("%-30s %.2f kWh\n", "Projected total:", projected)
	fmt.Printf("%-30s $%.2f\n", "Projected cost:", cost)
	
	return projected, cost
}

func displayLastCycle(startDate time.Time, data []UsageData, projected, cost float64) {
	lastYearStart := startDate.AddDate(-1, 0, 0)
	lastYearEnd := lastYearStart.AddDate(0, 1, 0)
	
	total := 0.0
	current := lastYearStart
	for current.Before(lastYearEnd) {
		dateStr := current.Format("2006-01-02")
		usage := getUsageForDate(data, dateStr)
		if usage > 0 {
			total += usage
		}
		current = current.AddDate(0, 0, 1)
	}
	
	fmt.Println()
	fmt.Printf("%-30s %.2f kWh\n", "Last year cycle usage:", total)
	
	if projected > 0 && total > 0 {
		var diff float64
		var text string
		
		if total > projected {
			pct := (projected / total) * 100
			diff = 100.0 - pct
			text = "less"
		} else {
			pct := (total / projected) * 100
			diff = 100.0 - pct
			text = "more"
		}
		
		fmt.Printf("%-30s %.1f%% %s\n", "Projected vs last year:", diff, text)
		
		if text == "less" {
			lastCost := total * COST_KWH
			savings := lastCost - cost
			fmt.Printf("%-30s $%.2f\n", "Savings vs last year:", savings)
		}
	}
}

func displayCycleAverage(startDate time.Time, data []UsageData, firstYear int, today time.Time, projected, cost float64) {
	total := 0.0
	count := 0
	
	lastYear := today.AddDate(-1, 0, 0).Year()
	
	for year := firstYear; year <= lastYear; year++ {
		yearStart := time.Date(year, startDate.Month(), startDate.Day(), 0, 0, 0, 0, location)
		yearEnd := yearStart.AddDate(0, 1, 0)
		
		cycleTotal := 0.0
		current := yearStart
		for current.Before(yearEnd) {
			dateStr := current.Format("2006-01-02")
			usage := getUsageForDate(data, dateStr)
			if usage > 0 {
				cycleTotal += usage
			}
			current = current.AddDate(0, 0, 1)
		}
		
		if cycleTotal > 0 {
			total += cycleTotal
			count++
		}
	}
	
	avg := 0.0
	if count > 0 {
		avg = total / float64(count)
	}
	
	avgCost := avg * COST_KWH
	
	fmt.Println()
	fmt.Printf("%-30s %.2f kWh\n", "Cycle average(all years):", avg)
	fmt.Printf("%-30s $%.2f\n", "Cycle average cost:", avgCost)
	
	if projected > 0 && avg > 0 {
		var diff float64
		var text string
		
		if avg > projected {
			pct := (projected / avg) * 100
			diff = 100.0 - pct
			text = "less"
		} else {
			pct := (avg / projected) * 100
			diff = 100.0 - pct
			text = "more"
		}
		
		fmt.Printf("%-30s %.1f%% %s\n", "Projected vs average:", diff, text)
		
		if text == "less" {
			savings := avgCost - cost
			fmt.Printf("%-30s $%.2f\n", "Savings vs average:", savings)
		}
	}
}

func displayFeed(startDate, endDate time.Time, cost float64) {
	total := getFeedForRange(startDate, endDate)
	
	// Calculate cycle days
	cycleDays := int(endDate.Sub(startDate).Hours() / 24)
	
	avg := total / float64(cycleDays)
	projected := avg * float64(cycleDays)
	if projected < total {
		projected = total
	}
	
	returns := projected * FEED_RATE
	overallCost := cost - returns
	
	fmt.Println()
	fmt.Printf("%-30s %.2f kWh\n", "Total power fed back to grid:", total)
	fmt.Printf("%-30s %.2f kWh\n", "Daily average of total:", avg)
	fmt.Printf("%-30s %.2f kWh\n", "Projected monthly total:", projected)
	fmt.Printf("%-30s $%.2f\n", "Projected monthly return:", returns)
	
	if cost > 0 {
		fmt.Println()
		fmt.Printf("%-30s $%.2f\n", ">>> Overall projected cost:", overallCost)
	}
}

func displayHourly(reportDate time.Time) {
	query := fmt.Sprintf(
		"select sum(input_power)*-0.0000027778 from inverter where input_power < 0 and time > '%s 00:00:00' and time < '%s 23:59:59' group by time(1h) fill(0)",
		reportDate.Format("2006-01-02"), reportDate.Format("2006-01-02"),
	)
	
	resp, err := influxQueryWithEpoch(query, "ns")
	if err != nil || len(resp.Results) == 0 || len(resp.Results[0].Series) == 0 {
		return
	}
	
	fmt.Println()
	fmt.Println("Hourly usage:")
	fmt.Printf("%-25s %6s\n", "Timestamp", "kWh")
	
	total := 0.0
	for _, series := range resp.Results[0].Series {
		for _, value := range series.Values {
			if len(value) >= 2 {
				// Parse timestamp (nanoseconds)
				var ts time.Time
				switch v := value[0].(type) {
				case float64:
					ns := int64(v)
					ts = time.Unix(0, ns).In(location)
				case string:
					// Try parsing as nanoseconds
					if ns, err := strconv.ParseInt(v, 10, 64); err == nil {
						ts = time.Unix(0, ns).In(location)
					} else {
						// Try parsing as RFC3339
						ts, _ = time.Parse(time.RFC3339, v)
					}
				}
				
				// Parse value
				var usage float64
				if value[1] != nil {
					switch v := value[1].(type) {
					case float64:
						usage = v
					case int:
						usage = float64(v)
					}
				}
				
				fmt.Printf("%-25s %6.2f\n", ts.Format("2006-01-02 15:04:05"), usage)
				total += usage
			}
		}
	}
	
	fmt.Printf("%-25s %6.2f\n", "Total:", total)
}

func displayLastHourly(reportDate time.Time) {
	lastYear := reportDate.AddDate(-1, 0, 0)
	
	query := fmt.Sprintf(
		"select sum(input_power)*-0.0000027778 from inverter where input_power < 0 and time > '%s 00:00:00' and time < '%s 23:59:59' group by time(1h) fill(0)",
		lastYear.Format("2006-01-02"), lastYear.Format("2006-01-02"),
	)
	
	resp, err := influxQueryWithEpoch(query, "ns")
	if err != nil || len(resp.Results) == 0 || len(resp.Results[0].Series) == 0 {
		return
	}
	
	fmt.Println()
	fmt.Println("Previous Year hourly usage:")
	fmt.Printf("%-25s %6s\n", "Timestamp", "kWh")
	
	total := 0.0
	for _, series := range resp.Results[0].Series {
		for _, value := range series.Values {
			if len(value) >= 2 {
				// Parse timestamp (nanoseconds)
				var ts time.Time
				switch v := value[0].(type) {
				case float64:
					ns := int64(v)
					ts = time.Unix(0, ns).In(location)
				case string:
					// Try parsing as nanoseconds
					if ns, err := strconv.ParseInt(v, 10, 64); err == nil {
						ts = time.Unix(0, ns).In(location)
					} else {
						// Try parsing as RFC3339
						ts, _ = time.Parse(time.RFC3339, v)
					}
				}
				
				// Parse value
				var usage float64
				if value[1] != nil {
					switch v := value[1].(type) {
					case float64:
						usage = v
					case int:
						usage = float64(v)
					}
				}
				
				fmt.Printf("%-25s %6.2f\n", ts.Format("2006-01-02 15:04:05"), usage)
				total += usage
			}
		}
	}
	
	fmt.Printf("%-25s %6.2f\n", "Total:", total)
}

func displayHistorical(startDate, endDate time.Time, data []UsageData, firstYear int, today time.Time) {
	fmt.Println()
	fmt.Println("Historical Usage:")
	fmt.Println()
	
	// Build header
	fmt.Printf("%-5s ", "date")
	
	thisYear := today.Year()
	histEndYear := endDate.Year()
	if startDate.Year() != endDate.Year() {
		histEndYear = endDate.Year() - 1
	}
	
	for year := firstYear; year <= histEndYear; year++ {
		fmt.Printf("  %7d", year)
	}
	fmt.Println()
	
	// Header separator
	fmt.Printf("%-5s ", "=====")
	for year := firstYear; year <= histEndYear; year++ {
		fmt.Printf("  %7s", "=======")
	}
	fmt.Println()
	
	// Initialize totals
	totals := make(map[int]float64)
	
	// Display data
	current := startDate
	for current.Before(endDate) {
		monthDay := current.Format("01-02")
		fmt.Printf("%s ", monthDay)
		
		for year := firstYear; year <= histEndYear; year++ {
			dateStr := fmt.Sprintf("%d-%s", year, monthDay)
			usage := getUsageForDate(data, dateStr)
			
			// Don't show today's data for current year
			if year == thisYear && dateStr == today.Format("2006-01-02") {
				fmt.Printf("  %7s", "NULL")
			} else if usage > 0 {
				totals[year] += usage
				fmt.Printf("  %7.2f", usage)
			} else {
				fmt.Printf("  %7s", "NULL")
			}
		}
		fmt.Println()
		
		// Handle leap year
		if monthDay == "02-28" {
			// Check if next day would be Feb 29
			nextDay := current.AddDate(0, 0, 1)
			if nextDay.Day() == 29 {
				current = nextDay.AddDate(0, 0, 1) // Skip to March 1
			} else {
				current = nextDay
			}
		} else if monthDay == "02-29" {
			current = current.AddDate(0, 0, 2) // Skip to March 1
		} else {
			current = current.AddDate(0, 0, 1)
		}
	}
	
	// Footer separator
	fmt.Printf("%-5s ", "=====")
	for year := firstYear; year <= histEndYear; year++ {
		fmt.Printf("  %7s", "=======")
	}
	fmt.Println()
	
	// Totals
	fmt.Printf("%-5s ", "Total")
	for year := firstYear; year <= histEndYear; year++ {
		fmt.Printf("  %7.2f", totals[year])
	}
	fmt.Println()
}

func main() {
	// Get system timezone
	timezone = getSystemTimezone()
	
	// Define command line flags
	var dateArg string
	flag.StringVar(&influxHost, "host", DEFAULT_INFLUX_HOST, "InfluxDB host (hostname, hostname:port, or full URL)")
	flag.StringVar(&influxHost, "H", DEFAULT_INFLUX_HOST, "InfluxDB host (shorthand)")
	flag.StringVar(&timezone, "tz", timezone, "Timezone for queries (default: system timezone)")
	
	// Parse command line arguments
	flag.Parse()
	args := flag.Args()
	
	if len(args) > 0 {
		dateArg = args[0]
	}
	
	// Normalize the host URL
	influxHost = normalizeHostURL(influxHost)
	
	// Load timezone location
	var err error
	location, err = time.LoadLocation(timezone)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error loading timezone: %v\n", err)
		os.Exit(1)
	}
	
	// Determine report date
	var today time.Time
	if dateArg != "" {
		today, err = time.ParseInLocation("2006-01-02", dateArg, location)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error parsing date: %v\n", err)
			os.Exit(1)
		}
	} else {
		today = time.Now().In(location)
	}
	
	reportDate := today.AddDate(0, 0, -1)
	
	// Calculate billing cycle dates
	var startDate, endDate time.Time
	
	if reportDate.Day() < BILL_DAY {
		// Previous month's billing cycle
		startDate = time.Date(reportDate.Year(), reportDate.Month()-1, BILL_DAY, 0, 0, 0, 0, location)
		endDate = time.Date(reportDate.Year(), reportDate.Month(), BILL_DAY, 0, 0, 0, 0, location)
	} else {
		// Current month's billing cycle
		startDate = time.Date(reportDate.Year(), reportDate.Month(), BILL_DAY, 0, 0, 0, 0, location)
		endDate = time.Date(reportDate.Year(), reportDate.Month()+1, BILL_DAY, 0, 0, 0, 0, location)
	}
	
	// Get first year of data
	firstYear := getFirstYear()
	
	// Limit how far back we go (5 years)
	fiveYearsAgo := today.Year() - 5
	if firstYear < fiveYearsAgo {
		firstYear = fiveYearsAgo
	}
	
	// Collect all data from first year to end date
	dataStartDate := time.Date(firstYear, startDate.Month(), startDate.Day(), 0, 0, 0, 0, location)
	allData, err := collectData(dataStartDate, endDate.AddDate(1, 0, 0))
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error collecting data: %v\n", err)
		os.Exit(1)
	}
	
	// Ensure we include report year in firstYear if needed
	if reportDate.Year() < firstYear {
		firstYear = reportDate.Year()
	}
	
	// Display reports
	displayUsage(reportDate, allData, firstYear)
	projected, cost := displayCycle(startDate, endDate, today, allData)
	displayLastCycle(startDate, allData, projected, cost)
	displayCycleAverage(startDate, allData, firstYear, today, projected, cost)
	displayFeed(startDate, endDate, cost)
	displayHourly(reportDate)
	displayLastHourly(reportDate)
	displayHistorical(startDate, endDate, allData, firstYear, today)
}