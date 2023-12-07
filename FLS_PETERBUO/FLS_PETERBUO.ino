#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#ifdef ESP32
  #include <SPIFFS.h>
#endif

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <HTTPClient.h>
#include <Base64.h>

#include <HTTPUpdate.h> // Firmware updates

#include <WiFi.h>
const int MAX_USERNAME_LEN = 32;
const int MAX_PASSWORD_LEN = 32;
const int MAX_PLAYLIST_LEN = 48;

// Authorization tokens (fetched once wifi connects)
char access_token[256];
char refresh_token[256];
int firmware_version = -1;
int temp_version = -1;

// Custom Parameter Values (overwritten when loadConfig)
char musicbox_username[MAX_USERNAME_LEN];
char musicbox_password[MAX_PASSWORD_LEN];
char musicbox_playlist_1[MAX_PLAYLIST_LEN] = "MusicBox 1";
char musicbox_playlist_2[MAX_PLAYLIST_LEN] = "MusicBox 2";

// Custom Parameters
WiFiManagerParameter custom_musicbox_username("username", "MusicBox Username", musicbox_username, MAX_USERNAME_LEN);
WiFiManagerParameter custom_musicbox_password("password", "MusicBox Password", musicbox_password, MAX_PASSWORD_LEN);
WiFiManagerParameter custom_musicbox_playlist_1("playlist1", "Playlist 1", musicbox_playlist_1, MAX_PLAYLIST_LEN);
WiFiManagerParameter custom_musicbox_playlist_2("playlist2", "Playlist 2", musicbox_playlist_2, MAX_PLAYLIST_LEN);



// IO Setup
const int buttonPin = 34; // GPIO pin where the button is connected
const int redPin = 27;    // GPIO pin for the red LED
const int greenPin = 32; // GPIO pin for the green LED
const int bluePin = 4;  // GPIO pin for the blue LED

int mode = 1;

void checkForUpdates() {
  HTTPClient http;
  http.begin("http://52.7.199.236:3000/version");

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    // Parse the server response to get update information
    String version_str = http.getString();
    int latestVersion = version_str.toInt();

    if (latestVersion > firmware_version) {
      Serial.println("Update available!");
      // Store the version number so we can save it if success
      temp_version = latestVersion;
      // Retrieve the download link and update the firmware
      updateFirmware();
    } else {
      Serial.println("Firmware is up to date.");
    }
  } else {
    Serial.printf("Version check request failed with error %d\n", httpCode);
  }

  http.end();
}

void updateFirmware() {
  Serial.println("Updating firmware...");

  WiFiClient client;
  t_httpUpdate_return ret = httpUpdate.update(client, "http://52.7.199.236:3000/uploads/musicbox.bin");

  switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("No firmware updates!");
        break;

      case HTTP_UPDATE_OK:
        Serial.println("Firmware updated!");
        break;

      default:
        Serial.println("Error updating firmware");
        break;
    }
}

void update_started() {
  Serial.println("CALLBACK:  HTTP update process started");
}

void update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");

  // Store the new version
  firmware_version = temp_version;
  saveConfig();
  // delay(1500);
  // ESP.restart();
}

void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}


// const char* spotifyClientId = "fffd853196474a44bb86a4d17717faab";
// const char* spotifyClientSecret = "606bd9a5cbdc46afbd7841996e3feaef";
// const char* spotifyRedirectUri = "http://192.168.4.1/callback";
void lights(int mode_in) {
  mode = mode_in;
  // Mode 0 is setup (blue)
  // Mode 1 is ready (green)
  // Mode 2 is error (red)

  if (mode == 0) {
    digitalWrite(redPin, LOW);
    digitalWrite(greenPin, LOW);
    digitalWrite(bluePin, HIGH);
  }
  else if (mode == 1) {
    digitalWrite(redPin, LOW);
    digitalWrite(greenPin, HIGH);
    digitalWrite(bluePin, LOW);
  }
  else if (mode == 2) {
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, LOW);
    digitalWrite(bluePin, LOW);

    delay(100);
    lights(-1);
    delay(100);
    lights(4);
    delay(100);
    lights(-1);
    delay(100);
    lights(4);
    delay(100);
  }
  else if (mode == 3) { // Booting (purple)
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, LOW);
    digitalWrite(bluePin, HIGH);
  }
  // Red light on (helper for flash error, which is mode 2)
  else if (mode == 4) {
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, LOW);
    digitalWrite(bluePin, LOW);
  }
  else if (mode == -1) // Off
  {
    digitalWrite(redPin, LOW);
    digitalWrite(greenPin, LOW);
    digitalWrite(bluePin, LOW);

  }
}

