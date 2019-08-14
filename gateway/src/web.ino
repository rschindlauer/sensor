/*

WEBSERVER MODULE

Copyright (C) 2016-2017 by Xose Pérez <xose dot perez at gmail dot com>

*/

#if WEB_SUPPORT

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Hash.h>
#include <FS.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <vector>
#include "web.h"

#if WEB_EMBEDDED
#include "static/index.html.gz.h"
#endif // WEB_EMBEDDED

#if ASYNC_TCP_SSL_ENABLED & WEB_SSL_ENABLED
#include "static/server.cer.h"
#include "static/server.key.h"
#endif // ASYNC_TCP_SSL_ENABLED & WEB_SSL_ENABLED

// -----------------------------------------------------------------------------

AsyncWebServer * _server;
char _last_modified[50];
Ticker _web_defer;

// -----------------------------------------------------------------------------

AsyncWebSocket _ws("/ws");
typedef struct {
    IPAddress ip;
    unsigned long timestamp = 0;
} ws_ticket_t;
ws_ticket_t _ticket[WS_BUFFER_SIZE];

// -----------------------------------------------------------------------------
// WEBSOCKETS
// -----------------------------------------------------------------------------

void _wsMQTTCallback(unsigned int type, const char * topic, const char * payload) {

    if (type == MQTT_CONNECT_EVENT) {
        wsSend_P(PSTR("{\"mqttStatus\": true}"));
    }

    if (type == MQTT_DISCONNECT_EVENT) {
        wsSend_P(PSTR("{\"mqttStatus\": false}"));
    }

}

