#!/usr/bin/env python2

import urllib2
import json
import paho.mqtt.client as paho
import time
import os
import logging

MQTT_BROKER_IP = "192.168.1.40"
MQTT_BROKER_PORT = 1883
CONFIG_TOPIC = '/home/outside'
CONFIG_WU_API_KEY = '16b941b91e0a8ffc'

# with supervisor we can simply log to stdout, it will take care of the right file
# destination
formatter = logging.Formatter('[%(asctime)s] %(name)s %(levelname)s: %(message)s')
# fh = logging.FileHandler('/var/log/mqtt_wunderground.log')
loghandler = logging.StreamHandler()
loghandler.setFormatter(formatter)
log = logging.getLogger()
log.addHandler(loghandler)
log.setLevel(logging.DEBUG)


# Component config
config = {}
config['deviceid'] = "wunderground"
config['publish_topic'] = CONFIG_TOPIC
config['updaterate'] = 900  # in seconds
config['wu_api_key'] = CONFIG_WU_API_KEY
config['country'] = "WA"
config['city'] = "Seattle"
config['broker_address'] = MQTT_BROKER_IP
config['broker_port'] = MQTT_BROKER_PORT

# Get config topic
config['config_topic'] = CONFIG_TOPIC
if config['config_topic'] is None:
    logger.info("CONFIG_TOPIC is not set, exiting")
    raise SystemExit

# Get Weather Underground API key
config['wu_api_key'] = CONFIG_WU_API_KEY
if config['wu_api_key'] is None:
    logger.info("CONFIG_WU_API_KEY is not set, exiting")
    raise SystemExit


# Create the callbacks for Mosquitto
def on_connect(mqttc, obj, rc):
    if rc == 0:
        logger.info("Connected to broker " + str(config['broker_address'] + ":" + str(config['broker_port'])))


def on_subscribe(mqttc, obj, mid, granted_qos):
    logger.info("Subscribed with message ID " + str(mid) + " and QOS " + str(granted_qos) + " acknowledged by broker")


def on_message(mqttc, obj, msg):
    logger.info("Received message: " + msg.topic + ":" + msg.payload)


def on_publish(mqttc, obj, mid):
    # logger.info("Published message with message ID: "+str(mid))
    pass


def wunderground_get_weather():
    if not config['wu_api_key'] or not config['country'] or not config['city'] or not config['publish_topic']:
        logger.info("Required configuration items not set, skipping the Weather Underground update")
        return

    # Parse the WeatherUnderground json response
    wu_url = "http://api.wunderground.com/api/" + config['wu_api_key'] + \
        "/conditions/q/" + config['country'] + "/" + config['city'] + ".json"
    logger.info("Getting Weather Underground data from " + wu_url)

    try: 
        response = urllib2.urlopen(wu_url)
    except urllib2.URLError as e:
        logger.error('URLError: ' + str(wu_url) + ': ' + str(e.reason))
        return None
    except Exception:
        import traceback
        logger.error('Exception: ' + traceback.format_exc())
        return None

    parsed_json = json.load(response)
    response.close()

    try:
        # times 100, since that's what we are getting from the sensor nodes
        temperature = float(parsed_json['current_observation']['temp_f']) * 100
    except ValueError:
        logger.error('Could not convert temperature reading %s to a float',
            parsed_json['current_observation']['temp_f'])
        temperature = None

    # Strip off the last character of the relative humidity because we want an int
    # but we get a % as return from the weatherunderground API
    humidity = str(parsed_json['current_observation']['relative_humidity'][:-1])

    # TODO fix return value for precip from WU API of t/T for trace of rain to 0
    # or sth like 0.1
    try:
        precipitation = str(int(parsed_json['current_observation']['precip_1hr_metric']))
    except ValueError:
        logger.info("Precipitation returned a wrong value '" + str(parsed_json['current_observation']['precip_1hr_metric'][:-1]) +"', replacing with '0'")
        precipitation = str(0)

    pressure = str(parsed_json['current_observation']['pressure_mb'])
    windspeed = str(parsed_json['current_observation']['wind_kph'])
    winddirection = str(parsed_json['current_observation']['wind_degrees'])

    # Publish the values we parsed from the feed to the broker
    if temperature:
        mqttclient.publish(config['publish_topic'] + "/temperature", temperature, retain=True)

    mqttclient.publish(config['publish_topic'] + "/humidity", humidity, retain=True)
    mqttclient.publish(config['publish_topic'] + "/precipitation", precipitation, retain=True)
    mqttclient.publish(config['publish_topic'] + "/pressure", pressure, retain=True)
    mqttclient.publish(config['publish_topic'] + "/windspeed", windspeed, retain=True)
    mqttclient.publish(config['publish_topic'] + "/winddirection", winddirection, retain=True)

    logger.info("Published " + str(config['deviceid']) + " data to " + str(config['publish_topic']))



mqttclient = paho.Client()

mqttclient.on_connect = on_connect
mqttclient.on_subscribe = on_subscribe
mqttclient.on_message = on_message
mqttclient.on_publish = on_publish

logger.info("Connecting to broker " + config['broker_address'] + ":" + str(config['broker_port']))
mqttclient.connect(config['broker_address'], config['broker_port'], 60)
mqttclient.loop_start()

time.sleep(5)

rc = 0
while rc == 0:
    wunderground_get_weather()
    time.sleep(config['updaterate'])
    pass