#include "dw3000.h"

#define ANCHOR_ID 3          // <<< 1 or 2

#define PIN_RST 27
#define PIN_IRQ 34
#define PIN_SS  4

#define TX_ANT_DLY 16385
#define RX_ANT_DLY 16385

#define ALL_MSG_COMMON_LEN 10
#define ALL_MSG_SN_IDX 2
#define ALL_MSG_DEST_IDX 7
#define RESP_MSG_POLL_RX_TS_IDX 10
#define RESP_MSG_RESP_TX_TS_IDX 14
#define RESP_MSG_TS_LEN 4

#define POLL_RX_TO_RESP_TX_DLY_UUS 1000
#ifndef UUS_TO_DWT_TIME
#define UUS_TO_DWT_TIME 63898
#endif

static dwt_config_t config = {
  5, DWT_PLEN_128, DWT_PAC8, 9, 9, 1, DWT_BR_6M8, DWT_PHRMODE_STD,
  DWT_PHRRATE_STD, (129 + 8 - 8), DWT_STS_MODE_OFF, DWT_STS_LEN_64, DWT_PDOA_M0
};
extern dwt_txconfig_t txconfig_options;

static uint8_t rx_poll_msg[] = {0x41, 0x8C, 0, 0xCA, 0xDE, 'V', 'E', ANCHOR_ID, 'W', 0xE0, 0, 0};
static uint8_t tx_resp_msg[] = {0x41, 0x8C, 0, 0xCA, 0xDE, 'V', 'E', ANCHOR_ID, 'W', 0xE1,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t frame_seq_nb = 0;
static uint8_t rx_buffer[24];
static uint32_t status_reg = 0;

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

  Serial.print("Anchor ready, ID = ");
  Serial.println(ANCHOR_ID);
}

void loop() {
  dwt_setrxtimeout(0);                      // listen forever
  dwt_rxenable(DWT_START_RX_IMMEDIATE);

  while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) &
           (SYS_STATUS_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_ERR))) {}

  if (status_reg & SYS_STATUS_RXFCG_BIT_MASK) {
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG_BIT_MASK);
    uint32_t frame_len = dwt_read32bitreg(RX_FINFO_ID) & FRAME_LEN_MAX_EX;
    if (frame_len <= sizeof(rx_buffer)) {
      dwt_readrxdata(rx_buffer, frame_len, 0);
      rx_buffer[ALL_MSG_SN_IDX] = 0;
      Serial.print("heard frame for anchor "); Serial.println(rx_buffer[ALL_MSG_DEST_IDX]);

      // Only answer polls addressed to THIS anchor
      if (memcmp(rx_buffer, rx_poll_msg, ALL_MSG_COMMON_LEN) == 0) {
        uint64_t poll_rx_ts = get_rx_timestamp_u64();
        uint32_t resp_tx_time = (poll_rx_ts + ((uint64_t)POLL_RX_TO_RESP_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;
        dwt_setdelayedtrxtime(resp_tx_time);
        uint64_t resp_tx_ts = (((uint64_t)(resp_tx_time & 0xFFFFFFFEUL)) << 8) + TX_ANT_DLY;

        resp_msg_set_ts(&tx_resp_msg[RESP_MSG_POLL_RX_TS_IDX], poll_rx_ts);
        resp_msg_set_ts(&tx_resp_msg[RESP_MSG_RESP_TX_TS_IDX], resp_tx_ts);
        tx_resp_msg[ALL_MSG_SN_IDX] = frame_seq_nb;

        dwt_writetxdata(sizeof(tx_resp_msg), tx_resp_msg, 0);
        dwt_writetxfctrl(sizeof(tx_resp_msg), 0, 1);

        if (dwt_starttx(DWT_START_TX_DELAYED) == DWT_SUCCESS) {
          while (!(dwt_read32bitreg(SYS_STATUS_ID) & SYS_STATUS_TXFRS_BIT_MASK)) {}
          dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS_BIT_MASK);
          frame_seq_nb++;
          Serial.print("poll #"); Serial.print(frame_seq_nb); Serial.println(" answered");
        } else {
          Serial.println("TX FAILED (reply too late)");
        }
      }
    }
  } else {
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_ERR);
  }
}