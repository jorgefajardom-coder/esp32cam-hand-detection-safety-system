/*
 * Proyecto: Sistema de transmisión de video ESP32-CAM vía WebSocket
 * Autor: Jorge Andrés Fajardo Mora
 * Desarrollado por: Jorge Andrés Fajardo Mora
 * 
 * © 2026 Jorge Andrés Fajardo Mora. Todos los derechos reservados.
 * 
 * Este código es propiedad del autor. Queda prohibida su reproducción,
 * distribución o modificación sin autorización expresa.
 */
#include <WiFi.h>
#include <WebSocketsServer.h>
#include "esp_camera.h"


// Configurar los detalles de la red WiFi
const char* ssid = "";
const char* password = "";

// Crear un servidor WebSocket
WebSocketsServer webSocket = WebSocketsServer(81);

// Pines del LED y configuración de la cámara para AI Thinker
#define LED_PIN 4           // El LED flash de la ESP32-CAM AI Thinker está en el pin GPIO 4
#define LED_CHANNEL 0       // Canal para controlar el PWM del LED
#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27
#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

// Función para parpadear el LED con tiempo ajustable
void blinkLED(int times, int onTime, int offTime) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);  // Encender LED
    delay(onTime);                 // Mantener encendido por el tiempo especificado
    digitalWrite(LED_PIN, LOW);   // Apagar LED
    delay(offTime);                // Mantener apagado por el tiempo especificado
  }
}

// Función para manejar eventos del WebSocket
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_DISCONNECTED) {
    Serial.printf("[%u] Cliente desconectado\n", num);
    blinkLED(2, 200, 1000);  // Parpadear 2 veces (200 ms encendido, 1000 ms apagado)
  } else if (type == WStype_CONNECTED) {
    Serial.printf("[%u] Cliente conectado\n", num);
    blinkLED(3, 200, 1000);  // Parpadear 3 veces (200 ms encendido, 1000 ms apagado)
  } else if (type == WStype_TEXT) {
    Serial.printf("[%u] Mensaje recibido: %s\n", num, payload);
  }
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconectando a Wi-Fi...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nWi-Fi reconectado");
  }
}

void setup() {
  // Configuración básica
  Serial.begin(115200);
  
  // Configurar el LED como salida
  pinMode(LED_PIN, OUTPUT);

  // Conectar al Wi-Fi
  Serial.println("Conectando a Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado");

  // Mostrar la IP de la ESP32 en el monitor serial
  Serial.print("Dirección IP de la ESP32-CAM: ");
  Serial.println(WiFi.localIP());

  blinkLED(2, 200, 1000);  // Parpadea el LED 2 veces (200 ms encendido, 1000 ms apagado)

  // Configurar la cámara
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;  // Mantenemos JPEG para detección de línea negra
  config.frame_size = FRAMESIZE_SVGA; 
  config.jpeg_quality = 10; 
  config.fb_count = 2; 

  // Inicializar la cámara
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Error al inicializar la cámara");
    return;
  }

  // Iniciar el servidor WebSocket
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("Servidor WebSocket iniciado");
  Serial.print("Puedes acceder al stream de video en: ws://");
  Serial.print(WiFi.localIP());
  Serial.println(":81");
}

void loop() {
  // Reconectar Wi-Fi si es necesario
  reconnectWiFi();

  // Loop del WebSocket
  webSocket.loop();

  // Capturar una imagen
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Error capturando imagen");
    return;
  }

  // Medir la latencia antes y después de enviar la imagen
  unsigned long startTime = millis(); // Tiempo de inicio
  webSocket.broadcastBIN(fb->buf, fb->len);
  unsigned long endTime = millis(); // Tiempo de finalización

  // Liberar el frame buffer para el siguiente fotograma
  esp_camera_fb_return(fb);

  // Informar sobre el envío de imagen, velocidad y latencia
  unsigned long latency = endTime - startTime; // Calcular latencia
  Serial.print("Imagen enviada. Tamaño: ");
  Serial.print(fb->len);
  Serial.print(" bytes. Latencia: ");
  Serial.print(latency);
  Serial.println(" ms.");

  // Pequeño retraso para estabilizar la tasa de fotogramas
  delay(5);  // Ajustado para más FPS si es necesario