/*  HTTPS on ESP8266 with follow redirects, chunked encoding support
 *  Version 3.0
 *  Author: Sujay Phadke
 *  Github: @electronicsguy
 *  Copyright (C) 2018 Sujay Phadke <electronicsguy123@gmail.com>
 *  All rights reserved.
 *
 *  Example Arduino program
 */
/*
 * Se connecte en WIFI au reséau locale
 * fait une requete Get sur google script
 * ce dernier recherche dans un tableau la ligne correspondant à la date du jour
 * Arrosage   Idx Nb jour Heure départ  durée (s) tempo durée
 * 01/01/2022  1  3       18            10  
 * 01/02/2022  2  3       18            10  
 * et retourne les entêtes puis les valeurs au format JSON
 * "Idx,Nb jour,Heure départ,durée (s),tempo durée,;1,3,18,10,"
 * dans cet exemple, on arrose tout les 3 jours, à 18h, pendant 10s
 * 
 * L'appli boot
 * regarde s'il est temps de travailler grace au compteur dans la mémoire rtc
 * si non, décrémente le compteur dans la mémoire rtc et s'endort pendant 4 s
 * si on y est presque, décrémente le compteur dans la mémoire rtc et active le WIFI pour le prochain réveil
 * si oui, 
 * se connecte au WIFI
 * récupère l'heure NTP
 * récupère les consignes d'arrosage sur google sheet, via google script doGet?
 * si on arrive pas à se connecter, on utilise les info stocké dans la mémoire rtc
 * arrose pendant le temps prévu
 * calcul le compteur correspondant au nb de jour à rester en veille et l'enregistre dans la mémoire rtc 
 * enregistre dans la mémoire rtc les consignes d'arrosage
 * envoie le log de la dernière activité: date et temps d'arrosage via google script doGet?tps_on=10&comment=prochaine_val_3s 
 * s'endort
 */


#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "HTTPSRedirect.h"
#include "DebugMacros.h"
#include "MyData.h"

#define RTCMEMORYSTART 65 // 256 bytes system, 256 bytes user => start 65
#define MARKER_RTC 0x1ACF


char Ver[] = "Arrosage-wifi_03022022";
// Fill ssid and password with your network credentials
//const char* ssid = "mySsid";
//const char* password = "myPassword";
// const char fingerprint[] PROGMEM = "";

int LED = 2;

const char* host = "script.google.com";
// project : AKfycbwXH_KA_9lgZUGShkzJxYxU5DuTQht0hBcStch7f0_wsJIS7nM5j1Yto0w48ePeJK-s
// lanch script https://script.google.com/macros/s/AKfycbyS4fKU9SqgfREqdUAMWp2hOOGlVdBX0lXz_A_mdqD9SRp2yrabTY37DYuEZJ0XeEb_/exec // "Integ"
// lanch script https://script.google.com/macros/s/AKfycbxf_sIqI8Gb9OFpU5MSRX4kdWtaYYnz2-ovjOgN7gl_QFj5x2pDNIr1DbWcLaW16lDq/exec // "Integ2"
// test script en dev https://script.google.com/macros/s/AKfycbyYuSVsI_-sfBMaoF_GeZUvGi2gg6et_XwCMJWy26w/dev?p=sss
// google shhet : https://docs.google.com/spreadsheets/d/1P3OJiMyDQzGuwrHbno4i8qffbFtkZpRMuy0_7JVo0I0/edit#gid=0
//const char *GScriptId = "AKfycbyYuSVsI_-sfBMaoF_GeZUvGi2gg6et_XwCMJWy26w";  // dev mais ne fonctionne pas depuis ESP8266, rtn  "Connection to re-directed URL failed"
//const char *GScriptId = "AKfycbyS4fKU9SqgfREqdUAMWp2hOOGlVdBX0lXz_A_mdqD9SRp2yrabTY37DYuEZJ0XeEb_"; // deployé "Integ", rtn 302 "...n'est pas un type de réponse compatible"
//const char *GScriptId = "AKfycbxf_sIqI8Gb9OFpU5MSRX4kdWtaYYnz2-ovjOgN7gl_QFj5x2pDNIr1DbWcLaW16lDq"; // deployé "Integ2", rtn 302 "Connection to re-directed URL failed!"
//const char *GScriptId = "AKfycbz7Tb-VOYyJljMe-kbLhCeCYBx9sswynjB9jTud1I8GPB7dynR4t95Iwmhbsj-3TfnP"; // deployé "Integ3", rtn 200, OK avec header, style, content
//const char *GScriptId = "AKfycbzfaoNGfsZTEI5T2gYHou1_0TbqBXV2xUGOsynUeQ8mPo2MVke9Ih_9J8luqtuIozgA"; // deployé "Integ4", rtn 200, OK avec content
//const char *GScriptId = "AKfycbykVGCdorSVcTV7xyzuXm6lrdb-7t1VvHMAFu5DGHDm9x9WchgD82X6YfBw11YWzgts"; // deployé "Integ5", rtn 200, OK avec content html+json
const char *GScriptId = "AKfycbzo2p8sUyZVTnt4A_0XkEusHGGJH8xr8v0BCXRClypaU9ON2n0oi0xy9SxIApuT8kAq"; // deployé "Integ6", rtn 302, NOK avec content json




