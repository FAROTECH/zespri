# ANDROMEDA Firmware

Firmware per la piattaforma **ANDROMEDA @ ZESPRI**, basata su **ESP32**, dedicata all’acquisizione di dati ambientali e di irrigazione e alla loro trasmissione tramite **LoRaWAN**.

La baseline corrente integra:

- acquisizione temperatura e umidità via sensore **SHT3x** su I2C;
- supporto per sensore di umidità del terreno basato su **Adafruit Seesaw**;
- conteggio impulsi da contatore acqua **ZENNER** tramite **PCF8574**;
- costruzione di un payload binario applicativo versionato;
- radio **SX1262 / E22-900M30S**;
- stack **LoRaWAN OTAA / EU868**;
- persistenza del provisioning e della sessione LoRaWAN;
- uplink periodici e uplink manuali da console;
- integrazione end-to-end con **The Things Stack / The Things Network** e **Datacake**.

---

## 1. Stato della baseline

La baseline corrente è stata validata end-to-end lungo la catena:

```text
Sensori / ingressi
      ↓
ANDROMEDA / ESP32
      ↓
Payload v4
      ↓
SX1262 / LoRaWAN
      ↓
Laird Sentrius RG186
      ↓
The Things Stack (EU1)
      ↓
Webhook
      ↓
Datacake
```

Sono stati verificati con successo:

- inizializzazione stabile del bus I2C;
- acquisizione SHT3x;
- gestione corretta dei sensori opzionali assenti;
- gestione PCF8574;
- generazione Payload v4;
- inizializzazione SX1262;
- join LoRaWAN OTAA;
- uplink manuale;
- uplink ricevuto e decodificato su The Things Stack;
- forwarding TTS → Datacake;
- decoder Datacake;
- persistenza configurazione LoRaWAN in NVS;
- persistenza completa della sessione LoRaWAN anche dopo reset e power cycle.


## 2. Piattaforma hardware

### MCU

- **ESP32**
- framework Arduino tramite PlatformIO

### Bus I2C

Configurazione:

```text
SDA       GPIO21
SCL       GPIO22
Clock     100 kHz
Timeout   50 ms
```

Dispositivi previsti:

| Dispositivo | Indirizzo | Funzione |
|---|---:|---|
| PCF8574 | `0x20` | ingresso digitale per contatore acqua |
| SHT3x | `0x44` | temperatura e umidità relativa |
| Adafruit Seesaw moisture | `0x36` | misura umidità terreno |

Il sensore moisture è opzionale: se `0x36` non è presente sul bus, il firmware non forza l’inizializzazione della libreria Seesaw e marca la misura come `INVALID`.

### Radio LoRa

Modulo:

- **SX1262 / E22-900M30S**

Pinmap:

| Segnale | GPIO |
|---|---:|
| NSS | 5 |
| SCK | 18 |
| MISO | 19 |
| MOSI | 23 |
| RESET | 14 |
| BUSY | 33 |
| DIO1 | 25 |
| RXEN | non utilizzato / non validato |

## 3. Struttura firmware

Il firmware è organizzato in moduli indipendenti.

### `src/main.cpp`

Responsabilità principali:

- bootstrap del sistema;
- inizializzazione hardware;
- inizializzazione dei servizi;
- scheduler runtime non bloccante;
- console seriale;
- stato diagnostico;
- gestione uplink periodici e manuali.

### `env_service.{h,cpp}`

Gestisce:

- SHT3x;
- temperatura;
- umidità relativa;
- sensore moisture Seesaw;
- normalizzazione moisture;
- stato `DRY / MOIST / WET / INVALID`.

### `pulse_counter.{h,cpp}`

Gestisce:

- PCF8574;
- ingresso del contatore acqua;
- debounce;
- conteggio impulsi;
- conversione impulsi → litri.

### `payload_builder.{h,cpp}`

Gestisce:

- serializzazione del Payload v4;
- encoding big-endian;
- flag di validità/presenza;
- sentinel dei valori non disponibili;
- conversione payload → stringa HEX per diagnostica.

### `lora_service.{h,cpp}`

Gestisce:

- SPI;
- SX1262;
- RadioLib;
- LoRaWAN OTAA;
- join;
- uplink;
- restore sessione;
- persistenza completa della sessione in NVS.

### `lorawan_provisioning.{h,cpp}`

Gestisce:

- configurazione LoRaWAN runtime;
- salvataggio e caricamento da NVS;
- factory defaults;
- DevEUI / JoinEUI / AppKey;
- periodo uplink;
- FPort;
- confirmed/unconfirmed;
- abilitazione TX.


## 4. Logica di avvio

All’avvio il firmware:

1. inizializza la seriale a `115200`;
2. inizializza I2C su GPIO21/GPIO22;
3. inizializza il PCF8574;
4. inizializza i sensori ambientali;
5. esegue una prima acquisizione immediata;
6. carica il provisioning LoRaWAN da NVS;
7. usa i factory defaults solo se non esiste un provisioning valido;
8. inizializza SX1262 e lo stack LoRaWAN;
9. tenta il restore della sessione LoRaWAN;
10. se non esiste una sessione valida, effettua un nuovo join OTAA;
11. entra nel loop operativo.

Un boot con sessione persistita correttamente produce, ad esempio:

```text
[LORAWAN] Provisioning loaded from NVS
LORA RADIO : OK
[LORAWAN] Joining or restoring session...
[LORAWAN] full session restored from NVS
LORA JOIN  : OK status=SESSION_RESTORED (-1117)
```

---

## 5. Scheduler runtime

Il loop principale è progettato per non bloccare l’acquisizione.

Parametri principali:

| Funzione | Periodo |
|---|---:|
| polling water counter | `5 ms` |
| acquisizione sensori ambientali | `60 s` |
| stampa stato locale | `60 s` |
| uplink LoRaWAN di default | `3600 s` / `1 ora` |

Il periodo LoRaWAN è modificabile a runtime e persistito in NVS.

### Water counter

Il PCF8574 viene interrogato ogni:

```text
5 ms
```

con debounce:

```text
20 ms
```

Il firmware mantiene il conteggio impulsi come dato primario. La conversione corrente è:

```text
liters = pulse_count / 87.0
```

ovvero:

```text
87 impulsi / litro
```

Il valore `pulse_count` è il dato autorevole trasmesso nel Payload v4. I litri utilizzati da TTS/Datacake vengono ricalcolati downstream dai decoder tramite il parametro:

```text
WATER_PULSES_PER_LITER = 87.0
```

In questo modo la calibrazione metrologica può essere corretta senza riflashare il firmware. Il valore `87.0` resta provvisorio fino alla validazione su un volume noto.

---

## 6. Moisture

Il sensore moisture previsto è un dispositivo **Adafruit Seesaw** all’indirizzo:

```text
0x36
```

La calibrazione attuale utilizza valori derivati da test preliminari e deve essere rifatta nell’installazione reale Zespri.

Il firmware produce:

- `moisture_raw`;
- `moisture_pct`;
- `moisture_state`.

Nel Payload v4 questi tre valori restano presenti per compatibilità e diagnostica, ma il dato primario/autorevole per la calibrazione applicativa è `moisture_raw`.

I decoder TTS e Datacake ricalcolano downstream percentuale e stato usando attualmente:

```text
MOISTURE_DRY_RAW              = 395
MOISTURE_WET_RAW              = 1015
MOISTURE_DRY_THRESHOLD_PCT    = 20
MOISTURE_WET_THRESHOLD_PCT    = 60
```

Questi parametri possono quindi essere modificati dopo la calibrazione sul campo senza aggiornare il firmware del device.

Stati:

```text
0 = INVALID
1 = DRY
2 = MOIST
3 = WET
```

Se il sensore non è collegato:

```text
MOIST INVALID
```

e nel payload vengono usati i sentinel previsti.

Non viene mai interpretata l’assenza del sensore come `0 %`.

---

## 7. LoRaWAN

### Configurazione

Feature switch:

```cpp
#define ENABLE_LORAWAN 1
```

Configurazione corrente:

```text
Region                EU868
Activation            OTAA
Class                 A
FPort                 1
Confirmed uplink      NO
Periodic TX default   YES
Uplink default        3600 s / 1 ora
```

