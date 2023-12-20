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
const int MAX_PASSWORD_LEN = 48;
const int MAX_PLAYLIST_LEN = 48;

// Authorization tokens (fetched once wifi connects)
char access_token[256];
char refresh_token[132];
int firmware_version = -1;
int temp_version = -1;

// Built By Peter login api
const char * bbp_auth_url = "http://52.86.18.252:3001/authorize_musicbox";

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
const int playlistPin = 35; // GPIO for playlist toggle

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


String getSelectedPlaylist() {

  // Choose playlist based on GPIO pin state
  if (digitalRead(playlistPin) == HIGH)
    return musicbox_playlist_1;
    return musicbox_playlist_2;

}

// Check if the given song is in the provided playlist
bool isTrackInPlaylist(const String& trackId, const String& playlistId) {
  // Check if the track is in the playlist using Spotify API
  String url = "https://api.spotify.com/v1/playlists/" + playlistId + "/tracks";
  String authorizationHeader = "Bearer " + String(access_token);

  HTTPClient http;

  http.begin(url);
  http.addHeader("Authorization", authorizationHeader.c_str());

  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    if (httpResponseCode == HTTP_CODE_OK) {
      String response = http.getString();

      // Check if the track ID is present in the playlist response
      return response.indexOf(trackId) != -1;
    } else {
      Serial.print("Failed to get playlist tracks: ");
      Serial.println(httpResponseCode);
    }
  } else {
    Serial.print("Error occurred during HTTP request for playlist tracks: ");
    Serial.println(http.errorToString(httpResponseCode).c_str());
  }

  http.end();
  return false;
}

// Get or create the currently selected playlist (1 or 2)
// Return the ID
String getOrCreatePlaylist(String playlistName, HTTPClient& httpClient) {
  String url = "https://api.spotify.com/v1/me/playlists?limit=50";

  httpClient.begin(url);
  String authorizationHeader = "Bearer " + String(access_token);
  httpClient.addHeader("Authorization", authorizationHeader.c_str());

  int httpResponseCode = httpClient.GET();

  if (httpResponseCode > 0) {
    if (httpResponseCode == HTTP_CODE_OK) {
      String response = httpClient.getString();

      // Look for the playlist, check if it exists
      String searchString = "\"" + String(playlistName) + "\"";
      int endIndex = response.indexOf(searchString);

      // Playlist did not exist, make it
      if (endIndex == -1) {
        // Playlist does not exist, create it
        String url2 = "https://api.spotify.com/v1/me/playlists";
        httpClient.begin(url2);

        char jsonPayload[100];
        snprintf(jsonPayload, sizeof(jsonPayload), "{\"name\":\"%s\",\"public\":true}", playlistName.c_str());

        httpClient.addHeader("Authorization", authorizationHeader.c_str());
        int httpResponseCode2 = httpClient.POST(jsonPayload);

        // Playlist created successfully (It did not exist previously)
        if (httpResponseCode2 > 0) {
          if (httpResponseCode2 == HTTP_CODE_CREATED) {
            String response = httpClient.getString();

            // Find the new playlist's ID by its name
            int endIndex = response.indexOf(searchString);
            String playlist = extractPlaylistID(response, endIndex);

            Serial.print("Newly created playlist: ");
            Serial.println(playlist);

            // End the second HTTP request
            httpClient.end();

            return playlist;
          }
        } else {
          Serial.println("HTTP Error creating playlist");
        }

        // End the second HTTP request in case of an error
        httpClient.end();
      } else {
        // Playlist exists! Get the id.
        String playlist = extractPlaylistID(response, endIndex);
        Serial.print("Existing playlist: ");
        Serial.println(playlist);

        return playlist;
      }
    } else {
      Serial.print("Failed to get playlists: ");
      Serial.println(httpResponseCode);
    }
  } else {
    Serial.print("Error occurred during HTTP for get playlists: ");
    Serial.println(httpClient.errorToString(httpResponseCode).c_str());
  }

  // End the first HTTP request
  httpClient.end();

  return "";
}