void resetSettings()
{
  // Callback function to reset WiFi settings
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  delay(1000);
  ESP.restart();
  delay(1000);
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("config mode");
  lights(0);

}

void saveConfigCallback () {
}



const unsigned long configModeDuration = 2000; // 2 seconds in milliseconds



String clientID = "fffd853196474a44bb86a4d17717faab";
String clientSecret = "606bd9a5cbdc46afbd7841996e3feaef"; 

// Function to create a playlist on Spotify
void createPlaylist(String playlistName) {
  HTTPClient http;

  // Spotify API endpoint to create a playlist
  String url = "https://api.spotify.com/v1/me/playlists";

  String jsonPayload = "{\"name\":\"" + playlistName + "\",\"public\":true}";

  http.begin(url);
  String authorizationHeader = "Bearer " + String(access_token);

  http.addHeader("Authorization", authorizationHeader.c_str());
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode > 0) {
    if (httpResponseCode == HTTP_CODE_CREATED) {
      Serial.println("Playlist created successfully!");
    } else {
      Serial.print("Failed to create playlist. Error code: ");
      Serial.println(httpResponseCode);
    }
  } else {
    Serial.print("Error occurred: ");
    Serial.println(http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}

// Get or create the currently selected playlist (1 or 2)
// Return the ID
String getOrCreatePlaylist()
{
  return ""; // TODO
}

// Function to add a track to a playlist on Spotify
void addTrackToPlaylist(String trackURI) {
  HTTPClient http;

  String url = "https://api.spotify.com/v1/playlists/" + getOrCreatePlaylist() + "/tracks";
  String jsonPayload = "{\"uris\":[\"" + trackURI + "\"]}";

  http.begin(url);
  String authorizationHeader = "Bearer " + String(access_token);

  http.addHeader("Authorization", authorizationHeader.c_str());
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode > 0) {
    if (httpResponseCode == HTTP_CODE_CREATED) {
      Serial.println("Track added to the playlist successfully!");
    } else {
      Serial.print("Failed to add track to the playlist. Error code: ");
      Serial.println(httpResponseCode);
    }
  } else {
    Serial.print("Error occurred: ");
    Serial.println(http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}

// Function to retrieve the current song's information (including URI) from Spotify
String getCurrentPlayingTrackURI() {
  HTTPClient http;

  String url = "https://api.spotify.com/v1/me/player/currently-playing";

  http.begin(url);
  String authorizationHeader = "Bearer " + String(access_token);

  http.addHeader("Authorization", authorizationHeader.c_str());

  int httpResponseCode = http.GET();

  String trackURI = "";

  if (httpResponseCode > 0) {
    if (httpResponseCode == HTTP_CODE_OK) {
      String payload = http.getString();

      DynamicJsonDocument doc(2048); // Adjust the size as needed
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        if (doc["is_playing"].as<bool>()) {
          trackURI = doc["item"]["uri"].as<String>();
        } else {
          Serial.println("No track is currently playing.");
        }
      } else {
        Serial.println("Failed to parse JSON response.");
      }
    } else {
      // Here, check if we need to refresh the token
      if (httpResponseCode == 401)
      {
        Serial.println("Token expired, refreshing...");
        String newRefreshToken = refreshAccessToken();

        // Convert String to char array and assign it to refresh_token
        newRefreshToken.toCharArray(refresh_token, sizeof(refresh_token));  
        saveConfig(); // Store the new access token

        // Try the request again (recursive)
        trackURI = getCurrentPlayingTrackURI();
      }
      else
      {
        // Unknown error
        Serial.print("Get current playing FAILED with code ");
        Serial.println(httpResponseCode);
        lights(2);

        lights(1); // back to green

        // RESTART ON ERROR
        //ESP.restart();
        //delay(1000);
      }
      
    }
  } else {
    Serial.println("Error occurred during HTTP request.");
    lights(2);

    lights(1); // back to green

    // RESTART ON ERROR
    // ESP.restart();
    //delay(1000);
  }

  http.end();
  return trackURI;
}



// Function to refresh the Spotify access token
String refreshAccessToken() {
  HTTPClient http;
 String tokenURL = "https://accounts.spotify.com/api/token";
  String base64encoded = base64::encode(String(clientID) + ":" + String(clientSecret));
  
  http.begin(tokenURL);
  http.addHeader("Authorization", "Basic " + base64encoded);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String requestBody = "grant_type=refresh_token&refresh_token=" + String(refresh_token);
  
  int httpResponseCode = http.POST(requestBody);

  

  String newAccessToken = "";

  if (httpResponseCode > 0) {
    if (httpResponseCode == HTTP_CODE_OK) {
      String payload = http.getString();

      DynamicJsonDocument doc(1024); // Adjust the size as needed
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        newAccessToken = doc["access_token"].as<String>();
      } else {
        Serial.println("Failed to parse JSON response.");
      }
    } else {
      Serial.print("HTTP request failed with error code: ");
      Serial.println(httpResponseCode);
    }
  } else {
    Serial.println("Error occurred during HTTP request.");
  }

  http.end();
  return newAccessToken;
}

