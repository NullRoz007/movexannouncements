#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include "time.h"

#include "./bytepack.h"
#include "./schedule.h"

#define EEPROM_SIZE   1
#define READ_CONFIG   0xAA
#define READ_DST      0x7A
#define WRITE_DST     0x7F
#define RESET_CONFIG  0x8A
#define WRITE_CONFIG  0x80

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
(byte & 0x80 ? '1' : '0'), \
(byte & 0x40 ? '1' : '0'), \
(byte & 0x20 ? '1' : '0'), \
(byte & 0x10 ? '1' : '0'), \
(byte & 0x08 ? '1' : '0'), \
(byte & 0x04 ? '1' : '0'), \
(byte & 0x02 ? '1' : '0'), \
(byte & 0x01 ? '1' : '0') 

// WiFi network name and password:
const char *networkName = "flipoutwap1";
const char *networkPswd = "trampoline";

// WiFi network name and password:
//const char * networkName = "Up Dog?";
//const char * networkPswd = "pringles69";

WiFiServer config_server(80);

//IP address to send UDP data to:
const char *udpAddress = "192.168.1.255"; // broadcast the data to all devices on network
const int udpPort = 4444;

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 11 * 60 * 60; // the 3600 is for daylight savings time //remove when going back to std time
const int daylightOffset_sec = 60*60;

#define opening_hour 9 // opening at 9am
bool first_time_through = true;

boolean connected = false;
int saved_time_pointer = -1;
int current_time_pointer = -1;

//The udp library class
WiFiUDP udp;
int length_of_UDP_Command = 18;

void set_day_active(int d) {
  char packed = EEPROM.read(0);
  bool* unpacked = unpack(packed, 8);

  unpacked[d] = 1;
  packed = pack(unpacked, 8);

  EEPROM.write(0, packed);
  EEPROM.commit();
}

void set_day_inactive(int d) {
  char packed = EEPROM.read(0);
  bool* unpacked = unpack(packed, 8);

  unpacked[d] = 0;
  packed = pack(unpacked, 8);

  EEPROM.write(0, packed);
  EEPROM.commit();
}

int get_time_offset() {
  char offset = EEPROM.read(1);
  return (int)offset;
}

void set_time_offset(int o) {
  EEPROM.write(1, o);
  EEPROM.commit();
}

bool is_day_active(int d) {
  char packed = EEPROM.read(0);
  bool* unpacked = unpack(packed, 8);
  print_array(unpacked, 8);
  return unpacked[d] == 1;
}

bool validate_config(char conf) {
  char mask = 1 << 7;
  Serial.println("Validating Config: ");
  bool* unpacked = unpack(conf, 8);
  print_array(unpacked, 8);
  if(unpacked[7] == 1) { 
    Serial.println("invalid config: error bit was set.");
    return false;
  } 
  
  free(unpacked);

  if(conf == 0) { 
    Serial.println("invalid config: conf == 0");
    return false;
  } 

  return true;
}


void setTimezone(String timezone){
  Serial.printf("  Setting Timezone to %s\n",timezone.c_str());
  setenv("TZ",timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

void initTime(String timezone){
//struct tm timeinfo;

Serial.println("Setting up time");
  configTime(0, 0, ntpServer);    // First connect to NTP server, with 0 TZ offset
  if(!getLocalTime(&timeinfo)){
    Serial.println("  Failed to obtain time");
    return;
  }
  Serial.println("  Got the time from NTP");
  // Now we can set the real timezone
  setTimezone(timezone);
}

void printLocalTime1(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time 1");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
}


//wifi event handler
void WiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case SYSTEM_EVENT_STA_GOT_IP:
    //When connected set
    Serial.print("WiFi connected! IP address: ");
    Serial.println(WiFi.localIP());
    //initializes the UDP state
    //This initializes the transfer buffer
    udp.begin(WiFi.localIP(), udpPort);
    connected = true;
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    Serial.println("WiFi lost connection");
    connected = false;
    break;
  }
}

