# Sensor

## Install Ansible

Part of `requirements.txt`.

## Deploy on PiZero

```
ansible-playbook site.yaml -i hosts
```

## Flask App

TBD: ansible flask app role

### Debug Locally

Use VSCode launch config to debug (needs path to flask executable in the Python env).

Local curl tests:
```
echo '{"message": "Hello **world**!"}' | curl -d @- -H 'Content-Type: application/json' http://127.0.0.1:5000/sensor/log
```

## ESP8266

### Preliminaries:

1. `pip install esptool`
2. Download upython flash: `wget http://micropython.org/resources/firmware/esp8266-20170108-v1.8.7.bin`
3. Install serial cable driver for OSX: http://www.prolific.com.tw/US/ShowProduct.aspx?p_id=229&pcid=41

### Flash upython

1. Before connecting to USB: `ll /dev/tty* >ttybefore.txt`
2. Connect to USB
3. Now: `ll /dev/tty* >ttyafter.txt` and compare, to find out the right serial port: `dev/tty.usbserial`
4. Put into flash mode (press and hold GPIO0, press reset briefly, release GPIO0).
5. Clear flash: `esptool.py --port /dev/tty.usbserial erase_flash`
4. Put into flash mode again.
6. Program: `esptool.py --port /dev/tty.usbserial --baud 460800 write_flash --flash_size=detect 0 esp8266-20170108-v1.8.7.bin`
7. Press reset

### Test REPL

1. Connect: `screen /dev/tty.usbserial 115200`
2. Might need to press ctrl-c
3. Now the prompt `>>>` should show
4. `print("hello world")`

To exit REPL, get out of the screen: `ctrl-a d`

To close the screen connection and free it up: `ctrl-a ctrl-\`

### Program in Micropython

Follow https://docs.micropython.org/en/latest/esp8266/esp8266/tutorial/intro.html

Using urequests for a nice wrapper around GET and POST.

### Transferring Files

mpfshell: https://github.com/wendlers/mpfshell

```
mpfshell -n -s deploy.mpf
```

## RFM69

Lower power consumption that Wifi!

### Sensor node

- Uses a Moteino, programmed with a FTDI USB connection.
- Install Arduino SDK
- In the SDK, install the Moteino board (Add the Moteino core json definition URL to your Board Manager). Follow https://lowpowerlab.com/guide/moteino/programming-libraries/
- connect to USB, select the right port in Tools menu
- Try Node example, should run and produce serial output (set baud rate to 115200 in serial UI)
- add other libraries:
  - https://github.com/LowPowerLab/LowPower
  - https://github.com/adafruit/Adafruit_Sensor
  - https://github.com/adafruit/DHT-sensor-library
  
### Receiver node

- Uses a RFM69 breakout board, connected to a Raspberry Pi Zero
- Wiring according to https://github.com/etrombly/RFM69
- Prerequisites:
  ```
  sudo apt-get install python-spidev
  sudo apt-get install python-rpi.gpio python3-rpi.gpio
  git clone https://github.com/Gadgetoid/py-spidev
  cd py-spidev
  sudo python setup.py install
  sudo raspi-config # enable SPI (also S2C?)
  ```
- modify `RFM69/example`:
  - node, receiver Ids
  - comment out high power mode
  - encryption key
