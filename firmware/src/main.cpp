// ESP32-DIY-GPS firmware: NEO-M9N GNSS bridged to USB and Bluetooth LE.
//
// At boot the NEO-M9N is configured (RAM layer only) for UART1 @ 115200, standard
// NMEA output at NAV_RATE_HZ and UBX accepted in both directions. After that the
// firmware is a transparent bridge:
//   GNSS -> USB CDC serial and BLE Nordic UART Service (TX notify)
//   USB CDC serial and BLE Nordic UART Service (RX write) -> GNSS
// so NMEA consumers, gpsd and u-center can all talk to the receiver directly.
// Diagnostics go to UART0 (debug header), never to the USB data stream.
#include <Arduino.h>
#include <NimBLEDevice.h>

#define GPS_RX 18
#define GPS_TX 17
#define GPS_BAUD 115200
#ifndef NAV_RATE_HZ
#define NAV_RATE_HZ 10   // NMEA at 115200 baud carries the default sentence set up to ~10 Hz
#endif

#define Log Serial0

static const char *NUS_SVC = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *NUS_RX  = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *NUS_TX  = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

NimBLECharacteristic *nusTx;
volatile bool connected = false;
volatile bool subscribed = false;
volatile uint16_t peerMtu = 23;

// ---------------- UBX (boot-time configuration only) ----------------
static void ubxSend(uint8_t cls, uint8_t id, const uint8_t *p, uint16_t n) {
  uint8_t h[6] = {0xB5, 0x62, cls, id, (uint8_t)n, (uint8_t)(n >> 8)};
  uint8_t a = 0, b = 0;
  for (int i = 2; i < 6; i++) { a += h[i]; b += a; }
  for (int i = 0; i < n; i++) { a += p[i]; b += a; }
  Serial1.write(h, 6); Serial1.write(p, n); Serial1.write(a); Serial1.write(b);
}

struct Kv { uint32_t key; uint32_t val; uint8_t size; };
static void valsetRam(const Kv *kv, int n) {
  uint8_t p[128]; int len = 0;
  p[len++] = 0; p[len++] = 0x01; p[len++] = 0; p[len++] = 0;   // version, layer=RAM
  for (int i = 0; i < n; i++) {
    memcpy(p + len, &kv[i].key, 4); len += 4;
    memcpy(p + len, &kv[i].val, kv[i].size); len += kv[i].size;
  }
  ubxSend(0x06, 0x8A, p, len);
}

uint8_t ubx[512]; int ubxLen = 0;   // MON-VER replies are ~228 bytes
int lastAck = -1;                   // 1 = ACK, 0 = NAK for CFG-VALSET
bool monVerSeen = false;

static void onUbx(uint8_t cls, uint8_t id, const uint8_t *p, uint16_t n) {
  if (cls == 0x05 && n >= 2 && p[0] == 0x06 && p[1] == 0x8A) lastAck = id;   // 05 01 ACK, 05 00 NAK
  else if (cls == 0x0A && id == 0x04) monVerSeen = true;
}

static void parseUbx() {
  while (Serial1.available()) {
    uint8_t c = Serial1.read();
    if (ubxLen == 0 && c != 0xB5) continue;
    if (ubxLen == 1 && c != 0x62) { ubxLen = 0; continue; }
    ubx[ubxLen++] = c;
    if (ubxLen >= 6) {
      uint16_t n = ubx[4] | (ubx[5] << 8);
      if (n + 8 > (int)sizeof ubx) { ubxLen = 0; continue; }
      if (ubxLen == n + 8) {
        uint8_t a = 0, b = 0;
        for (int i = 2; i < n + 6; i++) { a += ubx[i]; b += a; }
        if (a == ubx[n + 6] && b == ubx[n + 7]) onUbx(ubx[2], ubx[3], ubx + 6, n);
        ubxLen = 0;
      }
    }
  }
}

static bool waitFor(bool (*cond)(), uint32_t ms) {
  uint32_t t = millis();
  while (millis() - t < ms) { parseUbx(); if (cond()) return true; delay(1); }
  return false;
}
static bool gotMonVer() { return monVerSeen; }
static bool gotAck() { return lastAck >= 0; }

static bool probe(uint32_t baud) {
  Serial1.updateBaudRate(baud); delay(20); while (Serial1.available()) Serial1.read();
  monVerSeen = false; ubxLen = 0;
  ubxSend(0x0A, 0x04, nullptr, 0);   // poll MON-VER
  return waitFor(gotMonVer, 600);
}

