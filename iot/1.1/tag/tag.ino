#include "dw3000.h"
#include <WiFi.h>
#include <WebSocketsServer.h>

// ---------------- Wi-Fi settings ----------------
// true  = the tag makes its own network (no router needed). Join "UWB-TAG" from your PC.
// false = the tag joins your existing network (set STA_SSID / STA_PASS below).
#define USE_AP true
const char *AP_SSID  = "UWB-TAG";
const char *AP_PASS  = "uwb12345";        // at least 8 characters
const char *STA_SSID = "SAKKY-GUEST";
const char *STA_PASS = "";

WebSocketsServer ws(81);                  // viewer connects to ws://<tag-ip>:81

// ---------------- UWB settings ----------------
#define NUM_ANCHORS 4

#define PIN_RST 27
#define PIN_IRQ 34
#define PIN_SS  4

#define RNG_DELAY_MS 10

#define TX_ANT_DLY 16385                  // use your calibrated values here
#define RX_ANT_DLY 16385

#define ALL_MSG_COMMON_LEN 10
#define ALL_MSG_SN_IDX 2
#define ALL_MSG_DEST_IDX 7
#define RESP_MSG_POLL_RX_TS_IDX 10
#define RESP_MSG_RESP_TX_TS_IDX 14
#define RESP_MSG_TS_LEN 4

#define POLL_TX_TO_RESP_RX_DLY_UUS 0
#define RESP_RX_TIMEOUT_UUS 2500

static dwt_config_t config = {
  5, DWT_PLEN_128, DWT_PAC8, 9, 9, 1, DWT_BR_6M8, DWT_PHRMODE_STD,
  DWT_PHRRATE_STD, (129 + 8 - 8), DWT_STS_MODE_OFF, DWT_STS_LEN_64, DWT_PDOA_M0
};
extern dwt_txconfig_t txconfig_options;

static uint8_t tx_poll_msg[] = {0x41, 0x8C, 0, 0xCA, 0xDE, 'V', 'E', 0, 'W', 0xE0, 0, 0};
static uint8_t rx_resp_msg[] = {0x41, 0x8C, 0, 0xCA, 0xDE, 'V', 'E', 0, 'W', 0xE1,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t frame_seq_nb = 0;
static uint8_t rx_buffer[24];
static uint32_t status_reg = 0;

// Send one line to USB and to every connected Wi-Fi viewer
void sendLine(const char *s) {
  Serial.println(s);
  ws.broadcastTXT(s);
}

void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_CONNECTED) Serial.println("Viewer connected over Wi-Fi");
  if (type == WStype_DISCONNECTED) Serial.println("Viewer disconnected");
}

// Returns true and fills 'distance' (metres) on success.
bool rangeAnchor(uint8_t anchorId, double &distance) {
  tx_poll_msg[ALL_MSG_DEST_IDX] = anchorId;
  rx_resp_msg[ALL_MSG_DEST_IDX] = anchorId;
  tx_poll_msg[ALL_MSG_SN_IDX] = frame_seq_nb;

  dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS_BIT_MASK);
  dwt_writetxdata(sizeof(tx_poll_msg), tx_poll_msg, 0);
  dwt_writetxfctrl(sizeof(tx_poll_msg), 0, 1);
  dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

  while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) &
           (SYS_STATUS_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR))) {}
  frame_seq_nb++;

  if (!(status_reg & SYS_STATUS_RXFCG_BIT_MASK)) {
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
    return false;
  }
  dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG_BIT_MASK);

  uint32_t frame_len = dwt_read32bitreg(RX_FINFO_ID) & FRAME_LEN_MAX_EX;
  if (frame_len > sizeof(rx_buffer)) return false;
  dwt_readrxdata(rx_buffer, frame_len, 0);
  rx_buffer[ALL_MSG_SN_IDX] = 0;
  if (memcmp(rx_buffer, rx_resp_msg, ALL_MSG_COMMON_LEN) != 0) return false;

  uint32_t poll_tx_ts = dwt_readtxtimestamplo32();
  uint32_t resp_rx_ts = dwt_readrxtimestamplo32();
  float clockOffsetRatio =
      dwt_readcarrierintegrator() * (FREQ_OFFSET_MULTIPLIER * HERTZ_TO_PPM_MULTIPLIER_CHAN_5 / 1.0e6);

  uint32_t poll_rx_ts, resp_tx_ts;
  resp_msg_get_ts(&rx_buffer[RESP_MSG_POLL_RX_TS_IDX], &poll_rx_ts);
  resp_msg_get_ts(&rx_buffer[RESP_MSG_RESP_TX_TS_IDX], &resp_tx_ts);

  int32_t rtd_init = resp_rx_ts - poll_tx_ts;
  int32_t rtd_resp = resp_tx_ts - poll_rx_ts;
  double tof = ((rtd_init - rtd_resp * (1 - clockOffsetRatio)) / 2.0) * DWT_TIME_UNITS;
  distance = tof * SPEED_OF_LIGHT;
  return true;
}

void startWifi() {
  if (!USE_AP) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(STA_SSID, STA_PASS);
    Serial.print("Joining Wi-Fi");
    for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) { delay(500); Serial.print("."); }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Joined. Tag address: ");
      Serial.println(WiFi.localIP());
      return;
    }
    Serial.println("Could not join, starting own network instead");
  }
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("Wi-Fi network: ");
  Serial.print(AP_SSID);
  Serial.print("  password: ");
  Serial.println(AP_PASS);
  Serial.print("Tag address: ");
  Serial.println(WiFi.softAPIP());          // normally 192.168.4.1
}

void setup() {
  Serial.begin(115200);
  UART_init();
  spiBegin(PIN_IRQ, PIN_RST);
  spiSelect(PIN_SS);
  delay(200);

  while (!dwt_checkidlerc()) { Serial.println("IDLE_RC failed"); delay(500); }
  if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR) {
    Serial.println("INIT failed");
    while (1) delay(1000);
  }
  dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);
  if (dwt_configure(&config)) {
    Serial.println("CONFIG failed");
    while (1) delay(1000);
  }
  dwt_configuretxrf(&txconfig_options);
  dwt_setrxantennadelay(RX_ANT_DLY);
  dwt_settxantennadelay(TX_ANT_DLY);
  dwt_setrxaftertxdelay(POLL_TX_TO_RESP_RX_DLY_UUS);
  dwt_setrxtimeout(RESP_RX_TIMEOUT_UUS);

  startWifi();
  ws.begin();
  ws.onEvent(onWsEvent);

  Serial.println("Tag ready");
}

void loop() {
  for (uint8_t id = 1; id <= NUM_ANCHORS; id++) {
    double d;
    char line[32];
    if (rangeAnchor(id, d)) snprintf(line, sizeof(line), "R,%u,%.3f", id, d);
    else                    snprintf(line, sizeof(line), "T,%u", id);
    sendLine(line);
    ws.loop();                               // keep Wi-Fi clients serviced
    delay(RNG_DELAY_MS);
  }
}