void loadConfig()
{
  //clean FS, for testing
 //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

 #if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if ( ! deserializeError ) {
#else
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
#endif
          Serial.println("\nparsed json");
          strcpy(musicbox_username, json["musicbox_username"]);
          strcpy(musicbox_password, json["musicbox_password"]);
          strcpy(musicbox_playlist_1, json["musicbox_playlist_1"]);
          strcpy(musicbox_playlist_2, json["musicbox_playlist_2"]);

          // Spotify tokens
          strcpy(access_token, json["access_token"]);
          strcpy(refresh_token, json["refresh_token"]);

          // Firmware Version
          firmware_version = json["firmware_version"];

          // Update the parameter values
          custom_musicbox_username.setValue(musicbox_username, MAX_USERNAME_LEN);
          custom_musicbox_password.setValue(musicbox_password, MAX_PASSWORD_LEN);
          custom_musicbox_playlist_1.setValue(musicbox_playlist_1, MAX_PLAYLIST_LEN);
          custom_musicbox_playlist_2.setValue(musicbox_playlist_2, MAX_PLAYLIST_LEN);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

}

void saveConfig()
{
  Serial.println("saving config");
 #if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
    DynamicJsonDocument json(1024);
#else
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
#endif
    // Save local params to disk
    json["musicbox_username"] = musicbox_username;
    json["musicbox_password"] = musicbox_password;
    json["musicbox_playlist_1"] = musicbox_playlist_1;
    json["musicbox_playlist_2"] = musicbox_playlist_2;

    // Save spotify tokens (local values to disk)
    json["access_token"] = access_token;
    json["refresh_token"] = refresh_token;

    // Store latest Firmware Version
    json["firmware_version"] = firmware_version;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
    serializeJson(json, Serial);
    serializeJson(json, configFile);
#else
    json.printTo(Serial);
    json.printTo(configFile);
#endif
    configFile.close();
    //end save

}

