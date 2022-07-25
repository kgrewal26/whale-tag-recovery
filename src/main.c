#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "aprs.h"
#include "gps.h"
#include "tag.h"

const uint LED_PIN = 29;

// status arrays (updated in-place by functions)
float coords[2] = {0,0};
uint16_t aCS[3] = {0,0,0};
char lastGpsUpdate[100] = "$CMD_ERROR";
char lastDtUpdate[100] = "$CMD_ERROR";

// APRS communication config (change per tag)
const uint32_t aprsInterval = 30000; // APRS TX interval

// SWARM communication config (change ONLY when necessary)
const uint TX_GPS = 0;
const uint RX_GPS = 1;
const uint BAUD_GPS = 9600;

// TAG communication config (change ONLY when necessary)
const uint TX_TAG = 4;
const uint RX_TAG = 5;
const uint BAUD_TAG = 115200;
const uint32_t tagInterval = 10000;

// Which system are running
bool aprsRunning = false;
bool tagConnected = false;

// runtime vars
uint32_t prevAprsTx = 0;
bool gpsInteractive = false;
uint32_t prevTagTx = 0;

void set_bin_desc(void);
void setLed(bool state);
void initLed(void);
void setup(void);

void set_bin_desc() {
  bi_decl(bi_program_description("Recovery process binary for standalone recovery board 2-#"));
  bi_decl(bi_1pin_with_name(LED_PIN, "On-board LED"));
  bi_decl(bi_1pin_with_name(TX_GPS, "TX UART to GPS"));
  bi_decl(bi_1pin_with_name(RX_GPS, "RX UART to GPS"));
  bi_decl(bi_1pin_with_name(TX_TAG, "TX UART to TAG modem"));
  bi_decl(bi_1pin_with_name(RX_TAG, "RX UART to TAG modem"));
  describeConfig();
}

void setLed(bool state) {gpio_put(LED_PIN, state);}
void initLed() {gpio_init(LED_PIN); gpio_set_dir(LED_PIN, GPIO_OUT);}

void txAprs(bool aprsDebug, int style) {
  getPos(coords);
  getACS(aCS);
  prevAprsTx = to_ms_since_boot(get_absolute_time());
  setLed(true);
  if (aprsDebug)
    sendTestPackets(style);
  else
    sendPacket(coords, aCS);
  setLed(false);
}

void txTag () {
  prevTagTx = to_ms_since_boot(get_absolute_time());
  getLastPDtBufs(lastGpsUpdate, lastDtUpdate);
  writeGpsToTag(lastGpsUpdate, lastDtUpdate);
  // detachTag();
  // reqTagState();
}

void setup() {
  set_bin_desc();
  stdio_init_all();

  aprsRunning = true;
  gpsInteractive = true;

  tagConnected = true;

  initLed();
  setLed(true);

  gpsInit(TX_GPS, RX_GPS, BAUD_GPS, uart0, gpsInteractive);

  if (aprsRunning) {
    // APRS communication config (change per tag)
    char mycall[8] = "J75Y";
    int myssid = 1;
    char dest[8] = "APLIGA";
    char digi[8] = "WIDE2";
    int digissid = 1;
    char mycomment[128] = "New Sign Testing";
    configureAPRS(mycall, myssid, dest, digi, digissid, mycomment);
  }

  initTagComm(TX_TAG, RX_TAG, BAUD_TAG, uart1);
  setLed(false);
}

int main() {
  // Setup
  setup();

  // Loop
  while (true) {
    readFromGps();
    if (((to_ms_since_boot(get_absolute_time()) - prevAprsTx) >= aprsInterval) && aprsRunning) {
      txAprs(false, 2);
    }

    if (((to_ms_since_boot(get_absolute_time()) - prevTagTx) >= tagInterval) && tagConnected) {
      txTag();
    }
  }
}
