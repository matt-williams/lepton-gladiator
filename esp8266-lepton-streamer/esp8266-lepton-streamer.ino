#include <Wire.h>
#include <SPI.h>
#include <Lepton.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

const char* ssid = ...;
const char* password = ...;
const char* targetHost = ...;

// Initialize Lepton
// Pin 4 is connected to SDA
// Pin 5 is connected to SCL
// Pin 15 is connected to SPI CS
Lepton lepton(4, 5, 15);

WiFiUDP udp;

uint16_t frameData[80 * 60];
uint16_t frameNo = 0;
uint32_t seqNo = 0;

// RFC 4175
uint8_t rtpHdr[14] = {
  128, // Version = 2
  96, // Payload type = 96
  0, 0, // Sequence number
  0, 0, 0, 0, // Timestamp
  0, 0, 0, 0, // SSRC
  0, 0, // Extended sequence number
};
uint8_t rtpRowHdr[6] = {
  80, 0, // Length = 80
  0, 0, // Line number
  0, 0, // Offset
};
uint8_t rtpRow[80];

uint32_t htonl(uint32_t value) {
  uint32_t tmpA = (value & 0xff000000) >> 24;
  uint32_t tmpB = (value & 0x00ff0000) >> 8;
  uint32_t tmpC = (value & 0x0000ff00) << 8 ;
  uint32_t tmpD = (value & 0x000000ff) << 24;
  value = tmpD | tmpC | tmpB | tmpA;
  return value;
}

uint16_t htons(uint16_t value) {
  uint16_t tmpA = (value & 0xff00) >> 8;
  uint16_t tmpB = (value & 0x00ff) << 8;
  value = tmpB | tmpA;
  return value;
}

void setup() {
  Serial.begin(9600);

  pinMode(2, OUTPUT);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  udp.begin(16384);

  lepton.begin();

  Serial.print("Status: ");
  Serial.println(lepton.readRegister(Lepton::REG_STATUS), HEX);

  // Enable AGC
  uint16_t agcEnable[2];
  agcEnable[0] = 0;
  agcEnable[1] = 1;
  lepton.doSetCommand(Lepton::CMD_AGC_ENABLE, agcEnable, 2);

  // Check updated AGC value
  lepton.doGetCommand(Lepton::CMD_AGC_ENABLE, agcEnable);
  Serial.printf("AGC Enable: %4x %4x\n", agcEnable[0], agcEnable[1]);

  // Enable AGC
  uint16_t agcCalcEnable[2];
  agcEnable[0] = 0;
  agcEnable[1] = 1;
  lepton.doSetCommand(Lepton::CMD_AGC_CALC_ENABLE_STATE, agcCalcEnable, 2);

  // Check updated AGC value
  lepton.doGetCommand(Lepton::CMD_AGC_CALC_ENABLE_STATE, agcCalcEnable);
  Serial.printf("AGC Enable: %4x %4x\n", agcEnable[0], agcEnable[1]);

  lepton.syncFrame();
}

void loop() {
  if (lepton.readFrame(frameData)) {
    for (int row = 0; row < 60; row += 15) {
      udp.beginPacket(targetHost, 16384);
      rtpHdr[1] = (rtpHdr[1] & (~0x80)) | ((row + 15 >= 60) ? 0x80 : 0);
      *(uint16_t*)(&rtpHdr[2]) = htons(seqNo & 0xffff);
      *(uint32_t*)(&rtpHdr[4]) = htonl(frameNo);
      *(uint16_t*)(&rtpHdr[8]) = htons(seqNo >> 16);
      udp.write(rtpHdr, sizeof(rtpHdr));
      for (int row2 = row; row2 < row + 15; row2++) {
        *(uint16_t*)(&rtpRowHdr[2]) = htons(row2);
        udp.write(rtpRowHdr, sizeof(rtpRowHdr));
      }
      for (int row2 = row; row2 < row + 15; row2++) {
        for (int col = 0; col < 80; col++) {
          rtpRow[col] = (uint8_t)(frameData[row2 * 80 + col] >> 6);
        }
        udp.write(rtpRow, sizeof(rtpRow));
      }
      udp.endPacket();
      seqNo++;
    }
    frameNo++;
  } else {
    frameNo = 0;
  }
  digitalWrite(2, (frameNo & 1) ? HIGH : LOW);
}
