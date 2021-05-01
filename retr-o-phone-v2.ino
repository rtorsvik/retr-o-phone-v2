/*
 *
 * 
 * @author: Rein Åsmund Torsvik
 */

//include
//____________________________________________________________________________________________________
#include "Adafruit_FONA.h"
#include <SoftwareSerial.h>


//definitions
//____________________________________________________________________________________________________
char SIM_PIN[4] = "7515";

#define DEBOUNCEDELAY 50 //[ms]
#define T_IDLE_BEFORE_CALLING 4000 //[ms] millieconds after last dialed number before initiating call

//pinout
//____________________________________________________________________________________________________
#define PIN_PULSE 11
#define PIN_COUNTING 12
#define PIN_HANGUP 2
#define PIN_BUZZER 3

#define PIN_FONA_RX 9
#define PIN_FONA_TX 8
#define PIN_FONA_RI_INTERRUPT 7
#define PIN_FONA_RST 4




//objects
//____________________________________________________________________________________________________
SoftwareSerial fonaSS = SoftwareSerial(PIN_FONA_TX, PIN_FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;
Adafruit_FONA fona = Adafruit_FONA(PIN_FONA_RST);

//variables
//____________________________________________________________________________________________________
//debouncing
bool pin_pulse_state = false;
bool pin_pulse_state_prev = false;
long pin_pulse_ms_last = 0;

bool pin_counting_state = false;
bool pin_counting_state_prev = false;

//fona
char replybuffer[255];
uint8_t fona_type;

bool incoming_call = false;
long incoming_call_timeout = 0;


//others
int mode = 0; //0=standby/hangup, 1=dialing, 2=calling
bool dialing = false;
bool ready_to_call = false;

bool pulse = false;
bool counting = false;
int number = 10;

long last_time_counting = 0; 

int phone_number[16];
int phone_number_pointer = 0;

String default_country_code_str = "47"; //NOR
String phone_number_str = "";

long cnt = 0;



//setup
//____________________________________________________________________________________________________
void setup() {

  //pinmodes
  pinMode(PIN_PULSE, INPUT_PULLUP);
  pinMode(PIN_COUNTING, INPUT_PULLUP);
  pinMode(PIN_HANGUP, INPUT_PULLUP);
  pinMode(PIN_FONA_RI_INTERRUPT, INPUT);

  //initialize serial
  Serial.begin(115200);
  //while(!Serial){;}
  Serial.println("Initializing...\n");

  //fona
  Serial.print("Initializing FONA...\t"),
  fonaSerial->begin(9600);
  if(!fona.begin(*fonaSerial))
  {
    Serial.println("[FAILED] Could not find FONA module");
  }
  else
  {
    fona_type = fona.type();

    Serial.print(F("Initializing FONA...\tfound "));
    switch (fona_type) 
    {
      case FONA800L:
        Serial.println(F("FONA 800L")); break;
      case FONA800H:
        Serial.println(F("FONA 800H")); break;
      case FONA808_V1:
        Serial.println(F("FONA 808 (v1)")); break;
      case FONA808_V2:
        Serial.println(F("FONA 808 (v2)")); break;
      case FONA3G_A:
        Serial.println(F("FONA 3G (American)")); break;
      case FONA3G_E:
        Serial.println(F("FONA 3G (European)")); break;
      default: 
        Serial.println(F("???")); break;
    }

    //enable verbous error messages
    fona.write("AT+CMEE=2");

    //report imei  
    char imei[16] = {0}; // MUST use a 16 character buffer for IMEI!
    uint8_t imeiLen = fona.getIMEI(imei);
    if (imeiLen > 0) 
    {
      Serial.print("Initializing FONA...\tModule IMEI: "); Serial.println(imei);
    }

    //log in to simcard
    Serial.print("Initializing FONA...\tUnlocking SIM card, pin=");
    Serial.print(SIM_PIN);
    if (!fona.unlockSIM(SIM_PIN)) 
      Serial.println(F("Initializing FONA...\tfailed to unlock SIM"));
    else
      Serial.println(F("Initializing FONA...\tSIM card unlocked successfully"));

    //enable incoming call notifications
    if(fona.callerIdNotification(true, PIN_FONA_RI_INTERRUPT))
      Serial.println(F("Initializing FONA...\tCaller id notification enabled."));
    else
      Serial.println(F("Initializing FONA...\tCaller id notification disabled"));
    
    //attachInterrupt(digitalPinToInterrupt(PIN_FONA_RI_INTERRUPT), fitta, CHANGE);

    //report battery
    uint16_t vbat;
    uint16_t pbat;
    Serial.println("Initializing FONA...\treading battery status ");
    if (!fona.getBattVoltage(&vbat) || !fona.getBattPercent(&pbat)) 
    {
      Serial.println("Initializing FONA...\tfailed to read battery status");
    }
    else
    {
      Serial.print("Initializing FONA...\tBattery status: ");
      Serial.print(F("V=")); Serial.print(vbat); Serial.print(F("mV"));
      Serial.print(F("P=")); Serial.print(pbat); Serial.println(F("%"));
    }

    //set audio to external output
    Serial.println("Initializing FONA...\tsetting audio to external speaker");
    fona.setAudio(1);

    //read audio volume level
    uint8_t volume = fona.getVolume();
    Serial.print("Initializing FONA...\taudio volume=");
    Serial.println(volume);

    fona.setVolume(50);
    fona.write("AT+CMEDIAVOL=100");

    Serial.println(F("Initializing FONA...\t[SUCCESS] FONA initialized"));
  }
  

  
  

  Serial.println("\n\nInitialization complete\nRed five, standing by...");
}


//loop
//____________________________________________________________________________________________________
void loop() 
{

  /*
  while (Serial.available()) 
  {
    delay(1);
    fona.write(Serial.read());
  }

  
  if (fona.available()) {
    Serial.write(fona.read());
  }
  */
  

  cnt++;
  

  //handle modes
  //mode=standby
  if(!digitalRead(PIN_HANGUP) == HIGH)
  {
    //do once
    if(mode != 0)
    {
      mode = 0;

      //hang up
      Serial.println("Hanging up");
      fona.hangUp();

      //stop playing dial tone
      fona.playToolkitTone(1, 10);
      fona.setPWM(2000, 0);
      
      //reset variables
      for(int i=0; i<16; i++)
        phone_number[i] = 0;
      phone_number_pointer = 0;
      phone_number_str = "";
      ready_to_call = false;
      incoming_call=false;

    }


    //check for incoming calls
    char incoming_phone_number[32] = {0};
    if(fona.incomingCallNumber(incoming_phone_number) && incoming_call == false)
    {
      Serial.println(F("RING PLING!"));
      Serial.print(F("Phone Number: "));
      Serial.println(incoming_phone_number);

      incoming_call = true;
      incoming_call_timeout = millis();

      //play ring tone
      fona.playToolkitTone(8, 60000);
    }
    

    //incoming call timeout
    else if (millis() > incoming_call_timeout + 20000 && incoming_call == true)
    {
      Serial.println(F("incoming call timed out"));

      incoming_call = false;

      //stop play ring tone
      fona.playToolkitTone(8, 10);
    }
    
    

    //vibrate if incoming call
    /*
    if(incoming_call)
    {
      if(int(millis()/1000) % 2 == 0)
        fona.setPWM(2000, 50);
      else
        fona.setPWM(2000, 0);
    } 
    */
      
  }

  //mode=handset up
  else if(!digitalRead(PIN_HANGUP) == LOW)
  {
    //do once
    if(mode != 1)
    {
      mode = 1;

      Serial.println("Handset up");

      

      //check for incoming calls, if true, pick up
      if(incoming_call)
      {
        Serial.println("Picked up call");

        //stop play ring tone
        fona.playToolkitTone(8, 10);
        fona.setPWM(2000, 0);
        fona.pickUp();
      }
      //else, start dialing
      else
      {
        Serial.println("Start dialing");

        //Start playing dial tone
        fona.playToolkitTone(1, 60000);
        
        dialing = true;
      }
      
    }

    if(dialing)
    {
      //read phone number
      //pin_pulse debouncing
      pin_pulse_state = digitalRead(PIN_PULSE);
      if(pin_pulse_state != pin_pulse_state_prev)
      {
        pin_pulse_ms_last = millis();
      }
      pin_pulse_state_prev = pin_pulse_state;
    
      //if pulse accepted
      if((millis() - pin_pulse_ms_last) > DEBOUNCEDELAY)
      {
        if(pin_pulse_state != pulse)
        {
          pulse = pin_pulse_state;
    
          if(pulse)
            number--;
        }
      }
    
      pin_counting_state = !digitalRead(PIN_COUNTING);
      if(pin_counting_state != pin_counting_state_prev)
      {
        counting = pin_counting_state;
        if(counting) //reset counter before counting
          number = 10;
        else //done counting, transfer number
        {
          phone_number[phone_number_pointer++] = number;
          last_time_counting = millis();
        }
        
      }
      pin_counting_state_prev = pin_counting_state;

      //if done dialing (4 sec idle after dialing)
      if(phone_number_pointer > 0 && millis() > last_time_counting + T_IDLE_BEFORE_CALLING)
      {
        //do once
        if(!ready_to_call)
        {
          ready_to_call = true;
          dialing = false;

          //format number
          //check if country code is not dialed, if not, add it
          phone_number_str = "";
          if(!(phone_number[0] == 0 && phone_number[1] == 0))
          {
            phone_number_str += "00";
            phone_number_str += default_country_code_str;
          }

          for(int i = 0; i < phone_number_pointer; i++)
            phone_number_str += phone_number[i];

          //stop playing dial tone
          fona.playToolkitTone(20, 10);

          Serial.println("Calling ");
          Serial.println(phone_number_str);

          int str_len = phone_number_str.length() + 1;  
          char phone_number_char[str_len];
          phone_number_str.toCharArray(phone_number_char, str_len);

          if (!fona.callPhone(phone_number_char)) 
            Serial.println(F("Failed"));
          else
            Serial.println(F("calling!"));

        }

      }

    }
    
  }

  

  



  

  


  /*
  Serial.print("c=");
  Serial.print(digitalRead(pin_counting));
  Serial.print(" p=");
  Serial.print(digitalRead(pin_pulse));
  Serial.print(" h=");
  Serial.print(digitalRead(pin_hangup));
  Serial.print(" C=");
  Serial.print(counting);
  Serial.print(" P=");
  Serial.print(pulse);
  Serial.print(" n=");
  Serial.print(number);
  Serial.print(" mode=");
  if(mode==0)
    Serial.print("hangup");
  else if(mode==1)
    Serial.print("dialing");
  else if(mode==2)
    Serial.print("calling");
  else
    Serial.print("unknown");
  

  if(ready_to_call)
  {
    Serial.print(" calling=");
    Serial.print(phone_number_str);
    Serial.println("");
  }
  else
  {
    Serial.print(" N=[");
    for(int i = 0; i < 12; i++)
    {
      Serial.print(phone_number[i]);
    }
    Serial.println("]");
  }
  */
  
  

  



  
 delay(1);
 
}

void fitta()
{
  Serial.println("FITTA");
  incoming_call = true;
}
