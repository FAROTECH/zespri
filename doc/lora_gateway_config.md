# LoRaWAN Gateway -- Parametri e Configurazione

## 1. Parametri letti da Raspberry (config esistente)

**File:** /home/pi/sx1302_hal/bin/global_conf.json

### Gateway ID

-   Valore: `1216ca01ffff3469`
-   Uso:
    -   Identificatore univoco del gateway su TTN
    -   Inserito come Gateway EUI nella console TTN

### Server Address (originale)

-   Valore iniziale: `213.32.88.61`
-   Uso:
    -   Server legacy/custom precedente (non TTN)

### Server Address (modificato)

-   Nuovo valore: `eu1.cloud.thethings.network`
-   Uso:
    -   Endpoint TTN (The Things Stack v3 -- regione EU)

### Porte UDP

-   serv_port_up: 1700
-   serv_port_down: 1700

### Keepalive

-   10 s

### Stat interval

-   30 s

### Forwarding policy

-   forward_crc_valid: true
-   forward_crc_error: false
-   forward_crc_disabled: false

### GPS

-   /dev/ttyS0 (non utilizzato)

## 2. Parametri hardware (SX1302)

-   radio_0: 867500000 Hz
-   radio_1: 868500000 Hz

## 3. Parametri rete

-   MAC: b8:27:eb:c8:74:99
-   Gateway EUI: 1216CA01FFFF3469

## 4. TTN

-   Gateway ID: andromeda-gw
-   Frequency Plan: EU868
-   Auth: Disabled

## 5. Servizio

-   lora_pkt_fwd.service

## 6. DNS Fix

/etc/resolv.conf: nameserver 8.8.8.8 nameserver 1.1.1.1

## 7. Stato

-   Connected
-   UDP
-   ACK 100%
-   RX 0
