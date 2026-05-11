/*#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <AccelStepper.h>

#define PIN_SERVO_DER 14
#define PIN_SERVO_IZQ 13
#define LED1_PIN 25
#define LED2_PIN 26
#define LED3_PIN 27
#define LED_ENTRADA 33
#define PIN_NEOPIXEL 18
#define PIN_SERVO_DISCO 19
#define PIN_SERVO_PLUMA 23 // <--- PIN DEL MOTOR DE LA PLUMA

// --- PINES ELEVADOR Y TRANSISTOR ---
#define PIN_STEP 32
#define PIN_DIR 21
#define PIN_LEDS_PISO2 22

// --- PIN DEL VENTILADOR (L298N) ---
#define PIN_ENA_VENTILADOR 4

// --- AJUSTES SERVOS ENTRADA ---
int cerradoIzq = 0, abiertoIzq = 80;
int cerradoDer = 180, abiertoDer = 100;
bool estadoPuertasActual = false;
Servo servoIzq, servoDer;

// --- AJUSTES SERVO ESTACIONAMIENTO ---
Servo servoPluma; // <--- VARIABLE PARA LA PLUMA

// --- AJUSTES MODO FIESTA ---
#define NUM_LEDS 16
Adafruit_NeoPixel tira = Adafruit_NeoPixel(NUM_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
Servo servoDisco;
int minAnguloDisco = 20, maxAnguloDisco = 55, anguloDisco = 37;
bool subiendoDisco = true;
unsigned long tiempoUltimoColor = 0;
bool oficinaPintada = false;

// --- AJUSTES ELEVADOR ---
AccelStepper elevador(1, PIN_STEP, PIN_DIR);
int piso1 = 0;
int piso2 = -1050;

bool elevadorEnPiso1 = true;
bool elevadorEnViaje = false;
bool esperandoPasajeros = false;
unsigned long tiempoApertura = 0;

// --- AJUSTE VELOCIDAD VENTILADOR ---
int velocidadVentilador = 200;

// --- DICCIONARIO ACTUALIZADO ---
typedef struct struct_message
{
    bool presenciaPasillo;
    bool presenciaEntrada;
    bool fiestaActiva;
    bool touchPiso1;
    bool touchPiso2;
    bool ventiladorActivo;
    bool abrirPluma; // <--- LA ORDEN QUE LLEGA
} struct_message;

struct_message datosRecibidos;

void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    memcpy(&datosRecibidos, incomingData, sizeof(datosRecibidos));
}

void setup()
{
    Serial.begin(115200);
    tira.begin();
    tira.show();

    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);
    pinMode(LED3_PIN, OUTPUT);
    pinMode(LED_ENTRADA, OUTPUT);
    pinMode(PIN_LEDS_PISO2, OUTPUT);

    pinMode(PIN_ENA_VENTILADOR, OUTPUT);
    analogWrite(PIN_ENA_VENTILADOR, 0);

    digitalWrite(PIN_LEDS_PISO2, LOW);

    // Asignar timers para que los servos funcionen chido
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3); // <--- Uno extra para la pluma

    servoIzq.attach(PIN_SERVO_IZQ, 500, 2400);
    servoDer.attach(PIN_SERVO_DER, 500, 2400);
    servoDisco.attach(PIN_SERVO_DISCO, 500, 2400);

    // Inicializar el servo de la pluma
    servoPluma.attach(PIN_SERVO_PLUMA, 500, 2400);
    servoPluma.write(0); // Empezamos cerrados (0 grados)

    servoIzq.write(cerradoIzq);
    servoDer.write(cerradoDer);

    elevador.setMaxSpeed(150);
    elevador.setAcceleration(80);

    WiFi.mode(WIFI_STA);
    esp_now_init();
    esp_now_register_recv_cb(esp_now_recv_cb_t(onDataRecv));
}

void loop()
{
    unsigned long tiempoActual = millis();

    // 1. PASILLO
    digitalWrite(LED1_PIN, datosRecibidos.presenciaPasillo);
    digitalWrite(LED2_PIN, datosRecibidos.presenciaPasillo);
    digitalWrite(LED3_PIN, datosRecibidos.presenciaPasillo);

    // 2. ENTRADA
    if (datosRecibidos.presenciaEntrada != estadoPuertasActual)
    {
        estadoPuertasActual = datosRecibidos.presenciaEntrada;
        if (estadoPuertasActual)
        {
            servoIzq.write(abiertoIzq);
            servoDer.write(abiertoDer);
            digitalWrite(LED_ENTRADA, HIGH);
        }
        else
        {
            servoIzq.write(cerradoIzq);
            servoDer.write(cerradoDer);
            digitalWrite(LED_ENTRADA, LOW);
        }
    }

    // 3. FIESTA
    if (datosRecibidos.fiestaActiva)
    {
        oficinaPintada = false;
        if (tiempoActual - tiempoUltimoColor >= 1000)
        {
            for (int i = 0; i < NUM_LEDS; i++)
                tira.setPixelColor(i, tira.Color(random(255), random(255), random(255)));
            tira.show();
            tiempoUltimoColor = tiempoActual;
        }
        if (subiendoDisco)
            anguloDisco += 2;
        else
            anguloDisco -= 2;
        if (anguloDisco >= maxAnguloDisco)
            subiendoDisco = false;
        if (anguloDisco <= minAnguloDisco)
            subiendoDisco = true;
        servoDisco.write(anguloDisco);
    }
    else
    {
        if (!oficinaPintada)
        {
            for (int i = 0; i < NUM_LEDS; i++)
                tira.setPixelColor(i, tira.Color(5, 5, 5));
            tira.show();
            oficinaPintada = true;
        }
        servoDisco.write(37);
    }

    // 4. ELEVADOR
    if (!elevadorEnViaje && !esperandoPasajeros)
    {
        if (datosRecibidos.touchPiso1 || datosRecibidos.touchPiso2)
        {
            esperandoPasajeros = true;
            tiempoApertura = tiempoActual;
        }
    }
    if (esperandoPasajeros)
    {
        if (tiempoActual - tiempoApertura >= 3000)
        {
            esperandoPasajeros = false;
            elevadorEnViaje = true;
            if (elevadorEnPiso1)
                elevador.moveTo(piso2);
            else
                elevador.moveTo(piso1);
        }
    }
    if (elevadorEnViaje)
    {
        elevador.run();
        if (elevador.distanceToGo() == 0)
        {
            elevadorEnViaje = false;
            elevadorEnPiso1 = !elevadorEnPiso1;
            if (elevadorEnPiso1)
                digitalWrite(PIN_LEDS_PISO2, LOW);
            else
                digitalWrite(PIN_LEDS_PISO2, HIGH);
        }
    }

    // 5. VENTILADOR (CLIMA)
    if (datosRecibidos.ventiladorActivo)
        analogWrite(PIN_ENA_VENTILADOR, velocidadVentilador);
    else
        analogWrite(PIN_ENA_VENTILADOR, 0);

    // ==========================================
    // 6. LÓGICA DEL MOTOR DE LA PLUMA
    // ==========================================
    if (datosRecibidos.abrirPluma)
    {
        servoPluma.write(34); // Sube a 34 grados
    }
    else
    {
        servoPluma.write(0); // Baja a 0 grados
    }
}
    */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <AccelStepper.h>

