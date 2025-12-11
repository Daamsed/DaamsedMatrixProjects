/*******************************************************************
    Display an image loaded from the internt on a matrix display

    This is a more generic version of the functionality worked on
    for the Spotify library.

    This loads an image from imgur. It uses https, but it could be adapted to
    http by just using a WiFiClient, rahter than WiFiClientSecure

    The image loaded is a 64 x 64 image, but it should display on smaller
    matrix (just be cut off).

    Parts Used:
      ESP32 Trinity - https://github.com/witnessmenow/ESP32-Trinity

    If you find what I do useful and would like to support me,
    please consider becoming a sponsor on Github
    https://github.com/sponsors/witnessmenow/

    Written by Brian Lough
      YouTube: https://www.youtube.com/brianlough
      Tindie: https://www.tindie.com/stores/brianlough/
      Twitter: https://twitter.com/witnessmenow
 *******************************************************************/

// ----------------------------
// Standard Libraries
// ----------------------------

#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <FS.h>
#include "SPIFFS.h"

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
// This is the library for interfacing with the display

// Can be installed from the library manager (Search for "ESP32 MATRIX DMA")
// https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA

#include <ArduinoJson.h>
// Library for parsing JSON from GitHub API
// Search for "ArduinoJson" in the Arduino Library manager
// https://github.com/bblanchon/ArduinoJson

#include <PNGdec.h>
// Library for decoding PNG files
// Install "PNGdec" via Arduino Library Manager

// ----------------------------
// Dependency Libraries - each one of these will need to be installed.
// ----------------------------

// Adafruit GFX library is a dependency for the matrix Library
// Can be installed from the library manager
// https://github.com/adafruit/Adafruit-GFX-Library

// ----------------------------
// Include Other Tabs
// ----------------------------

#include "WebRequest.h"
#include "secrets.h"  // WiFi credentials - DO NOT COMMIT TO GIT

// -------------------------------------
// -------   Matrix Config   ------
// -------------------------------------

const int panelResX = 64;      // Number of pixels wide of each INDIVIDUAL panel module.
const int panelResY = 64;     // Number of pixels tall of each INDIVIDUAL panel module.
const int panel_chain = 1;      // Total number of panels chained one to another

// See the "displaySetup" method for more display config options

//------------------------------------------------------------------------------------------------------------------

// -------------------------------------
//------- Other Config - Replace the following! ------
// -------------------------------------

// WiFi credentials are now in secrets.h
// Copy secrets_example.h to secrets.h and fill in your credentials
// Make sure secrets.h is in your .gitignore

// If you need to add a server cert for verifying the endpoint,
// Line 15 of web_fetch.h is where you would add it

//------- ---------------------- ------

// GitHub API endpoint for images directory
const char githubApiUrl[] = "api.github.com";
const char githubApiPath[] = "/repos/Daamsed/DaamsedMatrixProjects/contents/images/output_img";
const char rawGithubUrl[] = "https://raw.githubusercontent.com/Daamsed/DaamsedMatrixProjects/main/images/output_img/";

// Note the "/" at the start
const char imageLocation[] = "/img.png";

MatrixPanel_I2S_DMA *dma_display = nullptr;
PNG png;
WiFiClientSecure client;

// This next function will be called during decoding of the image file to
// render each block to the Matrix. If you use a different display
// you will need to adapt this function to suit.
// PNG draw callback
int PNGDraw(PNGDRAW *pDraw)
{
  // Only process up to 64 pixels per line
  int w = pDraw->iWidth;
  if (w > 64) w = 64;
  static uint16_t lineBuf[64];
  uint8_t *src = pDraw->pPixels;

  // Option 1: Use PNGdec's built-in conversion
  png.getLineAsRGB565(pDraw, lineBuf, PNG_RGB565_LITTLE_ENDIAN, 0x0000);

  // Option 2: Manual conversion (with optional gamma correction)
  // for (int i = 0; i < w; i++) {
  //   uint8_t r = src[i*3];
  //   uint8_t g = src[i*3+1];
  //   uint8_t b = src[i*3+2];
  //   // Apply gamma correction if needed
  //   lineBuf[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  // }

  // Center image horizontally
  int xOffset = (dma_display->width() - w) / 2;
  if (xOffset < 0) xOffset = 0;

  dma_display->drawRGBBitmap(xOffset, pDraw->y, lineBuf, w, 1);

  return 1;
}

