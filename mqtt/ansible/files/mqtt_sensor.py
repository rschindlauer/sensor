#!/usr/bin/env python2

import paho.mqtt.client as mqtt
import datetime
import time
from influxdb import InfluxDBClient
import logging
import yaml
import argparse
import sys
import telepot

MQTT_BROKER_IP = "192.168.1.40"
MQTT_BROKER_PORT = 1883
MQTT_BASE_PATH = "home/#"

CONFIG = {}
CONFIG['INFLUXDB_HOST'] = '192.168.1.40'
CONFIG['INFLUXDB_PORT'] = 8086 
CONFIG['INFLUXDB_USER'] = 'root'
CONFIG['INFLUXDB_PASSWORD'] = 'root'
CONFIG['INFLUXDB_DB'] = 'logger'

telegram_bot = telepot.Bot('414165500:AAHBkyY6hshDQynYe6nTKYIHvSb8iGFvYuc')
telegram_chat_id = -233166988

formatter = logging.Formatter('[%(asctime)s] %(name)s %(levelname)s: %(message)s')
fh = logging.FileHandler('/var/log/mqtt_influxdb.log')
fh.setFormatter(formatter)
log = logging.getLogger()
log.addHandler(fh)
log.setLevel(logging.INFO)

def on_connect(client, userdata, rc):
    log.info("Connected with result code {}".format(rc))
    client.subscribe(MQTT_BASE_PATH)

def on_message(client, userdata, msg):
    """Generic message handler
    """
    # Don't act on retained messages:
    if msg.retain:
        return

    log.info("generic: {} = {}".format(msg.topic,
                                       msg.payload.decode("utf-8")))

def on_message_influxdb(client, userdata, msg):
    # Don't act on retained messages:
    if msg.retain:
        return
        
    message = msg.payload.decode("utf-8")
    topic_path = msg.topic.split('/')
    measurement = topic_path[-1]
    sensor_id = topic_path[-2]

    isfloatValue = False
    try:
        # Convert the string to a float so that it is stored as a number and
        # not a string in the database
        val = float(message)
        isfloatValue = True
    except:
        isfloatValue = False

    if isfloatValue:
        log.info("{}: {} = {}".format(datetime.datetime.now(), msg.topic, val))

        # Let InfluxDB add timestamps
        json_body = [
            {
                "measurement": measurement,
                "tags": {
                    "sensor": sensor_id
                },
                "fields": {
                    "value": val
                }
            }
        ]

        global dbclient
        dbclient.write_points(json_body)


def on_message_telegram(client, userdata, msg):
    # Don't act on retained messages:
    if msg.retain:
        return
    
    # Only alert at specific times
    # if datetime.datetime.now()

    # the date doesn't matter, we are just comparing the time:
    time_start = datetime.datetime(2017, 8, 1, 20, 0)
    time_end = datetime.datetime(2017, 8, 1, 6, 30)

    if datetime.datetime.now().time() > time_start.time():
        return

    if datetime.datetime.now().time() < time_end.time():
        return

    delay = datetime.datetime.now() - on_message_telegram.last_telegram_time

    # ignore new messages within 5min
    if delay.seconds < 300:
        return

    on_message_telegram.last_telegram_time = datetime.datetime.now()

    message = msg.payload.decode("utf-8")
    topic_path = msg.topic.split('/')
    measurement = topic_path[-1]
    sensor_id = topic_path[-2]

    if measurement == 'motion':
        telegram_bot.sendMessage(telegram_chat_id, 'Asha is on the move!')

def main():
    global dbclient
    dbclient = InfluxDBClient(CONFIG['INFLUXDB_HOST'],
                              CONFIG['INFLUXDB_PORT'],
                              CONFIG['INFLUXDB_USER'],
                              CONFIG['INFLUXDB_PASSWORD'],
                              CONFIG['INFLUXDB_DB'])
    

    # Initialize the MQTT client that should connect to the Mosquitto broker
    global client
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    client.message_callback_add('/home/#', on_message_influxdb)
    client.message_callback_add('/home/bedroom_hallway/motion', on_message_telegram)

    # initialize debounce timer:
    on_message_telegram.last_telegram_time = datetime.datetime.now()


    connOK = False

    while connOK == False:
        try:
            client.connect(MQTT_BROKER_IP, MQTT_BROKER_PORT, 60)
            connOK = True
        except:
            connOK = False

        time.sleep(2)

    # Blocking loop to the Mosquitto broker
    client.loop_forever()

if __name__ == '__main__':
    main()
    sys.exit(0)
