package main

import (
	"fmt"
	"time"
)

func main() {
	// Test coordinates from location.conf
	lat := 31.809158
	lon := -95.144338

	// Test date
	date := time.Date(2024, 9, 12, 12, 0, 0, 0, time.Local)

	sunrise, sunset := GetSunTimes(date, lat, lon)

	fmt.Printf("Date: %s\n", date.Format("2006-01-02"))
	fmt.Printf("Latitude: %.6f\n", lat)
	fmt.Printf("Longitude: %.6f\n", lon)
	fmt.Printf("Sunrise: %s\n", sunrise.Format("2006-01-02 15:04:05 MST"))
	fmt.Printf("Sunset: %s\n", sunset.Format("2006-01-02 15:04:05 MST"))
	fmt.Printf("Sunrise + 2h: %s\n", sunrise.Add(2*time.Hour).Format("2006-01-02 15:04:05 MST"))
	fmt.Printf("Sunset + 2h: %s\n", sunset.Add(2*time.Hour).Format("2006-01-02 15:04:05 MST"))
}