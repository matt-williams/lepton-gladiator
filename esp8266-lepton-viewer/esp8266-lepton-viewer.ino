#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

class ErrorDiffuser {
public:
  static const uint8_t MID = 0x7c; 

  void reset() {
    memset(error, 0, sizeof(error));
  }

  uint8_t diffuse(uint8_t x, uint8_t y, uint8_t val) {
    val = val - (error[x + (y & 1) * 80] >> 4);
    error[x + (y & 1) * 80] = 0;
    int errorDelta = val - MID;
    int errorDeltaBL = errorDelta * 3;
    int errorDeltaB = errorDelta * 5;
    int errorDeltaBR = errorDelta;
    int errorDeltaR = errorDelta * 7;
    if (x > 0) {
      error[x - 1 + ((y + 1) & 1) * 80] += errorDeltaBL;
    }
    error[x + ((y + 1) & 1) * 80] += errorDeltaB;
    if (x < 79) {
      error[x + 1 + (y & 1) * 80] += errorDeltaR;
      error[x + 1 + ((y + 1) & 1) * 80] += errorDeltaBR;
    }
    return (errorDelta > 0) ? 0xff : 0;
  }

private:
  uint8_t lastX;
  uint8_t lastY;
  int16_t error[160];
};

// Hardware SPI (faster, but must use certain hardware pins):
// SCK is LCD serial clock (SCLK) - this is pin 13 on Arduino Uno
// MOSI is LCD DIN - this is pin 11 on an Arduino Uno
// pin 5 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 3 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(16, 15, 5);
// Note with hardware SPI MISO and SS pins aren't used but will still be read
// and written to during SPI transfer.  Be careful sharing these pins!

const char* ssid = ...;
const char* password = ...;
WiFiUDP udp;

uint16_t frameNo = 0;
ErrorDiffuser diffuser;
uint8_t packet[1500];

uint16_t htons(uint16_t value) {
  uint16_t tmp_a = (value & 0xff00) >> 8;
  uint16_t tmp_b = (value & 0x00ff) << 8;
  value = tmp_b | tmp_a;
  return value;
}

void setup()   {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

  pinMode(2, OUTPUT);

  display.begin();
  display.setContrast(60);
  display.clearDisplay();
  display.setTextColor(BLACK);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  display.print("Connecting to ");
  display.println(ssid);
  display.display();
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.print(".");
    display.display();
  }
  
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  display.println("");
  display.println("IP address: ");
  display.println(WiFi.localIP());
  display.display();

  udp.begin(16384);
}

void loop() {
  int cb = udp.parsePacket();
  if ((cb != 0) &&
      (cb < sizeof(packet))) {
    // We've received a packet, read the data from it
    udp.read(packet, sizeof(packet)); // read the packet into the buffer
    
    int row = (int)htons(*(uint16_t*)(&packet[16]));
    uint16_t newFrameNo = htons(*(uint16_t*)(&packet[6]));
    if (newFrameNo >= frameNo) {
      newFrameNo = frameNo;
      diffuser.reset();
      for (int subRow = 0; subRow < 15; subRow++) {
        for (int col = 0; col < 80; col++) {
          uint8_t pix = packet[104 + subRow * 80 + col];
          pix = diffuser.diffuse(col, row + subRow, pix);
          display.drawPixel(col, row + subRow, ((pix & 0xf8) << 8) | ((pix & 0xfc) << 3) | (pix >> 3));
        }
      }
      digitalWrite(2, (frameNo & 1) ? HIGH : LOW);
      if (packet[1] & 0x80) { // Marker bit is set - indicates end of frame
        display.display();
      }
    }
  }
}
