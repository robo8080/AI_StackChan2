#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "WebVoiceVoxRootCA.h"
#include <AudioGeneratorMP3.h>
#include <AudioFileSourceBuffer.h>
#include "AudioFileSourceHTTPSStream.h"
//#include "AudioOutputM5Speaker.h"

//-----------------------------------------------------
extern String VOICEVOX_API_KEY;
extern AudioGeneratorMP3 *mp3;
extern AudioFileSourceBuffer *buff;
extern AudioFileSourceHTTPSStream *file;
extern int preallocateBufferSize;
extern uint8_t *preallocateBuffer;
//extern AudioOutputM5Speaker out;
void StatusCallback(void *cbData, int code, const char *string);
void playMP3(AudioFileSourceBuffer *buff);
//-----------------------------------------------------

String https_get(const char* url, const char* root_ca) {
  String payload = "";
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client -> setCACert(root_ca);
    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
      HTTPClient https;
  
      Serial.print("[HTTPS] begin...\n");
      if (https.begin(*client, url)) {  // HTTPS
//      if (https.begin(*client, "https://api.tts.quest/v1/voicevox/?text=こんにちは世界！)) {  // HTTPS
//      if (https.begin(*client, "https://jigsaw.w3.org/HTTP/connection.html")) {  // HTTPS&speaker=1"
        Serial.print("[HTTPS] GET...\n");
        // start connection and send HTTP header
        int httpCode = https.GET();
  
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            payload = https.getString();
          }
        } else {
          Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }  
        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }
      // End extra scoping block
    }  
    delete client;
  } else {
    Serial.println("Unable to create client");
  }
  return payload;
}

/* //https_post_json(const char* url, const char* json_string, root_ca_openai);
String https_post_json(const char* url, const char* json_string, const char* root_ca) {
  String payload = "";
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client -> setCACert(root_ca);
    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
      HTTPClient https;
      https.setTimeout( 25000 ); 
  
      Serial.print("[HTTPS] begin...\n");
      if (https.begin(*client, url)) {  // HTTPS
        Serial.print("[HTTPS] POST...\n");
        // start connection and send HTTP header
        //https.setTimeout( 10000 ); 
        https.addHeader("Content-Type", "application/json");
        https.addHeader("Authorization", "Bearer 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");
        //int httpCode = https.GET();
        int httpCode = https.POST((uint8_t *)json_string, strlen(json_string));
  
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTPS] POST... code: %d\n", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            payload = https.getString();
          }
        } else {
          Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }  
        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }
      // End extra scoping block
    }  
    delete client;
  } else {
    Serial.println("Unable to create client");
  }
  return payload;
}
 */
bool voicevox_tts_json_status(const char* url, const char* json_key, const char* root_ca) {
  bool json_data = false;
  DynamicJsonDocument doc(1000);
  String payload = https_get(url, root_ca);
  if(payload != ""){
    Serial.println(payload);
 //    StaticJsonDocument<1000> doc;
//    JsonObject object = doc.as();
    DeserializationError error = deserializeJson(doc, payload.c_str());
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return json_data;
    }
    json_data = doc[json_key];
//    Serial.println(json_data);
  }
  return json_data;
}

//String tts_status_url;
String voicevox_tts_url(const char* url, const char* root_ca) {
  String tts_url = "";
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client -> setCACert(root_ca);
    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
      HTTPClient https;
  
      Serial.print("[HTTPS] begin...\n");
      if (https.begin(*client, url)) {  // HTTPS
        Serial.print("[HTTPS] GET...\n");
        // start connection and send HTTP header
        int httpCode = https.GET();
  
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = https.getString();
            Serial.println(payload);
            //StaticJsonDocument<1000> doc;
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, payload.c_str());
            if (error) {
              Serial.print(F("deserializeJson() failed: "));
              Serial.println(error.f_str());
              return tts_url;
            }
  String json_string;
  serializeJsonPretty(doc, json_string);
  Serial.println("====================");
  Serial.println(json_string);
  Serial.println("====================");

            if(!doc["success"]) return tts_url;
//            const char* mp3url = doc["mp3DownloadUrl"];
            const char* mp3url = doc["mp3StreamingUrl"];
            Serial.println(mp3url);
            Serial.print("isApiKeyValid:");
            if(doc["isApiKeyValid"]) Serial.println("OK");
            else Serial.println("NG");
            tts_url = String(mp3url);

            // const char* status_url = doc["audioStatusUrl"];
            // Serial.println(status_url);
            // //tts_status_url = String(status_url);
            // if(voicevox_tts_json_status(status_url, "isAudioError ", root_ca))  return tts_url;
            // while(!voicevox_tts_json_status(status_url, "isAudioReady", root_ca)) delay(1);
          }
        } else {
          Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }  
        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }
      // End extra scoping block
    }  
    delete client;
  } else {
    Serial.println("Unable to create client");
  }
  return tts_url;
}

static String URLEncode(const char* msg) {
  const char *hex = "0123456789ABCDEF";
  String encodedMsg = "";

  while (*msg != '\0') {
    if ( ('a' <= *msg && *msg <= 'z')
         || ('A' <= *msg && *msg <= 'Z')
         || ('0' <= *msg && *msg <= '9')
         || *msg  == '-' || *msg == '_' || *msg == '.' || *msg == '~' ) {
      encodedMsg += *msg;
    } else {
      encodedMsg += '%';
      encodedMsg += hex[*msg >> 4];
      encodedMsg += hex[*msg & 0xf];
    }
    msg++;
  }
  return encodedMsg;
}

// char *text0 = "みなさん、こんにちは！私の名前はスタックチャンです。よろしくね！";
// char *tts_parms0 = "&speaker=1";
// char *tts_parms01 = "&speaker=19";
// char *tts_parms02 = "&speaker=28";
// char *tts_parms03 = "&speaker=42";
// char *tts_parms04 = "&speaker=12";
// char *tts_parms05 = "&speaker=45";
// char *tts_parms06 = "&speaker=3";

void Voicevox_tts(char *text,char *tts_parms){
//  String tts_url = String("https://api.tts.quest/v1/voicevox/?text=") +  URLEncode(text) + String(tts_parms);
//  String tts_url = String("https://api.tts.quest/v3/voicevox/synthesis?key=y958S773N4I7356&text=") +  URLEncode(text) + String(tts_parms);
  String tts_url = String("https://api.tts.quest/v3/voicevox/synthesis?key=")+ VOICEVOX_API_KEY +  String("&text=") +  URLEncode(text) + String(tts_parms);
  String URL = voicevox_tts_url(tts_url.c_str(), root_ca);
  Serial.println(tts_url);

  if(URL == "") return;
//  while(!voicevox_tts_json_status(tts_status_url.c_str(), "isAudioReady", root_ca)) delay(1);
//delay(2500);
  file = new AudioFileSourceHTTPSStream(URL.c_str(), root_ca);
//  file->RegisterStatusCB(StatusCallback, (void*)"file");
  buff = new AudioFileSourceBuffer(file, preallocateBuffer, preallocateBufferSize);
//  mp3->begin(buff, &out);
  playMP3(buff);
}
