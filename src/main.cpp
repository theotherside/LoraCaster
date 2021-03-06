/*===================================================================================*
  :::         ...    :::::::..    :::.       .,-:::::   :::.     .::::::.::::::::::::
  ;;;      .;;;;;;;. ;;;;``;;;;   ;;`;;    ,;;;'````'   ;;`;;   ;;;`    `;;;;;;;;''''
  [[[     ,[[     [[,[[[,/[[['  ,[[ '[[,  [[[         ,[[ '[[, '[==/[[[[,    [[
  $$'     $$$,     $$$$$$$$$c   c$$$cc$$$c $$$        c$$$cc$$$c  '''    $    $$
 o88oo,.__"888,_ _,88P888b "88bo,888   888,`88bo,__,o, 888   888,88b    dP    88,
 """"YUMMM  "YMMMMMP" MMMM   "W" YMM   ""`   "YUMMMMMP"YMM   ""`  "YMmMY"     MMM   ER
 *===================================================================================*
  LoraCaster - broadcast messages on TTN LoraWan network
   MIT License - Copyright (c) 2018 Valerio Vaccaro
   Based on https://github.com/gonzalocasas/arduino-uno-dragino-lorawan/blob/master/LICENSE
   Based on examples from https://github.com/matthijskooijman/arduino-lmic
 *===================================================================================*/
const char source_filename[] = __FILE__;
const char compile_date[] = __DATE__ " " __TIME__;
const char version[] = "LoraCaster v.0.0.5";

#include <Arduino.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <SSD1306.h>
#include <soc/efuse_reg.h>

#include "ttn_config.h"
#include "config.h"
#include "hardware.h"

// --- Software config
// Internal buffer
#define MAX_BUFF_SIZE 4*1024
//int BUFF_SIZE = MAX_BUFF_SIZE;
static uint8_t buff[MAX_BUFF_SIZE] = {0x00};
int buff_size = 1;
//int buff_size = 4;

uint8_t counter = 0;
static osjob_t sendjob;
uint8_t TX_INTERVAL = 0;
bool TX_RETRASMISSION = false;
bool VERBOSE = false;
bool HALT = false;
bool CONTINUOSLY = false;
uint8_t CONTINUOSLY_NO = 0;
char TTN_response[30];
uint8_t PAYLOAD_SIZE = 100;
bool LAST_TX=false;
int SF=7;

uint8_t r1 = esp_random();
uint8_t r2 = esp_random();

SSD1306 display (OLED_I2C_ADDR, OLED_SDA, OLED_SCL);

// Lora radio pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 18,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 14,
    .dio = {26, 33, 32}  // Pins for the Heltec ESP32 Lora board/ TTGO Lora32 with 3D metal antenna
};

void saveConfigAll(){
  int conf_b = PAYLOAD_SIZE;
  int conf_r = TX_RETRASMISSION;
  int conf_w = TX_INTERVAL;
  int conf_f = SF;
  if (!saveConfig(conf_b, conf_r, conf_w, conf_f)) {
    Serial.println("Failed to save config");
  } else {
    Serial.println("Config saved");
  }
}

void loadConfigAll(){
  int conf_b, conf_r, conf_w, conf_f;
  if (!loadConfig(&conf_b, &conf_r, &conf_w, &conf_f)) {
    Serial.println("Failed to load config");
  } else {
    Serial.println("Config loaded");
  }
  if(conf_r==0) TX_RETRASMISSION=false;
  else TX_RETRASMISSION=true;
  SF = conf_f;
  TX_INTERVAL = conf_w;
  PAYLOAD_SIZE = conf_b;
}

void mydisplay(char* function, char* message, char* error, int counter, int max){
  char tmp[32];
  display.clear();
  display.drawString (0, 0, version);
  display.drawString (0, 10, function);
  display.drawString (0, 20, message);
  display.drawString (0, 30, error);
  sprintf(tmp, "ID 0x%.2X 0x%.2X - SF %d",r1,r2,SF);
  display.drawString (0, 40, tmp);
  if (max==-1)
    sprintf(tmp, "Message counter: %d",counter);
  else
      sprintf(tmp, "Message counter: %d/%d",counter, max+1);
  display.drawString (0, 50, tmp);
  display.display ();
}