void send_wristband_command(char *command_tosend, byte sizeofcommand)
{
  Serial.print("Sending command");
  for (int i = 0; i < strlen(command_tosend); i++)
  {
    Serial.print(command_tosend[i]);
  }

  Serial.println(" to screens");

  udp.beginPacket(udpAddress, udpPort);

  for (int i = 0; i < strlen(command_tosend); i++)
  {
    udp.write((uint8_t)command_tosend[i]);
  }
  //char  command_send[] = {" "};

  //udp.write(command_tosend, sizeofcommand);
  //  udp.write(red_command, sizeofcommand);
  //  Serial.println(sizeofcommand);
  udp.endPacket();
}

int find_last_sun_of_month(int year, int month) { //returns the last sunday of a given month
  int y, m, d;  
  int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  days[1]   -= (y % 4) ||       //account for leap years
                (!(y % 100) &&
                (y % 400));    
  m = month;
  d = days[m - 1];    
  y = year;

  //convert to gregorian so we can use the day
  int day = days[m-1] - ((d += m < 3 ? y-- : y - 2, 23 * m / 9 + d + 4 + y / 4 - y / 100 + y / 400 ) %7); //Shoutout Michael Keith & Craver (1990)
  return day;
} 

/*
Daylight saving starts	Daylight saving ends
26 September 2021	         3 April 2022
25 September 2022	         2 April 2023
24 September 2023	         7 April 2024 */
int find_local_hour_offset() // returns the offset from GMT
{
  return 1;
}


int check_time() // returns an index into the colour array
{
  struct tm timeinfo;
  int colour_pointer = -1;
  //send_wristband_command ( time_command_array[0], number_of_colours);

  if (getLocalTime(&timeinfo))
  {
    int daylight_hour_offset = find_local_hour_offset();
    int number_half_hour_since_opening = ((timeinfo.tm_hour + daylight_hour_offset)-opening_hour) * 2; // this gives us the point into the array ;
    Serial.print("timeof day = ");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    Serial.print("current hour = ");
    Serial.println(timeinfo.tm_hour);
    Serial.print("Find_local_hour_offset = ");
    Serial.println(daylight_hour_offset);
    Serial.print( "Number of half hours since opening = ");
    Serial.println( number_half_hour_since_opening);
    Serial.print("colour pointer = ");
    Serial.println(colour_pointer);

    //  starts at 9am
    if ((number_half_hour_since_opening >= 0) && (number_half_hour_since_opening < number_of_colours))
    {
      if ((timeinfo.tm_min == 30) || (timeinfo.tm_min == 0))
      //delay(120000); //delay 2 minutes
      {
        if (timeinfo.tm_min == 30)
        {
          colour_pointer = number_half_hour_since_opening + 1; // becasue we are on the 1/2 hour we need to go one more with the pointer
        }
        else
        {
          colour_pointer = number_half_hour_since_opening;
        }
      }
    }
  }
  Serial.print(" returned colour pointer = ");
  Serial.println(colour_pointer);

  if(!is_day_active(timeinfo.tm_wday)) {
    Serial.printf("day %d is not active, colour_pointer = -1.\n", timeinfo.tm_wday);
    return -1;
  } else {
    Serial.printf("day %d is active, calculating colour_pointer.\n", timeinfo.tm_wday);
  }

  return (colour_pointer);
}

void connectToWiFi(const char *ssid, const char *pwd)
{
  Serial.println("Connecting to WiFi network: " + String(ssid));

  // delete old config
  WiFi.disconnect(true);
  //register event handler
  WiFi.onEvent(WiFiEvent);

  //Initiate connection
  WiFi.begin(ssid, pwd);

  Serial.print("Waiting for WIFI connection on ");
  Serial.println(ssid);
}