Il provisioning include:

- JoinEUI;
- DevEUI;
- AppKey;
- regione;
- periodo uplink;
- FPort;
- confirmed/unconfirmed;
- TX enabled.

Le chiavi reali non sono documentate nel README.

### Factory defaults e NVS

I valori definiti in:

```text
include/lorawan_config.h
```

sono **factory defaults**.

Se è presente una configurazione valida in NVS, questa ha precedenza.

Quindi:

```text
lorawan_config.h
        ↓
factory defaults
        ↓
solo primo boot / reset provisioning
```

mentre normalmente:

```text
NVS
 ↓
configurazione runtime effettiva
```

### Persistenza della sessione

La baseline utilizza due livelli di persistenza:

1. persistenza prevista da `LoRaWAN_ESP32`;
2. copia completa del session buffer RadioLib in NVS.

Questo consente di ripristinare una sessione LoRaWAN anche dopo:

- `ESP.restart()`;
- reset hardware;
- power cycle completo.

Il comportamento validato è:

```text
[LORAWAN] full session restored from NVS
LORA JOIN : OK status=SESSION_RESTORED (-1117)
```

invece di effettuare un nuovo OTAA join ad ogni reboot.

Dopo un nuovo join viene invece riportato:

```text
NEW_SESSION (-1118)
```

---

## 8. Console seriale

Velocità:

```text
115200 baud
```

Avvio monitor PlatformIO:

```bash
pio device monitor -b 115200
```

Per vedere l’intero log di boot durante lo sviluppo è preferibile aprire prima il monitor e poi premere `RESET / EN` sul device.

### `help`

Mostra l’elenco dei comandi disponibili.

```text
help
```

### `status`

Forza una nuova acquisizione ambientale e stampa lo stato operativo completo.

```text
status
```

Esempio:

```text
===== ANDROMEDA STATUS =====
UPTIME 1694 ms
WATER IF=OK pulses=0 liters=0.000 line=HIGH lastPulseMs=0
SHT    temp=31.28 C hum=43.90 %RH
MOIST  INVALID
BATT   INVALID / NOT IMPLEMENTED
PAYLOAD v4 HEX=04050000000000000C381126FFFFFF00FFFF
LORA   ready=YES joined=YES tx=YES period=3600s status=SESSION_RESTORED (-1117)
============================
```

### `lora`

Mostra lo stato completo LoRaWAN:

```text
lora
```

Include:

- radio ready;
- stato join;
- ultimo status RadioLib;
- origine provisioning;
- versione provisioning;
- regione;
- JoinEUI;
- DevEUI;
- AppKey;
- TX enabled;
- uplink period;
- FPort;
- confirmed/unconfirmed.

> Nota: nella baseline di sviluppo il comando può mostrare anche materiale sensibile. Non usare output contenente chiavi in log pubblici o sistemi di ticketing.

### `lora join`

Tenta un join se il device non è già joined:

```text
lora join
```

### `lora send`

Invia immediatamente un singolo uplink:

```text
lora send
```

Il comando è disponibile anche se il TX periodico è disabilitato.

È il comando consigliato per test end-to-end controllati.

### `lora tx off`

Disabilita gli uplink periodici:

```text
lora tx off
```

La modifica viene salvata in NVS.

### `lora tx on`

Abilita gli uplink periodici:

```text
lora tx on
```

La modifica viene salvata in NVS.

### `lora period <sec>`

Modifica dinamicamente il periodo di uplink:

```text
lora period 3600
```

Esempio:

```text
LoRaWAN uplink period: 3600 seconds (saved to NVS)
```

Il nuovo valore resta valido dopo reboot/power cycle.

### `i2c scan`

Esegue uno scan I2C:

```text
i2c scan
```

### `reboot`

Riavvia il device:

```text
reboot
```

---

## 9. Payload applicativo v4

Il Payload v4 è un pacchetto binario compatto di **18 byte**, codificato **big-endian / MSB first**.

### Layout