void _wsParse(AsyncWebSocketClient *client, uint8_t * payload, size_t length) {

    // Get client ID
    uint32_t client_id = client->id();

    // Parse JSON input
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject((char *) payload);
    if (!root.success()) {
        DEBUG_MSG_P(PSTR("[WEBSOCKET] Error parsing data\n"));
        wsSend_P(client_id, PSTR("{\"message\": 3}"));
        return;
    }

    // Check actions
    if (root.containsKey("action")) {

        String action = root["action"];

        DEBUG_MSG_P(PSTR("[WEBSOCKET] Requested action: %s\n"), action.c_str());

        if (action.equals("reset")) {
            customReset(CUSTOM_RESET_WEB);
            ESP.restart();
        }

        if (action.equals("restore") && root.containsKey("data")) {

            JsonObject& data = root["data"];
            if (!data.containsKey("app") || (data["app"] != APP_NAME)) {
                wsSend_P(client_id, PSTR("{\"message\": 4}"));
                return;
            }

            for (unsigned int i = EEPROM_DATA_END; i < SPI_FLASH_SEC_SIZE; i++) {
                EEPROM.write(i, 0xFF);
            }

            for (auto element : data) {
                if (strcmp(element.key, "app") == 0) continue;
                if (strcmp(element.key, "version") == 0) continue;
                setSetting(element.key, element.value.as<char*>());
            }

            saveSettings();

            wsSend_P(client_id, PSTR("{\"message\": 5}"));

        }

        if (action.equals("reconnect")) {

            // Let the HTTP request return and disconnect after 100ms
            _web_defer.once_ms(100, wifiDisconnect);

        }

        if (action.equals("clear-counts")) clearCounts();

    };

    // Check config
    if (root.containsKey("config") && root["config"].is<JsonArray&>()) {

        JsonArray& config = root["config"];
        DEBUG_MSG_P(PSTR("[WEBSOCKET] Parsing configuration data\n"));

        unsigned char webMode = WEB_MODE_NORMAL;

        bool save = false;
        bool changed = false;
        bool changedMQTT = false;
        bool changedNTP = false;

        unsigned int network = 0;
        unsigned int mappingCount = getSetting("mappingCount", "0").toInt();
        unsigned int mapping = 0;
        String adminPass;

        for (unsigned int i=0; i<config.size(); i++) {

            String key = config[i]["name"];
            String value = config[i]["value"];

            // Skip firmware filename
            if (key.equals("filename")) continue;

            // Web portions
            if (key == "webPort") {
                if ((value.toInt() == 0) || (value.toInt() == 80)) {
                    save = changed = true;
                    delSetting(key);
                    continue;
                }
            }

            if (key == "webMode") {
                webMode = value.toInt();
                continue;
            }

            // Check password
            if (key == "adminPass1") {
                adminPass = value;
                continue;
            }
            if (key == "adminPass2") {
                if (!value.equals(adminPass)) {
                    wsSend_P(client_id, PSTR("{\"message\": 7}"));
                    return;
                }
                if (value.length() == 0) continue;
                wsSend_P(client_id, PSTR("{\"action\": \"reload\"}"));
                key = String("adminPass");
            }

            if (key == "ssid") {
                key = key + String(network);
            }
            if (key == "pass") {
                key = key + String(network);
            }
            if (key == "ip") {
                key = key + String(network);
            }
            if (key == "gw") {
                key = key + String(network);
            }
            if (key == "mask") {
                key = key + String(network);
            }
            if (key == "dns") {
                key = key + String(network);
                ++network;
            }

            if (key == "nodeid") {
                if (value == "") break;
                key = key + String(mapping);
            }
            if (key == "key") {
                key = key + String(mapping);
            }
            if (key == "topic") {
                key = key + String(mapping);
                ++mapping;
            }

            if (value != getSetting(key)) {
                //DEBUG_MSG_P(PSTR("[WEBSOCKET] Storing %s = %s\n", key.c_str(), value.c_str()));
                setSetting(key, value);
                save = changed = true;
                if (key.startsWith("mqtt")) changedMQTT = true;
                #if NTP_SUPPORT
                    if (key.startsWith("ntp")) changedNTP = true;
                #endif
            }

        }

        if (webMode == WEB_MODE_NORMAL) {

		    // delete remaining mapping
		    for (unsigned int i=mapping; i<mappingCount; i++) {
		        delSetting("nodeid" + String(i));
		        delSetting("key" + String(i));
		        delSetting("topic" + String(i));
                save = changed = true;
		    }

		    String value = String(mapping);
		    setSetting("mappingCount", value);

            // Clean wifi networks
            int i = 0;
            while (i < network) {
                if (getSetting("ssid" + String(i)).length() == 0) {
                    delSetting("ssid" + String(i));
                    break;
                }
                if (getSetting("pass" + String(i)).length() == 0) delSetting("pass" + String(i));
                if (getSetting("ip" + String(i)).length() == 0) delSetting("ip" + String(i));
                if (getSetting("gw" + String(i)).length() == 0) delSetting("gw" + String(i));
                if (getSetting("mask" + String(i)).length() == 0) delSetting("mask" + String(i));
                if (getSetting("dns" + String(i)).length() == 0) delSetting("dns" + String(i));
                ++i;
            }
            while (i < WIFI_MAX_NETWORKS) {
                if (getSetting("ssid" + String(i)).length() > 0) {
                    save = changed = true;
                }
                delSetting("ssid" + String(i));
                delSetting("pass" + String(i));
                delSetting("ip" + String(i));
                delSetting("gw" + String(i));
                delSetting("mask" + String(i));
                delSetting("dns" + String(i));
                ++i;
            }

        }

        // Save settings
        if (save) {

            saveSettings();
            wifiConfigure();
            otaConfigure();
            if (changedMQTT) {
                mqttConfigure();
                mqttDisconnect();
            }
            #if NOFUSS_SUPPORT
                nofussConfigure();
            #endif
            #if NTP_SUPPORT
                if (changedNTP) ntpConfigure();
            #endif

        }

        if (changed) {
            wsSend_P(client_id, PSTR("{\"message\": 8}"));
        } else {
            wsSend_P(client_id, PSTR("{\"message\": 9}"));
        }

    }

}