void displaySetup() {
  HUB75_I2S_CFG mxconfig(
    panelResX,   // module width
    panelResY,   // module height
    panel_chain    // Chain length
  );

  // If you are using a 64x64 matrix you need to pass a value for the E pin
  // The trinity connects GPIO 18 to E.
  // This can be commented out for any smaller displays (but should work fine with it)
  mxconfig.gpio.e = 18;

  // May or may not be needed depending on your matrix
  // Example of what needing it looks like:
  // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/134#issuecomment-866367216
  mxconfig.clkphase = false;

  // Some matrix panels use different ICs for driving them and some of them have strange quirks.
  // If the display is not working right, try this.
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
}

void setup()
{
  Serial.begin(115200);
  
  // Seed the random number generator with a value from an unconnected analog pin
  randomSeed(analogRead(A0) + micros());

  displaySetup();

  dma_display->fillScreen(dma_display->color565(0, 0, 0));

  dma_display->setTextSize(1);     // size 1 == 8 pixels high
  dma_display->setTextWrap(false); // Don't wrap at end of line - will do ourselves
  dma_display->setTextColor(dma_display->color565(255, 0, 255));

  dma_display->setCursor(1, 0);    // start at top left, with 8 pixel of spacing

  dma_display->print("SPIFFS: ");
  dma_display->setCursor(48, 0);
  // Initialise SPIFFS, if this fails try .begin(true)
  if (!SPIFFS.begin() && !SPIFFS.begin(true)) {
    dma_display->print("NO!");
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  } else {
    dma_display->print("OK!");
  }

  dma_display->setCursor(1, 10);    // start at top left, with 8 pixel of spacing
  dma_display->print("WiFi: ");
  dma_display->setCursor(48, 10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(WIFI_SSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  dma_display->print("OK!");

  // this line ignores the HTTPS validation check
  // You could set a cert here instead to check the server
  // you are talking to is who you expect
  client.setInsecure();

  // Makes the example a little simpler to have a global for it in WebRequest
  webClient = &client;

}

fs::File myfile;

// File callbacks (used by decoders if needed)

void * myOpen(const char *filename, int32_t *size) {
  myfile = SPIFFS.open(filename);
  *size = myfile.size();
  return &myfile;
}
void myClose(void *handle) {
  if (myfile) myfile.close();
}
int32_t myRead(PNGFILE *handle, uint8_t *buffer, int32_t length) {
  if (!myfile) return 0;
  return myfile.read(buffer, length);
}
int32_t mySeek(PNGFILE *handle, int32_t position) {
  if (!myfile) return 0;
  return myfile.seek(position);
}

String getRandomImageUrl() {
  // Check WiFi connection first
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return "";
  }
  
  Serial.println("Creating secure client...");
  // Fetch the directory listing from GitHub API
  WiFiClientSecure apiClient;
  apiClient.setInsecure();
  apiClient.setTimeout(10000);  // 10 second timeout
  
  Serial.print("Connecting to ");
  Serial.print(githubApiUrl);
  Serial.println(":443...");
  
  if (!apiClient.connect(githubApiUrl, 443)) {
    Serial.println("Failed to connect to GitHub API");
    apiClient.stop();
    delay(2000);
    return "";
  }
  
  Serial.println("Connected! Sending request...");
  Serial.print("Path: ");
  Serial.println(githubApiPath);
  
  // Send HTTP request
  String request = String("GET ") + githubApiPath + " HTTP/1.1\r\n" +
                   "Host: " + githubApiUrl + "\r\n" +
                   "User-Agent: ESP32-Arduino\r\n" +
                   "Accept: application/vnd.github.v3+json\r\n" +
                   "Connection: close\r\n\r\n";
  
  apiClient.print(request);
  
  // Wait for response
  delay(500);
  
  // Skip HTTP headers
  Serial.println("Skipping headers...");
  int headerLines = 0;
  while (apiClient.connected()) {
    String line = apiClient.readStringUntil('\n');
    headerLines++;
    if (line == "\r") {
      Serial.print("Headers read: ");
      Serial.println(headerLines);
      break;
    }
  }
  
  // Read the JSON response - allocate larger buffer
  Serial.println("Reading JSON payload...");
  String payload = "";
  unsigned long startTime = millis();
  
  // Read data until connection closes or timeout
  while (apiClient.connected() || apiClient.available()) {
    if (apiClient.available()) {
      char c = apiClient.read();
      payload += c;
    } else {
      delay(10);
    }
    
    // Timeout after 10 seconds
    if (millis() - startTime > 10000) {
      Serial.println("Timeout reading response");
      break;
    }
  }
  apiClient.stop();
  
  if (payload.length() == 0) {
    Serial.println("Empty response from GitHub API");
    return "";
  }
  
  Serial.print("Total response length: ");
  Serial.println(payload.length());
  Serial.print("First 200 chars: ");
  Serial.println(payload.substring(0, 200));
  
  // Parse JSON - use a larger capacity
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::NestingLimit(50));
  
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    Serial.print("Payload ends with: ");
    Serial.println(payload.substring(payload.length() - 100));
    return "";
  }
  
  // Count image files (filter for .png)
  int imageCount = 0;
  for (JsonObject item : doc.as<JsonArray>()) {
    const char* name = item["name"];
    const char* type = item["type"];
    if (strcmp(type, "file") == 0) {
      if (strstr(name, ".png") != nullptr || strstr(name, ".PNG") != nullptr) {
        imageCount++;
        if (imageCount <= 5) {
          Serial.print("  Found image ");
          Serial.print(imageCount);
          Serial.print(": ");
          Serial.println(name);
        }
      }
    }
  }
  
  Serial.print("Total images found: ");
  Serial.println(imageCount);
  
  if (imageCount == 0) {
    Serial.println("No images found in GitHub folder");
    return "";
  }
  
  // Pick a random image
  int randomIndex = random(0, imageCount);
  Serial.print("Random index selected: ");
  Serial.print(randomIndex);
  Serial.print(" out of ");
  Serial.println(imageCount);
  
  int currentIndex = 0;
  
  for (JsonObject item : doc.as<JsonArray>()) {
    const char* name = item["name"];
    const char* type = item["type"];
      if (strcmp(type, "file") == 0) {
      if (strstr(name, ".png") != nullptr || strstr(name, ".PNG") != nullptr) {
        if (currentIndex == randomIndex) {
          String url = String(rawGithubUrl) + String(name);
          Serial.print("Selected image at index ");
          Serial.print(currentIndex);
          Serial.print(": ");
          Serial.println(url);
          return url;
        }
        currentIndex++;
      }
    }
  }
  
  return "";
}

