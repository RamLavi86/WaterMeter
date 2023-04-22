/*
  Rui Santos
  Complete project details at:
   - ESP32: https://RandomNerdTutorials.com/esp32-send-email-smtp-server-arduino-ide/
   - ESP8266: https://RandomNerdTutorials.com/esp8266-nodemcu-send-email-smtp-server-arduino/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
  Example adapted from: https://github.com/mobizt/ESP-Mail-Client
*/

// To send Emails using Gmail on port 465 (SSL), you need to create an app password: https://support.google.com/accounts/answer/185833

#include <Arduino.h>
#include <EEPROM.h>
/*
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
*/
#include <WiFi.h>
#include <ESP_Mail_Client.h>

#define WIFI_SSID "Lavi-Home-5GHz"
#define WIFI_PASSWORD "0528200151"

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

#define ONBOARD_LED 2
#define WATER_METER_INPUT 5

// tajhvtpswnyxtmvu
// Double22

/* The sign in credentials */
#define AUTHOR_EMAIL "ramlavipushnotification@gmail.com"
#define AUTHOR_PASSWORD "tajhvtpswnyxtmvu"

/* Recipient's email*/
#define RECIPIENT_EMAIL "lavi.ram86@gmail.com"

#define MIN_IN_DAY 1440
#define MINUTE_IN_MICRO 60000000

struct digInput {
	const uint8_t PIN;
	uint32_t count;
	bool rise;
};

digInput waterMeter = {WATER_METER_INPUT, 0, false};

uint32_t counterList[MIN_IN_DAY];

int minutesCounterDailyCount = 0;

//variables to keep track of the timing of recent interrupts
unsigned long input_time = 0;  
unsigned long last_input_time = 0;

hw_timer_t *minuteTimer = NULL;
bool minuteIsrFlag = false;

/* The SMTP Session object used for Email sending */
SMTPSession smtp;

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);

/* Connect to WiFi function */
void connectToWifi();

/* Send E-mail contain the time frame (minutes) and water count (waterCount) */
void sendEmail(uint32_t minutes, uint32_t waterCount);

/* water mete interrupt service routine */
void IRAM_ATTR wm_isr();

/* zeros all counter places */
void zerosCounterList();

/* insert this minute read */
void insertToCounterList(uint32_t *tCounterList, uint32_t tCounter);

/* Write Uint32_t to flash */
void writeUint32ToFlash(int startAddress, uint32_t num);

/* Read Uint32_t from flash */
void readFlashToUint32(int startAddress, uint32_t *num);

/* */
void IRAM_ATTR minuteISR();

void setup(){

  Serial.begin(9600);
  EEPROM.begin(512);
  Serial.println();
  
  waterMeter.count = EEPROM.readUInt(0);

  pinMode(ONBOARD_LED,OUTPUT);

  pinMode(waterMeter.PIN, INPUT_PULLUP);
  attachInterrupt(waterMeter.PIN, wm_isr, RISING);

  minuteTimer = timerBegin(0, 80 ,true);
  timerAttachInterrupt(minuteTimer, &minuteISR, true);
  timerAlarmWrite(minuteTimer, MINUTE_IN_MICRO, true);
  timerAlarmEnable(minuteTimer);
}

