# -*- coding: utf-8 -*-`
# """
# Entry point for ESP8266
# """

import os
import time
import machine
import dht
import utils
import secrets
# import urequests

SERVER_URL = 'http://192.168.1.40/'
PERIOD_S = 300
DHT_PIN = 4

def work():
    d = dht.DHT22(machine.Pin(DHT_PIN))
    d.measure()

    # convert to F
    t = 9.0/5.0 * float(d.temperature()) + 32
    h = d.humidity()

    getstring = 'http://io.adafruit.com/api/groups/{}/send.json'.format(utils.MACHINE_ID)
    getstring += '?x-aio-key={}'.format(secrets.AIO_KEY)
    getstring += '&temperature={:.1f}'.format(t)
    getstring += '&humidity={:.1f}'.format(h)

    try:
        utils.uget(getstring)
        # urequests.get(getstring)
    except:
        pass

    # utils.send_data(SERVER_URL,
    #                 {"temperature": d.temperature(), "humidity": d.humidity()})

    utils.blink(1, 0.1)
    return

def main():
    try:
        ip_address = utils.wifi_connect(secrets.WIFI_ESSID, secrets.WIFI_PWD)
        # utils.send_log(SERVER_URL, 'connected at {}'.format(ip_address))
    except Exception as e:
        print(e)
        utils.blink(2)
        time.sleep(60)
        machine.reset()
        return

    # we are good:
    utils.blink(1)
    # utils.send_log(SERVER_URL,
    #                'Last reset cause: {}'.format(utils.RESET_CAUSES[machine.reset_cause()]))
    # utils.send_log(SERVER_URL,
    #                'Starting timer with period {}s'.format(PERIOD_S))
    
    timer = machine.Timer(-1)
    timer.init(period=PERIOD_S * 1000,
               mode=machine.Timer.PERIODIC,
               callback=lambda t: work())

main()