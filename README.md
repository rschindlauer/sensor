# Sensor

An IoT framework for measuring and tracking environment metrics. Relies heavily on other people's work.

## Components

- Sensor nodes transmitting packets over RF
- A gateway translating RF to MQTT over Wifi
- A receiving host, running
  - An MQTT broker
  - InfluxDB
  - An MQTT subscriber, receiving the sensor messages and storing them in InfluxDB
  - Grafana reading from InfluxDB and serving charts

## Sensor Nodes

The sensor nodes are based on the amazing [Moteino platform](https://lowpowerlab.com/guide/moteino/). The actual sensor is the [Si7021 from Adafruit](https://www.adafruit.com/product/3251), connected to the Moteino over SPI.

The Moteino is programmed over an FTDI USB connection. To program, we can use any Arduino IDE, see below.

### Arduino SDK

- Install Arduino SDK
- In the SDK, install the Moteino board (Add the Moteino core json definition URL to your Board Manager). Follow https://lowpowerlab.com/guide/moteino/programming-libraries/
- Connect to USB, select the right port in Tools menu
- Try Node example, should run and produce serial output (set baud rate to 115200 in serial UI)
- Add other libraries:
  - https://github.com/LowPowerLab/LowPower
  - https://github.com/adafruit/Adafruit_Sensor
  - https://github.com/adafruit/DHT-sensor-library

### PlatformIO

I actually prefer PlatformIO over the original Arduino SDK, as it is more suitable for scripting integration. Specifically, I am using the Visual Studio Code IDE with the PlatformIO plugin. Install the platformio VSCode plugin, and the platformio Python library (`pip install platformio`). Deploy the node code as follows:

```
cd node
platformio run --target upload -e moteino
```

Don't forget to give each node a unique node ID in `src/node.cpp`!

While connected to the serial port, you can monitor the serial output:

```
platformio device monitor --baud 115200 --port /dev/cu.usbserial-DN021JLW
```

This assumes a certain usb-serial port, depending on the cable being used (I used an FTDI header).

The node code itself is very straightforward. It uses Xose's RFM69GW project's node code, with a modified RFM69Manager, see below.

### Motion

The code under `motion/` is for PIR-based motion detector nodes, loosely based on LowPowerLab's MotionMotes.

## Gateway

The gateway is in folder `gateway`, based on Xose Pérez' excellent project https://bitbucket.org/xoseperez/rfm69gw. I used Adafruit's [ESP8266 Huzzah board](https://www.adafruit.com/product/2471), together with their [RFM69 breakout board](https://www.adafruit.com/product/3070). The wiring is straightforward (my newer version of the ESP8266 didn't require the swapping of GPIO #4 and #5 anymore), make sure to also connect the RFM69's reset pin (I used pin 16). I made the following changes to the gateway code:

- Reset the radio in `radioLoop`
- Change RFM69Manager to allow for sending and receiving a list of key-value pairs in the same message (also used by the node code)
- Also send RSSI values to MQTT

I programmed the ESP8266 through a regular serial (non-FTDI) cable, hence the definition of the upload port `/dev/cu.usbserial` in file `platformio.ini`.

Program as follows:

```
cd gateway
platformio run --target upload -e wire
```

Once programmed, you can also update it OTA--make sure to adjust its IP address in `platformio.ini` in section `[env:ota]` accordingly:

```
platformio run --target upload -e ota
```

Once deployed, you can configure the gateway. It will create its own Wifi AP, connect to it to make it join your actual WiFi network. Then configure where to send the MQTT messages (see below). You also want to add MQTT mappings, to translate the raw node data to proper MQTT paths. For example, I am transmitting values of key `T` from node `2` to the MQTT path `/home/garage/temperature`, while values of key `T` from node `3` go to `/home/attic/temperature`. This path is relevant for the MQTT -> InfluxDB script discussed below.

To debug, listen to the console when connected:

```
platformio device monitor --baud 115200 --port /dev/cu.usbserial
```

### EEPROM

If you change settings (like the admin password), they will be stored in the device's flash memory. You can inspect it through a telnet connection:

```
telnet 192.168.1.12
```

Then use the embedis commands:

```
keys
```

```
get adminPass
``` 

## MQTT broker

The MQTT broker runs anywhere on your network, reachable by the gateway. I am using a Raspberry Pi Zero.

Install the following:

```
sudo apt-get install mosquitto
sudo apt-get install mosquitto-clients
```

This will start a default MQTT broker, which is good enough for now.

## InfluxDB

I am storing the sensor data in InfluxDB. Go to http://ftp.us.debian.org/debian/pool/main/i/influxdb/ and find the latest deb package. Then pick the right one for your architecture--in my case (RPi), I need ARM:

```
sudo dpkg -i influxdb_....deb
sudo dpkg -i influxdb-client_1.1.1+dfsg1-4+b1_armhf.deb
```

Use the Influx CLI: `influx` to create a database:

```
create database logger
```


## Receiver Script

We need a service that subscribes to our sensor queues in MQTT and writes to InfluxDB. For simplicity we use Python. First install the influxdb module:

```
sudo pip install influxdb
```

I wanted the mqtt -> influxdb script to run as a daemon to make it more robust. I am using [ansible](https://www.ansible.com/) for the daemon deployment. The nice thing about ansible is that you can control the deployment and launch of the daemon entirely from your client, without having to install any management software on the target node. It should take care of setting up the daemon from scratch, including reloading the daemon (which normally is done through `sudo /bin/systemctl daemon-reload` to register it as a service). Then you can run `sudo service mqtt_influxdb status` and such.

Install ansible (a python package), and edit `mqtt/ansible/hosts` to point at your server. Then:

```
cd mqtt/ansible
ansible-playbook site.yaml -i hosts -u <username>
```

Note that `<username>` must be able to become root on the server, i.e., be member of sudoers.

The script itself just subscribes to all paths `home/#` and writes them to the DB, using the following convention:
```
home/<sensor>/<metric>
```

For example:
```
home/attic/temperature
```

The _sensor_ is a column (i.e., tag) in InfluxDB, the _metric_ is a measurement.

## Grafana

Final piece in this framework is the visualization piece. Grafana plays nicely with InfluxDB, looks pretty, and has a built-in web server. I needed a specific ARMv6 (RPi Zero is like Rpi 1) package (see https://github.com/fg2it/grafana-on-raspberry/wiki):

```
sudo apt-get install adduser libfontconfig
curl -L https://dl.bintray.com/fg2it/deb-rpi-1b/main/g/grafana_4.2.0_armhf.deb -o grafana_4.2.0_armhf.deb
dpkg -i grafana_4.2.0_armhf.deb
```

Then:

```
sudo /bin/systemctl daemon-reload
sudo /bin/systemctl enable grafana-server
sudo /bin/systemctl start grafana-server
```

Go to http://<your server address>:3000 and log in with `admin`, `admin`. Now we can build charts on top of InfluxDB measurements!


## Weather Data

I wanted to include outside weather data without having to measure it myself. We can use Weather Underground for that.

The script `mqtt_wunderground` requests wunderground data through the developer API for a given location, then submits it to MQTT. By using the path `home/outside`, the InfluxDB importer will pick it up and store it. Just like the MQTT -> InfluxDB script, the Wunderground importer runs as a daemon and is deployed through ansible.


# Appendix

### Micropython on the esp8266

1. `pip install esptool`
2. Download upython flash: `wget http://micropython.org/resources/firmware/esp8266-20170108-v1.8.7.bin`
3. Install serial cable driver for OSX: http://www.prolific.com.tw/US/ShowProduct.aspx?p_id=229&pcid=41

#### Flash upython

1. Before connecting to USB: `ll /dev/tty* >ttybefore.txt`
2. Connect to USB
3. Now: `ll /dev/tty* >ttyafter.txt` and compare, to find out the right serial port: `dev/tty.usbserial`
4. Put into flash mode (press and hold GPIO0, press reset briefly, release GPIO0).
5. Clear flash: `esptool.py --port /dev/tty.usbserial erase_flash`
4. Put into flash mode again.
6. Program: `esptool.py --port /dev/tty.usbserial --baud 460800 write_flash --flash_size=detect 0 esp8266-20170108-v1.8.7.bin`
7. Press reset

#### Test REPL

1. Connect: `screen /dev/tty.usbserial 115200`
2. Might need to press ctrl-c
3. Now the prompt `>>>` should show
4. `print("hello world")`

To exit REPL, get out of the screen: `ctrl-a d`

To close the screen connection and free it up: `ctrl-a ctrl-\`

#### Program in Micropython

Follow https://docs.micropython.org/en/latest/esp8266/esp8266/tutorial/intro.html

Using urequests for a nice wrapper around GET and POST.

#### Transferring Files

mpfshell: https://github.com/wendlers/mpfshell

```
mpfshell -n -s deploy.mpf
```