void loop(){
  if (WiFi.status() != WL_CONNECTED){
    digitalWrite(ONBOARD_LED, LOW);
    connectToWifi();
  }
  else {
    digitalWrite(ONBOARD_LED, HIGH);
    if (waterMeter.rise){
      waterMeter.rise = false;
    }
  }
  if (minuteIsrFlag){ // performs every minute
    for (int i = 0 ; i < 10 ; i++){
      //Serial.printf("CounterList:");
      //Serial.printf("counterList[%d] = %u", i , counterList[i]);
      //Serial.println();      
    }
    minutesCounterDailyCount++;
    if (minutesCounterDailyCount >= MIN_IN_DAY){
      sendEmail(MIN_IN_DAY, (counterList[0] - counterList[MIN_IN_DAY-1]));
      minutesCounterDailyCount = 0;      
    }
    Serial.printf("minutesCounterDailyCount = %d, counterList[0] = %d, counterList[MIN_IN_DAY-1] = %d",minutesCounterDailyCount,counterList[0],counterList[MIN_IN_DAY-1]);
    Serial.println();
    minuteIsrFlag = false;

    // Write to EEPROM every 60 minutes
    if (minutesCounterDailyCount % 60 == 0){
      EEPROM.writeUInt(0,waterMeter.count);
      EEPROM.commit();
      Serial.printf("write water meter count to EEPROM = %u",waterMeter.count);
      Serial.println();
    }
    if (minutesCounterDailyCount % 10 == 2){
      Serial.printf("read water meter count from EEPROM = %u",EEPROM.readUInt(0));
      Serial.println();
    }
  }


}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status){
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success()){
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++){
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      //ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
      //ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients);
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject);
    }
    Serial.println("----------------\n");
  }
}

void connectToWifi(){
  Serial.print("Connecting to AP");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(200);
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}

void sendEmail(uint32_t minutes, uint32_t waterCount){
  /** Enable the debug via Serial port
   * none debug or 0
   * basic debug or 1
  */
  smtp.debug(1);

  /* Set the callback function to get the sending results */
  smtp.callback(smtpCallback);

  /* Declare the session config data */
  ESP_Mail_Session session;

  /* Set the session config */
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";

  /* Declare the message class */
  SMTP_Message message;

  /* Set the message headers */
  message.sender.name = "Ram Lavi";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "Water meter message";
  message.addRecipient("Admin", RECIPIENT_EMAIL);

  /*Send HTML message*/
  String htmlMsg1 = "<div style=\"color:#2f4468;\"><h1>Water meter message:</h1><p>Last ";
  String htmlMsg2 = String(minutes);
  String htmlMsg3 = " minutes water count was ";
  String htmlMsg4 = String(waterCount);
  String htmlMsg5 = "</p></div>";
  String htmlMsg = htmlMsg1 + htmlMsg2 + htmlMsg3 + htmlMsg4 + htmlMsg5;
  message.html.content = htmlMsg.c_str();
  //message.html.content = htmlMsg.c_str();
  message.text.charSet = "us-ascii";
  message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  /*
  //Send raw text message
  String textMsg = "Hello World! - Sent from ESP board";
  message.text.content = textMsg.c_str();
  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;
  message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;*/

  /* Set the custom message header */
  //message.addHeader("Message-ID: <abcde.fghij@gmail.com>");

  /* Connect to server with the session config */
  if (!smtp.connect(&session))
    return;

  /* Start sending Email and close the session */
  if (!MailClient.sendMail(&smtp, &message))
    Serial.println("Error sending Email, " + smtp.errorReason());
}

void IRAM_ATTR wm_isr() {
  input_time = millis();
  if (input_time - last_input_time > 250)
  {
    if (waterMeter.count >= INT32_MAX){
      waterMeter.count = 0;
    }
	  waterMeter.count++;
	  waterMeter.rise = true;
    //Serial.printf("water meter count is %u", waterMeter.count);
    //Serial.println();
    last_input_time = input_time;
  }
}

void zerosCounterList(){
  for (int i = 0 ; i < MIN_IN_DAY ; i++){
    counterList[i] = waterMeter.count;
  }
}

/* insert this minute read */
void insertToCounterList(uint32_t *tCounterList, uint32_t tCounter){
  for (int i = MIN_IN_DAY ; i > 0 ; i--){
    tCounterList[i] = tCounterList[i-1];
  }
  counterList[0] = tCounter;
}

void IRAM_ATTR minuteISR(){
  insertToCounterList(counterList, waterMeter.count);
  minuteIsrFlag = true;
}