//const char *GScriptId = "AKfycbxhFVhM0TJHIui4tTXCQrjQSDw6Bx2jc3rzBkkuFwlpjtb0W4g"; // projet Client_google_sheet


//String url = String("/macros/s/") + GScriptId + "/dev?";
String url = String("/macros/s/") + GScriptId + "/exec?";
const int httpsPort = 443;
HTTPSRedirect* client = nullptr;

// NTP 
// https://lastminuteengineers.com/esp8266-ntp-server-date-time-tutorial/
// Installing NTP Client Library : https://lastminuteengineers.com/esp8266-ntp-server-date-time-tutorial/
// Define NTP Client to get time
const long utcOffsetInSeconds = 3600;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "fr.pool.ntp.org", utcOffsetInSeconds);

int volatile cpt_loop = 0;
const int time_sleep = 15*60; // en s
//const int time_sleep = 10; //  en s - TEST

typedef struct {
  int markerFlag;
  int cpt_loop;
  int nextDays;
  int nextHours;
  int duration;
} rtcStore __attribute__((aligned(4))); // not sure if the aligned stuff is necessary, some posts suggest it is for RTC memory?

rtcStore rtcMem;

typedef struct {
  int nextDays;
  int nextHours;
  int duration;
} tArrosCtx; 

void setup() {
  Serial.begin(115200);
  Serial.flush();

  D2PRINTLN("");
  D2PRINTLN("Setup");
  D1PRINT("Version : ");
  D1PRINTLN(Ver);

  pinMode(LED, OUTPUT);

  //  Force the ESP into client-only mode
  //WiFi.mode(WIFI_STA); 

  //  Enable light sleep
  //wifi_set_sleep_type(LIGHT_SLEEP_T);
}

void loop() {

  //int i;
  tArrosCtx arrosCtx;
  int secToWait = 0;
  
  //long *rtcMemory = (long *)0x60001200; // the RTC User-Memory begins at address 60001200h
  cpt_loop = readAndInitRtcMem(arrosCtx);

  D2PRINT("Boucle : cpt_loop=");
  D2PRINTLN(cpt_loop);
  digitalWrite(LED, 0);
  delay(1); // attente en ms
  digitalWrite(LED, 1);

  if(cpt_loop == 0) {
    cpt_loop = 1; // Si erreur, on essai à la prochaine boucle, sinon, cpt_loop sera modifié

    if(connectWifi() >= 0) {
      secToWait = getNtpTimeUpTo(arrosCtx.nextDays, arrosCtx.nextHours);
      connectServHttps(); // 8 à 10 s
      //getServHttps(arrosCtx);
      sendServHttps(arrosCtx);
      disconnectServHttps();
    }
  
    // Calcul du temps restant avant le prochain envoi, avec le temps NTP 
    if (secToWait > 0) {
      cpt_loop = secToWait / time_sleep;
    } 
  }

  Serial.flush(); 

  D2PRINT("Je parts en sleep : cpt_loop=");
  D2PRINT(cpt_loop);
  cpt_loop--;
  WriteRtcMem(cpt_loop, arrosCtx);
  if(cpt_loop == 0) {
    D2PRINTLN(" prochain réveil AVEC WIFI");
    ESP.deepSleep(time_sleep* 1000000); // attente en us.
  } else {
    // Pas besoin de modem
    D2PRINTLN(" prochain réveil SANS WIFI");
    ESP.deepSleep(time_sleep* 1000000, RF_DISABLED); // attente en us.
  }
}


