#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// --- PINES DE LOS LEDS ---
#define LED1_PIN 25
#define LED2_PIN 26
#define LED3_PIN 27

// La caja de datos debe ser idéntica a la del Maestro
typedef struct struct_message
{
    bool presenciaPasillo;
} struct_message;

struct_message datosRecibidos;

// Función que se ejecuta al recibir un mensaje
void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    memcpy(&datosRecibidos, incomingData, sizeof(datosRecibidos));

    if (datosRecibidos.presenciaPasillo)
    {
        Serial.println("¡Alguien en el pasillo! Prendiendo luces...");
        digitalWrite(LED1_PIN, HIGH);
        digitalWrite(LED2_PIN, HIGH);
        digitalWrite(LED3_PIN, HIGH);
    }
    else
    {
        digitalWrite(LED1_PIN, LOW);
        digitalWrite(LED2_PIN, LOW);
        digitalWrite(LED3_PIN, LOW);
    }
}

void setup()
{
    Serial.begin(115200);

    // Configurar los pines como salidas
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);
    pinMode(LED3_PIN, OUTPUT);

    // Asegurarnos de que empiecen apagados
    digitalWrite(LED1_PIN, LOW);
    digitalWrite(LED2_PIN, LOW);
    digitalWrite(LED3_PIN, LOW);

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
        return;
    esp_now_register_recv_cb(esp_now_recv_cb_t(onDataRecv));
}

void loop()
{
    // El Esclavo sigue sin hacer nada aquí. Todo pasa en "onDataRecv"
}