| Offset | Size | Tipo | Campo | Note |
|---:|---:|---|---|---|
| 0 | 1 | `uint8` | version | `0x04` |
| 1 | 1 | `uint8` | flags | bitfield |
| 2–3 | 2 | `uint16` | uptime_min | saturato a `0xFFFF` |
| 4–7 | 4 | `uint32` | pulse_count | impulsi contatore |
| 8–9 | 2 | `int16` | temperature_centi | °C × 100 |
| 10–11 | 2 | `uint16` | humidity_centi | %RH × 100 |
| 12–13 | 2 | `uint16` | moisture_raw | valore raw |
| 14 | 1 | `uint8` | moisture_pct | 0…100 |
| 15 | 1 | `uint8` | moisture_state | enum |
| 16–17 | 2 | `uint16` | battery_mv | millivolt |

### Sentinel

| Campo | Sentinel |
|---|---|
| temperature | `-32768` |
| humidity | `0xFFFF` |
| moisture_raw | `0xFFFF` |
| moisture_pct | `0xFF` |
| moisture_state | `0 = INVALID` |
| battery_mv | `0xFFFF` |

### Flags

Byte `[1]`:

| Bit | Significato |
|---:|---|
| 0 | SHT3x present |
| 1 | moisture sensor present |
| 2 | water line state (`1 = HIGH`) |
| 3 | battery valid |
| 4 | battery low |
| 5 | battery critical |
| 6 | solar present |
| 7 | charging |

### Esempio

```text
04050000000000000C381126FFFFFF00FFFF
```

può rappresentare:

- payload version 4;
- SHT presente;
- water line HIGH;
- pulse count = 0;
- temperatura valida;
- umidità valida;
- moisture non disponibile;
- batteria non disponibile.

### Litri acqua

I litri non vengono trasmessi direttamente nel payload.

Vengono derivati downstream da:

```text
water_liters = pulse_count / 87.0
```

Questo evita di trasmettere un valore ridondante e mantiene come dato primario il conteggio impulsi.

---

## 10. The Things Stack / The Things Network

### Gateway

Gateway validato:

```text
Laird Connectivity Sentrius RG186
EU 863–870 MHz
```

Gateway EUI:

```text
B0FB15FFFFC7A6FC
```

Configurazione:

```text
Mode          Semtech UDP
Server        eu1.cloud.thethings.network
Port UP       1700
Port DOWN     1700
Region        EU
```

Frequency plan usato sul portale:

```text
Europe 863–870 MHz
SF9 for RX2 - recommended
```

### End device

Device TTS:

```text
andromeda-zespri-1
```

DevEUI:

```text
70B3D57ED0077083
```

Activation:

```text
OTAA
```

JoinEUI:

```text
0000000000000000
```

L’AppKey deve coincidere con il provisioning del device ma non viene riportata in questo README.

### Decoder TTS

Il decoder TTS interpreta integralmente Payload v4 e mantiene disponibili sia i dati primari sia quelli derivati.

Principali campi prodotti:

```text
version
uptime_min
pulse_count
water_liters

temperature_c
humidity_rh

moisture_raw
moisture_pct
moisture_state
moisture_state_label

battery_v

sht30_present
moisture_present
water_line_high
battery_valid
battery_low
battery_critical
solar_present
charging
```

La calibrazione applicativa viene eseguita downstream:

```text
water_liters  = pulse_count / WATER_PULSES_PER_LITER

moisture_raw
      ↓
MOISTURE_DRY_RAW / MOISTURE_WET_RAW
      ↓
moisture_pct
      ↓
DRY / MOIST / WET thresholds
      ↓
moisture_state
```

Il Payload v4 non deve quindi essere modificato per correggere in futuro il fattore impulsi/litro o le soglie moisture.

Il decoder è stato validato anche tramite la funzione **Simulate Uplink** di The Things Stack, verificando la corretta decodifica dei 18 byte e la successiva propagazione verso Datacake.

Quando una misura non è valida, viene restituita come `null` o esclusa dall’integrazione downstream secondo il campo interessato.

---

## 11. Integrazione Datacake

L’applicazione TTS `zespri` inoltra gli uplink a Datacake tramite webhook.

Endpoint:

```text
https://api.datacake.co/integrations/lorawan/tti
```

Configurazione TTS:

```text
Webhook format     JSON
Uplink message     enabled
Authorization      Token <DATACAKE_TOKEN>
```

Il token non deve essere inserito nel repository o nella documentazione.

### Device Datacake

Device:

```text
andromeda-zespri-1
```

Il matching avviene tramite DevEUI:

```text
70B3D57ED0077083
```

### Decoder Datacake

Per l’hardware consegnato con questa baseline, il decoder Datacake espone **12 field**:

```text
VERSION
UPTIME_MIN

PULSE_COUNT
WATER_LITERS

TEMPERATURE_C
HUMIDITY_RH

MOISTURE_RAW
MOISTURE_PCT
MOISTURE_STATE

FLAG_SHT30_PRESENT
FLAG_MOISTURE_PRESENT
FLAG_WATER_LINE_STATE
```

La scelta distingue i dati primari dai dati applicativi:

```text
PULSE_COUNT     dato primario acqua
WATER_LITERS    dato derivato per UI/storico

MOISTURE_RAW    dato primario moisture
MOISTURE_PCT    dato derivato per UI/storico
MOISTURE_STATE  dato derivato per UI
```

`WATER_LITERS`, `MOISTURE_PCT` e `MOISTURE_STATE` vengono ricalcolati dal decoder con i parametri di calibrazione definiti nello script.

Per questa revisione hardware **non vengono esportati a Datacake**:

```text
BATTERY_V
FLAG_BATTERY_VALID
FLAG_BATTERY_LOW
FLAG_BATTERY_CRITICAL
FLAG_SOLAR_PRESENT
FLAG_CHARGING
```

Questi campi restano comunque riservati nel Payload v4 e disponibili nel decoder TTS, così da poter essere abilitati su una futura revisione hardware senza modificare il formato LoRaWAN.

I campi associati a misure non valide non vengono aggiornati con valori artificiali. In particolare, l’assenza del sensore moisture non viene trasformata in `0 %`.

### Validazione end-to-end senza device

La catena applicativa è stata validata anche tramite un uplink simulato in The Things Stack:

```text
TTS Simulate Uplink
      ↓
TTS Payload Formatter
      ↓
decoded_payload
      ↓
TTS Webhook
      ↓
Datacake
      ↓
Datacake Decoder
      ↓
12 database fields
```

Un test coerente ha prodotto:

```text
VERSION                  4
UPTIME_MIN               123
PULSE_COUNT              87
WATER_LITERS             1
TEMPERATURE_C            31.5
HUMIDITY_RH              41.4
MOISTURE_RAW             755
MOISTURE_PCT             58.0645...
MOISTURE_STATE           MOIST
FLAG_SHT30_PRESENT       true
FLAG_MOISTURE_PRESENT    true
FLAG_WATER_LINE_STATE    true
```

### Datacake Free plan

La baseline utilizza Datacake Free.

Vincoli correnti rilevanti:

```text
Free devices                         5
Time-series retention               7 giorni
Datapoint limit                     500 / device / giorno
Quota reset                         00:00 UTC
```

Con la configurazione ANDROMEDA corrente:

```text
Datacake fields definiti            12
Uplink period                       60 min
Uplink / giorno                     24
Max datapoint / uplink              12
Max datapoint / giorno              288
Free-plan limit                     500
Margine massimo                     212 datapoint / giorno
Utilizzo massimo teorico            57.6 %
```

Il valore `288` rappresenta il caso peggiore in cui tutti i 12 field vengono aggiornati a ogni uplink. Quando un sensore opzionale è assente, il consumo reale è inferiore perché i relativi field di misura non vengono scritti.

Il periodo di uplink deve essere valutato insieme al numero di field persistiti: ridurre significativamente il periodo può superare il budget giornaliero del piano Free.

### Dashboard

La dashboard Datacake è stata finalizzata per la revisione hardware corrente.

La UI principale privilegia le grandezze applicative:

```text
Online status
Temperature
Humidity
Water Consumed

Soil Moisture
Soil Status

Environment Sensor
Moisture Sensor
Water Line

Historical charts / tables
```

