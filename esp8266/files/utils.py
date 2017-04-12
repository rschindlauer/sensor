# -*- coding: utf-8 -*-`
# """
# Utilities
# """

import urequests
import machine
import ubinascii

MACHINE_ID = ubinascii.hexlify(machine.unique_id()).decode('UTF-8')

RESET_CAUSES = {
    machine.PWRON_RESET: 'power-on reset',      # 0
    machine.HARD_RESET: 'hard reset',           # 6
    machine.WDT_RESET: 'watchdog reset',        # 1 
    machine.DEEPSLEEP_RESET: 'deepsleep reset', # 5
    machine.SOFT_RESET: 'soft reset',           # 4
    2: 'Fatal Exception'
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


def uget(url):
    import socket
    proto, dummy, host, path = url.split("/", 3)
    if proto == "http:":
        port = 80
    elif proto == "https:":
        import ussl
        port = 443
    else:
        raise ValueError("Unsupported protocol: " + proto)

    ai = socket.getaddrinfo(host, port)
    addr = ai[0][4]
    s = socket.socket()
    s.connect(addr)
    if proto == "https:":
        s = ussl.wrap_socket(s)
    # s.write(b"GET /%s HTTP/1.0\r\n" % path)
    # s.write(b"Host: %s\r\n" % host)
    # s.write(b"\r\n")
    s.send(bytes('GET /%s HTTP/1.0\r\nHost: %s\r\n\r\n' % (path, host), 'utf8'))
    s.close()
    return

    # l = s.readline()
    # protover, status, msg = l.split(None, 2)
    # status = int(status)
    # #print(protover, status, msg)
    # while True:
    #     l = s.readline()
    #     if not l or l == b"\r\n":
    #         break
    #     #print(l)
    #     if l.startswith(b"Transfer-Encoding:"):
    #         if b"chunked" in l:
    #             raise ValueError("Unsupported " + l)
    #     elif l.startswith(b"Location:") and not 200 <= status <= 299:
    #         raise NotImplementedError("Redirects not yet supported")

    # resp = Response(s)
    # resp.status_code = status
    # resp.reason = msg.rstrip()
    # return resp

# def send_log(server, logstring):
#     full_logstring = '{};{}'.format(MACHINE_ID, logstring)
#     try:
#         urequests.post('{}sensor/log'.format(server),
#                     json={'message': full_logstring},
#                     headers={'Content-Type': 'application/json'})
#     except:
#         pass


# def send_data(server, data):
    # data['sensor_id'] = MACHINE_ID
    # try:
    #     urequests.post('{}sensor/data'.format(server),
    #                 json=data,
    #                 headers={'Content-Type': 'application/json'})
    # except:
    #     pass