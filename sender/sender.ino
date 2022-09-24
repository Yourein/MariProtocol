#define CLOCK_PROVIDER 12
#define HANDSHAKER 11
#define DTX 10

#define CLOCK_BROADCAST 0
#define DATA_WAIT 1
#define DATA_SENDING 2

const uint8_t syncClock = 120;
const uint8_t sendClock = 10;

int WAIT = syncClock;
uint8_t syncState = CLOCK_BROADCAST;
int lpCnt = 1;

bool HSRaised = false;

uint8_t serialBuf[1] = {};
uint8_t charaBuf[3] = {};
bool isUCode = false;
uint8_t charaCnt = 0;

uint8_t transBuf[256] = {};
uint16_t transLen = 0;

uint16_t sendingInd = 0;
uint8_t bitInd = 0;

void setup() {
  Serial.begin(9600);

  pinMode(CLOCK_PROVIDER, OUTPUT);
  pinMode(HANDSHAKER, INPUT);
  pinMode(DTX, OUTPUT);
}

uint32_t indFinder[] = {
  0xe38182, 0xe38184, 0xe38186, 0xe38188, 0xe3818A, 
  0xe3818B, 0xe3818D, 0xe3818F, 0xe38191, 0xe38193,
  0xe38195, 0xe38197, 0xe38199, 0xe3819B, 0xe3819D,
  0xe3819f, 0xe381a1, 0xe381a4, 0xe381a6, 0xe381a8,
  0xe381aa, 0xe381ab, 0xe381ac, 0xe381ad, 0xe381ae,
  0xe381af, 0xe381b2, 0xe381b5, 0xe381b8, 0xe381bb,
  0xe381be, 0xe381bf, 0xe38280, 0xe38281, 0xe38282,
  0xe38284, 0xe38286, 0xe38288, 0xe383bc, 0xefbc81,
  0xe38289, 0xe3828a, 0xe3828b, 0xe3828c, 0xe3828d,
  0xe3828f, 0xe38292, 0xe38293, 0xefbc9f, 0xe38080,
  0xefbc90, 0xefbc91, 0xefbc92, 0xefbc93, 0xefbc94,
  0xefbc95, 0xefbc96, 0xefbc97, 0xefbc98, 0xefbc99,
  0xe28099, 0xe38081, 0xe38082, 0x000000, 0x000000
};

uint8_t findCode(uint32_t target) {
  for (int i = 0; i < 65; i++){
    if (target == indFinder[i]) return i;
  }
  return -1;
}

uint8_t convCode(uint8_t Buf[]){
  uint8_t ind;
  uint32_t target = 0;
  if (Buf[0] < 0xe2) {
    if (48 <= Buf[0] && Buf[0] <= 57) { //isNumeric
      target = 0xefbc90;
      target += Buf[0]-48;
    }
    else if (65 <= Buf[0]){
      return Buf[0]-64; //65引くとNULLと衝突するので、Buf[0]-65+1を計算する
    }
    else {
      if (Buf[0] == 33) target = 0xefbc81; //!
      else if (Buf[0] == 45) target = 0xe383bc; //-
      else if (Buf[0] == 63) target = 0xefbc9f; //?
      else if (Buf[0] == 32) target = 0xe38080; //SPACE
      else if (Buf[0] == '\'') target = 0xe28099;
      else if (Buf[0] == ',') target = 0xe38081;
      else if (Buf[0] == '.') target = 0xe38082;
    }

    if (target == 0) ind = -1;
    else ind = findCode(target);
  }
  else {  
    uint32_t nBuf = Buf[0];
    for (uint8_t i = 1; i < 3; i++) nBuf = (nBuf<<8) | Buf[i];
    ind = findCode(nBuf);
  }

  if (ind == -1) return 0; //return NULL character
  uint8_t res = 0b10000000;
  res |= ((ind/5)<<3)|(ind%5);
  return res;
}

//立ち上がりでデータ書き込んで、立ち下がりでデータを読ませる

//コネクションが確立できて、シリアルからCRが読めたら送信開始 -> 0x0D

void loop() {
  lpCnt++;
  if (syncState == CLOCK_BROADCAST){
    if (lpCnt%2 == 0) digitalWrite(CLOCK_PROVIDER, HIGH);
    else digitalWrite(CLOCK_PROVIDER, LOW);

    if (HSRaised == false && digitalRead(HANDSHAKER)) HSRaised = true;
    else if (HSRaised == true && digitalRead(HANDSHAKER) == LOW) {
      HSRaised = false;
      digitalWrite(CLOCK_PROVIDER, LOW);
      syncState = DATA_WAIT;
      WAIT = sendClock;
      Serial.println("CONNECTION ESTABLISHED; READY.");
    }
  }
  else if (syncState == DATA_WAIT){
    if (transLen < 255){
      while(Serial.available()) {
        if (transLen >= 255) {
          charaCnt = 0;
          isUCode = false;
          transBuf[0] = 0xff;
          syncState = DATA_SENDING;

          Serial.println("SENDING...");
          lpCnt = 1;
          break;
        }

        Serial.readBytes(serialBuf, 1);
        if (serialBuf[0] == 0xd) {
          transBuf[transLen] = 0xff;
          syncState = DATA_SENDING;

          Serial.println("SENDING...");
          lpCnt = 1;
          break;
        }

        if (isUCode == false) {
          if (serialBuf[0] >= 0xe2) {
            isUCode = true;
            charaBuf[0] = serialBuf[0];
            charaCnt++;
          }
          else {
            charaBuf[0] = serialBuf[0];
            uint8_t nxt = convCode(charaBuf);
            transBuf[transLen] = nxt;
            transLen++;
            Serial.println(nxt, BIN);
          }
        }
        else {
          charaBuf[charaCnt] = serialBuf[0];
          charaCnt++;
          if (charaCnt >= 3) {
            charaCnt = 0;
            isUCode = false;

            uint8_t nxt = convCode(charaBuf);
            Serial.println(nxt, BIN);
            transBuf[transLen] = nxt;
            transLen++;

            charaBuf[0] = charaBuf[1] = charaBuf[2] = 0;
          }
        }
      }
    }
    else {
      transBuf[transLen] = 0xd;
      syncState = DATA_SENDING;
      lpCnt = 1;
    }
  }
  else {
    if (lpCnt%2 == 0 && sendingInd <= transLen) {
      digitalWrite(CLOCK_PROVIDER, HIGH);
      uint8_t nxt = ((transBuf[sendingInd]>>(7-bitInd))&1)?HIGH:LOW;
      //Serial.println(nxt);
      digitalWrite(DTX, nxt);
    }
    else {
      digitalWrite(CLOCK_PROVIDER, LOW);
      bitInd++;
      if (bitInd > 7) {
        sendingInd++;
        bitInd = 0;

        if (sendingInd > transLen) {
          sendingInd = 0;
          digitalWrite(CLOCK_PROVIDER, LOW);
          digitalWrite(DTX, LOW);
          syncState = DATA_WAIT;
          lpCnt = 1;

          for (int i = 0; i < 256; i++){
            transLen = 0;
            transBuf[i] = 0;
          }
          Serial.println("FINISHED. WAITING NEW DATA...");
        }
      }
    }
  }
  delay(WAIT);
}