String extractPlaylistID(const String& response, int endIndex) {
  String substring = "\"id\" : ";

  // Search the string in reverse order
  for (int i = endIndex - substring.length(); i >= 0; --i) {
    if (response.substring(i, i + substring.length()) == substring) {
      int idStartIndex = i + substring.length() + 1; // Move to the beginning of the ID value
      int idEndIndex = response.indexOf("\"", idStartIndex);

      return response.substring(idStartIndex, idEndIndex);
    }
  }

  
}

// Function to add a track to a playlist on Spotify
void addTrackToPlaylist(String trackURI) {
  HTTPClient http;
  String playlistID = getOrCreatePlaylist(getSelectedPlaylist(), http);

  // Check if the song is in the playlist already
  if(isTrackInPlaylist(trackURI, playlistID))
  {
    Serial.println("Song already in playlist, ignoring...");
    return; // Already in
  }

  String url = "https://api.spotify.com/v1/playlists/" + playlistID + "/tracks";
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
      String response = http.getString();

      int startIndex = response.indexOf("spotify:track:");
      int endIndex = response.indexOf("\"", startIndex);

      String result = response.substring(startIndex, endIndex + 1);
      // Remove quotes from the result
      result.replace("\"", "");

      trackURI = result;

    } else {
      // Here, check if we need to refresh the token
      if (httpResponseCode == 401 || strlen(access_token) < 2)
      {
        Serial.println("Token expired, refreshing...");
        String newAccessToken = refreshAccessToken();
        if (newAccessToken)
        {
          // Convert String to char array and assign it to access_token
          newAccessToken.toCharArray(access_token, sizeof(access_token));  
          saveConfig(); // Store the new access token

          // Try the request again (recursive)
          trackURI = getCurrentPlayingTrackURI();

        }

        
      }
      else if (httpResponseCode == HTTP_CODE_NO_CONTENT)
      {
        // No track playing
        Serial.println("No track is currently playing.");
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
  Serial.println(requestBody);
  
  int httpResponseCode = http.POST(requestBody);

  

  String newAccessToken = "";

  if (httpResponseCode > 0) {
    if (httpResponseCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.print("Refresh result: ");
      Serial.println(payload);


      DynamicJsonDocument doc(1024); // Adjust the size as needed
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        newAccessToken = doc["access_token"].as<String>();
      } else {
        Serial.println("Failed to parse JSON response.");
      }
    } else {
      Serial.print("Refresh Token request failed with error code: ");
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
  if (strlen(refresh_token) < 2)
  {
    // Need to get the tokens from my server
    bool fail = true;
    // Did we provide login info to BuiltByPeter
    if (strlen(musicbox_username) + strlen(musicbox_password) > 2)
    {
      // Request tokens from my server
      HTTPClient http;


      String jsonPayload = "{\"username\":\"" + String(musicbox_username) + "\",\"pass\":\"" + String(musicbox_password) + "\"}";


      http.begin(bbp_auth_url);
      http.addHeader("Content-Type", "application/json");

      int httpResponseCode = http.POST(jsonPayload);

      if (httpResponseCode > 0) {
        if (httpResponseCode == HTTP_CODE_OK) {
          Serial.println("Successfully recieved spotify auth data!"); 
          String res = http.getString();
          Serial.println(res);
          
          // Use strncpy to copy the string to refresh_token
          strcpy(refresh_token, res.c_str()); 
          res = "";  
          fail = false;

          
        } else {
          Serial.print("Failed to authorize with BBP account: ");
          Serial.println(httpResponseCode);
          
          if (httpResponseCode == HTTP_CODE_UNAUTHORIZED)
          {
            Serial.println("(Wrong account password!)");
          }
          else if (httpResponseCode == HTTP_CODE_NOT_FOUND)
          {
            Serial.println("(User not found)");
          }
          else if (httpResponseCode == HTTP_CODE_ACCEPTED)
          {
            Serial.println("(User did not link their Spotify yet)");
          }
          

          
        }
      } else {
        Serial.print("Error trying to auth with BuiltByPeter: ");
        Serial.print(http.errorToString(httpResponseCode).c_str());
        Serial.println(httpResponseCode);
      }

      http.end();
      jsonPayload = "";

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
