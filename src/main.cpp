#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>
#include <driver/i2s.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <ESPmDNS.h> // Pour le mDNS

// Nombre maximum de LEDs autorisées
#define MAX_LEDS 144
#define LED_PIN 4

// Tableau de LEDs, gestion des couleurs (capacité max)
CRGB leds[MAX_LEDS];
CRGB baseColor = CRGB::White;

// Paramètres internes
float maxVolume = 500;        // Interne : 200 à 3000
float smoothingFactor = 0.05; // Interne : 1.0 à 0.01
float smoothedVolume = 0;
int ledCount = 10;            // Nombre de LEDs

// Objet Preferences
Preferences preferences;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<title>Réglages de la Lampe</title>
<style>
  body {
    margin: 0;
    padding: 20px;
    background-color: #FFFFCC;
    font-family: "Instrument Sans", sans-serif;
    color: #333;
    font-weight: bold;
  }

  h1 {
    font-size: 48px;
    color: #00BFFF;
    margin: 0;
    padding: 0;
    font-weight: bold;
  }

  .container {
    width: 80%;
    margin: 0 auto;
    text-align: left;
  }

  .label {
    font-size: 20px;
    font-weight: bold;
    color: #000;
    margin-top: 30px;
    margin-bottom: 10px;
  }

  input[type="range"] {
    width: 100%;
    -webkit-appearance: none; 
    background: #ccc; 
    height: 10px;
    border-radius: 5px;
    outline: none;
    margin-bottom: 10px;
  }

  input[type="range"]::-webkit-slider-thumb {
    -webkit-appearance: none; 
    width: 20px;
    height: 20px;
    background: #00BFFF;
    border-radius: 50%;
    cursor: pointer;
    border: 2px solid #333;
  }

  input[type="color"] {
    width: 100%;
    height: 40px;
    border: none;
    padding: 0;
    background: #ccc;
    cursor: pointer;
    margin-bottom: 20px;
  }

  .footer-image {
    display: block;
    margin: 40px auto 0;
    width: 80%;
  }
</style>
</head>
<body>
<div class="container">
  <h1>DenisDenis</h1>
  <form id="controlForm" action="/set" method="GET">

    <div class="label">Nombre de LEDs</div>
    <input type="range" name="ledCount" min="0" max="144" value="%LED_COUNT%" oninput="this.nextElementSibling.value = this.value" onchange="updateValues()">
    <output>%LED_COUNT%</output>

    <div class="label">sensibility</div>
    <!-- Slider 0-100, interne : maxVolume 3000 à 200 -->
    <input type="range" name="sensibility" min="0" max="100" value="%SENSIBILITY%" oninput="this.nextElementSibling.value = this.value" onchange="updateValues()">
    <output>%SENSIBILITY%</output>

    <div class="label">smoothness</div>
    <!-- Slider 0-100, interne : smoothingFactor 1.0 à 0.01 -->
    <input type="range" name="smoothness" min="0" max="100" value="%SMOOTHNESS_SLIDER%" oninput="this.nextElementSibling.value = this.value" onchange="updateValues()">
    <output>%SMOOTHNESS_SLIDER%</output>

    <div class="label">color picker</div>
    <input type="color" name="baseColor" value="#ffffff" oninput="updateValues()">

  </form>

  <img src="https://denisdenis.gaetanstreel.com/denis.png" alt="Rock Hand" class="footer-image">
</div>

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

// Serveur Web
WebServer server(80);

