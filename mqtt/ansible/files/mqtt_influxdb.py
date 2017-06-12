#!/usr/bin/env python2

import paho.mqtt.client as mqtt
import datetime
import time
from influxdb import InfluxDBClient
import logging

MQTT_BROKER_IP = "192.168.1.40"
MQTT_BROKER_PORT = 1883
MQTT_SUSBCRIBE_PATH = "home/#"

INFLUXDB_IP = "192.168.1.40"
INFLUXDB_PORT = 8086
INFLUXDB_USER = "root"
INFLUXDB_PASSWORD = "root"
INFLUXDB_DB = "logger"

formatter = logging.Formatter('[%(asctime)s] %(name)s %(levelname)s: %(message)s')
fh = logging.FileHandler('/var/log/mqtt_influxdb.log')
fh.setFormatter(formatter)
log = logging.getLogger()
log.addHandler(fh)
log.setLevel(logging.INFO)

def on_connect(client, userdata, rc):
    log.info("Connected with result code {}".format(rc))
    client.subscribe(MQTT_SUSBCRIBE_PATH)

def on_message(client, userdata, msg):
    # Don't store retained messages:
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

        dbclient.write_points(json_body)

# Set up a client for InfluxDB
dbclient = InfluxDBClient(INFLUXDB_IP, INFLUXDB_PORT, INFLUXDB_USER, INFLUXDB_PASSWORD, INFLUXDB_DB)

# Initialize the MQTT client that should connect to the Mosquitto broker
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
connOK=False
while(connOK == False):
    try:
        client.connect(MQTT_BROKER_IP, MQTT_BROKER_PORT, 60)
        connOK = True
    except:
        connOK = False
    time.sleep(2)

# Blocking loop to the Mosquitto broker
client.loop_forever()