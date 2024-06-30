#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <string>

#ifdef ESP32
  #include <SPIFFS.h>
#endif

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <HTTPClient.h>
#include <Base64.h>

#include <HTTPUpdate.h> // Firmware updates

#include <WiFi.h>


// Set true while developing to prevent updating while testing new code
// But it would only update if I upload code myself during development, which I wouldnt do.
// So this is probably unneeded, but available.
const bool skipUpdates = false;
const bool force_format = false;


const int MAX_USERNAME_LEN = 32;
const int MAX_PASSWORD_LEN = 48;
const int MAX_PLAYLIST_LEN = 48;

// Create an instance of WiFiManager
WiFiManager wifiManager;

// Authorization tokens (fetched once wifi connects)
char access_token[256];
char refresh_token[132];
int firmware_version = -1;
int temp_version = -1;

// MusicBox login api
const char * musicbox_auth_url = "https://musicbox-backend-178z.onrender.com/authorize_musicbox";

// Custom Parameter Values (overwritten when loadConfig)
// @params
char musicbox_username[MAX_USERNAME_LEN];
char musicbox_password[MAX_PASSWORD_LEN];

char musicbox_playlist_1[MAX_PLAYLIST_LEN] = "MusicBox 1";
char musicbox_playlist_2[MAX_PLAYLIST_LEN] = "MusicBox 2";
char musicbox_playlist_3[MAX_PLAYLIST_LEN] = "MusicBox 3";

char net_delay[3] = "0";

char sleep_minutes[4] = "45";

char ssid_csv[(MAX_USERNAME_LEN * 4)];
char pass_csv[(MAX_PASSWORD_LEN * 4)];
char pw[MAX_PASSWORD_LEN];


// Custom Parameters
// @params
WiFiManagerParameter custom_musicbox_username("username", "MusicBox Username", musicbox_username, MAX_USERNAME_LEN);
WiFiManagerParameter custom_musicbox_password("password", "MusicBox Password", musicbox_password, MAX_PASSWORD_LEN);

WiFiManagerParameter custom_musicbox_playlist_1("playlist1", "Playlist 1", musicbox_playlist_1, MAX_PLAYLIST_LEN);
WiFiManagerParameter custom_musicbox_playlist_2("playlist2", "Playlist 2", musicbox_playlist_2, MAX_PLAYLIST_LEN);
WiFiManagerParameter custom_musicbox_playlist_3("playlist3", "Playlist 3", musicbox_playlist_3, MAX_PLAYLIST_LEN);

WiFiManagerParameter custom_net_delay("netdelay", "Network Delay", net_delay, 3);
WiFiManagerParameter custom_sleep_minutes("sleepminutes", "Minutes until Sleep", sleep_minutes, 4);
WiFiManagerParameter custom_ssid_csv("ssidcsv", "WiFi Networks (CSV)", ssid_csv, (MAX_USERNAME_LEN * 4));
WiFiManagerParameter custom_pass_csv("passcsv", "WiFi Passwords (CSV)", pass_csv, (MAX_PASSWORD_LEN * 4));

WiFiManagerParameter custom_pw("pw", "Portal Password", pw, sizeof(pw));



// Standard IO Setup
const int buttonPin = 34; // GPIO pin where the button is connected
//const int playlistPin = 35; // GPIO for playlist toggle: No longer using AB system
const int redPin = 27;    // GPIO pin for the red LED
const int greenPin = 32; // GPIO pin for the green LED
const int bluePin = 4;  // GPIO pin for the blue LED

//Reversed IO Setup (for my prototype)

// const int redPin = 4;    // GPIO pin for the red LED
// const int greenPin = 32; // GPIO pin for the green LED
// const int bluePin = 27;  // GPIO pin for the blue LED


// Button variables
bool pressed = false;
unsigned long press_begin = 0;
unsigned long press_end = 0;
int debounce = 50;

// Quick Press feature variables
int quick_presses = 0; // Current number of presses in window
unsigned long quickpress_window = 1000; // Window of time to press x times within for event to trigger
unsigned long quickpress_start = 0;