String htmlProcessor(const char* html) {
  String page = String(html);

  // maxVolume -> sensibility (0-100)
  float constrainedMaxVolume = maxVolume;
  if (constrainedMaxVolume > 3000) constrainedMaxVolume = 3000;
  if (constrainedMaxVolume < 200) constrainedMaxVolume = 200;
  
  int sensibilitySlider = (int)((3000 - constrainedMaxVolume) / 28.0);
  if (sensibilitySlider < 0) sensibilitySlider = 0;
  if (sensibilitySlider > 100) sensibilitySlider = 100;

  // smoothingFactor -> smoothnessSlider (0-100)
  int smoothnessSlider = (int)((1.0 - smoothingFactor) / 0.0099);
  if (smoothnessSlider < 0) smoothnessSlider = 0;
  if (smoothnessSlider > 100) smoothnessSlider = 100;

  page.replace("%SENSIBILITY%", String(sensibilitySlider));
  page.replace("%SMOOTHNESS_SLIDER%", String(smoothnessSlider));
  page.replace("%LED_COUNT%", String(ledCount));

  return page;
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](){
    String page = htmlProcessor(index_html);
    server.send(200, "text/html; charset=utf-8", page);
  });

  server.on("/set", HTTP_GET, [](){
    bool changed = false;

    // sensibility -> maxVolume
    if (server.hasArg("sensibility")) {
      int sliderValue = server.arg("sensibility").toInt();
      float newMaxVolume = 3000 - 28.0 * sliderValue;
      maxVolume = newMaxVolume;
      preferences.putFloat("maxVolume", maxVolume);
    }

    // smoothness -> smoothingFactor
    if (server.hasArg("smoothness")) {
      int sliderValue = server.arg("smoothness").toInt();
      float newSmoothingFactor = 1.0 - (0.0099 * sliderValue);
      smoothingFactor = newSmoothingFactor;
      preferences.putFloat("smoothingFactor", smoothingFactor);
    }

    if (server.hasArg("baseColor")) {
      String color = server.arg("baseColor");
      long rgb = strtol(color.substring(1).c_str(), NULL, 16);
      baseColor = CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
      preferences.putUChar("baseColorR", baseColor.r);
      preferences.putUChar("baseColorG", baseColor.g);
      preferences.putUChar("baseColorB", baseColor.b);
      changed = true;
    }

    if (server.hasArg("ledCount")) {
      ledCount = server.arg("ledCount").toInt();
      if (ledCount < 0) ledCount = 0;
      if (ledCount > MAX_LEDS) ledCount = MAX_LEDS;
      preferences.putUInt("ledCount", (unsigned int)ledCount);

      fill_solid(leds, MAX_LEDS, CRGB::Black);
      fill_solid(leds, ledCount, baseColor);
      FastLED.show();
    }

    if (changed) {
      fill_solid(leds, ledCount, baseColor);
      FastLED.show();
    }

    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });

  server.begin();
}

void setupI2SMic() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
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
  for (int i = 0; i < (int)(bytesRead / 2); i++) {
    sum += abs(samples[i]);
  }

  int volume = 0;
  if ((bytesRead / 2) > 0) {
    volume = sum / (bytesRead / 2);
  }

  return volume;
}

uint8_t lerp(uint8_t start, uint8_t end, float t) {
  return start + (end - start) * t;
}

CRGB getColorFromVolume(int volume) {
  if (ledCount == 0) {
    return CRGB::Black;
  }

  float t;
  uint8_t r, g, b;

  if (volume <= maxVolume / 3) {
    t = (float)volume / (maxVolume / 3);
    r = lerp(baseColor.r, 0, t);
    g = lerp(baseColor.g, 255, t);
    b = lerp(baseColor.b, 0, t);
  } else if (volume <= 2 * maxVolume / 3) {
    t = (float)(volume - maxVolume / 3) / (maxVolume / 3);
    r = lerp(0, 255, t);
    g = lerp(255, 255, t);
    b = lerp(0, 0, t);
  } else if (volume <= maxVolume) {
    t = (float)(volume - 2 * maxVolume / 3) / (maxVolume / 3);
    r = lerp(255, 255, t);
    g = lerp(255, 0, t);
    b = lerp(0, 0, t);
  } else {
    r = 255;
    g = 0;
    b = 0;
  }

  return CRGB(r, g, b);
}

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, MAX_LEDS);
  FastLED.clear();
  FastLED.show();

  preferences.begin("config", false);

  maxVolume = preferences.getFloat("maxVolume", 500);
  smoothingFactor = preferences.getFloat("smoothingFactor", 0.05);
  uint8_t r = preferences.getUChar("baseColorR", 255);
  uint8_t g = preferences.getUChar("baseColorG", 255);
  uint8_t b = preferences.getUChar("baseColorB", 255);
  baseColor = CRGB(r, g, b);

  ledCount = preferences.getUInt("ledCount", 10);
  if (ledCount > MAX_LEDS) ledCount = MAX_LEDS;

  fill_solid(leds, MAX_LEDS, CRGB::Black);
  fill_solid(leds, ledCount, baseColor);
  FastLED.show();

  // Configuration WiFi via WiFiManager
  WiFiManager wm;
  wm.autoConnect("AfricanChild", "fuckthatshit");
  Serial.print("Connecté au WiFi, adresse IP: ");
  Serial.println(WiFi.localIP());

  // Initialisation mDNS avec le nom "fuckthatshit"
  if (MDNS.begin("fuckthatshit")) {
    Serial.println("mDNS responder started. Accédez à la page sur http://fuckthatshit.local/");
  }

  setupWebServer();
  setupI2SMic();
}

void loop() {
  server.handleClient();

  int rawVolume = getVolume();
  smoothedVolume = (smoothingFactor * rawVolume) + ((1.0 - smoothingFactor) * smoothedVolume);

  Serial.print("Volume lissé: ");
  Serial.println(smoothedVolume);

  CRGB color = getColorFromVolume(smoothedVolume);
  fill_solid(leds, MAX_LEDS, CRGB::Black);
  fill_solid(leds, ledCount, color);
  FastLED.show();
  delay(50);
}
