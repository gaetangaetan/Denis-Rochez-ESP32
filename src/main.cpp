#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>
#include <driver/i2s.h>
#include <WiFiManager.h>    // WiFiManager pour la configuration WiFi
#include <Preferences.h>    // Pour stocker les paramètres dans la NVS (mémoire flash)

// Définition de la LED et de son nombre
#define LED_PIN 4
#define LED_COUNT 10

// Tableau de LEDs, gestion des couleurs
CRGB leds[LED_COUNT];
CRGB baseColor = CRGB::White;

// Paramètres configurables
float maxVolume = 500;
float smoothingFactor = 0.05;
float smoothedVolume = 0;

// Objet Preferences
Preferences preferences;

// Page HTML (avec placeholders pour les variables)
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
    background-color: #FFFFCC; /* Fond pâle jaune */
    font-family: "Instrument Sans", sans-serif;
    color: #333;
    font-weight: bold; /* Appliquer le gras globalement, ou sur les éléments souhaités */
  }

  h1 {
    font-size: 48px;
    color: #00BFFF;
    margin: 0;
    padding: 0;
    font-weight: bold; /* Assure que le titre soit en gras */
  }

  .container {
    width: 80%;
    margin: 0 auto;
    text-align: left;
  }

  .label {
    font-size: 20px;
    font-weight: bold; /* Les labels sont en gras */
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

    <div class="label">sensibility</div>
    <input type="range" name="maxVolume" min="0" max="1000" value="%MAX_VOLUME%" oninput="this.nextElementSibling.value = this.value" onchange="updateValues()">
    <output>%MAX_VOLUME%</output>

    <div class="label">smoothness</div>
    <input type="range" name="smoothingFactor" step="0.01" min="0.0" max="1.0" value="%SMOOTHING_FACTOR%" oninput="this.nextElementSibling.value = this.value" onchange="updateValues()">
    <output>%SMOOTHING_FACTOR%</output>

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

// Remplacement des placeholders dans le HTML
String htmlProcessor(const char* html) {
  String page = String(html);
  page.replace("%MAX_VOLUME%", String(maxVolume));
  page.replace("%SMOOTHING_FACTOR%", String(smoothingFactor, 2));
  return page;
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](){
    String page = htmlProcessor(index_html);
    server.send(200, "text/html; charset=utf-8", page);
  });

  server.on("/set", HTTP_GET, [](){
    if (server.hasArg("maxVolume")) {
      maxVolume = server.arg("maxVolume").toFloat();
      preferences.putFloat("maxVolume", maxVolume);
    }
    if (server.hasArg("smoothingFactor")) {
      smoothingFactor = server.arg("smoothingFactor").toFloat();
      preferences.putFloat("smoothingFactor", smoothingFactor);
    }
    if (server.hasArg("baseColor")) {
      String color = server.arg("baseColor");
      long rgb = strtol(color.substring(1).c_str(), NULL, 16);
      baseColor = CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
      fill_solid(leds, LED_COUNT, baseColor);
      FastLED.show();
      preferences.putUChar("baseColorR", baseColor.r);
      preferences.putUChar("baseColorG", baseColor.g);
      preferences.putUChar("baseColorB", baseColor.b);
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

// Lecture du volume depuis le microphone I2S
int getVolume() {
  int16_t samples[64];
  size_t bytesRead;

  i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, portMAX_DELAY);

  int32_t sum = 0;
  for (int i = 0; i < (int)(bytesRead / 2); i++) {
    sum += abs(samples[i]);
  }

  int volume = sum / (bytesRead / 2);
  return volume;
}

uint8_t lerp(uint8_t start, uint8_t end, float t) {
  return start + (end - start) * t;
}

// Calcul de la couleur en fonction du volume lissé
CRGB getColorFromVolume(int volume) {
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
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
  FastLED.clear();
  FastLED.show();

  // Ouverture des préférences
  preferences.begin("config", false);

  // Lecture des valeurs depuis la NVS (ou valeurs par défaut)
  maxVolume = preferences.getFloat("maxVolume", 500);
  smoothingFactor = preferences.getFloat("smoothingFactor", 0.05);
  uint8_t r = preferences.getUChar("baseColorR", 255);
  uint8_t g = preferences.getUChar("baseColorG", 255);
  uint8_t b = preferences.getUChar("baseColorB", 255);
  baseColor = CRGB(r, g, b);
  fill_solid(leds, LED_COUNT, baseColor);
  FastLED.show();

  // Configuration WiFi via WiFiManager
  WiFiManager wm;
  wm.autoConnect("AfricanChild", "chacal");
  Serial.print("Connecté au WiFi, adresse IP: ");
  Serial.println(WiFi.localIP());

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
  fill_solid(leds, LED_COUNT, color);
  FastLED.show();
  delay(50);
}
