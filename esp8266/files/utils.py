# -*- coding: utf-8 -*-`
# """
# Utilities
# """

import urequests
import machine
import ubinascii

MACHINE_ID = ubinascii.hexlify(machine.unique_id()).decode('UTF-8')

RESET_CAUSES = {
    machine.PWRON_RESET: 'power-on reset',
    machine.HARD_RESET: 'hard reset',
    machine.WDT_RESET: 'watchdog reset',
    machine.DEEPSLEEP_RESET: 'deepsleep reset',
    machine.SOFT_RESET: 'soft reset'
}

def blink(n, d=1):
     import time
     ms = int(d * 1000)
     pin = machine.Pin(2, machine.Pin.OUT)
     for i in range(0, n):
         pin.low()
         time.sleep_ms(ms)
         pin.high()
         time.sleep_ms(ms)


def wifi_connect(essid, pwd):
    import network
    import time
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        print('connecting to network...')
        wlan.connect(essid, pwd)
        tries = 0
        while not wlan.isconnected():
            time.sleep(1)
            tries += 1
            if tries > 5:
                raise Exception
            pass
    ip,_,_,_ = wlan.ifconfig()
    return ip


def send_log(server, logstring):
    full_logstring = '{};{}'.format(MACHINE_ID, logstring)
    urequests.post('{}sensor/log'.format(server),
                   json={'message': full_logstring},
                   headers={'Content-Type': 'application/json'})


def send_data(server, data):
    data['sensor_id'] = MACHINE_ID
    urequests.post('{}sensor/data'.format(server),
                   json=data,
                   headers={'Content-Type': 'application/json'})