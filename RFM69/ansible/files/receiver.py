#!/usr/bin/env python2

from RFM69 import RFM69 as rfm
from RFM69.RFM69registers import *
import datetime
import time
import Adafruit_IO as ada
import secrets
import requests
import logging
import logging.config
import json

# TODO make debug handler a cmd line arg

logging.config.dictConfig({
    'version': 1,
    'formatters': {
        'standard': {
            'format': '%(asctime)s [%(levelname)s] %(name)s: %(message)s'
        },
    },
    'handlers': {
        'default': {
            'class': 'logging.StreamHandler',
            'formatter': 'standard'
        },
        'file': {
            'class': 'logging.handlers.RotatingFileHandler',
            'formatter': 'standard',
            'filename': '/var/log/sensord.log',
            'backupCount': 3
        },
    },
    'loggers': {
        '': {
            'handlers': ['file'],
            'level': 'DEBUG',
            'propagate': True
        }
    }
})

logger = logging.getLogger()

ENCRYPT_KEY = "sampleEncryptKey"
MY_NODE_ID = 1
NETWORK_ID = 100

radio = rfm.RFM69(RF69_915MHZ, MY_NODE_ID, NETWORK_ID, True)
logger.info("class initialized")
logger.info("reading all registers")
results = radio.readAllRegs()
for result in results:
    logger.info(result)
logger.info("Performing rcCalibration")
radio.rcCalibration()
#logger.info("setting high power")
#test.setHighPower(True)
logger.info("Checking temperature")
logger.info(radio.readTemperature(0))
logger.info("setting encryption")

# TODO turn off encryption
radio.encrypt(ENCRYPT_KEY)

import struct
s = struct.Struct('<ff')

PERIOD = 8

while True:
    # with open('config.json') as config_file:
    #     config = json.load(config_file)

    # new_period = config['period']
    # if new_period != PERIOD:
    #     logger.info('Changing period to %s', new_period)
    #     if radio.sendWithRetry(2, str(new_period), 3, PERIOD):
    #         logger.info('Changing period: ack received')
    #     else:
    #         logger.info('Changing period: giving up')
    #     PERIOD = new_period

    try:
        radio.receiveBegin()
        while not radio.receiveDone():
            time.sleep(.1)

        data = radio.DATA
        id = radio.SENDERID
        rssi = radio.RSSI

        if len(radio.DATA) == 0:
            logger.info('radio DATA len: %s', len(radio.DATA))
            continue

        if radio.ACKRequested():
            radio.sendACK()

        t, h = s.unpack_from(buffer(bytearray(radio.DATA)))

        logger.debug("t={:.2f}, h={:.2f} from node {}, RSSI: {}".format(t, h, id, rssi))

        # convert to F:
        t = 9.0/5.0 * t + 32
        # aio.send('temperature_rfm', '{0:.1f}'.format(t))
        # aio.send('humidity_rfm', '{0:.1f}'.format(h))

        getstring = 'http://io.adafruit.com/api/groups/node{}/send.json'.format(id)
        getstring += '?x-aio-key={}'.format(secrets.AIO_KEY)
        getstring += '&temperature={:.1f}'.format(t)
        getstring += '&humidity={:.1f}'.format(h)
        getstring += '&rssi={:.1f}'.format(rssi)
        logger.debug(getstring)
        r = requests.get(getstring)
        logger.debug('Adafruit IO request status code: {}'.format(r.status_code))

    except:
        import sys
        e = sys.exc_info()[0]
        logger.debug(str(e))

logger.info("shutting down")
radio.shutdown()