// Sleep feature variables
unsigned long sleep_start = millis(); // Start of the sleep timer
bool sleeping = false;
int last_mode = 0;


const char* getCertificate() {

  HTTPClient http;
  http.begin("https://musicbox-backend-178z.onrender.com/certificate");
  int httpCode = http.GET();
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      char* rootCACertificate = new char[payload.length() + 1];
      strcpy(rootCACertificate, payload.c_str());
      http.end();
      return rootCACertificate;
    }
  } else {
    Serial.print("Error on HTTP request (getting certificate from musicbox backend): ");
    Serial.println(httpCode);
  }
  http.end();
  
  return nullptr;
}


void checkForUpdates() {
  // Serial.println(getCertificate()); 
  
  HTTPClient http;
  http.begin("https://musicbox-backend-178z.onrender.com/version");

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    // Parse the server response to get update information
    String version_str = http.getString();
    int latestVersion = version_str.toInt();

    if (latestVersion != firmware_version) {
      Serial.print("Update available! Version: ");
      Serial.println(latestVersion);

      // Store the version number so we can save it if success
      if (skipUpdates)
      {
        Serial.println("Skipping this update!");
        

        // Store the latest version so that we do not install this version ever,
        // even when turning off update skips. Do we need to do this?

        //firmware_version = temp_version;
      }
      else
      {
        temp_version = latestVersion;
        // Retrieve the download link and update the firmware
        
        updateFirmware();

      }
      

    } else {
      Serial.println("Firmware is up to date.");
    }
  } else {
    Serial.printf("Version check request failed with error %d\n", httpCode);
  }

  http.end();
}

void updateFirmware() {
  // get certificate 
  Serial.println("Fetching firmware certificate...");
  const char* rootCACertificate = getCertificate();

  Serial.println("Updating firmware...");

  
  WiFiClientSecure client;
  client.setCACert(rootCACertificate);
  t_httpUpdate_return ret = httpUpdate.update(client, "https://musicbox-backend-178z.onrender.com/uploads/musicbox.bin");

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
  callSaveConfig(false); // Reboot when done

  
}

void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);

  if (cur % 10 == 8) // If it ends in eight, toggle
  {
    // Toggle led 
    if (digitalRead(redPin) == HIGH) // Purple
      lights(-1);
    else
      lights(3);
  }
  
}

void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}


// Mode 0 is setup (blue)
// Mode 1 is ready (green)
// Mode 2 is error (red)
// Mode 3 is booting (purple)
// Mode 5 is success (flash green)
void lights(int mode) {
  if (mode >= 0)
  {
    last_mode = mode;
  }

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
    lights(4);
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
  else if (mode == 5) {
    lights(-1);
    delay(100);
    lights(1);
    delay(100);
    lights(-1);
    delay(100);
    lights(1);
  }
  else if (mode == -1) // Off
  {
    digitalWrite(redPin, LOW);
    digitalWrite(greenPin, LOW);
    digitalWrite(bluePin, LOW);

  }
}

// Enter sleep mode 
void enter_sleep()
{
  lights(-1);
  sleeping = true;
}

// Wake up from sleep mode
void leave_sleep()
{
  lights(last_mode);
  sleeping = false;
  sleep_start = millis();
}

void resetSettings()
{
  // Callback function to reset WiFi settings
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
  switch (quick_presses) {
        case 1:
        {
          return musicbox_playlist_1;
        }
        case 2:
        {
          return musicbox_playlist_2;
        }
        case 3:
        {
          return musicbox_playlist_3;
        }
        default:
            break;
    }

}

// Check if the giv
#include <ArduinoJson.h>
#include <HTTPClient.h>