// ====================================================================
// 1. PINES DE CONEXIÓN DEL HARDWARE
// ====================================================================
// --- Servomotores ---
#define PIN_SERVO_IZQ 13
#define PIN_SERVO_DER 14
#define PIN_SERVO_DISCO 19
#define PIN_SERVO_PLUMA 23

// --- Iluminación (LEDs y NeoPixel) ---
#define LED1_PIN 25
#define LED2_PIN 26
#define LED3_PIN 27
#define LED_ENTRADA 33
#define PIN_LEDS_PISO2 22
#define PIN_NEOPIXEL 18

// --- Elevador (Motor a Pasos) ---
#define PIN_STEP 32
#define PIN_DIR 21

// --- Ventilador (Módulo L298N) ---
#define PIN_ENA_VENTILADOR 4

// ====================================================================
// 2. AJUSTES MODIFICABLES (Calibración mecánica)
// ====================================================================
// --- Puertas de Entrada ---
const int cerradoIzq = 0;
const int abiertoIzq = 80;
const int cerradoDer = 180;
const int abiertoDer = 100;

// --- Pluma del Estacionamiento ---
const int plumaCerrada = 0;
const int plumaAbierta = 34;

// --- Modo Fiesta (Bola Disco y NeoPixels) ---
#define NUM_LEDS 16
const int minAnguloDisco = 20;
const int maxAnguloDisco = 55;
const int velocidadGiroDisco = 2; // Cuántos grados se mueve por ciclo

