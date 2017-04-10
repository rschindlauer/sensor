# Sensor

## Install Ansible on Pi Zero

1. follow http://docs.ansible.com/ansible/intro_installation.html#latest-releases-via-apt-debian

Wait, that's not actually necessary. Ansible is only needed on the client. Installing on the Mac.

## Deploy on PiZero

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