`PULSE_COUNT`, `MOISTURE_RAW`, `VERSION` e `UPTIME_MIN` restano disponibili nel database per diagnostica e calibrazione, ma non sono grandezze primarie dell’interfaccia utente.

La telemetria batteria/solar/charging non viene mostrata nella dashboard di questa revisione.

---

## 12. Parametrizzazioni principali

### Compile-time

In `board_config.h`:

```cpp
#define ENABLE_LORAWAN   1
```

### Scheduling locale

```text
I2C clock              100000 Hz
I2C timeout            50 ms
Water poll             5 ms
Water debounce         20 ms
Sensor sample          60000 ms
Status log             60000 ms
```

### Water meter

```text
87 pulses / liter
```

### LoRaWAN factory defaults

In `lorawan_config.h`:

```text
Region                 EU868
FPort                  1
Confirmed              false
TX enabled             true
Uplink period          3600000 ms / 1 ora
```

I valori runtime persistiti in NVS hanno precedenza.

---

## 13. Dipendenze principali

Baseline validata con:

```text
PlatformIO espressif32
Arduino ESP32 framework

LoRaWAN_ESP32       1.3.0
RadioLib            7.2.0
Adafruit SHT31      2.2.2
Adafruit Seesaw     1.7.9
```

---

## 14. Build, flash e monitor

Build:

```bash
pio run -e andromeda_esp32
```

Flash:

```bash
pio run -e andromeda_esp32 -t upload
```

Monitor seriale:

```bash
pio device monitor -b 115200
```

---

## 15. Validazione corrente

### Validato

- I2C ESP32;
- SHT3x;
- PCF8574;
- gestione graceful dei sensori opzionali assenti;
- Payload v4;
- SX1262;
- LoRaWAN EU868;
- OTAA;
- uplink manuale;
- uplink periodico configurabile;
- provisioning NVS;
- session restore dopo reboot;
- session restore dopo power cycle;
- gateway Laird RG186;
- The Things Stack;
- decoder TTS;
- webhook TTS → Datacake;
- decoder Datacake;
- ricalcolo downstream `pulse_count → water_liters`;
- ricalcolo downstream `moisture_raw → moisture_pct / moisture_state`;
- simulated uplink TTS → webhook → Datacake;
- schema Datacake finale a 12 field;
- dashboard Datacake per la revisione hardware corrente.

### Da completare / validare sul campo

- collegamento e calibrazione definitiva sensore moisture;
- validazione fisica del contatore acqua ZENNER;
- verifica definitiva `87 pulses/liter`;
- verifica/calibrazione hardware della lettura batteria su una futura sessione di test;
- validazione di lungo periodo della strategia di persistenza sessione/NVS.

---

## 16. Note di sicurezza

Le credenziali LoRaWAN e i token di integrazione sono materiale sensibile.

Non devono essere riportati in:

- README;
- issue pubbliche;
- screenshot condivisi;
- log pubblici;
- repository non protetti.

In particolare:

- `AppKey`;
- session keys;
- token Datacake;
- API key TTS.

Il firmware supporta factory defaults e provisioning persistente, ma la gestione delle credenziali deve essere trattata separatamente dalla documentazione tecnica.

---

## 17. Baseline corrente

La baseline corrente rappresenta un sistema end-to-end funzionante:

```text
ANDROMEDA
  ├── acquisizione sensori
  ├── gestione ingressi
  ├── Payload v4
  ├── console diagnostica
  ├── LoRaWAN OTAA
  ├── session persistence
  └── uplink configurabile
          ↓
     Sentrius RG186
          ↓
   The Things Stack
          ↓
       Datacake
```

Il TX periodico di default è impostato a **1 ora**.

La catena applicativa TTS → Datacake e la dashboard della revisione corrente sono finalizzate. Le calibrazioni acqua/moisture vengono mantenute nei decoder downstream, così da poterle correggere senza riflashare il device.

Per test e diagnostica rimangono disponibili:

```text
lora send
lora tx on
lora tx off
lora period <sec>
lora
status
```

Le attività residue riguardano principalmente la validazione fisica sul campo dei sensori non disponibili durante l’ultima sessione di test e la calibrazione definitiva del contatore acqua e del sensore moisture.