void drawImageFile(char *imageFileUri) {
  // Clear the display before drawing the image
  dma_display->fillScreen(dma_display->color565(0, 0, 0));

  // Diagnostic: check SPIFFS file exists, size and header bytes
  Serial.print("Checking image file: ");
  Serial.println(imageFileUri);
  File f = SPIFFS.open(imageFileUri, "r");
  if (!f) {
    Serial.println("ERROR: cannot open image file for diagnostic");
    return;
  }
  size_t sz = f.size();
  Serial.print("SPIFFS file size: ");
  Serial.println(sz);
  // read first 8 bytes
  uint8_t header[8] = {0,0,0,0,0,0,0,0};
  if (sz >= 8) {
    f.read(header, 8);
    Serial.print("Header bytes: ");
    for (int i=0;i<8;i++) {
      if (header[i] < 16) Serial.print('0');
      Serial.print(header[i], HEX);
      Serial.print(' ');
    }
    Serial.println();
  }
  f.close();

  unsigned long lTime = millis();

  // Decide decoder based on header (PNG signature 89 50 4E 47)
  if (header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47) {
    Serial.println("Detected PNG - using PNGdec");
    // Use PNGdec to decode from SPIFFS
    int status = png.open(imageFileUri, myOpen, myClose, myRead, mySeek, PNGDraw);
    if (status == 0) { // PNG_SUCCESS is 0
      png.decode(NULL, 0);
      png.close();
    } else {
      Serial.print("PNG open failed, status: ");
      Serial.println(status);
    }
  } else {
    Serial.println("Unsupported image format - not PNG. Skipping draw.");
    return;
  }

  Serial.print("Time taken to decode and display Image (ms): ");
  Serial.println(millis() - lTime);
}