// -----------------------------------------------------------------
int connectWifi()
{
  int ret = -1;
  D2PRINT(" - Connecting to Wifi: ");
  D2PRINTLN(ssid);
  // flush() is needed to print the above (connecting...) message reliably, 
  // in case the wireless connection doesn't go through
  Serial.flush();

  WiFi.begin(ssid, password);
  for (int i=0; i<20; i++){
    if(WiFi.status() == WL_CONNECTED) {
      D1PRINTLN("WiFi connected");
      D2PRINT(" - IP address: local addr : ");
      D2PRINT(WiFi.localIP());
      ret = 0;
      break;
    }
    delay(500);
    D2PRINT(".");
  }
  return ret;
}

// -----------------------------------------------------------------
void connectServHttps()
{
  // Use HTTPSRedirect class to create a new TLS connection
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");
  
  D2PRINT(" connecting to : ");
  D2PRINTLN(host);

  // Try to connect for a maximum of 5 times
  bool flag = false;
  for (int i=0; i<5; i++){
    int retval = client->connect(host, httpsPort);
    if (retval == 1) {
       flag = true;
       break;
    }
    else
      D1PRINTLN("Connection failed. Retrying...");
  }

  if (!flag){
    D1PRINT("Could not connect to server: ");
    D1PRINTLN(host);
    D1PRINTLN("Exiting...");
    return;
  }

  if (client->setFingerprint(fingerprint)) {
    D1PRINTLN(" - Certificate match.");
  } else {
    D1PRINTLN(" - Certificate mis-match");
  }
}

  
// -----------------------------------------------------------------
void disconnectServHttps()
{
  delete client;
  client = nullptr;
}

// -----------------------------------------------------------------
void sendServHttps(tArrosCtx arrosCtx)
{
  String url_param;
    // Note: setup() must finish within approx. 1s, or the the watchdog timer
  // will reset the chip. Hence don't put too many requests in setup()
  // ref: https://github.com/esp8266/Arduino/issues/34

  url_param = url + "lev_p=" + arrosCtx.nextHours;
  D1PRINTLN("Wifi SEND : " + url_param);
  
  // fetch spreadsheet data and dont display the html response
  //client->GET(url_param, host, 0);
  client->GET(url_param, host, 1); // temporat debug aff trace
  String resp = client->getResponseBody();
  D1PRINTLN("Wifi RECEIV : " + resp);
}

// -----------------------------------------------------------------
// return : nb de second jusqu'à nextDay,nextHours  si on arrive à se connecter, sinon -1
int getNtpTimeUpTo(int nextDays, int nextHours)
{
  unsigned long days, hours;
  int nextTime = 0;
  timeClient.begin();
  
  D1PRINT("NTP check");
  timeClient.update();
  
  unsigned long secsSince1970  = timeClient.getEpochTime();
  D1PRINT(" - NTP Seconds since Jan 1 1970 (unix) = ");
  D1PRINT(secsSince1970);
  D1PRINT(" ou ");
  D1PRINTLN(timeClient.getFormattedTime()); // en second

  // Calcul du temps restant jusqu'à nextDay,nextHours
  hours = (secsSince1970  % 86400L) / 3600;
  days = (secsSince1970  / 86400L);
  
  // Si on est avant 11h, on ajoute day + 0.5j (12h), sinon day+1,5j
  if (hours < nextHours-1) {
    nextTime = int((days * 86400L + nextDays * 86400L + nextHours * 3600L) - secsSince1970);
  } else {
    nextTime = int(((days+1) * 86400L + nextDays * 86400L + nextHours * 3600L) - secsSince1970);
  }

  D1PRINT("Nb de second jusqu'à la prochaine fois :");
  D1PRINTLN(nextTime); // en second
  timeClient.end();
  
  return nextTime;
}

// -----------------------------------------------------------------
// Lecture de la mémoire non volatile (en deep sleep)
// et l'initialise si elle ne l'est pas 
int readAndInitRtcMem(tArrosCtx arrosCtx)
{
  int ret = 0;
  
  system_rtc_mem_read(RTCMEMORYSTART, &rtcMem, sizeof(rtcMem));

  // Si les flags ne sont pas bon, c'ets que la mémoire n'est pas initialisée.
  if (rtcMem.markerFlag == MARKER_RTC) {
    ret = rtcMem.cpt_loop;
  } else {
    // Init
    WriteRtcMem(0, arrosCtx);
    D2PRINTLN("Init Rtc memory");
  }
  return ret;
}

// -----------------------------------------------------------------
void WriteRtcMem(int val, tArrosCtx arrosCtx)
{
  rtcMem.cpt_loop = val;
  rtcMem.markerFlag = MARKER_RTC;
  system_rtc_mem_write(RTCMEMORYSTART, &rtcMem, sizeof(rtcMem));
}
