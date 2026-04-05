// ============================================================
// ELECTROTERAPIA TENS - CON WiFiManager
// Configuración WiFi desde el celular (sin hardcodear)
// Pin de salida: GPIO23 (PWM)
// Controla: INTENSIDAD (0-255) y FRECUENCIA (1-150 Hz)
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>  // Librería para configuración WiFi fácil

// ================== PINES ==================
const int PIN_TENS = 23;        // GPIO23 - Salida PWM al optoacoplador/MOSFET
const int PIN_LED_BUILTIN = 2;  // LED integrado del ESP32

// ================== PARÁMETROS ==================
int intensidad = 0;           // 0-255 (0 = apagado, 255 = máximo)
int frecuenciaHz = 100;       // 1-150 Hz
bool terapiaActiva = false;   // Estado de la terapia

// Variables para generación de onda por frecuencia
unsigned long ultimoPulso = 0;
bool pulsoEncendido = false;

WebServer server(80);

// ================== CONFIGURACIÓN PWM ==================
void configurarPWM() {
  // Para ESP32 con core 3.0+, esta es la forma correcta
  ledcAttach(PIN_TENS, frecuenciaHz, 8);  // Pin, frecuencia base, resolución 8 bits
  ledcWrite(PIN_TENS, 0);                  // Apagar al inicio
}

void actualizarPWM(int valor) {
  ledcWrite(PIN_TENS, valor);
}

// ================== GENERACIÓN DE ONDA CON FRECUENCIA VARIABLE ==================
void generarOnda() {
  if (!terapiaActiva || intensidad == 0) {
    if (pulsoEncendido) {
      actualizarPWM(0);
      pulsoEncendido = false;
    }
    return;
  }
  
  unsigned long ahora = micros();
  unsigned long periodoUs = 1000000 / frecuenciaHz;  // Periodo total en microsegundos
  
  // El ancho del pulso es proporcional a la intensidad
  // Intensidad 255 = 50% del periodo, Intensidad 0 = 0%
  unsigned long anchoPulsoUs = (periodoUs * intensidad) / 510;  // Máximo 50% duty cycle
  
  if (!pulsoEncendido) {
    // Iniciar pulso
    actualizarPWM(intensidad);
    pulsoEncendido = true;
    ultimoPulso = ahora;
  } 
  else if ((ahora - ultimoPulso) >= anchoPulsoUs) {
    // Terminar pulso (pero el ciclo continúa)
    actualizarPWM(0);
    pulsoEncendido = false;
    ultimoPulso = ahora;
  }
}

