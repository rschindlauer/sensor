from flask import Flask, render_template, request
import simplejson
import Adafruit_IO as ada
aio = ada.Client('5c35a735953a4da5a44865ef0b338c0d')

class PrettyFloat(float):
    def __repr__(self):
        return '%.1f' % self

def pretty_floats(obj):
    if isinstance(obj, float):
        return PrettyFloat(obj)
    elif isinstance(obj, dict):
        return dict((k, pretty_floats(v)) for k, v in obj.items())
    elif isinstance(obj, (list, tuple)):
        return map(pretty_floats, obj)             
    return obj

app = Flask(__name__)

@app.route('/')
def hello_world():
    return render_template('index.html', body='nothing to see here')

@app.route('/sensor')
def sensor():
    try:
        with open('data.txt', 'r') as datafile:
            lineList = datafile.readlines()
        
        dataline = lineList[-1]
    except IOError:
        dataline = '2017-04-10T20:50:52.403223;{"sensor_id": "fc1c1500", "temperature": 21.8, "humidity": 45.7}'

    dataline = dataline.split(';')

    import json
    data = json.loads(dataline[1])

    import datetime
    updated = datetime.datetime.strptime(dataline[0], "%Y-%m-%dT%H:%M:%S.%f")
    updated = updated.strftime("%Y-%m-%d %H:%M:%S")

    # convert to F
    temperature = 9.0/5.0 * float(temperature) + 32

    temperature = '{0:.1f}'.format(data['temperature'])
    humidity = '{0:.1f}'.format(data['humidity'])
    
    return render_template('data.html', t=temperature, h=humidity, updated=updated)

@app.route('/sensor/log', methods=['GET', 'POST'])
def sensor_log():
    if request.method == 'POST':
        payload = request.get_json()
        logline = payload.get('message', 'Error: invalid log request')
        import datetime
        with open('log.txt', 'a') as logfile:
            ts = datetime.datetime.now().isoformat()
            logfile.write('{};{}\n'.format(ts, logline))

        return ('', 204)
    
    with open('log.txt', 'r') as myfile:
        content = myfile.read().splitlines()
    
    ret = ''
    for line in content:
        ret += '{}<br/>'.format(line)

    return render_template('index.html', body=ret)

@app.route('/sensor/data', methods=['GET', 'POST'])
def sensor_data():
    if request.method == 'POST':
        payload = request.get_json()
        import datetime
        import json
        with open('data.txt', 'a+') as datafile:
            ts = datetime.datetime.now().isoformat()
            datafile.write('{};{}\n'.format(ts, json.dumps(pretty_floats(payload))))

        aio.send('temperature', '{0:.1f}'.format(payload['temperature']))
        aio.send('humidity', '{0:.1f}'.format(payload['humidity']))
        
        return ('', 204)
    
    ret = ''
    
    try:
        with open('data.txt', 'r') as myfile:
            content = myfile.read().splitlines()
        
        for line in content:
            ret += '{}<br/>'.format(line)
    except IOError:
        ret = 'No data available'

    return render_template('index.html', body=ret)

if __name__ == '__main__':
    app.run(debug=True)
