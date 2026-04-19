# ANDROMEDA Firmware

Firmware ESP32 per la board **ANDROMEDA**, con architettura modulare per acquisizione sensori ambientali e trasmissione radio tramite **SX1262**.

## Stato baseline

Questa baseline è la prima versione stabile con:

- inizializzazione corretta del bus I2C
- lettura stabile dei sensori ambientali
- conteggio impulsi del contatore acqua via PCF8574
- costruzione payload binario applicativo
- inizializzazione radio **SX1262**
- **trasmissione LoRa raw funzionante**

### Componenti attivi

- **PulseCounterPcf8574**
  - acquisizione impulsi da contatore acqua ZENNER
  - conversione impulsi → litri

- **EnvService**
  - sensore **SHT30** per temperatura e umidità
  - sensore moisture basato su **Adafruit Seesaw**

- **PayloadBuilder**
  - costruzione payload binario compatto

- **LoraService**
  - gestione radio **SX1262**
  - trasmissione asincrona con callback (`startTransmit + packetSent callback + finishTransmit`)

### Componenti attualmente disabilitati / non inclusi nel flusso

- **GPS**
  - escluso dalla baseline attuale
  - `ENABLE_GPS = false`

- **LoRaWAN**
  - non ancora integrato in questa baseline
  - attualmente la radio lavora in **LoRa raw TX**

---

## Architettura firmware

Il firmware è organizzato in moduli separati:

- `main.cpp`
  - bootstrap sistema
  - inizializzazione servizi
  - loop principale
  - logging stato
  - invio periodico payload

- `board_config.h`
  - configurazione pin e parametri globali

- `env_service.{h,cpp}`
  - gestione sensori ambientali I2C

- `pulse_counter.{h,cpp}`
  - gestione PCF8574 e conteggio impulsi acqua

- `payload_builder.{h,cpp}`
  - serializzazione payload binario

- `lora_service.{h,cpp}`
  - gestione radio SX1262

- `gps_service.{h,cpp}`
  - modulo presente ma attualmente non usato nella baseline

---

## Hardware validato in questa baseline

### MCU
- ESP32

### Bus I2C
- `SDA = GPIO21`
- `SCL = GPIO22`
- clock `100 kHz`

### Dispositivi I2C rilevati
- `0x20` → PCF8574
- `0x44` → SHT30
- `0x36` → Seesaw moisture sensor

### Radio LoRa
Modulo basato su **SX1262**

Pinmap validata per la baseline attuale:

- `NSS  = GPIO5`
- `SCK  = GPIO18`
- `MISO = GPIO19`
- `MOSI = GPIO23`
- `RST  = GPIO14`
- `BUSY = GPIO33`
- `DIO1 = GPIO25`

Nota importante:
la configurazione radio stabile attuale usa **trasmissione asincrona con callback**.  
La modalità bloccante `transmit()` è stata verificata come non affidabile su questa baseline.

---

## Funzionamento runtime

All’avvio il firmware:

1. inizializza la seriale
2. inizializza il bus I2C
3. esegue lo scan I2C
4. inizializza:
   - contatore acqua su PCF8574
   - sensori ambientali
   - radio SX1262
5. entra nel loop principale

Nel loop:

- aggiorna il contatore acqua
- acquisisce temperatura, umidità e moisture raw
- costruisce il payload binario
- stampa lo stato su seriale
- trasmette periodicamente il payload via **LoRa raw**

---

## Payload

Il payload trasmesso contiene attualmente:

- uptime
- water pulses
- liters
- temperatura
- umidità
- moisture raw

Il formato è binario compatto ed è pensato per essere riutilizzato nel successivo passaggio a **LoRaWAN uplink**.

---

## Stato di validazione

### Validato
- I2C stabile
- nessun lock-up del bus
- SHT30 con valori coerenti
- Seesaw moisture con letture corrette
- conteggio impulsi acqua via PCF8574
- costruzione payload
- init radio SX1262
- TX radio raw riuscita

### Non ancora validato in questa baseline
- ricezione lato gateway
- stack LoRaWAN
- join OTAA / ABP
- backend MQTT / dashboard
- calibrazione moisture
- verifica metrologica definitiva impulsi → litri

---

## Note tecniche

### Moisture
Il valore moisture è al momento **raw** e non calibrato.  
Non va interpretato come misura assoluta senza una successiva fase di calibrazione su:

- aria
- terreno secco
- terreno umido
- acqua

### Water counter
La conversione attuale usa:

`liters = pulses / 87.0`

Da verificare con test fisico su volume noto.

### I2C warnings
Sono possibili warning del tipo:

`Wire.begin(): Bus already started in Master Mode`

Questo indica che uno o più moduli stanno reinizializzando `Wire` dopo il `setup()`.  
Non blocca la baseline, ma andrà ripulito in una fase successiva.

---

## Roadmap immediata

Prossimi step previsti:

1. conferma ricezione lato gateway
2. integrazione stack **LoRaWAN**
3. configurazione gateway / network server
4. primo uplink end-to-end
5. visualizzazione dati su dashboard leggera

---

## Obiettivo della baseline

Questa baseline rappresenta il primo punto fermo affidabile del progetto:

- firmware stabile
- sensori integrati
- payload pronto
- radio SX1262 funzionante in trasmissione

È la base di partenza per il passaggio a **LoRaWAN + backend + visualizzazione dati**.