// ================== INTERFAZ WEB ==================
String getHTML() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=yes">
  <title>TENS - Electroterapia Remota</title>
  <style>
    * { box-sizing: border-box; user-select: none; }
    body {
      font-family: 'Segoe UI', Roboto, sans-serif;
      background: linear-gradient(135deg, #0a0f1e 0%, #0a1a2a 100%);
      min-height: 100vh;
      margin: 0;
      padding: 20px;
      display: flex;
      justify-content: center;
      align-items: center;
    }
    .container {
      max-width: 550px;
      width: 100%;
      margin: auto;
    }
    .card {
      background: rgba(22, 34, 48, 0.95);
      backdrop-filter: blur(10px);
      border-radius: 32px;
      padding: 24px 20px 32px;
      margin-bottom: 20px;
      box-shadow: 0 20px 35px -10px rgba(0,0,0,0.5);
      border: 1px solid rgba(255,255,255,0.08);
    }
    h1 {
      font-size: 1.8rem;
      margin: 0 0 8px 0;
      font-weight: 600;
      background: linear-gradient(135deg, #aaffdd, #2ecc71);
      -webkit-background-clip: text;
      background-clip: text;
      color: transparent;
      text-align: center;
    }
    .subtitulo {
      text-align: center;
      color: #8aaec0;
      margin-bottom: 24px;
      font-size: 0.85rem;
    }
    .estado-panel {
      background: #07121c;
      border-radius: 48px;
      padding: 12px 20px;
      text-align: center;
      margin-bottom: 28px;
      border: 1px solid #2a4a6e;
    }
    .estado-led {
      display: inline-block;
      width: 14px;
      height: 14px;
      border-radius: 14px;
      background: #555;
      margin-right: 8px;
    }
    .estado-texto {
      font-weight: bold;
      letter-spacing: 1px;
    }
    .activo .estado-led { background: #2ecc71; box-shadow: 0 0 8px #2ecc71; }
    .inactivo .estado-led { background: #e74c3c; }
    
    .param-card {
      background: #0a1824;
      border-radius: 28px;
      padding: 16px 20px;
      margin-bottom: 20px;
    }
    .param-label {
      display: flex;
      justify-content: space-between;
      font-weight: 600;
      margin-bottom: 12px;
      color: #c0e0ff;
    }
    input[type=range] {
      width: 100%;
      height: 6px;
      -webkit-appearance: none;
      background: #2c4c6c;
      border-radius: 10px;
      outline: none;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 24px;
      height: 24px;
      background: #2ecc71;
      border-radius: 50%;
      cursor: pointer;
      box-shadow: 0 0 8px #2ecc71;
      border: none;
    }
    .valor-actual {
      font-size: 1.6rem;
      font-weight: bold;
      color: #f5f9ff;
      background: #071723;
      display: inline-block;
      padding: 4px 16px;
      border-radius: 40px;
      margin-top: 10px;
    }
    .botonera {
      display: flex;
      gap: 15px;
      justify-content: center;
      margin: 25px 0 10px;
    }
    button {
      border: none;
      padding: 14px 28px;
      border-radius: 60px;
      font-weight: bold;
      font-size: 1rem;
      background: #1f3e54;
      color: white;
      cursor: pointer;
      transition: 0.2s;
      flex: 1;
    }
    .btn-iniciar { background: #2ecc71; color: #000; }
    .btn-detener { background: #e74c3c; }
    .btn-emergencia { background: #c0392b; border: 1px solid #ff8866; }
    footer {
      text-align: center;
      font-size: 0.7rem;
      color: #2c5a7a;
      margin-top: 15px;
    }
    .badge {
      background: #0e2a3a;
      border-radius: 24px;
      padding: 6px 12px;
      font-size: 0.75rem;
      display: inline-block;
    }
    .ip-info {
      text-align: center;
      font-size: 0.7rem;
      color: #5a9ec2;
      margin-top: 10px;
    }
  </style>
</head>
<body>
<div class="container">
  <div class="card">
    <h1>⚡ TENS · Teleterapia</h1>
    <div style="text-align:center"><span class="badge">📡 Control WiFi remoto</span></div>
    
    <div id="estadoWidget" class="estado-panel inactivo">
      <span class="estado-led"></span>
      <span class="estado-texto" id="estadoTexto">SISTEMA INACTIVO</span>
    </div>
    
    <div class="param-card">
      <div class="param-label">
        <span>🎛️ INTENSIDAD</span>
        <span id="intensidadValor">0</span>
      </div>
      <input type="range" id="intensidadSlider" min="0" max="255" value="0">
      <div style="font-size:12px; opacity:0.7">0 = apagado &nbsp;&nbsp; 255 = máximo</div>
    </div>
    
    <div class="param-card">
      <div class="param-label">
        <span>🌀 FRECUENCIA (Hz)</span>
        <span id="frecuenciaValor">100</span>
      </div>
      <input type="range" id="frecuenciaSlider" min="1" max="150" value="100">
      <div style="font-size:12px; opacity:0.7">Rango terapéutico 1~150 Hz</div>
    </div>
    
    <div class="botonera">
      <button class="btn-iniciar" id="btnIniciar">▶ INICIAR</button>
      <button class="btn-detener" id="btnDetener">⏹ DETENER</button>
    </div>
    <button class="btn-emergencia" id="btnEmergencia">🛑 PARADA DE EMERGENCIA</button>
    
    <div style="margin-top: 24px; background:#07121c; border-radius: 28px; padding: 12px; text-align: center;">
      <span>🔋 Estado: </span><span id="estadoTerapia" style="font-weight:bold; color:#ffaa66">SIN TERAPIA</span>
      <div id="feedback" style="font-size:12px; margin-top: 8px;">✅ Conectado al ESP32</div>
    </div>
    <div class="ip-info" id="ipDisplay"></div>
    <footer>Electroterapia TENS · Control remoto por WiFi</footer>
  </div>
</div>

<script>
  const intensidadSlider = document.getElementById('intensidadSlider');
  const intensidadValor = document.getElementById('intensidadValor');
  const frecuenciaSlider = document.getElementById('frecuenciaSlider');
  const frecuenciaValor = document.getElementById('frecuenciaValor');
  const estadoTerapiaSpan = document.getElementById('estadoTerapia');
  const feedbackDiv = document.getElementById('feedback');
  const estadoWidget = document.getElementById('estadoWidget');
  const estadoTexto = document.getElementById('estadoTexto');

  let terapiaEncendida = false;

  intensidadSlider.oninput = () => {
    intensidadValor.innerText = intensidadSlider.value;
    if (terapiaEncendida) {
      enviarIntensidad(intensidadSlider.value);
    }
  };
  
  frecuenciaSlider.oninput = () => {
    frecuenciaValor.innerText = frecuenciaSlider.value;
    if (terapiaEncendida) {
      enviarFrecuencia(frecuenciaSlider.value);
    }
  };

  async function enviarIntensidad(val) {
    try {
      await fetch('/set_intensidad?v=' + val);
    } catch(e) { console.log(e); }
  }
  
  async function enviarFrecuencia(val) {
    try {
      await fetch('/set_frecuencia?hz=' + val);
    } catch(e) { console.log(e); }
  }

  async function iniciarTerapia() {
    const intensidad = intensidadSlider.value;
    const frecuencia = frecuenciaSlider.value;
    if (parseInt(intensidad) === 0) {
      feedbackDiv.innerHTML = "⚠️ La intensidad está en 0, sube el nivel antes de iniciar.";
      return;
    }
    const resp = await fetch('/iniciar?int=' + intensidad + '&freq=' + frecuencia);
    if (resp.ok) {
      terapiaEncendida = true;
      estadoTerapiaSpan.innerText = "▶ ACTIVA";
      estadoWidget.className = "estado-panel activo";
      estadoTexto.innerText = "TERAPIA ACTIVA";
      feedbackDiv.innerHTML = "🟢 Terapia en curso";
    }
  }

  async function detenerTerapia() {
    await fetch('/detener');
    terapiaEncendida = false;
    estadoTerapiaSpan.innerText = "⏹ INACTIVA";
    estadoWidget.className = "estado-panel inactivo";
    estadoTexto.innerText = "SISTEMA INACTIVO";
    feedbackDiv.innerHTML = "⚫ Terapia detenida";
    await actualizarEstadoCompleto();
  }

  async function emergenciaTotal() {
    await fetch('/emergencia');
    terapiaEncendida = false;
    intensidadSlider.value = 0;
    intensidadValor.innerText = "0";
    estadoTerapiaSpan.innerText = "⛔ EMERGENCIA";
    estadoWidget.className = "estado-panel inactivo";
    estadoTexto.innerText = "EMERGENCIA ⚠️";
    feedbackDiv.innerHTML = "🛑 PARADA DE EMERGENCIA ACTIVADA";
    setTimeout(() => {
      if(!terapiaEncendida) estadoTexto.innerText = "SISTEMA INACTIVO";
    }, 2000);
  }

  async function actualizarEstadoCompleto() {
    try {
      const res = await fetch('/estado_completo');
      const data = await res.json();
      intensidadSlider.value = data.intensidad;
      intensidadValor.innerText = data.intensidad;
      frecuenciaSlider.value = data.frecuencia;
      frecuenciaValor.innerText = data.frecuencia;
      terapiaEncendida = data.activa;
      if(terapiaEncendida) {
        estadoTerapiaSpan.innerText = "▶ ACTIVA";
        estadoWidget.className = "estado-panel activo";
        estadoTexto.innerText = "TERAPIA ACTIVA";
      } else {
        estadoTerapiaSpan.innerText = "⏹ INACTIVA";
        estadoWidget.className = "estado-panel inactivo";
        estadoTexto.innerText = "SISTEMA INACTIVO";
      }
    } catch(e) { console.log("error sync", e); }
  }

  // Mostrar IP (opcional)
  fetch('/ip').then(r=>r.text()).then(ip => {
    document.getElementById('ipDisplay').innerHTML = '🌐 ' + ip;
  });

  document.getElementById('btnIniciar').onclick = iniciarTerapia;
  document.getElementById('btnDetener').onclick = detenerTerapia;
  document.getElementById('btnEmergencia').onclick = emergenciaTotal;
  
  setInterval(actualizarEstadoCompleto, 1500);
  actualizarEstadoCompleto();
</script>
</body>
</html>
)rawliteral";
  return html;
}

// ================== CONFIGURACIÓN DE SERVIDOR WEB ==================
void setupServer() {
  server.on("/", []() {
    server.send(200, "text/html", getHTML());
  });
  
  server.on("/ip", []() {
    server.send(200, "text/plain", WiFi.localIP().toString());
  });
  
  // Endpoint para cambiar intensidad en tiempo real
  server.on("/set_intensidad", []() {
    if (server.hasArg("v")) {
      intensidad = server.arg("v").toInt();
      if (intensidad < 0) intensidad = 0;
      if (intensidad > 255) intensidad = 255;
      Serial.print("Intensidad: ");
      Serial.println(intensidad);
      server.send(200, "text/plain", "OK");
    }
  });
  
  // Endpoint para cambiar frecuencia en tiempo real
  server.on("/set_frecuencia", []() {
    if (server.hasArg("hz")) {
      frecuenciaHz = server.arg("hz").toInt();
      if (frecuenciaHz < 1) frecuenciaHz = 1;
      if (frecuenciaHz > 150) frecuenciaHz = 150;
      Serial.print("Frecuencia: ");
      Serial.println(frecuenciaHz);
      server.send(200, "text/plain", "OK");
    }
  });
  
  // Endpoint para iniciar terapia
  server.on("/iniciar", []() {
    if (server.hasArg("int")) {
      intensidad = server.arg("int").toInt();
      if (intensidad < 0) intensidad = 0;
      if (intensidad > 255) intensidad = 255;
    }
    if (server.hasArg("freq")) {
      frecuenciaHz = server.arg("freq").toInt();
      if (frecuenciaHz < 1) frecuenciaHz = 1;
      if (frecuenciaHz > 150) frecuenciaHz = 150;
    }
    terapiaActiva = true;
    Serial.println("=== TERAPIA INICIADA ===");
    Serial.print("Intensidad: ");
    Serial.println(intensidad);
    Serial.print("Frecuencia: ");
    Serial.println(frecuenciaHz);
    server.send(200, "text/plain", "OK");
  });
  
  // Endpoint para detener terapia
  server.on("/detener", []() {
    terapiaActiva = false;
    actualizarPWM(0);
    pulsoEncendido = false;
    Serial.println("=== TERAPIA DETENIDA ===");
    server.send(200, "text/plain", "OK");
  });
  
  // Endpoint para emergencia (detiene todo y pone intensidad 0)
  server.on("/emergencia", []() {
    terapiaActiva = false;
    intensidad = 0;
    actualizarPWM(0);
    pulsoEncendido = false;
    Serial.println("!!! PARADA DE EMERGENCIA !!!");
    server.send(200, "text/plain", "OK");
  });
  
  // Endpoint para obtener estado completo
  server.on("/estado_completo", []() {
    String json = "{";
    json += "\"activa\":" + String(terapiaActiva ? "true" : "false") + ",";
    json += "\"intensidad\":" + String(intensidad) + ",";
    json += "\"frecuencia\":" + String(frecuenciaHz);
    json += "}";
    server.send(200, "application/json", json);
  });
  
  server.begin();
  Serial.println("✅ Servidor web iniciado");
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=================================");
  Serial.println("⚡ ELECTROTERAPIA TENS - WiFiManager");
  Serial.println("=================================");
  
  // Configurar pines
  pinMode(PIN_LED_BUILTIN, OUTPUT);
  digitalWrite(PIN_LED_BUILTIN, LOW);
  
  // Configurar PWM en GPIO23
  ledcAttach(PIN_TENS, frecuenciaHz, 8);
  ledcWrite(PIN_TENS, 0);
  
  // ========== WIFIMANAGER - Configuración WiFi sin hardcodear ==========
  WiFiManager wifiManager;
  
  // Opcional: Descomenta esta línea si quieres borrar configuraciones previas
  // wifiManager.resetSettings();
  
  // Configurar timeout (si no se configura en 3 minutos, el ESP32 se reinicia)
  wifiManager.setConfigPortalTimeout(180);
  
  // Intentar conectar a WiFi guardado, si no, crear punto de acceso
  if (!wifiManager.autoConnect("TENS_Config_WiFi")) {
    Serial.println("❌ Error de conexión. Reiniciando...");
    ESP.restart();
  }
  
  // Si llegamos aquí, estamos conectados
  Serial.println("✅ Conectado a WiFi!");
  Serial.print("📡 IP del ESP32: ");
  Serial.println(WiFi.localIP());
  
  // Parpadeo para indicar que está listo
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(PIN_LED_BUILTIN, LOW);
    delay(200);
  }
  
  // Iniciar servidor web
  setupServer();
  
  Serial.println("=================================");
  Serial.print("🌐 Abre en tu navegador: http://");
  Serial.println(WiFi.localIP());
  Serial.println("=================================");
}

// ================== LOOP ==================
void loop() {
  server.handleClient();
  
  // Generar onda según frecuencia e intensidad
  generarOnda();
  
  // LED indicador: parpadea cuando la terapia está activa
  static unsigned long lastBlink = 0;
  if (terapiaActiva && intensidad > 0) {
    if (millis() - lastBlink > 500) {
      lastBlink = millis();
      digitalWrite(PIN_LED_BUILTIN, !digitalRead(PIN_LED_BUILTIN));
    }
  } else {
    digitalWrite(PIN_LED_BUILTIN, LOW);
  }
  
  delay(5);
}
