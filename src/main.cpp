#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const char* ssid = "Sephora ";
const char* password = "Jesusloveyou199";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

#define DATA_PIN      13  // LED adressables 
#define FAN_PIN       18  // Commande ventilateur
#define PUMP_PIN      19  // Commande pompe 
#define ONE_WIRE_BUS  23  // Capteur DS18B20
#define BUTTON_PIN    4   // BP

#define NUM_LEDS      47
#define LED_TYPE      WS2812B
#define COLOR_ORDER   RGB 

CRGB leds[NUM_LEDS];
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const float TEMP_SEUIL_ALLUMAGE   = 30.0;
const float TEMP_SEUIL_EXTINCTION = 28.0;

enum Mode { MANUEL, AUTOMATIQUE };
Mode modeActuel = MANUEL;

enum OptionCouleurManuel { ROUGE_DEFAUT, ARC_EN_CIEL, VIOLET, EAU };
OptionCouleurManuel couleurManuel = ROUGE_DEFAUT;

enum OptionCouleurAuto { VERT_DEFAUT, JAUNE, CRISTAL, NOEL };
OptionCouleurAuto couleurAuto = VERT_DEFAUT;

uint8_t teinteArcEnCiel = 0;
float derniereTemp = 0.0;
bool etatActuateurs = true;
uint8_t luminositeLeds = 50; 

// Chronomètres non-bloquants
unsigned long dernierTempsBouton   = 0;
const unsigned long DELAI_ANTI_REBOND = 300; 
unsigned long dernierTempsLecture  = 0;
const unsigned long INTERVALLE_TEMP = 1000;

bool enAnimation = false;
int indexLedAnimation = 0;
CRGB couleurCibleAnimation;
unsigned long dernierTempsAnimation = 0;
const unsigned long VITESSE_ANIMATION = 15;

// Prototypes
void verifierBouton();
void gererAnimation();
void gererModeAutomatique();
void demarrerAnimation(CRGB nouvelleCouleur);
void commanderActuateurs(uint8_t etat);
void afficherCouleursRuban();
void envoyerEtatWebSocket();
void basculerMode();

