# s10m - E3/DC S10 Modbus to MQTT connector
[![GitHub sourcecode](https://img.shields.io/badge/Source-GitHub-green)](https://github.com/pvtom/s10m/)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/pvtom/s10m)](https://github.com/pvtom/s10m/releases/latest)
[![GitHub last commit](https://img.shields.io/github/last-commit/pvtom/s10m)](https://github.com/pvtom/s10m/commits)
[![GitHub issues](https://img.shields.io/github/issues/pvtom/s10m)](https://github.com/pvtom/s10m/issues)
[![GitHub pull requests](https://img.shields.io/github/issues-pr/pvtom/s10m)](https://github.com/pvtom/s10m/pulls)
[![GitHub](https://img.shields.io/github/license/pvtom/s10m)](https://github.com/pvtom/s10m/blob/main/LICENSE)

This software module connects a home power station from E3/DC to an MQTT broker. It uses the Modbus interface of the S10 device.

Developed and tested with a Raspberry Pi and a Linux PC (x86_64).

The tool s10m queries the data from the home power station and sends it to the MQTT broker. The following topics are supported:

- s10/autarky
- s10/consumption
- s10/battery/charging/lock
- s10/battery/discharging/lock
- s10/emergency/ready
- s10/battery/weather_regulation
- s10/grid/limit
- s10/idle_period/charging/active
- s10/idle_period/discharging/active
- s10/battery/soc
- s10/battery/power
- s10/battery/state
- s10/emergency/mode
- s10/firmware
- s10/grid/power
- s10/grid/state
- s10/grid/power/L1
- s10/grid/power/L2
- s10/grid/power/L3
- s10/home/power
- s10/addon/power
- s10/manufacturer
- s10/model
- s10/serial_number
- s10/solar/power
- s10/current/string_1
- s10/current/string_2
- s10/power/string_1
- s10/power/string_2
- s10/voltage/string_1
- s10/voltage/string_2
- s10/pvi/apparent_power/L1
- s10/pvi/apparent_power/L2
- s10/pvi/apparent_power/L3
- s10/pvi/active_power/L1
- s10/pvi/active_power/L2
- s10/pvi/active_power/L3
- s10/pvi/reactive_power/L1
- s10/pvi/reactive_power/L2
- s10/pvi/reactive_power/L3
- s10/pvi/voltage/L1
- s10/pvi/voltage/L2
- s10/pvi/voltage/L3
- s10/pvi/current/L1
- s10/pvi/current/L2
- s10/pvi/current/L3
- s10/grid/frequency
- s10/pvi/power/string_1
- s10/pvi/power/string_2
- s10/pvi/voltage/string_1
- s10/pvi/voltage/string_2
- s10/pvi/current/string_1
- s10/pvi/current/string_2

If one or more E3/DC wallboxes are available:

- s10/wallbox/[0-7]/available
- s10/wallbox/[0-7]/sun_mode
- s10/wallbox/[0-7]/ready
- s10/wallbox/[0-7]/charging
- s10/wallbox/[0-7]/1phase
- s10/wallbox/total/power
- s10/wallbox/solar/power

The prefix of the topics can be configured by the attribute ROOT_TOPIC. By default all topics start with "s10". This can be changed to any other string that MQTT accepts as a topic.

## Docker

Instead of installing the package you can use the [Docker image](DOCKER.md).

## Prerequisite

- S10 configuration: Switch on the Modbus interface (mode E3/DC, not Sunspec)
- An existing MQTT broker in your environment, e.g. a Mosquitto (https://mosquitto.org)
- s10m needs the library libmosquitto. To install it on a Raspberry Pi enter:

```
sudo apt-get install -y build-essential git automake autoconf libtool libmosquitto-dev
```
- s10m connects the S10 via the Modbus protocol, so you have to install a Modbus library:
```
git clone https://github.com/stephane/libmodbus.git
cd libmodbus/
./autogen.sh
./configure
sudo make install
```

## Cloning the Repository

```
sudo apt-get install git # if necessary
git clone https://github.com/pvtom/s10m.git
```

## Compilation

```
cd s10m
make
```

## Installation

```
sudo mkdir -p /opt/s10m
sudo chown pi:pi /opt/s10m/
```
Adjust user and group (pi:pi) if you use another user.

```
cp -a s10m /opt/s10m
```

## Configuration

Copy the config template file into the directory `/opt/s10m`
```
cp config.template /opt/s10m/.config
```

Please change to the directory `/opt/s10m` and edit `.config` to adjust to your configuration:

```
cd /opt/s10m
nano .config
```

```
// Host name of the E3/DC S10 device
MODBUS_HOST=e3dc
// Port of the E3/DC S10 device, default is 502
MODBUS_PORT=502
// Target MQTT broker
MQTT_HOST=localhost
// Default port is 1883
MQTT_PORT=1883
// MQTT user / password authentication necessary? Depends on the MQTT broker configuration.
MQTT_AUTH=false
// if true, then enter here
MQTT_USER=
MQTT_PASSWORD=
// MQTT parameters
MQTT_QOS=0
MQTT_RETAIN=false
// Interval requesting the E3/DC S10 device in seconds
INTERVAL=1
// Root topic
ROOT_TOPIC=s10
// Force mode (publish also unchanged topics)
FORCE=false
```

s10m will also start wihout a `.config` file. In this case s10m will use the default values.

## Test

Start the program:

```
./s10m
```

If everything works properly, you get output like this:

```
Connecting...
E3DC system e3dc:502 (Modbus)
MQTT broker localhost:1883 qos = 0 retain = false
Fetching data every second.

Connecting to MQTT broker localhost:1883
MQTT: Connected successfully
E3DC_MODBUS: Connected successfully
MQTT: publish topic >s10/autarky< payload >100<
MQTT: publish topic >s10/consumption< payload >15<
MQTT: publish topic >s10/battery/charging/limit< payload >0<
MQTT: publish topic >s10/battery/charging/lock< payload >0<
MQTT: publish topic >s10/battery/discharging/lock< payload >0<
MQTT: publish topic >s10/emergency/ready< payload >1<
MQTT: publish topic >s10/grid/limit< payload >0<
MQTT: publish topic >s10/battery/soc< payload >60<
MQTT: publish topic >s10/battery/power< payload >4503<
MQTT: publish topic >s10/battery/state< payload >CHARGING<
MQTT: publish topic >s10/emergency/mode< payload >INACTIVE<
MQTT: publish topic >s10/firmware< payload >S10_2022_026<
MQTT: publish topic >s10/grid/power< payload >-3189<
MQTT: publish topic >s10/grid/state< payload >IN<
MQTT: publish topic >s10/grid/power/L1< payload >-1104<
MQTT: publish topic >s10/grid/power/L2< payload >-924<
MQTT: publish topic >s10/grid/power/L3< payload >-1161<
MQTT: publish topic >s10/home/power< payload >904<
MQTT: publish topic >s10/manufacturer< payload >HagerEnergy GmbH<
MQTT: publish topic >s10/model< payload >S10 E AIO<
MQTT: publish topic >s10/serial_number< payload >S10-XXXXXXXXXXXX<
MQTT: publish topic >s10/solar/power< payload >8596<
MQTT: publish topic >s10/string_1/current< payload >9.20<
MQTT: publish topic >s10/string_2/current< payload >9.36<
MQTT: publish topic >s10/string_1/power< payload >4434<
MQTT: publish topic >s10/string_2/power< payload >4162<
MQTT: publish topic >s10/string_1/voltage< payload >481<
MQTT: publish topic >s10/string_2/voltage< payload >443<
...
```

Check the configuration if the connections are not established.

If you use the Mosquitto tools you can subscribe the topics with

```
mosquitto_sub -h localhost -p 1883 -t 's10/#' -v
```

Stop s10m with Crtl-C and start it in the background.

## Daemon Mode

Start the program in daemon mode:

```
./s10m -d
```

If you like to start s10m during the system start, use `/etc/rc.local`. Add the following line before `exit 0`.

```
(cd /opt/s10m ; /usr/bin/sudo -H -u pi /opt/s10m/s10m -d)
```
Adjust the user (pi) if you use another user.

The daemon can be terminated with
```
pkill s10m
```
Be careful that the program runs only once.

Alternatively, s10m can be managed by systemd. To do this, copy the file `s10m.service` to the systemd directory:
```
sudo cp -a s10m.service /etc/systemd/system/
```
Configure the service `sudo nano s10m.service` (adjust user 'User=pi'), if needed.

Register the service and start it with:
```
sudo systemctl start s10m
sudo systemctl enable s10m
```

## Used external libraries

- Eclipse Mosquitto (https://github.com/eclipse/mosquitto)
- Modbus library (https://github.com/stephane/libmodbus)
