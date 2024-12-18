// Cadeau de Noël "cacahuète" pour Denis

#include <Arduino.h>
#include <driver/i2s.h>

#include <Adafruit_NeoPixel.h>

#define LED_PIN 4
#define LED_COUNT 10

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);


#define I2S_WS 15
#define I2S_SD 32
#define I2S_SCK 14

float previousVolume = 0;
#define ALPHA 0.1  // Filtre : ajuster entre 0.0 (lissage total) et 1.0 (aucun lissage)

#define SMOOTHING_FACTOR 0.05  // Ajuste entre 0.0 (pas de réactivité) et 1.0 (très réactif)
float smoothedVolume = 0;

#define MAX_VOLUME 500  // Ajuste selon la plage de volume détectée
#define MIN_VOLUME 0

// Fonction d'interpolation linéaire
uint8_t lerp(uint8_t start, uint8_t end, float t) {
  return start + (end - start) * t;
}

// Calcul de la couleur interpolée
uint32_t getColorFromVolume(int volume) {
  float t;  // Facteur d'interpolation (0.0 à 1.0)
  uint8_t r, g, b;

  if (volume <= MAX_VOLUME / 3) {
    // Interpolation Blanc -> Vert
    t = (float)volume / (MAX_VOLUME / 3);
    r = lerp(255, 0, t);
    g = lerp(255, 255, t);
    b = lerp(255, 0, t);
  } 
  else if (volume <= 2 * MAX_VOLUME / 3) {
    // Interpolation Vert -> Jaune
    t = (float)(volume - MAX_VOLUME / 3) / (MAX_VOLUME / 3);
    r = lerp(0, 255, t);
    g = lerp(255, 255, t);
    b = lerp(0, 0, t);
  } 
  else if (volume <= MAX_VOLUME){
    // Interpolation Jaune -> Rouge
    t = (float)(volume - 2 * MAX_VOLUME / 3) / (MAX_VOLUME / 3);
    r = lerp(255, 255, t);
    g = lerp(255, 0, t);
    b = lerp(0, 0, t);
  }
  else {
  r = 255;
  g = 0;
  b = 0;
  }

  return strip.Color(r, g, b);
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
      .bck_io_num = I2S_SCK,
      .ws_io_num = I2S_WS,
      .data_out_num = -1,
      .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

void setup() {
  Serial.begin(115200);
  setupI2SMic();
    strip.begin();
  strip.show();
}

int getVolume()
{
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


void loop() {
  
 int rawVolume = getVolume(); // Récupère le volume brut
  float filteredVolume = ALPHA * rawVolume + (1 - ALPHA) * previousVolume;
  previousVolume = filteredVolume;

  smoothedVolume = (SMOOTHING_FACTOR * rawVolume) + ((1.0 - SMOOTHING_FACTOR) * smoothedVolume);

  Serial.print(" | smoothedVolume : ");
  Serial.println(smoothedVolume);
  
  uint32_t color = getColorFromVolume(smoothedVolume);
  
  strip.fill(color);
  strip.show();
  delay(50);
  
}
