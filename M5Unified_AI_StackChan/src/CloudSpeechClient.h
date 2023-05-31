#ifndef _CLOUDSPEECHCLIENT_H
#define _CLOUDSPEECHCLIENT_H
#include <WiFiClientSecure.h>
#include "Audio.h"

enum Authentication {
  USE_ACCESSTOKEN,
  USE_APIKEY
};

class CloudSpeechClient {
  WiFiClientSecure client;
  void PrintHttpBody2(Audio* audio);
  String key;
//  Authentication authentication;
public:
  CloudSpeechClient(const char* root_ca, const char* api_key);
  ~CloudSpeechClient();
  String Transcribe(Audio* audio);
};

#endif // _CLOUDSPEECHCLIENT_H

