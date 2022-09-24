#define CLOCK_RECEIVE 3
#define HANDSHAKER 4
#define DRX 5

#define RECEIVE_CLOCK_WAIT 1
#define RECEIVE_HANDSHAKE 2
#define RECEIVE_BUFFER 3
#define RECEIVE_GREEN 4

bool clockRaised = false;
int receivePhase = RECEIVE_CLOCK_WAIT;
int lpCnt = 0;

const int syncClock = 120/2;
const int readClock = 30/2;

uint8_t Buf = 0;
uint8_t receiveInd = 0;

int WAIT = syncClock;

const String table[13][5] = {
  {u8"あ", u8"い", u8"う", u8"え", u8"お"},
  {u8"か", u8"き", u8"く", u8"け", u8"こ"},
  {u8"さ", u8"し", u8"す", u8"せ", u8"そ"},
  {u8"た", u8"ち", u8"つ", u8"て", u8"と"},
  {u8"な", u8"に", u8"ぬ", u8"ね", u8"の"},
  {u8"は", u8"ひ", u8"ふ", u8"へ", u8"ほ"},
  {u8"ま", u8"み", u8"む", u8"め", u8"も"},
  {u8"や", u8"ゆ", u8"よ", u8"ー", u8"!"},
  {u8"ら", u8"り", u8"る", u8"れ", u8"ろ"},
  {u8"わ", u8"を", u8"ん", u8"?", u8" "},
  {u8"0", u8"1", u8"2", u8"3", u8"4"},
  {u8"5", u8"6", u8"7", u8"8", u8"9"},
  {u8"\'", u8",", u8".", u8"", u8""}
};

void decode(uint8_t buf) {
  if (((buf>>7)&1) == 1) { //UCode
    if (buf == 0xff) {
      Serial.println("");
      return;
    }

    uint8_t column = (buf&0b111);
    uint8_t row = ((buf>>3)&0b1111);
    Serial.print(table[row][column]);
  }
  else { //ohterwise
    if (buf != 0){
      Serial.print(char(buf+64));
    }
    else {
      Serial.print(" ");
    }
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(CLOCK_RECEIVE, INPUT);
  pinMode(HANDSHAKER, OUTPUT);
  pinMode(DRX, INPUT);
}

//CRが受信できたら終了

void loop() {
  lpCnt++;

  if (receivePhase == RECEIVE_CLOCK_WAIT){
    if (clockRaised == false && digitalRead(CLOCK_RECEIVE)) {
      clockRaised = true;
    }
    else if (clockRaised == true && digitalRead(CLOCK_RECEIVE) == LOW){
      clockRaised = false;
      receivePhase = RECEIVE_HANDSHAKE;
      lpCnt = 0;
    }
  }
  else if (receivePhase == RECEIVE_HANDSHAKE){
    digitalWrite(HANDSHAKER, HIGH);
    delay(240);
    digitalWrite(HANDSHAKER, LOW);
    receivePhase = RECEIVE_BUFFER;
  }
  else if (receivePhase == RECEIVE_BUFFER){
    if (clockRaised == false && digitalRead(CLOCK_RECEIVE) == HIGH) clockRaised = true;
    else if (clockRaised == true && digitalRead(CLOCK_RECEIVE) == LOW) {
      receivePhase = RECEIVE_GREEN;
      clockRaised = false;
      Serial.println("CONNECTION ESTABLISHED; WAITING DATA.");
    }
  }
  else {
    //receivePhase = RECEIVE_CLOCK_WAIT;
    if (clockRaised == false) {
      if (digitalRead(CLOCK_RECEIVE) == HIGH) clockRaised = true;
    }
    else {
      if (digitalRead(CLOCK_RECEIVE) == LOW) {
        clockRaised = false;
        uint8_t bit = digitalRead(DRX);
        //Serial.println(bit);
        Buf |= (bit<<(7-receiveInd));
        receiveInd++;
        
        if (receiveInd >= 8) {
          //Serial.println(Buf, BIN);
          decode(Buf);
          Buf = 0;
          receiveInd = 0;
        }
      }
    }
  }
}