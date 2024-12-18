
#include <ESPAsyncWebServer.h>
#include <FastLED.h>

#define LED_PIN 4
#define LED_COUNT 10

CRGB leds[LED_COUNT];

const char* ssid = "mrVOOlpy";
const char* password = "youhououhou";

// Création du serveur web
AsyncWebServer server(80);

// Paramètres configurables
float maxVolume = 500;
float smoothingFactor = 0.05;

// Page HTML simple
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>Réglages de la Lampe</title></head>
<body>
<h1>Réglages de la Lampe</h1>
<form action="/set" method="GET">
  <label>Max Volume:</label>
  <input type="number" name="maxVolume" min="0" max="1000" value="%MAX_VOLUME%"><br><br>
  <label>Smoothing Factor:</label>
  <input type="number" name="smoothingFactor" step="0.01" min="0.0" max="1.0" value="%SMOOTHING_FACTOR%"><br><br>
  <input type="submit" value="Mettre à jour">
</form>
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
    request->send_P(200, "text/html", index_html, processor);
  });

  // Mise à jour des paramètres
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("maxVolume")) {
      maxVolume = request->getParam("maxVolume")->value().toFloat();
    }
    if (request->hasParam("smoothingFactor")) {
      smoothingFactor = request->getParam("smoothingFactor")->value().toFloat();
    }
    request->redirect("/");
  });

  server.begin();
}

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
  FastLED.clear();
  FastLED.show();
  setupWiFi();
  setupWebServer();
}

void loop() {
  // Exemple d'utilisation des nouvelles valeurs
  int rawVolume = random(0, 1000);  // Simuler un volume
  float smoothedVolume = (smoothingFactor * rawVolume) + ((1.0 - smoothingFactor) * maxVolume);

  // Exemple de changement de couleur
  int intensity = map(smoothedVolume, 0, maxVolume, 0, 255);
  fill_solid(leds, LED_COUNT, CRGB(intensity, 0, 255 - intensity));
  FastLED.show();
  delay(50);
}