void _wsStart(uint32_t client_id) {

    char chipid[7];
    snprintf_P(chipid, sizeof(chipid), PSTR("%06X"), ESP.getChipId());

    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();

    bool changePassword = false;
    #if WEB_FORCE_PASS_CHANGE
        String adminPass = getSetting("adminPass", ADMIN_PASS);
        if (adminPass.equals(ADMIN_PASS)) changePassword = true;
    #endif

    if (changePassword) {

        root["webMode"] = WEB_MODE_PASSWORD;

    } else {

        root["webMode"] = WEB_MODE_NORMAL;

        root["app_name"] = APP_NAME;
        root["app_version"] = APP_VERSION;
        root["app_build"] = buildTime();
        root["manufacturer"] = MANUFACTURER;
        root["chipid"] = chipid;
        root["mac"] = WiFi.macAddress();
        root["device"] = DEVICE;
        root["hostname"] = getSetting("hostname");
        root["network"] = getNetwork();
        root["deviceip"] = getIP();
        root["time"] = ntpDateTime();
        root["uptime"] = getUptime();
        root["heap"] = ESP.getFreeHeap();
        root["sketch_size"] = ESP.getSketchSize();
        root["free_size"] = ESP.getFreeSketchSpace();

        #if NTP_SUPPORT
            root["ntpVisible"] = 1;
            root["ntpStatus"] = ntpConnected();
            root["ntpServer1"] = getSetting("ntpServer1", NTP_SERVER);
            root["ntpServer2"] = getSetting("ntpServer2");
            root["ntpServer3"] = getSetting("ntpServer3");
            root["ntpOffset"] = getSetting("ntpOffset", NTP_TIME_OFFSET).toInt();
            root["ntpDST"] = getSetting("ntpDST", NTP_DAY_LIGHT).toInt() == 1;
        #endif

        root["mqttStatus"] = mqttConnected();
        root["mqttEnabled"] = mqttEnabled();
        root["mqttServer"] = getSetting("mqttServer", MQTT_SERVER);
        root["mqttPort"] = getSetting("mqttPort", MQTT_PORT);
        root["mqttUser"] = getSetting("mqttUser");
        root["mqttPassword"] = getSetting("mqttPassword");
        #if ASYNC_TCP_SSL_ENABLED
            root["mqttsslVisible"] = 1;
            root["mqttUseSSL"] = getSetting("mqttUseSSL", 0).toInt() == 1;
            root["mqttFP"] = getSetting("mqttFP");
        #endif
        root["mqttTopic"] = getSetting("mqttTopic", MQTT_TOPIC);

        root["webPort"] = getSetting("webPort", WEB_PORT).toInt();

	    root["defaultTopic"] = getSetting("defaultTopic", MQTT_TOPIC_DEFAULT);
        root["nodeCount"] = getNodeCount();
        root["packetCount"] = getPacketCount();
		JsonArray& mappings = root.createNestedArray("mapping");
		byte mappingCount = getSetting("mappingCount", "0").toInt();
		for (byte i=0; i<mappingCount; i++) {
		    JsonObject& mapping = mappings.createNestedObject();
		    mapping["nodeid"] = getSetting("nodeid" + String(i));
		    mapping["key"] = getSetting("key" + String(i));
		    mapping["topic"] = getSetting("topic" + String(i));
		}

        #if NOFUSS_SUPPORT
            root["nofussVisible"] = 1;
            root["nofussEnabled"] = getSetting("nofussEnabled", NOFUSS_ENABLED).toInt() == 1;
            root["nofussServer"] = getSetting("nofussServer", NOFUSS_SERVER);
        #endif

        #if TELNET_SUPPORT
            root["telnetVisible"] = 1;
            root["telnetSTA"] = getSetting("telnetSTA", TELNET_STA).toInt() == 1;
        #endif

        root["maxNetworks"] = WIFI_MAX_NETWORKS;
        JsonArray& wifi = root.createNestedArray("wifi");
        for (byte i=0; i<WIFI_MAX_NETWORKS; i++) {
            if (getSetting("ssid" + String(i)).length() == 0) break;
            JsonObject& network = wifi.createNestedObject();
            network["ssid"] = getSetting("ssid" + String(i));
            network["pass"] = getSetting("pass" + String(i));
            network["ip"] = getSetting("ip" + String(i));
            network["gw"] = getSetting("gw" + String(i));
            network["mask"] = getSetting("mask" + String(i));
            network["dns"] = getSetting("dns" + String(i));
        }

    }

    String output;
    root.printTo(output);
    wsSend(client_id, (char *) output.c_str());

}

