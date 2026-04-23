#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>

// --- PINES DEL ESCLAVO ---
#define LED1_PIN 25 // Pasillo
#define LED2_PIN 26 // Pasillo
#define LED3_PIN 27 // Pasillo

#define PIN_SERVO_IZQ 13 // Puerta Izquierda
#define PIN_SERVO_DER 14 // Puerta Derecha
#define LED_ENTRADA 33   // Foco de la entrada

// --- AJUSTES EDITABLES (SERVOS) ---
int cerradoIzq = 0;
int abiertoIzq = 80;

int cerradoDer = 180;
int abiertoDer = 100;

Servo servoIzq;
Servo servoDer;

// ¡LA CAJA DEBE SER IDÉNTICA AL MAESTRO!
typedef struct struct_message
{
    bool presenciaPasillo;
    bool presenciaEntrada;
} struct_message;

struct_message datosRecibidos;

void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    memcpy(&datosRecibidos, incomingData, sizeof(datosRecibidos));

    // 1. REACCIONAR AL PASILLO
    if (datosRecibidos.presenciaPasillo)
    {
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

    // 2. REACCIONAR A LA ENTRADA
    if (datosRecibidos.presenciaEntrada)
    {
        // Alguien llegó: Abrir puertas y prender luz
        servoIzq.write(abiertoIzq);
        servoDer.write(abiertoDer);
        digitalWrite(LED_ENTRADA, HIGH);
    }
    else
    {
        // No hay nadie: Cerrar puertas y apagar luz
        servoIzq.write(cerradoIzq);
        servoDer.write(cerradoDer);
        digitalWrite(LED_ENTRADA, LOW);
    }
}

void setup()
{
    Serial.begin(115200);

    // Configurar LEDs
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);
    pinMode(LED3_PIN, OUTPUT);
    pinMode(LED_ENTRADA, OUTPUT);

    // Configurar Servos
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    servoIzq.setPeriodHertz(50);
    servoDer.setPeriodHertz(50);
    servoIzq.attach(PIN_SERVO_IZQ, 500, 2400);
    servoDer.attach(PIN_SERVO_DER, 500, 2400);

    // Estado inicial: Todo apagado y puertas cerradas
    digitalWrite(LED1_PIN, LOW);
    digitalWrite(LED2_PIN, LOW);
    digitalWrite(LED3_PIN, LOW);
    digitalWrite(LED_ENTRADA, LOW);
    servoIzq.write(cerradoIzq);
    servoDer.write(cerradoDer);

    // Iniciar ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
        return;
    esp_now_register_recv_cb(esp_now_recv_cb_t(onDataRecv));
}

void loop()
{
    // El Esclavo sigue limpio, todo se controla en onDataRecv
}