void help(){
  static char * banner =
  "*===================================================================================*\n"
  " :::         ...    :::::::..    :::.       .,-:::::   :::.     .::::::.::::::::::::\n"
  " ;;;      .;;;;;;;. ;;;;``;;;;   ;;`;;    ,;;;'````'   ;;`;;   ;;;`    `;;;;;;;;''''\n"
  " [[[     ,[[     \[[,[[[,/[[['  ,[[ '[[,  [[[         ,[[ '[[, '[==/[[[[,    [[     \n"
  " $$'     $$$,     $$$$$$$$$c   c$$$cc$$$c $$$        c$$$cc$$$c  '''    $    $$     \n"
  "o88oo,.__\"888,_ _,88P888b \"88bo,888   888,`88bo,__,o, 888   888,88b    dP    88,    \n"
  "\"\"\"\"YUMMM  \"YMMMMMP\" MMMM   \"W\" YMM   \"\"`   \"YUMMMMMP\"YMM   \"\"`  \"YMmMY\"     MMM   ER\n"
  "                   LoraCaster, the Swiss Army Knife of LoraWan\n"
  "*===================================================================================*\n";
  Serial.print(banner);
  Serial.print(F("                 "));Serial.print(version);Serial.print(F(" build on "));Serial.println(compile_date);
  Serial.println(F("*===================================================================================*"));
  Serial.println(F("General commands:"));
  Serial.println(F("   h - shows this help"));
  Serial.println(F("   v - toggle verbosity"));
  Serial.println(F("   l - toggle led"));
  Serial.println(F("Communication commands:"));
  Serial.println(F("   p[hex payload]! - charge payload in the memory max 90k byte"));
  Serial.println(F("   d - dump actual payload present in the memory"));
  Serial.println(F("   S - start transmission"));
  Serial.println(F("   H - halt transmission"));
  Serial.println(F("Advanced commands:"));
  Serial.println(F("   t - send a Test message with content 0123456789 (ASCII)"));
  Serial.println(F("   c - shows LoraWan/TTN configuration"));
  Serial.println(F("   C - send continuosly messages, stop with H"));
  Serial.print(  F("   b[]! - set message dimension in byte 0-100"));;Serial.print(F(" (actual "));Serial.print(PAYLOAD_SIZE);Serial.println(F(") "));
  Serial.print(  F("   r - toggle retransmission (actual "));Serial.print(TX_RETRASMISSION);Serial.println(F(")"));
  Serial.print(  F("   w[]! - set delay betweet packets in second 0-255 (actual "));Serial.print(TX_INTERVAL);Serial.println(F(")"));
  Serial.print(  F("   f[] - set spreading factor between 7,8,9,10,11,12 (actual "));Serial.print(SF);Serial.println(F(")"));
  char tmp[16];
  sprintf(tmp, "0x%.2X 0x%.2X",r1,r2);
  Serial.print(  F("   R - generate a new random packet ID (actual "));Serial.print(tmp);Serial.println(F(")"));
  Serial.println(F("*===================================================================================*"));
}

void PrintHex8(uint8_t *data, uint8_t length){
  char tmp[16];
  for (int i=0; i<length; i++) {
      sprintf(tmp, "0x%.2X",data[i]);
      Serial.print(tmp); Serial.print(" ");
  }
}