bool _wsAuth(AsyncWebSocketClient * client) {

    IPAddress ip = client->remoteIP();
    unsigned long now = millis();
    unsigned short index = 0;

    for (index = 0; index < WS_BUFFER_SIZE; index++) {
        if ((_ticket[index].ip == ip) && (now - _ticket[index].timestamp < WS_TIMEOUT)) break;
    }

    if (index == WS_BUFFER_SIZE) {
        DEBUG_MSG_P(PSTR("[WEBSOCKET] Validation check failed\n"));
        wsSend_P(client->id(), PSTR("{\"message\": 10}"));
        return false;
    }

    return true;

}

void _wsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){

    if (type == WS_EVT_CONNECT) {

        // Authorize
        #ifndef NOWSAUTH
            if (!_wsAuth(client)) return;
        #endif

        IPAddress ip = client->remoteIP();
        DEBUG_MSG_P(PSTR("[WEBSOCKET] #%u connected, ip: %d.%d.%d.%d, url: %s\n"), client->id(), ip[0], ip[1], ip[2], ip[3], server->url());
        _wsStart(client->id());
        client->_tempObject = new WebSocketIncommingBuffer(&_wsParse, true);
        wifiReconnectCheck();

    } else if(type == WS_EVT_DISCONNECT) {
        DEBUG_MSG_P(PSTR("[WEBSOCKET] #%u disconnected\n"), client->id());
        if (client->_tempObject) {
            delete (WebSocketIncommingBuffer *) client->_tempObject;
        }
        wifiReconnectCheck();

    } else if(type == WS_EVT_ERROR) {
        DEBUG_MSG_P(PSTR("[WEBSOCKET] #%u error(%u): %s\n"), client->id(), *((uint16_t*)arg), (char*)data);

    } else if(type == WS_EVT_PONG) {
        DEBUG_MSG_P(PSTR("[WEBSOCKET] #%u pong(%u): %s\n"), client->id(), len, len ? (char*) data : "");

    } else if(type == WS_EVT_DATA) {
        WebSocketIncommingBuffer *buffer = (WebSocketIncommingBuffer *)client->_tempObject;
        AwsFrameInfo * info = (AwsFrameInfo*)arg;
        buffer->data_event(client, info, data, len);

    }


}

// -----------------------------------------------------------------------------

bool wsConnected() {
    return (_ws.count() > 0);
}

void wsSend(const char * payload) {
    if (_ws.count() > 0) {
        _ws.textAll(payload);
    }
}

void wsSend_P(PGM_P payload) {
    if (_ws.count() > 0) {
        char buffer[strlen_P(payload)];
        strcpy_P(buffer, payload);
        _ws.textAll(buffer);
    }
}

void wsSend(uint32_t client_id, const char * payload) {
    _ws.text(client_id, payload);
}

void wsSend_P(uint32_t client_id, PGM_P payload) {
    char buffer[strlen_P(payload)];
    strcpy_P(buffer, payload);
    _ws.text(client_id, buffer);
}

void wsSetup() {
    _ws.onEvent(_wsEvent);
    mqttRegister(_wsMQTTCallback);
    _server->addHandler(&_ws);
    _server->on("/auth", HTTP_GET, _onAuth);
}

// -----------------------------------------------------------------------------
// WEBSERVER
// -----------------------------------------------------------------------------

void _webLog(AsyncWebServerRequest *request) {
    DEBUG_MSG_P(PSTR("[WEBSERVER] Request: %s %s\n"), request->methodToString(), request->url().c_str());
}