// --- Elevador ---
const int piso1 = 0;
const int piso2 = -1005; // Pasos necesarios para subir al piso 2
const float velocidadElevador = 150.0;
const float aceleracionElevador = 80.0;
const unsigned long tiempoEsperaElevador = 3000; // Milisegundos con las puertas abiertas

// --- Clima ---
const int velocidadVentilador = 255; // 0 (apagado) a 255 (máximo)

// ====================================================================
// 3. VARIABLES INTERNAS DE ESTADO Y OBJETOS
// ====================================================================
Servo servoIzq, servoDer, servoDisco, servoPluma;
Adafruit_NeoPixel tira = Adafruit_NeoPixel(NUM_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
AccelStepper elevador(1, PIN_STEP, PIN_DIR);

bool estadoPuertasActual = false;
int anguloDisco = 37;
bool subiendoDisco = true;
unsigned long tiempoUltimoColor = 0;
bool oficinaPintada = false;

bool elevadorEnPiso1 = true;
bool elevadorEnViaje = false;
bool esperandoPasajeros = false;
unsigned long tiempoApertura = 0;

// ====================================================================
// 4. ESTRUCTURA ESP-NOW (Recepción de Datos)
// ====================================================================
typedef struct struct_message
{
    bool presenciaPasillo;
    bool presenciaEntrada;
    bool fiestaActiva;
    bool touchPiso1;
    bool touchPiso2;
    bool ventiladorActivo;
    bool abrirPluma;
} struct_message;

struct_message datosRecibidos;

// Función que se ejecuta cada vez que el Maestro envía un dato
void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    memcpy(&datosRecibidos, incomingData, sizeof(datosRecibidos));
}

// ====================================================================
// INICIALIZACIÓN DEL SISTEMA
// ====================================================================
void setup()
{
    Serial.begin(115200);

    // Inicializar NeoPixels
    tira.begin();
    tira.show();

    // Configurar pines de salida (LEDs y Ventilador)
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);
    pinMode(LED3_PIN, OUTPUT);
    pinMode(LED_ENTRADA, OUTPUT);
    pinMode(PIN_LEDS_PISO2, OUTPUT);
    pinMode(PIN_ENA_VENTILADOR, OUTPUT);

    // Apagar todo por seguridad al iniciar
    digitalWrite(PIN_ENA_VENTILADOR, LOW);
    digitalWrite(PIN_LEDS_PISO2, LOW);

    // Asignar Timers de hardware para los Servos (Vital en ESP32)
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    // Anclar Servos a sus pines
    servoIzq.attach(PIN_SERVO_IZQ, 500, 2400);
    servoDer.attach(PIN_SERVO_DER, 500, 2400);
    servoDisco.attach(PIN_SERVO_DISCO, 500, 2400);
    servoPluma.attach(PIN_SERVO_PLUMA, 500, 2400);

    // Posiciones Iniciales Mecánicas
    servoIzq.write(cerradoIzq);
    servoDer.write(cerradoDer);
    servoPluma.write(plumaCerrada);

    // Configuración del Motor a Pasos (Elevador)
    elevador.setMaxSpeed(velocidadElevador);
    elevador.setAcceleration(aceleracionElevador);

    // Inicializar ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK)
    {
        esp_now_register_recv_cb(esp_now_recv_cb_t(onDataRecv));
        Serial.println("Esclavo listo y escuchando...");
    }
    else
    {
        Serial.println("Error fatal iniciando ESP-NOW");
    }
}