// Setup callback functions, custom parameters, and timeouts
// Called whenever a wifiManager is started, usually (twice, one on boot one on user defined config mode)
void initializeConfigPortal(WiFiManager *wifiManager)
{
  wifiManager->setSaveConfigCallback(saveConfigCallback);
  
  // Add parameters only if they don't already exist
  if (wifiManager->getParametersCount() < 1)
  {
    wifiManager->addParameter(&custom_musicbox_username);
    wifiManager->addParameter(&custom_musicbox_password);
    wifiManager->addParameter(&custom_musicbox_playlist_1);
    wifiManager->addParameter(&custom_musicbox_playlist_2);

  }


  // Set callback to reset settings
  //wifiManager->setConfigPortalTimeout(180); // Set timeout for configuration portal
  wifiManager->setAPCallback(configModeCallback);
  wifiManager->setConnectTimeout(10);
}

void closedConfigPortal(WiFiManager *wifiManager)
{
  //read updated parameters
  strcpy(musicbox_username, custom_musicbox_username.getValue());
  strcpy(musicbox_password, custom_musicbox_password.getValue());
  strcpy(musicbox_playlist_1, custom_musicbox_playlist_1.getValue());
  strcpy(musicbox_playlist_2, custom_musicbox_playlist_2.getValue());

  Serial.println("The values in the file are: ");
  Serial.println("\tmusicbox_username : " + String(musicbox_username));
  Serial.println("\tmusicbox_password : " + String(musicbox_password));
  Serial.println("\tmusicbox_playlist_1 : " + String(musicbox_playlist_1));
  Serial.println("\tmusicbox_playlist_2 : " + String(musicbox_playlist_2));

  // Check for new firmware
  httpUpdate.onStart(update_started);
  httpUpdate.onEnd(update_finished);
  httpUpdate.onProgress(update_progress);
  httpUpdate.onError(update_error);

  checkForUpdates(); // Checks for updates

  // Set the spotify tokens if we dont have them
  if (strlen(access_token) < 2)
  {
    // Need to get the tokens from my server
    bool fail = true;
    // Did we provide login info to BuiltByPeter
    if (strlen(musicbox_username) + strlen(musicbox_password) > 2)
    {
      // Request tokens from my server
      HTTPClient http;

      // Built By Peter login api
      String url = "http://52.86.18.252/authorize_musicbox";

      String jsonPayload = "{\"username\":\"" + String(musicbox_username) + "\",\"password\":" + String(musicbox_password) + "}";

      http.begin(url);
      http.addHeader("Content-Type", "application/json");

      int httpResponseCode = http.POST(jsonPayload);

      if (httpResponseCode > 0) {
        if (httpResponseCode == HTTP_CODE_OK) {
          Serial.println("Successfully recieved spotify auth data!");  
          String payload = http.getString();

          DynamicJsonDocument doc(2048); // Adjust the size as needed
          DeserializationError error = deserializeJson(doc, payload);

          const char* accessTokenString = doc["access_token"];
          // Use strncpy to copy the string to access_token
          strncpy(access_token, accessTokenString, sizeof(access_token));

          const char* refreshTokenString = doc["refresh_token"];

          // Use strncpy to copy the string to refresh_token
          strncpy(refresh_token, refreshTokenString, sizeof(refresh_token));   

          fail = false;
          // Done. We have the data, must still save it (handled by saveConfig())
        } else {
          // Failure: user did not authenticate on website
          if (httpResponseCode == HTTP_CODE_UNAUTHORIZED)
          {
            Serial.print("User has a BBP account but did not connect their spotify");
          }
          
          Serial.print("Failed to hit authorize endpoint: ");
          Serial.println(httpResponseCode);
        }
      } else {
        Serial.print("Error trying to auth with BuiltByPeter: ");
        Serial.print(http.errorToString(httpResponseCode).c_str());
        Serial.println(httpResponseCode);
      }

      http.end();

    }

    if (fail)
    {
      Serial.println("MusicBox account auth failure");
      
      
      // Incase we changed other data
      saveConfig();

      lights(2);

      initializeConfigPortal(wifiManager);

      wifiManager->startConfigPortal("MusicBox Setup");

      closedConfigPortal(wifiManager); // The portal closed. Call THIS function (its recursive)

    }


  }

  // Here we're good! Saves all data, including access and refresh tokens.
  saveConfig(); // Will also store new version, if we updated

  // Connected to wifi! Will this detect its own network???
  if (WiFi.status() == WL_CONNECTED) {
    lights(1);
  }
  else //We aren't connected to wifi, reboot (we probably just changed wifi credentials)
  {
    delay(1000);
    ESP.restart();
    //delay(1000);
  }

}