bool isTrackInPlaylist(const String& trackId, const String& playlistId) {
  String url = "https://api.spotify.com/v1/playlists/" + playlistId + "/tracks";
  String authorizationHeader = "Bearer " + String(access_token);
  String fields = "items(track(id))"; // Specify fields to retrieve
  HTTPClient http;
  int offset = 0;
  const int limit = 50;
  bool trackFound = false;

  // Remove "spotify:track:" from trackId
  String cleanedTrackId = trackId;
  cleanedTrackId.replace("spotify:track:", "");

  while (!trackFound) {
    String paginatedUrl = url + "?offset=" + offset + "&limit=" + limit + "&fields=" + fields;
    http.begin(paginatedUrl);
    http.addHeader("Authorization", authorizationHeader.c_str());

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      if (httpResponseCode == HTTP_CODE_OK) {
        String response = http.getString(); // Get the complete JSON response
        //Serial.println("Response:");
        //Serial.println(response); // Print the JSON response for debugging

        // Count commas in the JSON response
        int songCount = 1;
        for (size_t i = 0; i < response.length(); ++i) {
          if (response[i] == ',') {
            songCount++;
          }
        }

        // If the track is in this response, we can set found to true
        if (response.indexOf(cleanedTrackId) != -1) {
          trackFound = true;
        }

        //Serial.println("Number of items in response: " + String(songCount)); // Adding 1 for the last item

        // Check if we have retrieved all items
        if (songCount < limit) {
          break;
        }

        offset += limit;
      } else {
        Serial.print("Failed to get playlist tracks: ");
        Serial.println(httpResponseCode);
        break;
      }
    } else {
      Serial.print("Error occurred during HTTP request for playlist tracks: ");
      Serial.println(http.errorToString(httpResponseCode).c_str());
      break;
    }

    http.end();
  }

  return trackFound;
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
        Serial.println(playlist); // playlist

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
  String substring = "\"id\":";
  // Search the string in reverse order  - substring.length()
  for (int i = endIndex; i >= 0; --i) {
    // Serial.println(response.substring(i, i + substring.length()));
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
    if (httpResponseCode == HTTP_CODE_CREATED || httpResponseCode == HTTP_CODE_OK) {
      Serial.println("Track added to the playlist successfully!");
      //lights(5);
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
          callSaveConfig(false); // Store the new access token

          // Try the request again (recursive)
          trackURI = getCurrentPlayingTrackURI();

        }
        else
        {
          // Error getting access token
          displayError(false);
        }

        
      }
      else if (httpResponseCode == HTTP_CODE_NO_CONTENT)
      {
        // No track playing
        Serial.println("No track is currently playing.");
        displayError(false);
      }
      else
      {
        // Unknown error
        Serial.print("Get current playing FAILED with code ");
        Serial.println(httpResponseCode);
        displayError(true);


      }
      
    }
  } else {
    Serial.println("Error occurred during HTTP request.");
    displayError(true);

  }

  http.end();
  return trackURI;
}

// Error function:
// Flash red. If fatal = true, open setup portal
void displayError(bool fatal)
{
  lights(2);
  if (fatal)
  {
    initializeConfigPortal();
    wifiManager.startConfigPortal(formSSID(), pw);
  }
  else
  {
    lights(1); //Not a fatal error: Back to ready state
  }
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
  // Serial.println(requestBody);
  
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
      Serial.print("Refresh Token request failed with error code: ");
      Serial.println(httpResponseCode);
    }
  } else {
    Serial.println("Error occurred during HTTP request.");
  }

  http.end();
  return newAccessToken;
}

const char * formSSID()
{
  // Form the Network name from the stored username, if it exists
  static std::string network_name_str;

  if (strlen(musicbox_username) > 1) {
    // Create a std::string from the username
    std::string username_str(musicbox_username);

    // Get the first 21 characters (or fewer if the username is shorter)
    network_name_str = "MusicBox (" + username_str.substr(0, 21) + ")";

    // Truncate network_name_str to ensure it does not exceed 32 characters
    network_name_str = network_name_str.substr(0, MAX_SSID_LEN - 1);

  } else {
      // Copy "MusicBox Setup" to network_name
      network_name_str = "MusicBox Setup";
  }

  return network_name_str.c_str();
}

