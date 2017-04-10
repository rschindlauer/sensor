# -*- coding: utf-8 -*-`
# """
# Entry point for ESP8266
# """

import os
import machine
import dht
import utils

SERVER_URL = 'http://192.168.1.40/'
PERIOD_S = 60
DHT_PIN = 4

last_changed = 0

def work():
    global last_changed
    current_changed = os.stat('main.py')[8]
    if current_changed > last_changed:
        utils.send_log(SERVER_URL,
                       'main.py boot timestamp {}, now modified at {}, '
                       'rebooting'.format(last_changed, current_changed))
        machine.reset()

    d = dht.DHT22(machine.Pin(DHT_PIN))
    d.measure()
    utils.send_data(SERVER_URL,
                    {"temperature": d.temperature(), "humidity": d.humidity()})

    utils.blink(1, 0.2)
    return

def main():
    global last_changed
    last_changed = os.stat('main.py')[8]
    try:
        ip_address = utils.wifi_connect(secrets.WIFI_ESSID, secrets.WIFI_PWD)
        utils.send_log(SERVER_URL, 'connected at {}'.format(ip_address))
    except:
        utils.blink(2)
        return

    # we are good:
    utils.blink(1)
    utils.send_log(SERVER_URL,
                   'Last reset cause: {}'.format(utils.RESET_CAUSES[machine.reset_cause()]))
    utils.send_log(SERVER_URL,
                   'Current main.py timestamp: {}'.format(last_changed))
    
    timer = machine.Timer(-1)
    timer.init(period=PERIOD_S * 1000,
               mode=machine.Timer.PERIODIC,
               callback=lambda t: work())

main()