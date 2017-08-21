#!/usr/bin/python

import argparse
import paho.mqtt.client as mqtt
from datetime import datetime
import telepot
bot = telepot.Bot('414165500:AAHBkyY6hshDQynYe6nTKYIHvSb8iGFvYuc')
# id = bot.getMe()['id']
CHAT_ID = -233166988

def on_connect(mqttc, userdata, rc):
    print('connected... rc=' + str(rc))

def on_disconnect(mqttc, userdata, rc):
    print('disconnected... rc=' + str(rc))

def on_message(mqttc, userdata, msg):
    print('[{}] {:<30} qos: {}, ret: {}, message: {}'.format(
        datetime.now().strftime('%Y-%m-%d %H:%M:%S.0'),
        str(msg.topic),
        str(msg.qos),
        str(msg.retain),
        str(msg.payload)))
    if msg.topic.split('/')[-1] == 'motion' and msg.retain == 0:
        bot.sendMessage(CHAT_ID, 'Asha is on the move!')

def on_subscribe(mqttc, userdata, mid, granted_qos):
    print('subscribed (qos=' + str(granted_qos) + ')')

def on_unsubscribe(mqttc, userdata, mid, granted_qos):
    print('unsubscribed (qos=' + str(granted_qos) + ')')


def main():
    parser = argparse.ArgumentParser(description='MQTT topic listener', add_help=False)

    parser.add_argument('-h', '--host', dest='host', type=str,
                     help='MQTT host', default='localhost')

    parser.add_argument('-p', '--port', dest='port', type=int,
                     help='MQTT port', default='1883')

    parser.add_argument('topic', type=str,
                     help='Topic to listen to. For example: "/devices/my-device/#"')

    args = parser.parse_args()

    mqttc = mqtt.Client()
    mqttc.on_connect = on_connect
    mqttc.on_disconnect = on_disconnect
    mqttc.on_message = on_message
    mqttc.on_subscribe = on_subscribe
    mqttc.on_unsubscribe = on_unsubscribe

    mqttc.connect(args.host, args.port)
    mqttc.subscribe(topic=args.topic, qos=0)

    mqttc.loop_forever()

if __name__ == '__main__':
    main()