void setup() {
  Serial.begin(115200);
  lights(3);

  pinMode(buttonPin, INPUT);

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  loadConfig(); // Load the custom data such as user login details, playlists, version, etc


  // Create an instance of WiFiManager
  WiFiManager wifiManager;
  initializeConfigPortal(&wifiManager);

  // Connect to Wi-Fi or start an Access Point if no credentials are stored
  if (!wifiManager.autoConnect("MusicBox Setup")) {

    // I disabled the timeout for now, the below will not occur
    // Because, if wifi fails, we should just wait until we enter new details. No need to restart!
    Serial.println("Failed to connect and hit timeout");
    // Save any parameters we may have entered, try again
    closedConfigPortal(&wifiManager);

    delay(1000);
    ESP.restart();
    //delay(3000);
  }

  // Configuration done
  closedConfigPortal(&wifiManager);

 
  

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  // Check if a playlist name is stored in EEPROM and create a playlist on Spotify
  // if (String(EEPROM.readString(40)).length() > 0) {
  //   createPlaylist(String(EEPROM.readString(40)), accessToken);
  // }
}

bool pressed = false;
unsigned long press_begin = 0;
unsigned long press_end = 0;
int debounce = 50;

// Quick Press feature variables
int quick_presses = 0; // Current number of presses in window
int quickpress_tresh = 3; // How many times to press to execute event (restart)
unsigned long quickpress_window = 1000; // Window of time to press x times within for event to trigger
unsigned long quickpress_start = 0;


void loop() {
  // Button press

  // Reset quick presses if time expired
  if (quickpress_start > 0 && millis() - quickpress_start >= quickpress_window)
  {
    quick_presses = 0;
  }

  // Execute once when it starts being pressed
  if (digitalRead(buttonPin) == HIGH)
  {
    // When it is first pressed, start timer
    if (!pressed)
    {
      pressed = true;
      press_begin = millis();

      
    }
    // During the duration of the press, check if it is held to enter config mode
    else
    {
      if (millis() - press_begin >= configModeDuration)
      {
        Serial.println("Entering Debug");
        WiFiManager wifiManager;

        initializeConfigPortal(&wifiManager);

        wifiManager.startConfigPortal("MusicBox Setup");

        closedConfigPortal(&wifiManager); // The portal closed

      }

    }

  }
  // Set pressed to false when button is released (and pressed is true)
  else if (digitalRead(buttonPin) == LOW && pressed)
  {
    pressed = false;
    press_end = millis();

    int press_duration = press_end - press_begin;

    // After releasing, if it was a short press, call api
    if (press_duration < configModeDuration && press_duration > debounce)
    {
      Serial.println("Short press");
      

      // Increiment short presses for restart
      quick_presses++;
      // Check if it is the first press, if so start another timer for the quick press feature
      if (quick_presses == 1)
      {
        quickpress_start = millis(); // Store the time of the first short press.
      }
      else if (quick_presses >= quickpress_tresh)
      {
        // Reached threshold, execute quickpress event
        // Restart
        quickpress_start = 0;
        quick_presses = 0;

        ESP.restart();
      }

      String trackURI = getCurrentPlayingTrackURI();
      Serial.println("Track: "+trackURI);
      if (trackURI != "") {
        addTrackToPlaylist(trackURI);
      }
    }
  // END BUTTON LOGIC
  }


  
  

}
