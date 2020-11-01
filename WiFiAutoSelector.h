/**
 * WiFiAutoSelector.h - Include file for class WiFiAutoSelector
 * Copyright (c) 2016 Andreas Schaefer <asc@schaefer-it.net>
 * 
 * A class to pick a wifi network from a list, based on the 
 * highest receive signal strength, and connect to it. 
 * Inspired by "WiFiMulti"
 * 
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * Usage example:
 * 
 * #include <ESP8266WiFi.h>
 * #include "WiFiAutoSelector.h"
 * 
 * #define WIFI_CONNECT_TIMEOUT 8000
 * WiFiAutoSelector wifiAutoSelector(WIFI_CONNECT_TIMEOUT);  
 * 
 * void setuo() {
 *   Serial.begin(9600);
 *   wifiAutoSelector.add("myApOne", "s4crzt");
 *   wifiAutoSelector.add("anotherApTwo", "moreSecret");
 * }
 * 
 * void loop() {
 *   if(WiFi.status() != WL_CONNECTED) {
 *     Serial.print("Connecting wifi ");
 *     if(-1 < wifiAutoSelector.scanAndConnect()) {
 *       int connectedIndex = wifiAutoSelector.getConnectedIndex();
 *       Serial.print("to '");
 *       Serial.print(wifiAutoSelector.getSSID(connectedIndex));
 *       Serial.println("'. Done.");
 *     }else{
 *       Serial.println("failed.");
 *     }
 *   }
 *   delay(1000);
 * }
 * 
 */
class WiFiAutoSelector {
private:

  typedef struct _NetworkInfo {
    int  rssi;
    char ssid[32];
    char key[64];
  } NetworkInfo;

protected:

  NetworkInfo* _networks;
  int          _count;
  int          _capacity;
  int          _connectedIndex;
  int          _connectTimeout;
  
  bool resize(int newCapacity) {
    if(newCapacity <= _capacity) return true;
    NetworkInfo* tni = (NetworkInfo*)malloc( sizeof(NetworkInfo) * newCapacity );
    if(!tni) return false;
    memset(tni, 0, sizeof(NetworkInfo) * newCapacity);
    for(int i=0; i < _count; i++) {
      tni[i] = _networks[i];
    }
    if(_networks) free(_networks);
    _networks = tni;
    _capacity = newCapacity;
    return true;
  }
  
public:
  int getCount() { return _count; }
  int getCapacity() { return _capacity; }
  int getConnectedIndex() { return _connectedIndex; }
  
  int getRSSI(int index) { return _networks[index].rssi; }
  char* getSSID(int index) { return _networks[index].ssid; }
  char* getKey(int index) { return _networks[index].key; }
  
  void clear() {
     memset(_networks, 0, sizeof(NetworkInfo) * _capacity );
     _count = 0;
  }

  int add(const char* ssid, const char* key) {
    bool canAdd = true;
    if((_count + 1) >= _capacity) { canAdd = resize( _capacity + 4 ); }
    if(canAdd) {
      _networks[_count].rssi = -1000;
      strncpy(_networks[_count].ssid, ssid, sizeof(_networks[_count].ssid));
      strncpy(_networks[_count].key, key, sizeof(_networks[_count].key));
      _count++;
      return _count - 1;    
    }
    return -1;
  }

  int scanAndConnect() {
    
    // Initialize some variables
    _connectedIndex = -1;
    // No networks, then return;
    if(!_networks || !_count) return _connectedIndex;

    // Reset RSSI for every known network to lowest value
    for(int i = 0; i < _count; i++) {
      _networks[i].rssi = -1000;
    }
    
    int foundNetworkCount = WiFi.scanNetworks();
    if(0 >= foundNetworkCount) return _connectedIndex;

    int bestRSSI = -1000;
    int bestNetworkIndex = -1;
    
    while(foundNetworkCount-- > 0 ) {
      
      bool inList = false;
      String foundSSID = WiFi.SSID(foundNetworkCount);

      for(int i = 0; i < _count; i++ ) {
        if(foundSSID.equals(_networks[i].ssid)) {
          inList = true;          
          // RSSI : the current RSSI / Received Signal Strength in dBm
          _networks[i].rssi = WiFi.RSSI(foundNetworkCount);

          // Keep the network index with the best signal strength
          if(bestRSSI < _networks[i].rssi) {
            bestRSSI = _networks[i].rssi;
            bestNetworkIndex = i;
          }
                    
          break;         
        }        
      } // for(int i = 0; i < _count; i++ ) ...

    } // while(foundNetworkCount > 0 ) ...

    // If we have found th best network, connect to it.
    if(-1 < bestNetworkIndex) {
      WiFi.begin(_networks[bestNetworkIndex].ssid, _networks[bestNetworkIndex].key);
      // Wait for the wifi to connect. This should happen with in the timeout period.
      for(int loop = _connectTimeout; loop > 0; loop -= 200 ) {
        delay(200);
        if(WiFi.status() == WL_CONNECTED) {
          _connectedIndex = bestNetworkIndex;      
          break;
        }
      }
    }
    
    return _connectedIndex;
  }

  ~WiFiAutoSelector() {
    if(_networks) free(_networks);
  }

  WiFiAutoSelector(int connectTimeout) : 
    _networks(0L), _count(0), _capacity(0)
  , _connectedIndex(-1) , _connectTimeout(connectTimeout) {
  }
  
};