bool _authenticate(AsyncWebServerRequest *request) {
    String password = getSetting("adminPass", ADMIN_PASS);
    char httpPassword[password.length() + 1];
    password.toCharArray(httpPassword, password.length() + 1);
    return request->authenticate(WEB_USERNAME, httpPassword);
}

void _onAuth(AsyncWebServerRequest *request) {

    _webLog(request);
    if (!_authenticate(request)) return request->requestAuthentication();

    IPAddress ip = request->client()->remoteIP();
    unsigned long now = millis();
    unsigned short index;
    for (index = 0; index < WS_BUFFER_SIZE; index++) {
        if (_ticket[index].ip == ip) break;
        if (_ticket[index].timestamp == 0) break;
        if (now - _ticket[index].timestamp > WS_TIMEOUT) break;
    }
    if (index == WS_BUFFER_SIZE) {
        request->send(429);
    } else {
        _ticket[index].ip = ip;
        _ticket[index].timestamp = now;
        request->send(204);
    }

}

void _onGetConfig(AsyncWebServerRequest *request) {

    _webLog(request);
    if (!_authenticate(request)) return request->requestAuthentication();

    AsyncJsonResponse * response = new AsyncJsonResponse();
    JsonObject& root = response->getRoot();

    root["app"] = APP_NAME;
    root["version"] = APP_VERSION;

    unsigned int size = settingsKeyCount();
    for (unsigned int i=0; i<size; i++) {
        String key = settingsKeyName(i);
        String value = getSetting(key);
        root[key] = value;
    }

    char buffer[100];
    snprintf_P(buffer, sizeof(buffer), PSTR("attachment; filename=\"%s-backup.json\""), (char *) getSetting("hostname").c_str());
    response->addHeader("Content-Disposition", buffer);
    response->setLength();
    request->send(response);

}

#if WEB_EMBEDDED
void _onHome(AsyncWebServerRequest *request) {

    _webLog(request);

    if (request->header("If-Modified-Since").equals(_last_modified)) {

        request->send(304);

    } else {

        #if ASYNC_TCP_SSL_ENABLED

            // Chunked response, we calculate the chunks based on free heap (in multiples of 32)
            // This is necessary when a TLS connection is open since it sucks too much memory
            DEBUG_MSG_P(PSTR("[MAIN] Free heap: %d bytes\n"), ESP.getFreeHeap());
            size_t max = (ESP.getFreeHeap() / 3) & 0xFFE0;

            AsyncWebServerResponse *response = request->beginChunkedResponse("text/html", [max](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {

                // Get the chunk based on the index and maxLen
                size_t len = index_html_gz_len - index;
                if (len > maxLen) len = maxLen;
                if (len > max) len = max;
                if (len > 0) memcpy_P(buffer, index_html_gz + index, len);

                DEBUG_MSG_P(PSTR("[WEB] Sending %d%%%% (max chunk size: %4d)\r"), int(100 * index / index_html_gz_len), max);
                if (len == 0) DEBUG_MSG_P(PSTR("\n"));

                // Return the actual length of the chunk (0 for end of file)
                return len;

            });

        #else

            AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html_gz, index_html_gz_len);

        #endif

        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Last-Modified", _last_modified);
        request->send(response);

    }

}
#endif

#if ASYNC_TCP_SSL_ENABLED & WEB_SSL_ENABLED