// HTML / CSS / JS
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AeroTerra - Smart Cooling</title>
    <link href="https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;600;700&display=swap" rel="stylesheet">
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Poppins', sans-serif; }
        body { 
            background: linear-gradient(135deg, #0f172a 0%, #1e1b4b 100%); 
            color: #f8fafc; 
            min-height: 100vh; 
            display: flex; 
            justify-content: center; 
            align-items: center; 
            padding: 20px; 
        }
        .container { width: 100%; max-width: 420px; display: flex; flex-direction: column; gap: 20px; }
        .card { 
            background: rgba(30, 41, 59, 0.7); 
            backdrop-filter: blur(12px); 
            border: 1px solid rgba(255, 255, 255, 0.08); 
            border-radius: 20px; 
            padding: 24px; 
            text-align: center; 
            box-shadow: 0 10px 30px rgba(0, 0, 0, 0.3); 
        }
        .header { display: flex; align-items: center; justify-content: center; gap: 10px; margin-bottom: 5px; }
        .header h2 { font-size: 1.4rem; font-weight: 700; background: linear-gradient(90deg, #38bdf8, #818cf8); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
        .subtitle { font-size: 0.8rem; color: #94a3b8; text-transform: uppercase; letter-spacing: 1px; font-weight: 600; }
        
        .temp-display { margin: 20px 0; }
        .temp-val { font-size: 3.8rem; font-weight: 700; color: #38bdf8; text-shadow: 0 0 20px rgba(56, 189, 248, 0.3); }
        
        .status-badge { 
            display: inline-flex; 
            align-items: center; 
            gap: 8px; 
            padding: 8px 16px; 
            border-radius: 30px; 
            font-size: 0.85rem; 
            font-weight: 600; 
            transition: all 0.3s ease; 
        }
        .status-badge.active { background: rgba(16, 185, 129, 0.15); color: #34d399; border: 1px solid rgba(16, 185, 129, 0.3); }
        .status-badge.inactive { background: rgba(239, 68, 68, 0.15); color: #f87171; border: 1px solid rgba(239, 68, 68, 0.3); }
        .dot { width: 8px; height: 8px; border-radius: 50%; display: inline-block; }
        .active .dot { background: #34d399; box-shadow: 0 0 8px #34d399; }
        .inactive .dot { background: #f87171; box-shadow: 0 0 8px #f87171; }

        .btn { 
            width: 100%; 
            padding: 14px; 
            border: none; 
            border-radius: 14px; 
            font-weight: 600; 
            font-size: 0.95rem; 
            cursor: pointer; 
            color: white; 
            transition: all 0.2s ease; 
            box-shadow: 0 4px 12px rgba(0,0,0,0.2); 
        }
        .btn:active { transform: scale(0.98); }
        .btn-mode { background: linear-gradient(135deg, #3b82f6, #2563eb); }
        .btn-mode.auto { background: linear-gradient(135deg, #10b981, #059669); }

        .section-title { font-size: 1rem; font-weight: 600; margin-bottom: 15px; color: #e2e8f0; text-align: left; }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
        .btn-color { 
            background: rgba(51, 65, 85, 0.5); 
            border: 1px solid rgba(255, 255, 255, 0.05); 
            color: #cbd5e1; 
            padding: 12px; 
            border-radius: 12px; 
            font-size: 0.85rem; 
            font-weight: 500; 
            cursor: pointer; 
            transition: all 0.2s ease; 
        }
        .btn-color.active { 
            border-color: #38bdf8; 
            background: rgba(56, 189, 248, 0.15); 
            color: #38bdf8; 
            box-shadow: 0 0 12px rgba(56, 189, 248, 0.2); 
        }

        .brightness-box { margin-top: 18px; padding-top: 15px; border-top: 1px solid rgba(255, 255, 255, 0.08); text-align: left; }
        .brightness-label { display: flex; justify-content: space-between; font-size: 0.85rem; color: #94a3b8; font-weight: 500; margin-bottom: 8px; }
        .brightness-slider { 
            width: 100%; 
            -webkit-appearance: none; 
            height: 6px; 
            border-radius: 5px; 
            background: #334155; 
            outline: none; 
        }
        .brightness-slider::-webkit-slider-thumb { 
            -webkit-appearance: none; 
            width: 18px; 
            height: 18px; 
            border-radius: 50%; 
            background: #38bdf8; 
            cursor: pointer; 
            box-shadow: 0 0 10px rgba(56, 189, 248, 0.5); 
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="card">
            <div class="header">
                <h2>❄️ AeroTerra</h2>
            </div>
            <span class="subtitle">Smart Cooling System</span>
            
            <div class="temp-display">
                <div class="temp-val" id="tempVal">-- °C</div>
            </div>
            
            <div class="status-badge inactive" id="actState">
                <span class="dot"></span>
                <span id="actText">Actuateurs : --</span>
            </div>
        </div>

        <div class="card">
            <div class="section-title">Mode de Fonctionnement</div>
            <button class="btn btn-mode" id="btnMode" onclick="toggleMode()">MODE MANUEL</button>
        </div>

        <div class="card">
            <div class="section-title">Palette d'Éclairage</div>
            <div class="grid" id="colorGrid"></div>

            <div class="brightness-box">
                <div class="brightness-label">
                    <span>💡 Intensité Lumineuse</span>
                    <span id="brightVal">50%</span>
                </div>
                <input type="range" min="0" max="255" value="50" class="brightness-slider" id="brightRange" oninput="changeBrightness(this.value)">
            </div>
        </div>
    </div>

    <script>
        let ws = new WebSocket(`ws://${window.location.hostname}/ws`);
        ws.onmessage = (e) => {
            let data = JSON.parse(e.data);
            document.getElementById('tempVal').innerText = data.temp.toFixed(1) + " °C";
            
            let actBadge = document.getElementById('actState');
            let actText = document.getElementById('actText');
            if (data.actuators) {
                actBadge.className = "status-badge active";
                actText.innerText = "Système : MARCHE";
            } else {
                actBadge.className = "status-badge inactive";
                actText.innerText = "Système : ARRÊT";
            }
            
            let btnM = document.getElementById('btnMode');
            btnM.innerText = "MODE " + data.mode;
            btnM.className = (data.mode === "AUTOMATIQUE") ? "btn btn-mode auto" : "btn btn-mode";
            
            let pct = Math.round((data.brightness / 255) * 100);
            document.getElementById('brightVal').innerText = pct + "%";
            document.getElementById('brightRange').value = data.brightness;

            let grid = document.getElementById('colorGrid');
            grid.innerHTML = '';
            let opts = (data.mode === 'MANUEL') 
                ? [{i:0,l:'🔴 Rouge'},{i:1,l:'🌈 Arc-en-ciel'},{i:2,l:'💜 Violet'},{i:3,l:'🌊 Eau'}]
                : [{i:0,l:'🟢 Vert'},{i:1,l:'🟡 Jaune'},{i:2,l:'💎 Cristal'},{i:3,l:'🎄 Noël'}];
            let curColor = (data.mode === 'MANUEL') ? data.colorManuel : data.colorAuto;
            opts.forEach(o => {
                let b = document.createElement('button');
                b.className = 'btn-color' + (curColor === o.i ? ' active' : '');
                b.innerText = o.l;
                b.onclick = () => ws.send(JSON.stringify({action: "setColor", value: o.i}));
                grid.appendChild(b);
            });
        };

        function toggleMode() { ws.send(JSON.stringify({action: "toggleMode"})); }
        function changeBrightness(val) {
            document.getElementById('brightVal').innerText = Math.round((val / 255) * 100) + "%";
            ws.send(JSON.stringify({action: "setBrightness", value: parseInt(val)}));
        }
    </script>
</body>
</html>
)rawliteral";

void envoyerEtatWebSocket() {
    StaticJsonDocument<256> doc;
    doc["temp"] = derniereTemp;
    doc["mode"] = (modeActuel == AUTOMATIQUE) ? "AUTOMATIQUE" : "MANUEL";
    doc["colorManuel"] = (int)couleurManuel;
    doc["colorAuto"] = (int)couleurAuto;
    doc["actuators"] = etatActuateurs;
    doc["brightness"] = luminositeLeds;

    String res;
    serializeJson(doc, res);
    ws.textAll(res);
}

// Fonction centralisée pour la bascule de mode
void basculerMode() {
    if (modeActuel == MANUEL) {
        modeActuel = AUTOMATIQUE;
        couleurAuto = VERT_DEFAUT;
        
        // On effectue une première lecture immédiate de température
        sensors.requestTemperatures();
        float tempC = sensors.getTempCByIndex(0);
        if (tempC != DEVICE_DISCONNECTED_C) {
            derniereTemp = tempC;
        }
        
        // Applique l'état selon la température actuelle
        if (derniereTemp >= TEMP_SEUIL_ALLUMAGE) {
            commanderActuateurs(HIGH);
        } else {
            commanderActuateurs(LOW);
        }
        demarrerAnimation(CRGB::Green);
    } else {
        modeActuel = MANUEL;
        couleurManuel = ROUGE_DEFAUT;
        commanderActuateurs(HIGH); // En manuel, activé par défaut
        demarrerAnimation(CRGB::Red);
    }
    envoyerEtatWebSocket();
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            StaticJsonDocument<256> doc;
            if (!deserializeJson(doc, data)) {
                const char* action = doc["action"];
                if (strcmp(action, "toggleMode") == 0) {
                    basculerMode();
                } else if (strcmp(action, "setColor") == 0) {
                    int val = doc["value"];
                    if (modeActuel == MANUEL) couleurManuel = (OptionCouleurManuel)val;
                    else couleurAuto = (OptionCouleurAuto)val;
                    envoyerEtatWebSocket();
                } else if (strcmp(action, "setBrightness") == 0) {
                    luminositeLeds = doc["value"];
                    FastLED.setBrightness(luminositeLeds);
                    envoyerEtatWebSocket();
                }
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(FAN_PIN, OUTPUT);
    pinMode(PUMP_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    sensors.begin();
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(luminositeLeds);
    FastLED.clear(true);

    commanderActuateurs(HIGH);
    couleurManuel = ROUGE_DEFAUT;
    afficherCouleursRuban();

    WiFi.begin(ssid, password);
    Serial.print("Connexion au Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connecté ! IP : " + WiFi.localIP().toString());

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
        req->send(200, "text/html", index_html);
    });
    server.begin();
}

void loop() {
    ws.cleanupClients();
    verifierBouton();

    if (enAnimation) {
        gererAnimation();
    } else {
        if (modeActuel == AUTOMATIQUE) {
            gererModeAutomatique();
        }
        afficherCouleursRuban();
    }
}

void commanderActuateurs(uint8_t etat) {
    digitalWrite(FAN_PIN, etat);
    digitalWrite(PUMP_PIN, etat);
    etatActuateurs = (etat == HIGH);
}

void afficherCouleursRuban() {
    if (modeActuel == MANUEL) {
        switch (couleurManuel) {
            case ROUGE_DEFAUT: fill_solid(leds, NUM_LEDS, CRGB::Red); break;
            case ARC_EN_CIEL: fill_rainbow(leds, NUM_LEDS, teinteArcEnCiel++, 7); break;
            case VIOLET: fill_solid(leds, NUM_LEDS, CRGB::Purple); break;
            case EAU: fill_solid(leds, NUM_LEDS, CRGB::DeepSkyBlue); break;
        }
    } else {
        switch (couleurAuto) {
            case VERT_DEFAUT: fill_solid(leds, NUM_LEDS, CRGB::Green); break;
            case JAUNE: fill_solid(leds, NUM_LEDS, CRGB::Yellow); break;
            case CRISTAL: fill_solid(leds, NUM_LEDS, CRGB::Cyan); break;
            case NOEL: 
                for (int i = 0; i < NUM_LEDS; i++) {
                    leds[i] = (i % 2 == 0) ? CRGB::Red : CRGB::Green;
                }
                break;
        }
    }
    FastLED.setBrightness(luminositeLeds);
    FastLED.show();
}

void demarrerAnimation(CRGB nouvelleCouleur) {
    enAnimation = true;
    indexLedAnimation = 0;
    couleurCibleAnimation = nouvelleCouleur;
    dernierTempsAnimation = millis();
}

void gererAnimation() {
    if (millis() - dernierTempsAnimation >= VITESSE_ANIMATION) {
        dernierTempsAnimation = millis();
        if (indexLedAnimation < NUM_LEDS) {
            leds[indexLedAnimation] = couleurCibleAnimation;
            FastLED.setBrightness(luminositeLeds);
            FastLED.show();
            indexLedAnimation++;
        } else {
            enAnimation = false;
        }
    }
}

void verifierBouton() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        if (millis() - dernierTempsBouton > DELAI_ANTI_REBOND) {
            dernierTempsBouton = millis();
            basculerMode();
        }
    }
}

void gererModeAutomatique() {
    if (millis() - dernierTempsLecture >= INTERVALLE_TEMP) {
        dernierTempsLecture = millis();
        sensors.requestTemperatures();
        float tempC = sensors.getTempCByIndex(0);

        if (tempC != DEVICE_DISCONNECTED_C) {
            derniereTemp = tempC;
            if (tempC >= TEMP_SEUIL_ALLUMAGE) {
                commanderActuateurs(HIGH);
            } else if (tempC <= TEMP_SEUIL_EXTINCTION) {
                commanderActuateurs(LOW);
            }
            envoyerEtatWebSocket();
        }
    }
}