static bool configureGnss() {
  Serial1.setRxBufferSize(4096);
  Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  if (!probe(GPS_BAUD)) {
    if (!probe(38400)) { Log.println("[GNSS] no reply at 115200 or 38400"); return false; }
    Kv b[] = {{0x40520001, GPS_BAUD, 4}};   // CFG-UART1-BAUDRATE
    valsetRam(b, 1); Serial1.flush(); delay(100);
    if (!probe(GPS_BAUD)) { Log.println("[GNSS] baud switch failed"); return false; }
  }
  Log.printf("[GNSS] talking at %d baud\n", GPS_BAUD);
  Kv cfg[] = {
    {0x10740001, 1, 1},                 // CFG-UART1OUTPROT-UBX
    {0x10740002, 1, 1},                 // CFG-UART1OUTPROT-NMEA
    {0x10730001, 1, 1},                 // CFG-UART1INPROT-UBX
    {0x10730002, 1, 1},                 // CFG-UART1INPROT-NMEA
    {0x209100bb, 1, 1},                 // CFG-MSGOUT-NMEA_ID_GGA_UART1 every epoch
    {0x209100ac, 1, 1},                 // CFG-MSGOUT-NMEA_ID_RMC_UART1 every epoch
    {0x209100b1, 1, 1},                 // CFG-MSGOUT-NMEA_ID_VTG_UART1 every epoch
    {0x209100c0, 1, 1},                 // CFG-MSGOUT-NMEA_ID_GSA_UART1 every epoch
    {0x209100c5, NAV_RATE_HZ, 1},       // CFG-MSGOUT-NMEA_ID_GSV_UART1 once per second
    {0x209100ca, 0, 1},                 // CFG-MSGOUT-NMEA_ID_GLL_UART1 off (duplicates GGA/RMC)
    {0x30210001, 1000 / NAV_RATE_HZ, 2},// CFG-RATE-MEAS
    {0x30210002, 1, 2},                 // CFG-RATE-NAV
  };
  lastAck = -1; valsetRam(cfg, sizeof cfg / sizeof cfg[0]);
  bool ok = waitFor(gotAck, 1000) && lastAck == 1;
  Log.printf("[GNSS] %d Hz NMEA config: %s\n", NAV_RATE_HZ, ok ? "ACK" : (lastAck == 0 ? "NAK" : "no reply"));
  return ok;
}

// ---------------- BLE ----------------
// Start the MTU exchange from the device so centrals that never request one still
// get notifications larger than 20 bytes.
static int onMtuExchanged(uint16_t, const struct ble_gatt_error *err, uint16_t mtu, void *) {
  if (err->status == 0) { peerMtu = mtu; Log.printf("[BLE] MTU %u\n", mtu); }
  else Log.printf("[BLE] MTU exchange failed: status %d\n", err->status);
  return 0;
}

class ServerCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *s, ble_gap_conn_desc *d) override {
    connected = true;
    s->updateConnParams(d->conn_handle, 6, 12, 0, 200);   // 7.5-15 ms interval
    int rc = ble_gattc_exchange_mtu(d->conn_handle, onMtuExchanged, nullptr);
    if (rc) Log.printf("[BLE] MTU exchange start failed: %d\n", rc);
    Log.println("[BLE] connected");
  }
  void onDisconnect(NimBLEServer *) override {
    connected = false; subscribed = false; peerMtu = 23;
    Log.println("[BLE] disconnected, advertising again");
    NimBLEDevice::startAdvertising();
  }
  void onMTUChange(uint16_t mtu, ble_gap_conn_desc *) override { peerMtu = mtu; Log.printf("[BLE] MTU %u\n", mtu); }
};

class TxCb : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic *, ble_gap_conn_desc *, uint16_t v) override {
    subscribed = v;
    Log.printf("[BLE] TX %s\n", v ? "subscribed" : "unsubscribed");
  }
};

class RxCb : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c) override {
    std::string v = c->getValue();
    Serial1.write((const uint8_t *)v.data(), v.size());
  }
};

static void startBle() {
  char name[32]; uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_BT);
  snprintf(name, sizeof name, "ESP32-DIY-GPS-%02X%02X", mac[4], mac[5]);
  NimBLEDevice::init(name);
  NimBLEDevice::setMTU(185);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEServer *srv = NimBLEDevice::createServer();
  srv->setCallbacks(new ServerCb());

  static TxCb txCb;
  static RxCb rxCb;
  NimBLEService *nus = srv->createService(NUS_SVC);
  nusTx = nus->createCharacteristic(NUS_TX, NIMBLE_PROPERTY::NOTIFY);
  nusTx->setCallbacks(&txCb);
  nus->createCharacteristic(NUS_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR)->setCallbacks(&rxCb);
  nus->start();

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SVC);
  adv->setScanResponse(true);
  adv->start();
  Log.printf("[BLE] advertising as %s\n", name);
}

// ---------------- bridge ----------------
uint8_t bleBuf[182]; size_t bleLen = 0; uint32_t bleLastByte = 0;

static void bleFlush() {
  if (bleLen && connected && subscribed) { nusTx->setValue(bleBuf, bleLen); nusTx->notify(); }
  bleLen = 0;
}

static void bridge() {
  uint8_t buf[256];
  size_t n = Serial1.read(buf, min<size_t>(Serial1.available(), sizeof buf));
  if (n) {
    if (Serial) Serial.write(buf, min<size_t>(n, Serial.availableForWrite()));   // drop rather than block
    size_t chunk = min<size_t>(peerMtu - 3, sizeof bleBuf);
    for (size_t i = 0; i < n; i++) {
      bleBuf[bleLen++] = buf[i];
      if (bleLen >= chunk || buf[i] == '\n') bleFlush();
    }
    bleLastByte = millis();
  } else if (bleLen && millis() - bleLastByte > 10) {
    bleFlush();   // UBX frames have no line ending
  }
  while (Serial.available()) Serial1.write(Serial.read());
}

// ---------------- main ----------------
uint32_t lastStat = 0;

void setup() {
  Log.begin(115200);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  Log.println("\n===== ESP32-DIY-GPS: GNSS bridge (USB + BLE) =====");
  configureGnss();
  startBle();
}

void loop() {
  bridge();
  if (millis() - lastStat >= 5000) {
    lastStat = millis();
    Log.printf("[STAT] usb=%s ble=%s mtu=%u\n", Serial ? "open" : "closed",
      connected ? (subscribed ? "streaming" : "connected") : "advertising", peerMtu);
  }
}
