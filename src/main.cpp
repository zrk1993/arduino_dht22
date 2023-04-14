#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h> 
#include <AsyncElegantOTA.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include <DHT.h>

#include "blink.h"
#include "config.h"

#define DHTPIN 13   // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22 

bool enable_mqtt = false;

unsigned long lastMqttReconnectTime = 0;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

AsyncWebServer server(80);
DNSServer dns;

float humidity = 0;
float fahrenheit = 0;

DHT dht(DHTPIN, DHTTYPE);

void connectWifi() {
  AsyncWiFiManager wifiManager(&server, &dns);
  wifiManager.autoConnect("ESPConfigAP");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

String chipInfo() {
  String str = "";
  str += "&ChipID=";
  str += ESP.getChipId();
  str += "&FlashSize=";
  str += ESP.getFlashChipSize();
  str += "&AvailableHeap=";
  str += ESP.getFreeHeap();
  str += "&StationSSID=" + WiFi.SSID();
  str += "&StationIP=" + WiFi.localIP().toString();
  str += "&StationMAC=" + WiFi.macAddress();
  return str;
}

void callback(char* topic, byte* payload, size_t length) {
	Serial.print("Topic:");
	Serial.println(topic);
	String msg = "";
	for (size_t i = 0; i < length; i++) {
		msg += (char)payload[i];
	}
	Serial.print("Msg:");
	Serial.println(msg);
}

bool checkMqttEnable () {
  enable_mqtt = *config.mqtt_host != '\0' && *config.mqtt_port != '\0' && *config.mqtt_key != '\0' && *config.mqtt_topic != '\0';
  return enable_mqtt;
}

void reconnect() {
  if (lastMqttReconnectTime != 0 && millis() > lastMqttReconnectTime && millis() < lastMqttReconnectTime + 1000 * 60 * 5) {
    return;
  }
  lastMqttReconnectTime = millis();
  int i = 5;
	while (!mqttClient.connected() && i--) {
		Serial.print("Attempting MQTT connection...");
		// Attempt to connect
		if (mqttClient.connect(config.mqtt_key)) {
			Serial.println("connected");
			Serial.print("subscribe:");
			Serial.println(config.mqtt_topic);
			//订阅主题，如果需要订阅多个主题，可发送多条订阅指令client.subscribe(topic2);client.subscribe(topic3);
			mqttClient.subscribe(config.mqtt_topic);
      // 格式应为#21#45#on，即是温度是21度，湿度是45。
      String payload = "#";
      payload += fahrenheit;
      payload += "#";
      payload += humidity;
      payload += "#on";
      mqttClient.publish(config.mqtt_topic, payload.c_str());
		} else {
			Serial.print("failed, rc=");
			Serial.print(mqttClient.state());
			Serial.println(" try again in 1 seconds");
			// Wait 1 seconds before retrying
			delay(1000);
		}
	}
}

void setServer() {
  if (MDNS.begin(config.host_name)) { // Start mDNS with name esp8266
		Serial.println("mDNS started");
		Serial.printf("http://%s.local\n", config.host_name);
	}

  AsyncElegantOTA.begin(&server); // Start ElegantOTA

  // star littlefs
  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  Serial.println("Littlefs is success open");
  AsyncStaticWebHandler* handler = &server.serveStatic("/", LittleFS, "/");
  handler->setDefaultFile("index.html");

  server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request) {
    String payload = "温度：";
    payload += fahrenheit;
    payload += " °C    湿度：";
    payload += humidity;
    payload += "%";
		request->send(200, "text/plain", payload);
	});

  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request) {
    String info = chipInfo();
		request->send(200, "text/plain", info);
	});
  server.on("/loadConfig", HTTP_GET, [](AsyncWebServerRequest *request) {
		String config_txt = getConfigTxt();
		request->send(200, "text/plain", config_txt);
	});
	server.on("/saveConfig", HTTP_GET, [](AsyncWebServerRequest *request) {
		strcpy(config.host_name, request->arg("host_name").c_str());
    strcpy(config.mqtt_host, request->arg("mqtt_host").c_str());
    strcpy(config.mqtt_port, request->arg("mqtt_port").c_str());
		strcpy(config.mqtt_key, request->arg("mqtt_key").c_str());
		strcpy(config.mqtt_topic, request->arg("mqtt_topic").c_str());
		saveConfig();
		request->send(200, "text/plain", "save ok, delay restart ...");
	});

  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", "restart");
		Serial.println("restart 。。。 ");
		ESP.restart();
	});

  server.begin();
  Serial.println("Web server started");
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("\nSerial star Running");
  pinMode(BLINK_PIN, OUTPUT);

  blink_on();
	loadConfig();
	Serial.println("config_txt: " + getConfigTxt());
  connectWifi();
  setServer();

  if (checkMqttEnable()) {
    Serial.println("MQTT Start ");
    mqttClient.setServer(config.mqtt_host, atoi(config.mqtt_port)); // 设置mqtt服务器
    mqttClient.setCallback(callback); // mqtt消息处理
    reconnect();
  }
  dht.begin();
  blink_ok();
  Serial.println("start ok.");
}

void run_dht() {
  // Wait a few seconds between measurements.
  delay(2000);

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = humidity = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = fahrenheit = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    humidity = 0;
    fahrenheit = 0;
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  // 格式应为#21#45#on，即是温度是21度，湿度是45。

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));

  Serial.print(f);
  Serial.print(F("°F  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));
}

void loop() {
  MDNS.update();
  run_dht();
  if (enable_mqtt) {
    if (mqttClient.connected()) {
      mqttClient.loop();
    } else {
      reconnect();
    }
    Serial.println("deep sleep for 5 seconds");
    delay(5000);
    // ESP.deepSleep(5e6);
  }
}