int displayImage(const char* image) {

  // In this example I re-use the same filename
  // over and over, but you could check does the image
  // exist and not fetch it again
  if (SPIFFS.exists(imageLocation) == true) {
    Serial.println("Removing existing image");
    SPIFFS.remove(imageLocation);
  }

  fs::File f = SPIFFS.open(imageLocation, "w+");
  if (!f) {
    Serial.println("file open failed");
    return -1;
  }

  unsigned long lTime = millis();

  bool gotImage = getImage((char *)image, &f);

  Serial.print("Time taken to get Image (ms): ");
  Serial.println(millis() - lTime);

  // Make sure to close the file!
  f.close();

  if (!gotImage) {
    return -2;
  }

  // Re-open the saved file and check header bytes to detect PNG
  File check = SPIFFS.open(imageLocation, "r");
  if (!check) {
    Serial.println("ERROR: cannot open saved image for verification");
    return -2;
  }
  uint8_t hdr[8] = {0};
  size_t n = check.read(hdr, 8);
  check.close();

  if (n >= 4) {
    // PNG signature starts with 0x89 0x50 0x4E 0x47
    if (hdr[0] == 0x89 && hdr[1] == 0x50 && hdr[2] == 0x4E && hdr[3] == 0x47) {
      Serial.println("Downloaded file is PNG - proceeding to decode");
    } else {
      Serial.println("Downloaded file is not PNG. Removing and skipping.");
      if (SPIFFS.exists(imageLocation)) SPIFFS.remove(imageLocation);
      return -3; // indicate unsupported format
    }
  }

  // If we reach here, attempt to draw
  drawImageFile((char *)imageLocation);
  return 0;
}

void loop()
{
  // Add delay to ensure WiFi is fully stable
  delay(3000);
  
  Serial.println("\n\n=== Fetching random image ===");
  // Try up to N attempts to fetch and display a non-PNG image
  const int maxAttempts = 6;
  int attempt = 0;
  while (attempt < maxAttempts) {
    attempt++;
    Serial.print("Attempt "); Serial.print(attempt); Serial.println("...");

    // Get a random image URL from GitHub
    String imageUrl = getRandomImageUrl();
    if (imageUrl.length() == 0) {
      Serial.println("Failed to get random image URL, retrying in 10 seconds...");
      delay(10000);
      continue;
    }

    Serial.print("Got URL: ");
    Serial.println(imageUrl);

    int displayImageResult = displayImage(imageUrl.c_str());
    if (displayImageResult == 0) {
      Serial.println("Image should be displayed!");
      break;
    } else if (displayImageResult == -3) {
      Serial.println("Downloaded file unsupported (likely PNG). Trying another image...");
      // try another image
      delay(500);
      continue;
    } else {
      Serial.print("Failed to display image (error): ");
      Serial.println(displayImageResult);
      // For other errors, wait before retrying
      delay(5000);
      continue;
    }
  }

  if (attempt >= maxAttempts) {
    Serial.println("Exceeded max attempts, waiting 60s before next cycle");
    delay(60000);
  } else {
    // Success - pause a bit before next run
    delay(60000);
  }
}
