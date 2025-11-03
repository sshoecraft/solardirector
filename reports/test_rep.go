package main

import (
	"fmt"
	"time"
)

func main() {
	fmt.Println("Test started")

	// Simple test to make sure the program runs
	reportDate := time.Now().AddDate(0, 0, -1)
	fmt.Printf("Report date: %s\n", reportDate.Format("2006-01-02"))

	fmt.Println("Test completed")
}