// ====================================================================
// CICLO PRINCIPAL (LOOP)
// ====================================================================
void loop()
{
    unsigned long tiempoActual = millis();

    // ==========================================
    // MODULO 1: ILUMINACIÓN DEL PASILLO
    // ==========================================
    digitalWrite(LED1_PIN, datosRecibidos.presenciaPasillo);
    digitalWrite(LED2_PIN, datosRecibidos.presenciaPasillo);
    digitalWrite(LED3_PIN, datosRecibidos.presenciaPasillo);

    // ==========================================
    // MODULO 2: PUERTAS PRINCIPALES
    // ==========================================
    if (datosRecibidos.presenciaEntrada != estadoPuertasActual)
    {
        estadoPuertasActual = datosRecibidos.presenciaEntrada;
        if (estadoPuertasActual)
        {
            servoIzq.write(abiertoIzq);
            delay(500);
            servoDer.write(abiertoDer);
            digitalWrite(LED_ENTRADA, HIGH);
        }
        else
        {
            servoIzq.write(cerradoIzq);
            delay(500);
            servoDer.write(cerradoDer);
            digitalWrite(LED_ENTRADA, LOW);
        }
    }

    // ==========================================
    // MODULO 3: MODO FIESTA (NEOPIXELS Y BOLA DISCO)
    // ==========================================
    if (datosRecibidos.fiestaActiva)
    {
        oficinaPintada = false; // Reseteamos bandera de apagado

        // Efecto de luces aleatorias cada segundo
        if (tiempoActual - tiempoUltimoColor >= 1000)
        {
            for (int i = 0; i < NUM_LEDS; i++)
            {
                tira.setPixelColor(i, tira.Color(random(255), random(255), random(255)));
            }
            tira.show();
            tiempoUltimoColor = tiempoActual;
        }

        // Movimiento de la bola disco (barrido)
        if (subiendoDisco)
            anguloDisco += velocidadGiroDisco;
        else
            anguloDisco -= velocidadGiroDisco;

        if (anguloDisco >= maxAnguloDisco)
            subiendoDisco = false;
        if (anguloDisco <= minAnguloDisco)
            subiendoDisco = true;

        servoDisco.write(anguloDisco);
    }
    else
    {
        // Apagar fiesta de forma limpia
        if (!oficinaPintada)
        {
            for (int i = 0; i < NUM_LEDS; i++)
            {
                tira.setPixelColor(i, tira.Color(5, 5, 5)); // Luz blanca muy tenue (oficina normal)
            }
            tira.show();
            oficinaPintada = true;
        }
        servoDisco.write(37); // Bola disco centrada y quieta
    }

    // ==========================================
    // MODULO 4: ELEVADOR MECÁNICO
    // ==========================================
    if (!elevadorEnViaje && !esperandoPasajeros)
    {
        if (datosRecibidos.touchPiso1 || datosRecibidos.touchPiso2)
        {
            esperandoPasajeros = true;
            tiempoApertura = tiempoActual;
        }
    }

    if (esperandoPasajeros)
    {
        if (tiempoActual - tiempoApertura >= tiempoEsperaElevador)
        {
            esperandoPasajeros = false;
            elevadorEnViaje = true;
            if (elevadorEnPiso1)
                elevador.moveTo(piso2);
            else
                elevador.moveTo(piso1);
        }
    }

    if (elevadorEnViaje)
    {
        elevador.run(); // ¡Esta línea debe ejecutarse lo más rápido posible!
        if (elevador.distanceToGo() == 0)
        {
            elevadorEnViaje = false;
            elevadorEnPiso1 = !elevadorEnPiso1;
            // Prender o apagar las luces del piso 2 según dónde esté
            if (elevadorEnPiso1)
                digitalWrite(PIN_LEDS_PISO2, LOW);
            else
                digitalWrite(PIN_LEDS_PISO2, HIGH);
        }
    }

    // ==========================================
    // MODULO 5: CONTROL DE CLIMA (VENTILADOR)
    // ==========================================
    if (datosRecibidos.ventiladorActivo)
    {
        digitalWrite(PIN_ENA_VENTILADOR, HIGH);
    }
    else
    {
        digitalWrite(PIN_ENA_VENTILADOR, LOW);
    }

    // ==========================================
    // MODULO 6: PLUMA DE ESTACIONAMIENTO VIP
    // ==========================================
    if (datosRecibidos.abrirPluma)
    {
        servoPluma.write(plumaAbierta);
    }
    else
    {
        servoPluma.write(plumaCerrada);
    }
}