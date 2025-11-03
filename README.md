# SolarDirector

A comprehensive solar power management system written in C that provides MQTT-based agents for monitoring and controlling various solar hardware components including BMS (Battery Management Systems), inverters, and other power electronics.

<<<<<<< Updated upstream
## Platform Support

- **Linux** (Debian-based distributions)
- **macOS** (with Homebrew or MacPorts)

## Dependencies

### Linux (Debian/Ubuntu)
```bash
apt-get install libssl-dev
apt-get install libcurl4-openssl-dev
apt-get install libreadline-dev
apt-get install libbluetooth-dev  # only if using bluetooth
```

### macOS
**Option 1: Homebrew**
```bash
brew install openssl curl readline
# For bluetooth support (optional):
brew install pkg-config glib
```

**Option 2: MacPorts**
```bash
sudo port install openssl curl readline
# For bluetooth support (optional):
sudo port install pkgconfig glib2
```

### Required Libraries
=======
	apt-get install libssl-dev
	apt-get install libcurl4-openssl-dev
	apt-get install libreadline-dev
	apt-get install libglib2.0-dev libbluetooth-dev (only if using bluetooth)

if you want to enable the JDB agent to use bluetooth, you need to set:

BLUETOOTH=yes

in make.opts


For MQTT on Linux (on macos just install paho-mqtt from homebrew):
paho.mqtt.c (https://github.com/eclipse/paho.mqtt.c.git)

	mkdir -p build && cd build
	cmake -DPAHO_WITH_SSL=ON -DPAHO_BUILD_SHARED=TRUE -DPAHO_BUILD_STATIC=TRUE -DPAHO_ENABLE_TESTING=FALSE -DPAHO_BUILD_SAMPLES=FALSE ..
	make && make install

notable Agents:
	bms/jbd - jbd bms agent
	bms/jk - jikong bms agent
	inverter/si - sma sunny island agent
	inverter/sb - sma sunny boy agent
>>>>>>> Stashed changes

**paho.mqtt.c** (https://github.com/eclipse/paho.mqtt.c.git)
```bash
git clone https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c
mkdir -p build && cd build
cmake -DPAHO_WITH_SSL=ON -DPAHO_BUILD_SHARED=TRUE -DPAHO_BUILD_STATIC=TRUE -DPAHO_ENABLE_TESTING=FALSE -DPAHO_BUILD_SAMPLES=FALSE ..
make && make install
```

**gattlib** (https://github.com/labapart/gattlib.git) - *Only for Bluetooth support*
```bash
git clone https://github.com/labapart/gattlib.git
cd gattlib
mkdir -p build && cd build
cmake -DGATTLIB_BUILD_EXAMPLES=NO -DGATTLIB_SHARED_LIB=NO -DGATTLIB_BUILD_DOCS=NO -DGATTLIB_PYTHON_INTERFACE=NO ..
make && make install
```

## Building

### Standard Build
```bash
git clone https://github.com/sshoecraft/solardirector.git
cd solardirector
make
```

### macOS Build Notes
The build system automatically detects macOS and configures the appropriate compiler (clang) and library paths. See [macOS Build Testing Guide](docs/macos-build-testing.md) for detailed testing instructions.

## Bluetooth Setup

If using Bluetooth, you must pair the device first:
```bash
$ bluetoothctl 
# scan on
(look for your device)
[NEW] Device XX:XX:XX:XX:XX:XX name
# trust XX:XX:XX:XX:XX:XX
# pair XX:XX:XX:XX:XX:XX
(it may ask for your passkey)
```

## Agents

Available hardware interface agents:
- **bms/jbd** - JBD BMS agent
- **bms/jk** - JiKong BMS agent  
- **inverter/si** - SMA Sunny Island agent
- **inverter/sb** - SMA Sunny Boy agent

Agents report to MQTT at regular intervals and can be configured via MQTT messages.

## Configuration

Use `util/sdconfig` to get/set parameters:

```bash
# Get parameter value
sdconfig pack_01 get BalanceWindow
sdconfig si get GdManStr

# Set parameter value  
sdconfig pack_01 set BalanceWindow 0.15
sdconfig si set GdManStr Start

# List all parameters for an agent
sdconfig -l sb
```

## Documentation

- [Project Planning](PLANNING.md) - Architecture and development guidelines
- [Task Tracking](TASK.md) - Current development status
- [macOS Build Testing](docs/macos-build-testing.md) - Testing guide for macOS builds

## Troubleshooting

For build issues on macOS, see the [macOS Build Testing Guide](docs/macos-build-testing.md).

For general build problems, ensure all dependencies are properly installed and accessible in your system's library paths.
