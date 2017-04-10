from flask import Flask, render_template, request

app = Flask(__name__)

@app.route('/')
def hello_world():
    return render_template('index.html', body='nothing to see here')

@app.route('/sensor')
def sensor():
    with open('data.txt', 'r') as datafile:
        lineList = datafile.readlines()

    dataline = lineList[-1].split(';')

    import json
    data = json.loads(dataline[1].replace("u\"","\"").replace("u\'","\'").replace("'", '"'))

    temperature = "{0:.1f}".format(data['temperature'])
    humidity = "{0:.1f}".format(data['humidity'])
    return render_template('data.html', t=temperature, h=humidity)

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
            datafile.write('{};{}\n'.format(ts, json.dumps(payload)))

        return ('', 204)
    
    with open('data.txt', 'r') as myfile:
        content = myfile.read().splitlines()
    
    ret = ''
    for line in content:
        ret += '{}<br/>'.format(line)

    return render_template('index.html', body=ret)

if __name__ == '__main__':
    app.run(debug=True)
