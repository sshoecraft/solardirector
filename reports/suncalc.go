package main

import (
	"bufio"
	"math"
	"os"
	"strconv"
	"strings"
	"time"
)

// Sun calculation constants
const (
	J1970 = 2440588.0
	J2000 = 2451545.0
	RAD   = math.Pi / 180.0
	E     = RAD * 23.4397 // obliquity of the Earth
)

// Coordinates structure
type Coordinates struct {
	Lat float64
	Lon float64
}

// Read location from /opt/sd/etc/location.conf
func getCoordinates() (*Coordinates, error) {
	file, err := os.Open("/opt/sd/etc/location.conf")
	if err != nil {
		return nil, err
	}
	defer file.Close()

	coords := &Coordinates{}
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasPrefix(line, "lat=") {
			coords.Lat, _ = strconv.ParseFloat(strings.TrimPrefix(line, "lat="), 64)
		} else if strings.HasPrefix(line, "lon=") {
			coords.Lon, _ = strconv.ParseFloat(strings.TrimPrefix(line, "lon="), 64)
		}
	}

	return coords, nil
}

// Julian date conversions
func toJulian(date time.Time) float64 {
	millis := date.Unix()*1000 + int64(date.Nanosecond()/1000000)
	return float64(millis)/86400000.0 - 0.5 + J1970
}

func fromJulian(j float64) time.Time {
	millis := int64((j + 0.5 - J1970) * 86400000)
	return time.Unix(0, millis*1000000)
}

func toDays(date time.Time) float64 {
	return toJulian(date) - J2000
}

// Solar calculations
func solarMeanAnomaly(d float64) float64 {
	return RAD * (357.5291 + 0.98560028*d)
}

func eclipticLongitude(M float64) float64 {
	C := RAD * (1.9148*math.Sin(M) + 0.02*math.Sin(2*M) + 0.0003*math.Sin(3*M))
	P := RAD * 102.9372
	return M + C + P + math.Pi
}

func declination(l, b float64) float64 {
	return math.Asin(math.Sin(b)*math.Cos(E) + math.Cos(b)*math.Sin(E)*math.Sin(l))
}

func julianCycle(d, lw float64) float64 {
	return math.Round(d - 0.0009 - lw/(2*math.Pi))
}

func approxTransit(Ht, lw, n float64) float64 {
	return 0.0009 + (Ht+lw)/(2*math.Pi) + n
}

func solarTransitJ(ds, M, L float64) float64 {
	return J2000 + ds + 0.0053*math.Sin(M) - 0.0069*math.Sin(2*L)
}

func hourAngle(h, phi, dec float64) float64 {
	cosH := (math.Sin(h) - math.Sin(phi)*math.Sin(dec)) / (math.Cos(phi) * math.Cos(dec))
	// Clamp value to avoid NaN from acos
	if cosH > 1 {
		cosH = 1
	} else if cosH < -1 {
		cosH = -1
	}
	return math.Acos(cosH)
}

func getSetJ(h, lw, phi, dec, n, M, L float64) float64 {
	w := hourAngle(h, phi, dec)
	a := approxTransit(w, lw, n)
	return solarTransitJ(a, M, L)
}

// GetSunTimes calculates sunrise and sunset times for a given date and location
func GetSunTimes(date time.Time, lat, lon float64) (sunrise, sunset time.Time) {
	lw := RAD * -lon
	phi := RAD * lat

	d := toDays(date)
	n := julianCycle(d, lw)
	ds := approxTransit(0, lw, n)

	M := solarMeanAnomaly(ds)
	L := eclipticLongitude(M)
	dec := declination(L, 0)

	Jnoon := solarTransitJ(ds, M, L)

	// Calculate sunrise and sunset (h = -0.833 degrees)
	h0 := -0.833 * RAD

	Jset := getSetJ(h0, lw, phi, dec, n, M, L)
	Jrise := Jnoon - (Jset - Jnoon)

	sunrise = fromJulian(Jrise)
	sunset = fromJulian(Jset)

	return sunrise, sunset
}