void printLocalTime()
{
  // struct tm timeinfo;
  
  if (!getLocalTime(&timeinfo))

  {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S not accounting for Daylight Saving time");
  
  int daylight_hour_offset = find_local_hour_offset();
  Serial.print ( "Hour offset from GMT is : ");
  Serial.println (daylight_hour_offset);
  Serial.print(" minutes = ");
  Serial.println(timeinfo.tm_min);
  if ((timeinfo.tm_wday == 0) || (timeinfo.tm_wday == 6)) // if today is saturday or sunday
  {
    Serial.println("It is a weekend schedule ...");
  }
  else
  {
    Serial.println("Today is a weekday schedule ...");
  }
}

void setup()
{
  // Initilize hardware serial:
  Serial.begin(9600);


  //Connect to the WiFi network
  connectToWiFi(networkName, networkPswd);
  EEPROM.begin(EEPROM_SIZE);

  if(!validate_config(EEPROM.read(0))) {
    Serial.println("Config in EEPROM is missing or invalid, flashing default configuration...");
    char packed = pack(days_array, 8);
    EEPROM.write(0, packed);
    EEPROM.commit();
    if(EEPROM.read(0) > 0) Serial.println("Changes commited to flash.");
    else {
      Serial.println("Failed to flash configuration!!");
    }
  }

  char config = EEPROM.read(0);
  bool* uconf = unpack(config, 8);
  Serial.print("Currenty Active Config: ");
  print_array(uconf, 8); 
  delay(5000);    
  // initTime("NZST-12NZDT,M9.5.0,M4.1.0/3");   // Set for Auckland NZ
  //init and get the time


  config_server.begin();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(1000);
  printLocalTime();

  
}


char last_byte = 0x00;
void process_udp_data() {
  WiFiClient client = config_server.available();
  if(client) {
    Serial.println("Recieving Bytes...");
    while(client.connected()) {
      while(client.available() > 0) {
        char c = client.read();

        if(c == last_byte) break;



        if(c) {
          Serial.printf("C = %2X\n", c);
          Serial.printf("last_byte = %2X\n", last_byte);

          if(c == RESET_CONFIG) {
            Serial.println("Resetting config in EEPROM...");
            EEPROM.write(0, 0x82);
            EEPROM.commit();
            Serial.println("Wrote 0x82 to EEPROM.");
            client.write(EEPROM.read(0));            
            client.stop();
            break;
          }

          if(c == READ_CONFIG) {
            Serial.println("Sending config to client...");
            char conf = EEPROM.read(0);
            client.write(conf);
            client.stop();
            break;
          }

          if(c == READ_DST) {
            Serial.println("Sending DST to client...");
            int dst = find_local_hour_offset();
            client.write(dst);
            client.stop();
            break;
          }

          if(last_byte == WRITE_CONFIG) {
            bool* arr = unpack(c, 8);
            EEPROM.write(0, c);
            EEPROM.commit();
            Serial.println("Wrote data to EEPROM: ");
            print_array(arr, 8);
            
            for(int i = 0; i < 8; i++) {
              days_array[i] = arr[i];
            }

            last_byte = 0x00;
            client.write(EEPROM.read(0));
            client.stop();
          }

          //manual time offset handler:
          int low_mask = 0xF;
          printf("MASKED LOW: %2X\n", c & low_mask);
          if((c & low_mask) == low_mask) {
              char sft = c >> 7;
              char masked = sft & 1;

              printf("SFT: %2X\n", masked);
          }

          last_byte = c;
        }            
      }

      delay(10);
    }

    client.stop();
  }
}

void loop()
{
  //only send data when connected
  if (connected)
  {
    //check for new config packet 
    
    //Send a packet
    // check to see if the current day and time is within the schedule for announcements.
    printLocalTime();

    current_time_pointer = check_time(); // check to see if it is time to

    //send_wristband_command ( time_command_array[current_time_pointer], number_of_colours);
    //delay(60000);
    
    if (((current_time_pointer != -1) && (saved_time_pointer != current_time_pointer))
        || (first_time_through))
    {
      // check to see what schedule is applicable for today
      if ((timeinfo.tm_wday == 0) || (timeinfo.tm_wday == 6)) // if today is saturday or sunday
      {
        send_wristband_command(weekend_time_command_array[current_time_pointer], number_of_colours);
      }
      else // today is a weekday
      {
        send_wristband_command(week_time_command_array[current_time_pointer], number_of_colours);
      }
      // send_wristband_command ( red_command, number_of_colours);
      saved_time_pointer = current_time_pointer;
    }
    /*
    Serial.println ( "Sending red command to screens");
  send_wristband_command(red_command, sizeof(red_command));
  delay(20000);
  Serial.println ( "Sending blue command to screens");
  send_wristband_command(blue_command, sizeof(blue_command));
  delay(20000);
  Serial.println ( "Sending green command to screens");
  send_wristband_command(green_command, sizeof(green_command));
  delay(20000);
  */
    int moment = millis();
    while(millis() < moment + 10000) {
      process_udp_data();
    }

    first_time_through = false;
    //printLocalTime();
  }
}

