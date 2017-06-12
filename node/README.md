# Sensor Node

There needs to be a folder `secrets` under `lib`, which needs to contain a file `secrets.h`. This file needs to define `ENCRYPTKEY`.

## Program

Connect moteino via FTDI header. Then install via platformio:

```
platformio run --target upload
```

To connect via serial:

```
pio device monitor --port /dev/cu.usbserial-DN021JLW --baud 115200
```
