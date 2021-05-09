/* This repository enables you to use an old rotary dial house telephone as a mobile phone.
 * 
 * Follow the instruction on https://github.com/rtorsvik/retr-o-phone-v2 to build your own
 * And, remember to put in your SIM card PIN number, and your default country code in the //definitions below
 * 
 * Per today, buzzer functionality is not implemented
 * 
 * @author: Rein Åsmund Torsvik, 2021-05-09
 */

//include
//____________________________________________________________________________________________________
#include "Adafruit_FONA.h"
#include <SoftwareSerial.h>


//definitions
//____________________________________________________________________________________________________
char SIM_PIN[4] = "1234"; //enter your SIM card pin number here
String default_country_code_str = "47"; //NOR

#define DEBOUNCEDELAY 10 //[ms]
#define T_IDLE_BEFORE_CALLING 4000 //[ms] millieconds after last dialed number before initiating call

#define DEBUG true

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

//fona
char replybuffer[255];
uint8_t fona_type;

bool incoming_call = false;
long incoming_call_timeout = 0;


//state handling variables
enum phone_state {
    IDLE,
    DIALING,
    CALLING,
    INCOMINGCALL
};
phone_state state = IDLE;

bool idle_setup_complete = false;
bool dialing_setup_complete = false;
bool calling_setup_complete = false;
bool incomingcall_setup_complete = false;

//debouncing
bool pin_pulse_state = false;
bool pin_pulse_state_prev = false;
long pin_pulse_ms_last = 0;

bool pin_counting_state = false;
bool pin_counting_state_prev = false;
long pin_counting_ms_last = 0;

//dialing variables
bool pulse = false;
bool counting = false;
int number = 0;

int phone_number[16];
int phone_number_index = 0;

long last_time_counting = 0; 

//phone number variables
String phone_number_str = "";
char incoming_phone_number[32] = {0};

//others
long call_status_ms_last = 0;
int cnt = 0;




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

    //attachInterrupt(digitalPinToInterrupt(PIN_FONA_RI_INTERRUPT), test_high, RISING);
    //attachInterrupt(digitalPinToInterrupt(PIN_FONA_RI_INTERRUPT), test_low, FALLING);


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
    fona.write("AT+CMEDIAVOL=50");

    Serial.println(F("Initializing FONA...\t[SUCCESS] FONA initialized"));
  }
  

  
  

  Serial.println("\n\nInitialization complete\nRed five, standing by...");
}