void loadConfig()
{
  // Format on fail, should setup the file system.
  if (SPIFFS.begin(true)) {

    // Format on first time
    // Check if formatting is needed
    if (force_format || !SPIFFS.exists("/formatted.txt")) {
      // FIRST SETUP!
      // Format and test lights.
      if(SPIFFS.format())
      {
        File flagFile = SPIFFS.open("/formatted.txt", "w");
        if (flagFile) {
          flagFile.println("Formatted");
          flagFile.close();
        }

        Serial.println("SPIFFS initialized and formatted successfully. Testing lights now!");
        // Test lights here
        // since we just formatted, we won't try to update because we have no wifi. So, after the flash, config mode should open.
        lights(-1);
        delay(2000);
        lights(2);
        delay(1000);
        lights(-1);
        delay(1000);
        lights(1);
        delay(1000);
        lights(-1);
        delay(1000);
        lights(0);
        delay(1000);
        lights(-1);


      }
      else
      {
        Serial.println("Failed to format SPIFFS");
      }

      
    }

    //Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      //Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        //Serial.println("opened config file");
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
        // Display the loaded json data (config)
        //json.printTo(Serial);

        if (json.success()) {
#endif
          // Load stored values into local memory
          // @params
          strcpy(musicbox_username, json["musicbox_username"]);
          strcpy(musicbox_password, json["musicbox_password"]);
          strcpy(musicbox_playlist_1, json["musicbox_playlist_1"]);
          strcpy(musicbox_playlist_2, json["musicbox_playlist_2"]);
          strcpy(musicbox_playlist_3, json["musicbox_playlist_3"]);
          strcpy(net_delay, json["net_delay"]);
          strcpy(sleep_minutes, json["sleep_minutes"]);
          strcpy(ssid_csv, json["ssid_csv"]);
          strcpy(pass_csv, json["pass_csv"]);
          strcpy(pw, json["pw"]);

          formSSID();



          // Spotify tokens
          strcpy(access_token, json["access_token"]);
          strcpy(refresh_token, json["refresh_token"]);

          // Firmware Version
          firmware_version = json["firmware_version"];

          // Update the config portal values to the stored values
          // @params
          custom_musicbox_username.setValue(musicbox_username, MAX_USERNAME_LEN);
          custom_musicbox_password.setValue(musicbox_password, MAX_PASSWORD_LEN);
          custom_musicbox_playlist_1.setValue(musicbox_playlist_1, MAX_PLAYLIST_LEN);
          custom_musicbox_playlist_2.setValue(musicbox_playlist_2, MAX_PLAYLIST_LEN);
          custom_musicbox_playlist_3.setValue(musicbox_playlist_3, MAX_PLAYLIST_LEN);
          custom_net_delay.setValue(net_delay, 3);
          custom_sleep_minutes.setValue(sleep_minutes, 4);

          custom_ssid_csv.setValue(ssid_csv, (4 * MAX_USERNAME_LEN));
          custom_pass_csv.setValue(pass_csv, (4 * MAX_PASSWORD_LEN));

          custom_pw.setValue(pw, sizeof(pw));


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

// Helper for save config which allows us to reboot after.
void callSaveConfig(bool reboot)
{
  // Save and restart if desired
  saveConfig();
  if (reboot)
  {
    delay(50);
    ESP.restart();
  }
  
}

// Save the config, and reboot after if true (when firmware updates)
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
    // @params
    json["musicbox_username"] = musicbox_username;
    json["musicbox_password"] = musicbox_password;
    json["musicbox_playlist_1"] = musicbox_playlist_1;
    json["musicbox_playlist_2"] = musicbox_playlist_2;
    json["musicbox_playlist_3"] = musicbox_playlist_3;
    json["net_delay"] = net_delay;
    json["sleep_minutes"] = sleep_minutes;
    json["ssid_csv"] = ssid_csv;
    json["pass_csv"] = pass_csv;
    json["pw"] = pw;



    // Save spotify tokens (local values to disk)
    if (strlen(musicbox_password) > 1)
    {
      json["access_token"] = access_token;
      json["refresh_token"] = refresh_token;
    }
    else // If we logout, delete our token
    {
      json["access_token"] ="";
      json["refresh_token"] = "";
    }
    

    // Store latest Firmware Version
    json["firmware_version"] = firmware_version;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
    // serializeJson(json, Serial);
    serializeJson(json, configFile);
#else
    // json.printTo(Serial);
    json.printTo(configFile);
#endif
    configFile.close();
    //end save

}

// Setup callback functions, custom parameters, and timeouts
// Called whenever a wifiManager is started, usually (twice, one on boot one on user defined config mode)
void initializeConfigPortal()
{
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Set callback for when portal closes for any reason
  wifiManager.setConfigClosedCallback(closedConfigPortal);


  // Set non blocking (so we must call wm.process() in loop, always)
  wifiManager.setConfigPortalBlocking(false);
  
  // Add parameters only if they don't already exist
  if (wifiManager.getParametersCount() < 1)
  {
    // @params
    wifiManager.addParameter(&custom_musicbox_username);
    wifiManager.addParameter(&custom_musicbox_password);
    wifiManager.addParameter(&custom_musicbox_playlist_1);
    wifiManager.addParameter(&custom_musicbox_playlist_2);
    wifiManager.addParameter(&custom_musicbox_playlist_3);
    wifiManager.addParameter(&custom_sleep_minutes);
    wifiManager.addParameter(&custom_net_delay);
    wifiManager.addParameter(&custom_ssid_csv);
    wifiManager.addParameter(&custom_pass_csv);
    wifiManager.addParameter(&custom_pw);


  }


  // Set callback to reset settings
  //wifiManager.setConfigPortalTimeout(180); // Set timeout for configuration portal
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setConnectTimeout(5);
}

// Try to login with musicbox credentials
void authenticate()
{
  bool fail = false;
  

  // If we do not have an access token, (or if we logged out) reauthenticate
  if (strlen(access_token) == 0 || ((strlen(musicbox_username) * strlen(musicbox_password)) == 0))
  {
    // Need to get the tokens from my server
    fail = true;
    
    // Did we provide login info to MusicBox
    if ((strlen(musicbox_username) * strlen(musicbox_password)) > 0)
    {
      // Request tokens from my server
      HTTPClient http;


      String jsonPayload = "{\"username\":\"" + String(musicbox_username) + "\",\"pass\":\"" + String(musicbox_password) + "\"}";


      http.begin(musicbox_auth_url);
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

          // Save the new token
          callSaveConfig(false);
          
        } else {
          Serial.print("Failed to authorize with musicbox account: ");
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
        Serial.print("Error trying to auth with MusicBox server: ");
        Serial.print(http.errorToString(httpResponseCode).c_str());
        Serial.println(httpResponseCode);
      }

      http.end();
      jsonPayload = "";

    }
    else
    {
      // We did not provide login info.
      // If there is an access token, delete it
      if (strlen(access_token) > 0 || strlen(refresh_token) > 0)
      {
        strcpy(refresh_token, "");
        strcpy(access_token, ""); 
      }
      // Erases the sensitive data from storage
      callSaveConfig(false);
    }

    
  }
  else
  {
    // We already had our access token!
    
  }

  if (fail)
  {
    Serial.println("MusicBox account auth failure");
    displayError(true);
  }
  else
  {
    // Success
    sleep_start = millis(); // Set sleep timer to now
    Serial.println("Ready!");
    lights(1);

  }
}

void closedConfigPortal()
{
  // store updated parameters into local memory
  // @params
  strcpy(musicbox_username, custom_musicbox_username.getValue());
  strcpy(musicbox_password, custom_musicbox_password.getValue());
  strcpy(musicbox_playlist_1, custom_musicbox_playlist_1.getValue());
  strcpy(musicbox_playlist_2, custom_musicbox_playlist_2.getValue());
  strcpy(musicbox_playlist_3, custom_musicbox_playlist_3.getValue());
  strcpy(net_delay, custom_net_delay.getValue());
  strcpy(sleep_minutes, custom_sleep_minutes.getValue());
  strcpy(ssid_csv, custom_ssid_csv.getValue());
  strcpy(pass_csv, custom_pass_csv.getValue());
  strcpy(pw, custom_pw.getValue());



  callSaveConfig(false); // Will also store new preferences in storage
  

  // Connected to wifi!
  if (WiFi.status() == WL_CONNECTED) {
    authenticate();
  }
  else //We aren't connected to wifi, reboot (we probably just changed wifi credentials)
  {
    delay(1000);
    ESP.restart();
  }

}

void startConfigPortal()
{
  if (!wifiManager.getConfigPortalActive())
  {
    Serial.println("Entering Config");
    initializeConfigPortal();
    wifiManager.startConfigPortal(formSSID(), pw);
  }
}

void setup() {
  Serial.begin(115200);
  lights(3);

  pinMode(buttonPin, INPUT);
  //pinMode(playlistPin, INPUT); No longer using AB system

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  loadConfig(); // Load the custom data such as user login details, playlists, version, etc

  
  initializeConfigPortal();
  // Delay if necessary
  int net_delay_int = atoi(net_delay);
  if (net_delay_int != 0 || strcmp(net_delay, "0") == 0) {
    if (net_delay_int > 0)
    {
      // Valid and positive network delay
      delay(1000 * net_delay_int);
    }
  } 

  // Connect to Wi-Fi or start an Access Point if no credentials are stored
  // Synchronous call: Wait for this to finish
  if (wifiManager.autoConnect(formSSID(), pw, ssid_csv, pass_csv)) {
    
    // Check for new firmware
    httpUpdate.onStart(update_started);
    httpUpdate.onEnd(update_finished);
    httpUpdate.onProgress(update_progress);
    httpUpdate.onError(update_error);

    checkForUpdates(); // Checks for updates

    // Ensure we are authed with spotify
    authenticate();
    
  }
  else
  {
    // Failed to connect, config portal will open
  }


}


void loop() {
  // Run wifimanager
  wifiManager.process();

  // Sleep feature
  // Check if the amount of minutes has passed (if we are not already sleeping)
  if (!sleeping && (((millis() - sleep_start) / 60000) >= std::stoi(sleep_minutes)))
  {
    enter_sleep();
  }



  // Button presses
  if (quickpress_start > 0 && millis() - quickpress_start >= quickpress_window) {
    pressed = false;
    if (quick_presses > 0) {
      // Postpone sleep 
      sleep_start = millis();
      
      // Check the number of quick presses when the window expires
      if (quick_presses < 4) { // Single double or triple press: playlist

        // Only love songs if not in config portal
        if(!wifiManager.getConfigPortalActive())
        {
          String trackURI = getCurrentPlayingTrackURI();
          
          if (trackURI != "") {
            Serial.println("Track: "+trackURI);
            addTrackToPlaylist(trackURI);
          }
        }

      } else { // More than 3 presses means config mode (spam)
        Serial.println("> 3 presses, enter config");
        startConfigPortal();
      }
      // Reset quick press variables
      quick_presses = 0;
      quickpress_start = 0;
    }
  }


  // Execute once when it starts being pressed
  if (digitalRead(buttonPin) == HIGH) {
    // When it is first pressed, start timer
    if (!pressed) {
      
      pressed = true;
      press_begin = millis();
      
    }
    // During the duration of the press, check if it is held to enter config mode
    else {
      if (millis() - press_begin >= configModeDuration) {
        Serial.println("Restarting manually!");
        ESP.restart();
      }
    }
  }
  // Set pressed to false when the button is released (and pressed is true)
  else if (digitalRead(buttonPin) == LOW && pressed) {

    pressed = false;
    press_end = millis();

    if (sleeping) // then wake up ( do not use button )
    {
        leave_sleep();
    }

    else // we were not sleeping, use button
    {

      int press_duration = press_end - press_begin;

      // After releasing, if it was a short press, call the appropriate function
      if (press_duration < configModeDuration && press_duration > debounce) {
        //Serial.println("Short press");

        // Increment short presses for restart
        quick_presses++;
        // Serial.println(quick_presses);

        // Check if it is the first press, if so start another timer for the quick press feature
        if (quick_presses == 1) {
          quickpress_start = millis(); // Store the time of the first short press.
        }
      }

    }

    
  }

  
  

}