int _onCertificate(void * arg, const char *filename, uint8_t **buf) {

#if WEB_EMBEDDED

    if (strcmp(filename, "server.cer") == 0) {
        uint8_t * nbuf = (uint8_t*) malloc(server_cer_len);
        memcpy_P(nbuf, server_cer, server_cer_len);
        *buf = nbuf;
        DEBUG_MSG_P(PSTR("[WEB] SSL File: %s - OK\n"), filename);
        return server_cer_len;
    }

    if (strcmp(filename, "server.key") == 0) {
        uint8_t * nbuf = (uint8_t*) malloc(server_key_len);
        memcpy_P(nbuf, server_key, server_key_len);
        *buf = nbuf;
        DEBUG_MSG_P(PSTR("[WEB] SSL File: %s - OK\n"), filename);
        return server_key_len;
    }

    DEBUG_MSG_P(PSTR("[WEB] SSL File: %s - ERROR\n"), filename);
    *buf = 0;
    return 0;

#else

    File file = SPIFFS.open(filename, "r");
    if (file) {
        size_t size = file.size();
        uint8_t * nbuf = (uint8_t*) malloc(size);
        if (nbuf) {
            size = file.read(nbuf, size);
            file.close();
            *buf = nbuf;
            DEBUG_MSG_P(PSTR("[WEB] SSL File: %s - OK\n"), filename);
            return size;
        }
        file.close();
    }
    DEBUG_MSG_P(PSTR("[WEB] SSL File: %s - ERROR\n"), filename);
    *buf = 0;
    return 0;

#endif

}

#endif

void _onUpgrade(AsyncWebServerRequest *request) {

    char buffer[10];
    if (!Update.hasError()) {
        sprintf_P(buffer, PSTR("OK"));
    } else {
        sprintf_P(buffer, PSTR("ERROR %d"), Update.getError());
    }

    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", buffer);
    response->addHeader("Connection", "close");
    if (!Update.hasError()) {
        _web_defer.once_ms(100, []() {
            customReset(CUSTOM_RESET_UPGRADE);
            ESP.restart();
        });
    }
    request->send(response);

}

void _onUpgradeData(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        DEBUG_MSG_P(PSTR("[UPGRADE] Start: %s\n"), filename.c_str());
        Update.runAsync(true);
        if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
            #ifdef DEBUG_PORT
                Update.printError(DEBUG_PORT);
            #endif
        }
    }
    if (!Update.hasError()) {
        if (Update.write(data, len) != len) {
            #ifdef DEBUG_PORT
                Update.printError(DEBUG_PORT);
            #endif
        }
    }
    if (final) {
        if (Update.end(true)){
            DEBUG_MSG_P(PSTR("[UPGRADE] Success:  %u bytes\n"), index + len);
        } else {
            #ifdef DEBUG_PORT
                Update.printError(DEBUG_PORT);
            #endif
        }
    } else {
        DEBUG_MSG_P(PSTR("[UPGRADE] Progress: %u bytes\r"), index + len);
    }
}

// -----------------------------------------------------------------------------

void webSetup() {

    // Cache the Last-Modifier header value
    snprintf_P(_last_modified, sizeof(_last_modified), PSTR("%s %s GMT"), __DATE__, __TIME__);

    // Create server
    #if ASYNC_TCP_SSL_ENABLED & WEB_SSL_ENABLED
    unsigned int port = 443;
    #else
    unsigned int port = getSetting("webPort", WEB_PORT).toInt();
    #endif
    _server = new AsyncWebServer(port);

    // Setup websocket
    wsSetup();

    // Rewrites
    _server->rewrite("/", "/index.html");

    // Serve home (basic authentication protection)
    #if WEB_EMBEDDED
        _server->on("/index.html", HTTP_GET, _onHome);
    #endif
    _server->on("/config", HTTP_GET, _onGetConfig);
    _server->on("/upgrade", HTTP_POST, _onUpgrade, _onUpgradeData);

    // Serve static files
    #if SPIFFS_SUPPORT
        _server->serveStatic("/", SPIFFS, "/")
            .setLastModified(_last_modified)
            .setFilter([](AsyncWebServerRequest *request) -> bool {
                _webLog(request);
                return true;
            });
    #endif

    // 404
    _server->onNotFound([](AsyncWebServerRequest *request){
        request->send(404);
    });

    // Run server
    #if ASYNC_TCP_SSL_ENABLED & WEB_SSL_ENABLED
    _server->onSslFileRequest(_onCertificate, NULL);
    _server->beginSecure("server.cer", "server.key", NULL);
    #else
    _server->begin();
    #endif
    DEBUG_MSG_P(PSTR("[WEBSERVER] Webserver running on port %d\n"), port);

}

#endif // WEB_SUPPORT