void do_send_cont(osjob_t* j){
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
      Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
      // Prepare upstream data transmission at the next possible time.
      LMIC_setTxData2(1, &CONTINUOSLY_NO, 1, 0);
      Serial.println(F("Sending uplink packet..."));
      digitalWrite(LEDPIN, HIGH);
      ++CONTINUOSLY_NO;
      mydisplay("Send","Sending CONTINUOSLY packet...","",CONTINUOSLY_NO, 0);
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void do_send(osjob_t* j){
    uint8_t message[11+PAYLOAD_SIZE] = "LWC*00*00*";
    const uint8_t max_counter = round(buff_size/PAYLOAD_SIZE);
    if (counter > max_counter){
      counter = 0;
    }
    message[4] = r1;
    message[5] = r2;
    message[7] = max_counter;
    message[8] = counter;
    if(message[7]==message[8]) LAST_TX = true;
    else LAST_TX = false;

    Serial.print(F("Counter ")); Serial.print(counter);
    int remaining = buff_size - (counter*PAYLOAD_SIZE);
    Serial.print(F(" remaining ")); Serial.println(remaining);
    if (remaining > 100) remaining = 100;
    for(uint8_t x=0; x<remaining; x++){
        message[10+x] = buff[(counter*PAYLOAD_SIZE)+x];
    }

    if (VERBOSE) {
      PrintHex8((uint8_t *)message, 10+remaining);
      Serial.println();
    }

    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
      Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
      // Prepare upstream data transmission at the next possible time.
      LMIC_setTxData2(1, message, 11+remaining-1, 0);
      Serial.println(F("Sending uplink packet..."));
      digitalWrite(LEDPIN, HIGH);
      ++counter;
      const uint8_t max_counter = round(buff_size/PAYLOAD_SIZE);
      mydisplay("Send","Sending uplink packet...","",counter, max_counter);
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            break;
        case EV_RFU1:
            Serial.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            mydisplay("Event","EV_TXCOMPLETE","",counter, -1);
            digitalWrite(LEDPIN, LOW);
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.println(F("Received "));
              Serial.println(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            //send new messages
            if (CONTINUOSLY){
              if (!HALT){
                os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send_cont);
              } else {
                Serial.println(F("HALT!"));
                //mydisplay("Send","HALT","",counter);
                HALT = false;
                CONTINUOSLY = false;
              }
            }
            else{
              if (LAST_TX){
                if (TX_RETRASMISSION){ // Schedule next transmission
                  if (!HALT){
                    os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
                    const uint8_t max_counter = round(buff_size/PAYLOAD_SIZE);
                    mydisplay("Send","Sending uplink packet...","",counter, max_counter);
                  } else {
                    Serial.println(F("HALT!"));
                    //mydisplay("Send","HALT","",counter);
                    HALT = false;
                  }
                }
              } else {
                if (!HALT){
                  os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
                  const uint8_t max_counter = round(buff_size/PAYLOAD_SIZE);
                  mydisplay("Send","Sending uplink packet...","",counter, max_counter);
                } else {
                  Serial.println(F("HALT!"));
                  HALT = false;
                  //mydisplay("Send","HALT","",counter);
                }
              }
            }

            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
         default:
            Serial.println(F("Unknown event"));
            break;
    }
}

void forceTxSingleChannelDr(_dr_eu868_t sf) {
    for(int i=0; i<9; i++) { // For EU; for US use i<71
        if(i != 0) {
            LMIC_disableChannel(i);
        }
    }
    // Set data rate (SF) and transmit power for uplink
    LMIC_setDrTxpow(sf, 14); //DR_SF7
}

void setup() {
   Serial.begin(115200);
   delay(1500);   // Give time for the seral monitor to start up

   if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
    Serial.println("SPIFFS Mount Failed");
    return;
   }

   loadConfigAll();

   help();

   // Use the Blue pin to signal transmission.
   pinMode(LEDPIN,OUTPUT);

   // reset the OLED
   pinMode(OLED_RESET,OUTPUT);
   digitalWrite(OLED_RESET, LOW);
   delay(50);
   digitalWrite(OLED_RESET, HIGH);

   display.init ();
   display.flipScreenVertically ();
   display.setFont (ArialMT_Plain_10);
   display.setTextAlignment (TEXT_ALIGN_LEFT);
   display.display ();

    // LMIC init
    os_init();

    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();

    // Set up the channels used by the Things Network, which corresponds
    // to the defaults of most gateways. Without this, only three base
    // channels from the LoRaWAN specification are used, which certainly
    // works, so it is good for debugging, but can overload those
    // frequencies, so be sure to configure the full frequency range of
    // your network here (unless your network autoconfigures them).
    // Setting up channels should happen after LMIC_setSession, as that
    // configures the minimal channel set.

    LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
    LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);      // g2-band
    // TTN defines an additional channel at 869.525Mhz using SF9 for class B
    // devices' ping slots. LMIC does not have an easy way to define set this
    // frequency and support for class B is spotty and untested, so this
    // frequency is not configured here.

    // Set static session parameters.
    LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);

    // Disable link check validation
    LMIC_setLinkCheckMode(0);

    // TTN uses SF9 for its RX2 window.
    LMIC.dn2Dr = DR_SF9;

    // Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library)
    //LMIC_setDrTxpow(DR_SF7,14);
    forceTxSingleChannelDr(DR_SF7);
    mydisplay("Start","Waiting commands","",0,-1);
}

