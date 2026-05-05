#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <AccelStepper.h> // ¡Nueva Librería!

#define PIN_SERVO_DER 14
#define PIN_SERVO_IZQ 13
#define LED1_PIN 25
#define LED2_PIN 26
#define LED3_PIN 27
#define LED_ENTRADA 33
#define PIN_NEOPIXEL 18
#define PIN_SERVO_DISCO 19

// --- NUEVOS PINES: ELEVADOR Y TRANSISTOR ---
#define PIN_STEP 32
#define PIN_DIR 21
#define PIN_LEDS_PISO2 22

// --- AJUSTES SERVOS ENTRADA ---
int cerradoIzq = 0, abiertoIzq = 80;
int cerradoDer = 180, abiertoDer = 100;
bool estadoPuertasActual = false;
Servo servoIzq, servoDer;

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
int piso2 = -1050; // Calibrado por el ingeniero

bool elevadorEnPiso1 = true;
bool elevadorEnViaje = false;
bool esperandoPasajeros = false;
unsigned long tiempoApertura = 0;

// --- DICCIONARIO ACTUALIZADO ---
typedef struct struct_message
{
    bool presenciaPasillo;
    bool presenciaEntrada;
    bool fiestaActiva;
    bool touchPiso1; // ¡Nuevo!
    bool touchPiso2; // ¡Nuevo!
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
    pinMode(PIN_LEDS_PISO2, OUTPUT); // Pin del Transistor

    digitalWrite(PIN_LEDS_PISO2, LOW); // LEDs del piso 2 apagados al iniciar

    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);

    servoIzq.attach(PIN_SERVO_IZQ, 500, 2400);
    servoDer.attach(PIN_SERVO_DER, 500, 2400);
    servoDisco.attach(PIN_SERVO_DISCO, 500, 2400);

    servoIzq.write(cerradoIzq);
    servoDer.write(cerradoDer);

    // Ajustes Premium del Elevador
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
            {
                tira.setPixelColor(i, tira.Color(random(255), random(255), random(255)));
            }
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

    // -----------------------------------------------------
    // 4. LÓGICA DEL ELEVADOR INTELIGENTE (Sin Delays)
    // -----------------------------------------------------

    // A) Detectar si presionan un botón (solo si el elevador no se está moviendo)
    if (!elevadorEnViaje && !esperandoPasajeros)
    {
        if (datosRecibidos.touchPiso1 || datosRecibidos.touchPiso2)
        {
            esperandoPasajeros = true;
            tiempoApertura = tiempoActual;
            Serial.println("Botón Touch presionado: Esperando 3 segundos...");
        }
    }

    // B) Temporizador de espera (3 segundos)
    if (esperandoPasajeros)
    {
        if (tiempoActual - tiempoApertura >= 3000)
        {
            esperandoPasajeros = false;
            elevadorEnViaje = true;

            // Enviamos el elevador al piso contrario
            if (elevadorEnPiso1)
            {
                Serial.println("Viajando al Piso 2...");
                elevador.moveTo(piso2);
            }
            else
            {
                Serial.println("Viajando al Piso 1...");
                elevador.moveTo(piso1);
            }
        }
    }

    // C) Mantener el motor en movimiento (se ejecuta súper rápido en cada loop)
    if (elevadorEnViaje)
    {
        elevador.run();

        // Si la distancia restante es 0, ya llegó.
        if (elevador.distanceToGo() == 0)
        {
            elevadorEnViaje = false;
            elevadorEnPiso1 = !elevadorEnPiso1; // Actualizamos el piso

            Serial.println("¡Elevador en destino!");

            // Encendemos/Apagamos el transistor 2N2222
            if (elevadorEnPiso1)
            {
                digitalWrite(PIN_LEDS_PISO2, LOW); // Apaga LEDs del piso 2
            }
            else
            {
                digitalWrite(PIN_LEDS_PISO2, HIGH); // Prende LEDs del piso 2
            }
        }
    }

    delay(20);
}