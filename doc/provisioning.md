# LoRaWAN Provisioning via Serial Console

## Overview

Il dispositivo supporta il provisioning dei parametri LoRaWAN tramite console seriale.
La configurazione viene salvata in memoria non volatile (NVS) e applicata al boot.

---

## Configurazione

La configurazione è composta da:

* credenziali OTAA
* parametri uplink
* impostazioni di trasmissione

### Parametri disponibili

| Parametro         | Descrizione               |
| ----------------- | ------------------------- |
| `joinEUI`         | JoinEUI (AppEUI)          |
| `devEUI`          | DevEUI                    |
| `appKey`          | AppKey                    |
| `region`          | Regione LoRaWAN           |
| `uplinkPeriodMs`  | Periodo trasmissione      |
| `uplinkFPort`     | FPort uplink              |
| `uplinkConfirmed` | Uplink confermati         |
| `txEnable`        | Abilitazione trasmissione |

---

## Persistenza

La configurazione è salvata in NVS:

* caricata automaticamente al boot
* mantenuta tra riavvii
* sovrascrive i valori di default

I valori di default sono definiti in `lorawan_config.h`.

---

## Accesso alla console

Connessione seriale:

* Baudrate: **115200**
* Terminazione: newline (`\n`)

---

## Comandi

### Visualizzazione configurazione

```text
lw show
```

---

### Impostazione parametri

#### JoinEUI

```text
lw set joineui <16 hex>
```

#### DevEUI

```text
lw set deveui <16 hex>
```

#### AppKey

```text
lw set appkey <32 hex>
```

#### Regione

```text
lw set region EU868
```

#### Periodo uplink (ms)

```text
lw set period <value>
```

#### FPort

```text
lw set fport <1..223>
```

#### Uplink confirmed

```text
lw set confirmed <0|1>
```

#### Trasmissione abilitata

```text
lw set tx <0|1>
```

---

### Salvataggio configurazione

```text
lw save
```

---

### Riavvio dispositivo

```text
lw reboot
```

---

### Reset configurazione

Ripristina i valori di default:

```text
lw reset
```

---

## Flusso operativo

### Configurazione iniziale

```text
lw set joineui <value>
lw set deveui <value>
lw set appkey <value>
lw save
lw reboot
```

---

### Aggiornamento parametri

```text
lw set <param> <value>
lw save
lw reboot
```

---

## Formati

| Parametro | Formato                  |
| --------- | ------------------------ |
| JoinEUI   | 16 caratteri esadecimali |
| DevEUI    | 16 caratteri esadecimali |
| AppKey    | 32 caratteri esadecimali |

---

## Note

* Le modifiche diventano attive dopo reboot
* I parametri non salvati vengono persi
* La configurazione runtime sovrascrive i valori di default

---

## Stato attuale

* Regione supportata: EU868
* Modalità: OTAA
* Persistenza: NVS (Preferences)

---
