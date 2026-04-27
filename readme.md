# 🩺 Care Plus — Health Tracker IoT

> **Sprint 2 — Prototipagem Digital e Arquitetura de Sistemas**  
> Dispositivo vestível baseado em ESP32 que captura métricas de saúde e envia via MQTT para Node-RED e FIWARE

## 📋 Visão Geral do Projeto

O **Care Plus Health Tracker** é um dispositivo vestível (smartwatch/health tracker) desenvolvido para a solução de saúde da Care Plus. O projeto utiliza um microcontrolador **ESP32** simulado no **Wokwi** para capturar métricas de saúde em tempo real e transmiti-las via protocolo **MQTT com TLS** para um broker **HiveMQ Cloud**, onde os dados são consumidos pelo **Node-RED** para visualização e alertas.

### Objetivo

Criar a camada de hardware e conectividade para monitoramento contínuo de pacientes, permitindo que profissionais de saúde acompanhem métricas vitais remotamente por meio de uma arquitetura IoT escalável e segura.

---

## 🏗️ Arquitetura da Solução

A solução é organizada em **4 camadas** independentes e interoperáveis:

```
┌─────────────────────────────────────────────────────────────────────┐
│  CAMADA EDGE — Dispositivo Vestível (ESP32 + Sensores Simulados)    │
│                                                                     │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌─────────────┐  │
│  │  MAX30102  │  │  DS18B20   │  │  MPU-6050  │  │ SSD1306     │  │
│  │  HR + SpO2 │→ │  Temp.     │→ │  Passos    │→ │ OLED 128x64 │  │
│  └────────────┘  └────────────┘  └────────────┘  └─────────────┘  │
│                          ↓  ESP32 DevKit (WiFi · C/C++)             │
└──────────────────────────┬──────────────────────────────────────────┘
                           │ JSON payload · TCP/IP
                           ▼
┌─────────────────────────────────────────────────────────────────────┐
│  CAMADA DE CONECTIVIDADE — MQTT com TLS                             │
│                                                                     │
│      HiveMQ Cloud · broker.hivemq.cloud · porta 8883 (TLS)         │
│      Autenticação: usuário + senha                                  │
│                                                                     │
│  Tópicos:                                                           │
│    careplus/{device_id}/metrics   → métricas consolidadas           │
│    careplus/{device_id}/status    → status do dispositivo           │
│    careplus/{device_id}/alert     → alertas de saúde (retain)       │
└──────────────────────────┬──────────────────────────────────────────┘
                           │ Subscribe
                           ▼
┌─────────────────────────────────────────────────────────────────────┐
│  CAMADA BACKEND — Processamento e Persistência                      │
│                                                                     │
│  ┌──────────────┐   ┌──────────────────┐   ┌────────────────────┐  │
│  │  Node-RED    │   │  FIWARE Orion    │   │  MongoDB           │  │
│  │  Flows +     │   │  Context Broker  │   │  Histórico de      │  │
│  │  Alertas     │   │  NGSI-v2         │   │  métricas          │  │
│  └──────────────┘   └──────────────────┘   └────────────────────┘  │
└──────────────────────────┬──────────────────────────────────────────┘
                           │ REST · WebSocket
                           ▼
┌─────────────────────────────────────────────────────────────────────┐
│  CAMADA APPLICATION — Visualização e Monitoramento                  │
│                                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────┐  ┌───────────┐  │
│  │  Node-RED UI │  │  App Mobile  │  │  Alertas │  │  Grafana  │  │
│  │  Dashboard   │  │  React Native│  │ Telegram │  │  Séries   │  │
│  │  /ui         │  │              │  │ Email    │  │ temporais │  │
│  └──────────────┘  └──────────────┘  └──────────┘  └───────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 📡 Métricas Monitoradas

| Sensor (Simulado) | Métrica | Faixa Normal | Unidade |
|---|---|---|---|
| MAX30102 | Frequência cardíaca (Batimentos) | 60 – 100 | bpm |
| MAX30102 | Saturação de oxigênio (SpO2) | 95 – 100 | % |
| DS18B20 | Temperatura corporal | 36,0 – 37,2 | °C |
| MPU-6050 | Passos acumulados | 0 – 10.000/dia | steps |

---

## 📦 Payload MQTT

Todas as métricas são enviadas consolidadas no tópico `careplus/{device_id}/metrics` com o seguinte formato JSON:

```json
{
  "Metrica": {
    "Batimentos cardiacos": "77 bpm",
    "Saturacao":            "97.1 %",
    "Temperatura":          "36.7 C",
    "Passos":               42
  },
  "device_id": "careplus-device-001",
  "battery":   "100 %",
  "alerta":    "NAO"
}
```

### Tópicos MQTT

| Tópico | Conteúdo | QoS | Retain |
|---|---|---|---|
| `careplus/{id}/metrics` | Métricas de saúde consolidadas | 0 | false |
| `careplus/{id}/status` | Status do dispositivo online/offline | 0 | true |
| `careplus/{id}/alert` | Alertas críticos de saúde | 1 | true |

---

## ⚠️ Sistema de Alertas

O firmware monitora continuamente as métricas e publica automaticamente no tópico `/alert` quando:

| Condição | Limite | Mensagem |
|---|---|---|
| Taquicardia | HR > 100 bpm | `"Taquicardia detectada!"` |
| Bradicardia | HR < 50 bpm | `"Bradicardia detectada!"` |
| Hipóxia | SpO2 < 95% | `"SpO2 baixo!"` |
| Febre | Temp > 37,2°C | `"Possivel febre"` |


## 🛠️ Tecnologias Utilizadas

| Camada | Tecnologia | Função |
|---|---|---|
| Edge | ESP32 DevKit V1 | Microcontrolador principal |
| Edge | SSD1306 OLED 128×64 | Display de métricas |
| Edge | Wokwi | Simulação digital do hardware |
| Conectividade | MQTT v3.1.1 | Protocolo de mensagens IoT |
| Conectividade | HiveMQ Cloud (Free) | Broker MQTT com TLS |
| Conectividade | PubSubClient 2.8 | Biblioteca MQTT para ESP32 |
| Backend | Node-RED v3+ | Processamento e flows |

---

## 🔌 Circuito do Dispositivo (Wokwi)

```
ESP32 DevKit V1
├── GPIO 21 (SDA) ──── SSD1306 OLED (SDA)
├── GPIO 22 (SCL) ──── SSD1306 OLED (SCL)
├── 3V3  ──────────── OLED VCC
├── GND  ──────────── OLED GND
├── GPIO  2 ──[330Ω]── LED Vermelho  (Indicador HR)
├── GPIO  4 ──[330Ω]── LED Azul      (Indicador SpO2)
├── GPIO  5 ──[330Ω]── LED Verde     (Indicador Temp)
└── GPIO 18 ──[330Ω]── LED Amarelo   (Indicador MQTT)
```

> Os sensores MAX30102, DS18B20 e MPU-6050 são **simulados por software** no firmware com variações realistas. Em uma implementação física, seriam conectados via I2C e OneWire.

---

## 📊 Display OLED — Telas Rotativas

O dispositivo apresenta 4 telas que alternam a cada 2 segundos:

| Tela | Conteúdo |
|---|---|
| 1 | Frequência Cardíaca (bpm) com ícone de coração |
| 2 | Saturação O2 — SpO2 (%) |
| 3 | Temperatura Corporal (°C) |
| 4 | Passos · Status MQTT · Bateria |

O ponto sólido (●) no canto superior direito indica conexão MQTT ativa.

## 👥 Participantes
| Nome                           | RM       |
| ------------------------------ | -------- |
| **Eduarda da Silva Brito**     | RM567347 |
| **Gustavo Castilho Gonçalves** | RM566970 |
| **Gustavo Moretim Canzi**      | RM567683 |
| **Lucca Ghiraldi Urso**        | RM556739 |