uint8_t htoi(char c){
  if(c >= '0' && c <= '9')
    return (c - '0');
  else if (c >= 'A' && c <= 'F')
    return (10 + (c - 'A'));
  else if (c >= 'a' && c <= 'f')
    return (10 + (c - 'a'));
  else
    return NULL;
}

uint8_t htoi(char c, char d){
  return htoi(c)*16+htoi(d);
}

void loop() {
    os_runloop_once();
    char v;
    char c = Serial.read();
    switch(c) {
      case 'h'  :
        help();
      break;

      case 'v'  :
        Serial.println(c);
        VERBOSE = !VERBOSE;
        Serial.println(F("OK"));
        Serial.println(F("*===================================================================================*"));
      break;

      case 'l' :
        Serial.println(c);
        digitalWrite(LEDPIN, !digitalRead(LEDPIN));
        Serial.println(F("OK"));
        Serial.println(F("*===================================================================================*"));
      break;

      case 'p'  :
        Serial.print(c);
        // payload
        uint8_t my_buff[MAX_BUFF_SIZE];
        r1 = esp_random();
        r2 = esp_random();
        int digit_num;
        digit_num = 0;
        char digit[2];
        int elements;
        elements = 0;
        bool valid;
        valid = false;
        c = Serial.read();
        while(true){
          if (c == '!') {
            Serial.print(c);
            valid = true;
            break;
          }
          if ( (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') ){
            Serial.print(c);
            digit[digit_num++] = c;
            if (digit_num > 1){
              digit_num = 0;
              my_buff[elements++] = htoi(digit[0], digit[1]);
              //Serial.printf("%.2x", my_buff[elements-1]);
            }
          } else {
            delay(100);
            //if (c!=0xff) break;
          }
          c = Serial.read();
       } //while
       Serial.println();
       if (valid) {
          // copy my_buff in buff
          for (int i=0;i<elements;i++){
            buff[i]=my_buff[i];
          }
          // copy my_buff_size in buff_size
          buff_size = elements;
          Serial.println(F("OK"));
        }
        else Serial.println(F("Not OK"));
        Serial.println(F("*===================================================================================*"));
      break;

      case 'd'  :
        Serial.println(c);
        PrintHex8(buff, buff_size);Serial.println("");
        Serial.println(F("OK"));
        Serial.println(F("*===================================================================================*"));
      break;

      case 'S'  :
        Serial.println(c);
        HALT = false;
        counter = -1;
        do_send(&sendjob);
        Serial.println(F("OK"));
        Serial.println(F("*===================================================================================*"));
      break;

      case 'H'  :
        Serial.println(c);
        HALT = true;
        Serial.println(F("OK"));
        Serial.println(F("*===================================================================================*"));
      break;

      case 't'  :
        Serial.println(c);
        HALT = true;
        // Check if there is not a current TX/RX job running
        if (LMIC.opmode & OP_TXRXPEND) {
          Serial.println(F("OP_TXRXPEND, not sending"));
          Serial.println(F("*===================================================================================*"));
        } else {
          // Prepare upstream data transmission at the next possible time.
          LMIC_setTxData2(1, (uint8_t*)"0123456789", 10, 0);
          digitalWrite(LEDPIN, HIGH);
          counter=1;
          mydisplay("Test","Sending test packet...","",counter,0);
          Serial.println(F("OK"));
          Serial.println(F("*===================================================================================*"));
        }
      break;

      case 'c'  :
        Serial.println(c);
        Serial.print(F("NWKSKEY: "));PrintHex8(NWKSKEY, 16);Serial.println("");
        Serial.print(F("APPSKEY: "));PrintHex8(APPSKEY, 16);Serial.println("");
        Serial.print(F("DEVADDR: "));PrintHex8((uint8_t *)DEVADDR, 4);Serial.println("");
        Serial.println(F("OK"));
        Serial.println(F("*===================================================================================*"));
      break;

      case 'C'  :
        Serial.println(c);
        CONTINUOSLY = true;
        CONTINUOSLY_NO = 0;
        os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send_cont);
        Serial.println(F("OK"));
        Serial.println(F("*===================================================================================*"));
      break;

      case 'b'  :
        Serial.print(c);
        int buffCount;
        buffCount = 0;
        int num;
        char bufNum[5];
        boolean validNum;
        validNum = false;
        c = Serial.read();
        while(true){
          if (c == '!') {
            Serial.print(c);
            validNum = true;
            break;
          }
          if (c >= '0' && c <= '9'){
            Serial.print(c);
            bufNum[buffCount] = c;
            buffCount++;
            if (buffCount > 3){
               Serial.println(F("ERROR: number too mutch long!"));
               break;
            }
          } else {
            if (c!=0xff) {
              Serial.println(F("ERROR: wrong char!"));
              break;
            }
          }
          c = Serial.read();
        } //while
        Serial.println();
        bufNum[buffCount]='\0';
        if (validNum){
          num = atoi(bufNum);
          if (num > 100){
             Serial.println(F("ERROR: number too big!"));
             break;
          } else{
             PAYLOAD_SIZE = num;
             Serial.println(F("OK"));
             saveConfigAll();
           }
        }
        Serial.println(F("*===================================================================================*"));
      break;

      case 'r'  :
        Serial.println(c);
        TX_RETRASMISSION = !TX_RETRASMISSION;
        Serial.println(F("OK"));
        saveConfigAll();
        Serial.println(F("*===================================================================================*"));
      break;

      case 'w'  :
        Serial.print(c);
        buffCount = 0;
        validNum = false;
        c = Serial.read();
        while(true){
          if (c == '!') {
            validNum = true;
            break;
          }
          if (c >= '0' && c <= '9'){
            bufNum[buffCount] = c;
            buffCount++;
            if (buffCount > 3){
               Serial.println(F("ERROR: number too mutch long!"));
               break;
            }
          } else {
            if (c!=0xff) {
              Serial.println(F("ERROR: wrong char!"));
              break;
            }
          }
          c = Serial.read();
        } //while
        Serial.println();
        bufNum[buffCount]='\0';
        if (validNum){
          num = atoi(bufNum);
          if (num > 100){
             Serial.println(F("ERROR: number too big!"));
             break;
          } else {
             TX_INTERVAL = num;
             Serial.println(F("OK"));
             saveConfigAll();
           }
        }
        Serial.println(F("*===================================================================================*"));
      break;

      case 'R' :
        Serial.println(c);
        r1 = esp_random();
        r2 = esp_random();
        Serial.println(F("OK"));
        Serial.println(F("*===================================================================================*"));
      break;

      case 'f' :
        Serial.print(c);
        v = Serial.read();
        while (v==0xff)
          v = Serial.read();
        Serial.print(v);
        switch (v){
          case '7':
              Serial.println();
              Serial.println(F("OK"));
              forceTxSingleChannelDr(DR_SF7);
              SF=7;
          break;
          case '8':
              Serial.println();
              Serial.println(F("OK"));
              forceTxSingleChannelDr(DR_SF8);
              SF=8;
          break;
          case '9':
              Serial.println();
              Serial.println(F("OK"));
              forceTxSingleChannelDr(DR_SF9);
              SF=9;
          break;
          case '1':
              char v2 = Serial.read();
              while (v2==0xff)
                v2 = Serial.read();
              Serial.print(v2);
              Serial.println();
              switch (v2) {
                case '0':
                  forceTxSingleChannelDr(DR_SF10);
                  SF=10;
                  Serial.println(F("OK"));
                break;
                case '1':
                  forceTxSingleChannelDr(DR_SF11);
                  SF=11;
                  Serial.println(F("OK"));
                break;
                case '2':
                  forceTxSingleChannelDr(DR_SF12);
                  SF=12;
                  Serial.println(F("OK"));
                break;
                default:
                  Serial.println(F("ERROR"));
              }
          break;
        }
        saveConfigAll();
        Serial.println(F("*===================================================================================*"));
      break;

    }
    delay(100);
}