//loop
//____________________________________________________________________________________________________
void loop()
{
    //state handling
    switch (state)
    {
        case IDLE:
            //idle setup
            if(!idle_setup_complete)
            {
                dialing_setup_complete = false;
                calling_setup_complete = false;
                incomingcall_setup_complete = false;

                //hang up
                Serial.println("[info] Hanging up");
                fona.hangUp();

                //stop playing dial tone
                fona.playToolkitTone(1, 10);

                //reset variables
                for(int i=0; i<16; i++)
                    phone_number[i] = 0;
                phone_number_index = 0;
                phone_number_str = "";


                //setup complete
                if(DEBUG) Serial.println("[info] STATE=IDLE. Entered idle state");
                idle_setup_complete = true;
            }

            //idle loop
            //if incoming call, enter incoming call state
            if(!digitalRead(PIN_FONA_RI_INTERRUPT))
            {
                state = INCOMINGCALL;
                delay(10); //kind of a debounce delay, remove later
            }

            //if handset is lifted, enter dialing state 
            if(digitalRead(PIN_HANGUP))
            {
                state = DIALING;
                delay(10); //kind of a debounce delay, remove later
            }

            break;
        
        case DIALING:
            //dialing setup
            if(!dialing_setup_complete)
            {
                idle_setup_complete = false;
                calling_setup_complete = false;
                incomingcall_setup_complete = false;

                //Start playing dial tone
                fona.playToolkitTone(1, 60000);


                //setup complete
                if(DEBUG) Serial.println("[info] STATE=DIALING. You are now ready to dial your friends number");
                dialing_setup_complete = true;
            }

            //dialing loop
            //start counting
            
            //pin_pulse debouncing
            pin_pulse_state = digitalRead(PIN_PULSE);
            if(pin_pulse_state != pin_pulse_state_prev)
            {
                pin_pulse_ms_last = millis();
            }
            pin_pulse_state_prev = pin_pulse_state;
            
            //if pulse pin change accepted
            if((millis() - pin_pulse_ms_last) > DEBOUNCEDELAY)
            {
                if(pin_pulse_state != pulse)
                {
                    pulse = pin_pulse_state;
                
                    if(pulse)
                        number--;
                }
            }

            //pin_counting debounce
            pin_counting_state = !digitalRead(PIN_COUNTING);
            if(pin_counting_state != pin_counting_state_prev)
            {
                pin_counting_ms_last = millis();
            }
            pin_counting_state_prev = pin_counting_state;

            //if counting pin change accepted
            if((millis() - pin_counting_ms_last) > DEBOUNCEDELAY)
            {
                if(pin_counting_state != counting)
                {
                    counting = pin_counting_state;

                    if(counting) //reset counter before counting
                    {
                        number = 10;
                        if(DEBUG) Serial.println("[info] reading digit");
                    }
                    else //done counting, transfer number
                    {
                        phone_number[phone_number_index++] = number;
                        last_time_counting = millis();

                        if(DEBUG) 
                        {
                            Serial.print("[info] finished reading digit (");
                            Serial.print(number);
                            Serial.print("). ");
                            Serial.print("the dialed number so far: ");
                            for (int i = 0; i < phone_number_index; i++)
                            {
                                Serial.print(phone_number[i]);
                            }
                            Serial.println();
                        }
                    } 
                }
            }

            //if done dialing, prepare and call (T_IDLE_BEFORE_CALLING milliseconds idle after dialing)
            if(phone_number_index > 0 && millis() > last_time_counting + T_IDLE_BEFORE_CALLING)
            {
                if(DEBUG) Serial.println("[info] ready to call");

                //stop playing dial tone
                fona.playToolkitTone(20, 10);

                //format phone number
                //check if country code is not dialed, if not, add it
                phone_number_str = "";
                if(!(phone_number[0] == 0))
                {
                    phone_number_str += "00";               
                    phone_number_str += default_country_code_str;
                }
                
                //add rest of the dialed digits to the number string
                for(int i = 0; i < phone_number_index; i++)
                    phone_number_str += phone_number[i];

                Serial.print("[info] Attempting to call ");
                Serial.println(phone_number_str);

                int str_len = phone_number_str.length() + 1;  
                char phone_number_char[str_len];
                phone_number_str.toCharArray(phone_number_char, str_len);
             
                if (!fona.callPhone(phone_number_char)) 
                    Serial.println(F("[error] Call failed"));
                else
                {
                    Serial.print(F("[info] Calling "));
                    Serial.println(phone_number_str);
                }

                state = CALLING;
                    
            }

            //if handset down, reenter idle state
            if(!digitalRead(PIN_HANGUP))
            {
                state = IDLE;
                delay(10); //kind of a debounce delay, remove later
            }


                

            break;

        case CALLING:
            //calling setup
            if(!calling_setup_complete)
            {
                idle_setup_complete = false;
                dialing_setup_complete = false;
                incomingcall_setup_complete = false;
                
                

                //setup complete
                if(DEBUG) Serial.println("[info] STATE=CALLING. Enjoy your conversation :-)))");
                calling_setup_complete = true;
            }

            //calling loop

            //get call status. 0=ready, 1=could not get status, 3=ring (incoming call), 4=in a call
            uint8_t call_status;
            if(millis() > call_status_ms_last + 1000)
            {
                call_status = fona.getCallStatus();
                call_status_ms_last = millis();
            }
                
            //if handset down, or the one at the other end of the call hangs up, enter idle state
            if(!digitalRead(PIN_HANGUP) || call_status == 0)
            {
                state = IDLE;
                delay(10); //kind of a debounce delay, remove later
            }


            break;

        case INCOMINGCALL:
            //incoing call setup
            if(!incomingcall_setup_complete)
            {
                idle_setup_complete = false;
                dialing_setup_complete = false;
                calling_setup_complete = false;

                //play ring tone
                //fona.playToolkitTone(8, 60000);

                //setup complete
                if(DEBUG) Serial.println("[info] STATE=INCOMINGCALL. Someone is calling you, pleas pick up.");
                incomingcall_setup_complete = true;
            }

            //if handset up, answer phone and enter CALLING state
            if(digitalRead(PIN_HANGUP))
            {
                //answer call 
                fona.pickUp();

                state = CALLING;
                delay(10); //kind of a debounce delay, remove later
            }

            //if caller hangs up before the phone is piched up, reenter IDLE state
            else if(digitalRead(PIN_FONA_RI_INTERRUPT))
            {
                if(DEBUG) Serial.println("[info] Caller hung up.");

                state = IDLE;
                delay(10); //kind of a debounce delay, remove later
            }

            
              
            break;

        default:
            break;
    }
   
    //simulate serial com when tether is not connected
    delay(2);
    
}

void test_high()
{
    Serial.println("###########################TEST HIGH");
}

void test_low()
{
    Serial.println("###########################TEST LOW");
}
