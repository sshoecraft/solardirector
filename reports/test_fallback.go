package main

import (
	"fmt"
	"time"
)

func main() {
	location, _ := time.LoadLocation("America/Chicago")
	date := time.Date(2024, 9, 12, 12, 0, 0, 0, location)

	// Test with missing location.conf (should use 8am-8pm)
	coords, err := getCoordinates()
	if err != nil {
		fmt.Printf("location.conf not found: %v\n", err)
		fmt.Printf("Using fallback: 8am to 8pm local time\n")
		startTime := time.Date(date.Year(), date.Month(), date.Day(), 8, 0, 0, 0, location)
		endTime := time.Date(date.Year(), date.Month(), date.Day(), 20, 0, 0, 0, location)
		fmt.Printf("Start time: %s\n", startTime.Format("2006-01-02 15:04:05 MST"))
		fmt.Printf("End time: %s\n", endTime.Format("2006-01-02 15:04:05 MST"))
	} else {
		fmt.Printf("Found coordinates: lat=%.6f, lon=%.6f\n", coords.Lat, coords.Lon)
	}
}