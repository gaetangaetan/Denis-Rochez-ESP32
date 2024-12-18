
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <FastLED.h>
#include <driver/i2s.h>

#define LED_PIN 4
#define LED_COUNT 10

CRGB leds[LED_COUNT];
CRGB baseColor = CRGB::White;

const char* ssid = "mrVOOlpy";
const char* password = "youhououhou";

// Création du serveur web
AsyncWebServer server(80);

// Paramètres configurables
float maxVolume = 500;
float smoothingFactor = 0.05;
float smoothedVolume = 0;

// Page HTML simple
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>Réglages de la Lampe</title></head>
<body>
<h1>Réglages de la Lampe</h1>
<form id="controlForm" action="/set" method="GET">
  <label>Max Volume:</label>
  <input type="range" name="maxVolume" min="0" max="1000" value="%MAX_VOLUME%" oninput="this.nextElementSibling.value = this.value" onchange="updateValues()">
  <output>%MAX_VOLUME%</output><br><br>

  <label>Smoothing Factor:</label>
  <input type="range" name="smoothingFactor" step="0.01" min="0.0" max="1.0" value="%SMOOTHING_FACTOR%" oninput="this.nextElementSibling.value = this.value" onchange="updateValues()">
  <output>%SMOOTHING_FACTOR%</output><br><br>

  <label>Base Color:</label>
  <input type="color" name="baseColor" value="#ffffff" oninput="updateValues()"><br><br>
</form>

<script>
function updateValues() {
  const form = document.getElementById('controlForm');
  const formData = new FormData(form);
  const queryString = new URLSearchParams(formData).toString();
  fetch(`/set?${queryString}`);
}
</script>
</body>
</html>
)rawliteral";

void setupWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connexion au WiFi...");
  }
  Serial.println("Connecté au WiFi !");
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());
}

String processor(const String& var) {
  if (var == "MAX_VOLUME") {
    return String(maxVolume);
  } else if (var == "SMOOTHING_FACTOR") {
    return String(smoothingFactor, 2);  // Précision à deux décimales
  }
  return String();
}

void setupWebServer() {
  // Page principale
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html; charset=utf-8", index_html, processor);
  });

  // Mise à jour des paramètres
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("maxVolume")) {
      maxVolume = request->getParam("maxVolume")->value().toFloat();
    }
    if (request->hasParam("smoothingFactor")) {
      smoothingFactor = request->getParam("smoothingFactor")->value().toFloat();
    }
if (request->hasParam("baseColor")) {
      String color = request->getParam("baseColor")->value();
      long rgb = strtol(color.substring(1).c_str(), NULL, 16);
      baseColor = CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
      fill_solid(leds, LED_COUNT, baseColor);
      FastLED.show();
    }
    request->redirect("/");
  });

  server.begin();
}

void setupI2SMic() {
  i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = 14,
    .ws_io_num = 15,
    .data_out_num = -1,
    .data_in_num = 32
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

int getVolume() {
  int16_t samples[64];
  size_t bytesRead;

  i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, portMAX_DELAY);

  int32_t sum = 0;
  for (int i = 0; i < bytesRead / 2; i++) {
    sum += abs(samples[i]);
  }

  int volume = sum / (bytesRead / 2);
  return volume;
}

uint8_t lerp(uint8_t start, uint8_t end, float t) {
  return start + (end - start) * t;
}


// Calcul de la couleur interpolée
CRGB getColorFromVolume(int volume) {
  float t;  // Facteur d'interpolation (0.0 à 1.0)
  uint8_t r, g, b;

  if (volume <= maxVolume / 3) {
    // Interpolation Blanc -> Vert
    t = (float)volume / (maxVolume / 3);
    r = lerp(baseColor.r, 0, t);
    g = lerp(baseColor.g, 255, t);
    b = lerp(baseColor.b, 0, t);
  } 
  else if (volume <= 2 * maxVolume / 3) {
    // Interpolation Vert -> Jaune
    t = (float)(volume - maxVolume / 3) / (maxVolume / 3);
    r = lerp(0, 255, t);
    g = lerp(255, 255, t);
    b = lerp(0, 0, t);
  } 
  else if (volume <= maxVolume){
    // Interpolation Jaune -> Rouge
    t = (float)(volume - 2 * maxVolume / 3) / (maxVolume / 3);
    r = lerp(255, 255, t);
    g = lerp(255, 0, t);
    b = lerp(0, 0, t);
  }
  else {
  r = 255;
  g = 0;
  b = 0;
  }

  return CRGB(r, g, b);
}

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
  FastLED.clear();
  FastLED.show();
  setupWiFi();
  setupWebServer();
  setupI2SMic();
}

void loop() {
  int rawVolume = getVolume();
  smoothedVolume = (smoothingFactor * rawVolume) + ((1.0 - smoothingFactor) * smoothedVolume);
  Serial.print(" | smoothedVolume : ");
  Serial.println(smoothedVolume);

  CRGB color = getColorFromVolume(smoothedVolume);
  fill_solid(leds, LED_COUNT, color);
  FastLED.show();
  delay(50);
}
