#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>
#include <StreamUtils.h>
#include <Servo.h>
#include <Utils.h>

#define PRINT(s, x)
#define PRINTS(x)
#define PRINTX(x)

// Hardware configuration
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define BUF_SIZE 250 // Buffer length
#define MAX_DEVICES 16 // Number of modules <- TODO - Change with your data
#define CLK_PIN D5 // SCK <- TODO - Change with your data (If needed)
#define DATA_PIN D7 // MOSI <- TODO - Change with your data (If needed)
#define CS_PIN D8 // SS <- TODO - Change with your data (If needed)
#define EEPROM_SIZE 512 // EEPROM size
#define SERVO_PIN D1 // Servo pin

AsyncWebServer server(80); // Web server
WiFiClient client; // Client object
HTTPClient http; // HTTP object
Servo servoMotor; // Servo object
const char accessPointSSID[] = "Bitcoin-Ticker"; // Access point SSID
char wiFiSSID[35] = ""; // Network SSID
char wiFiPassword[70] = ""; // Network password
char apiKey[35] = ""; // Your API key for financialmodelingprep.com
bool accessPointEnabled = false; // If access point enabled
bool disableAccessPoint = false; // If I need to disable the access point
enum connectionStatus { WIFI_TRY = 2, WIFI_OK = 1, WIFI_KO = 0 }; // Connection status
connectionStatus wiFiConnectionStatus = WIFI_KO; // Connection status
unsigned long currentMillis; // Current time
unsigned long timestampStockData = 0; // Timestamp stock data
unsigned long timestampWiFiConnection = 0; // Timestamp WiFi connection
unsigned long detachServoTimestamp = 0; // Timestamp for detaching the servo motor
enum formatNum { FORMAT_US = 1, FORMAT_EU = 2 }; // Numeric formatting type
formatNum formatType = FORMAT_US; // Current numeric formatting type
enum printType { PRINT_PRICE = 0, PRINT_CHANGE = 1, PRINT_MARKET_CAP = 2, PRINT_DAILY_HIGH_LOW = 3, PRINT_YEAR_HIGH_LOW = 4, PRINT_OPEN = 5, PRINT_VOLUME = 6, PRINT_BITCOIN_MINED = 7 }; // Print type
printType switchText = PRINT_PRICE; // Variable for the switch
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
uint8_t scrollDelay = 30; // Matrix refresh delay
textEffect_t scrollEffect = PA_SCROLL_LEFT; // Scrolling effect
textPosition_t scrollAlign = PA_LEFT; // Scroll direction
uint16_t scrollPause = 0; // Pause at the end of scrolling
bool currentPriceVisible = true; // Current price visible
bool priceChangeVisible = true; // Price change visible
bool marketCapVisible = true; // Market cap visible
bool dailyHighLowVisible = true; // Daily high/low visible
bool yearHighLowVisible = true; // Year high/low visible
bool openPriceVisible = true; // Open price visible
bool volumeVisible = true; // Volume visible
bool bitcoinMinedVisible = true; // Total bitcoin mined visible
uint16_t priceScrollPause = 30000; // Scroll pause for the current price
bool servoMotorEnabled = true; // If servo motor enabled
bool newDataToSaveToEEPROM = false; // If there is new data to save

// Global message buffers shared by serial and scrolling functions
char currentMessage[BUF_SIZE]; // Current message
char stripMessagePrice[BUF_SIZE]; // Price
char stripMessageDailyChange[BUF_SIZE]; // Change
char stripMessageMarketCap[BUF_SIZE]; // Market Cap
char stripMessageDailyHighLow[BUF_SIZE]; // Daily High/Low
char stripMessageYearHighLow[BUF_SIZE]; // Year High/Low
char stripMessageOpen[BUF_SIZE]; // Open
char stripMessageVolume[BUF_SIZE]; // Volume
char stripMessageBitcoinMined[BUF_SIZE]; // Total Bitcoin mined

// Print the message on the matrix
void printOnLedMatrix(const char* message, const byte stringLength, uint16_t messageStill = scrollPause) {
	stringCopy(currentMessage, message, stringLength); // Copy the message
	P.displayText(currentMessage, scrollAlign, scrollDelay, messageStill, scrollEffect, scrollEffect); // Print the message on the matrix
}

// Read data from the EEPROM
bool readEEPROM(JsonDocument& doc) {
    EepromStream eepromStream(0, EEPROM_SIZE);
	DeserializationError error = deserializeJson(doc, eepromStream);
    if (error) { // Check for errors during deserialization
        Serial.print("Error while reading the EEPROM: ");
        Serial.println(error.c_str()); // Print the error message
        return false;
    }
    return true; // Read success
}

// Write data on the EEPROM
bool writeEEPROM() {
	if (!newDataToSaveToEEPROM) // Check if the data needs to be saved
		return true; // If not exit the function
	EepromStream eepromStream(0, EEPROM_SIZE);
	JsonDocument doc; // JSON object
	if (!readEEPROM(doc)) // Read data from EEPROM
		Serial.println("Overwriting the saved data...");
	if (doc["apiKey"] != apiKey && apiKey[0] != '\0') // Check if the API key is the same
		doc["apiKey"] = apiKey; // Update API key
	if (doc["ssid"] != wiFiSSID && wiFiSSID[0] != '\0') // Check if the WiFi SSID is the same
		doc["ssid"] = wiFiSSID; // Update WiFi SSID
	if (doc["password"] != wiFiPassword) // Check if the WiFi password is the same (Password can be empty)
		doc["password"] = wiFiPassword; // Update WiFi password
	if (doc["currentPriceVisible"] != currentPriceVisible) // Check if the current price visibility is the same
		doc["currentPriceVisible"] = currentPriceVisible; // Update current price visibility
	if (doc["priceChangeVisible"] != priceChangeVisible) // Check if the price change visibility is the same
		doc["priceChangeVisible"] = priceChangeVisible; // Update price change visibility
	if (doc["marketCapVisible"] != marketCapVisible) // Check if the market cap visibility is the same
		doc["marketCapVisible"] = marketCapVisible; // Update market cap visibility
	if (doc["dailyHighLowVisible"] != dailyHighLowVisible) // Check if the daily high/low visibility is the same
		doc["dailyHighLowVisible"] = dailyHighLowVisible; // Update daily high/low visibility
	if (doc["yearHighLowVisible"] != yearHighLowVisible) // Check if the year high/low visibility is the same
		doc["yearHighLowVisible"] = yearHighLowVisible; // Update year high/low visibility
	if (doc["openPriceVisible"] != openPriceVisible) // Check if the open price visibility is the same
		doc["openPriceVisible"] = openPriceVisible; // Update open price visibility
	if (doc["volumeVisible"] != volumeVisible) // Check if the volume visibility is the same
		doc["volumeVisible"] = volumeVisible; // Update volume visibility
	if (doc["bitcoinMinedVisible"] != bitcoinMinedVisible) // Check if the total bitcoin mined visibility is the same
		doc["bitcoinMinedVisible"] = bitcoinMinedVisible; // Update total bitcoin mined visibility
	if (doc["formatType"].as<formatNum>() != formatType) // Check if the formatting type is the same
		doc["formatType"] = formatType; // Update formatting type
	if (doc["priceScrollPause"].as<uint16_t>() != priceScrollPause) // Check if the scroll pause is the same
		doc["priceScrollPause"] = priceScrollPause; // Update scroll pause
	if (!serializeJson(doc, eepromStream))
		return false; // Error while writing on EEPROM
	if (!EEPROM.commit()) // Commit changes
		return false; // Error while committing changes
	newDataToSaveToEEPROM = false; // Mark as saved
	Serial.println("Data saved on EEPROM");
	return true; // Write success
}

// Format currency
void formatCurrency(double value, char* output, const byte length) {
	if (formatType == FORMAT_US)
		stringCopy(output, addThousandsSeparators(value * 100, 2, '.', ','), length);
	else
		stringCopy(output, addThousandsSeparators(value * 100, 2, ',', '.'), length);
}

// Move the servo motor
void moveServoMotor(double percentage) {
	servoMotor.attach(SERVO_PIN); // Attach the servo motor
	const double maxPercentage = 10; // Max percentage
	const double minPercentage = -10; // Min percentage
	percentage = max(minPercentage, min(percentage, maxPercentage)); // Keep the value between min and max
  	const int angle = mapValue(percentage, minPercentage, maxPercentage, 0, 180); // Map the value between 0 and 180
  	servoMotor.write(angle); // Move the servo
	Serial.print("Moving the servo to an angle of ");
	Serial.println(angle);
	detachServoTimestamp = millis(); // Save the timestamp
}

// Detach the servo motor
void detachServo() {
	if (detachServoTimestamp == 0) // Check if the servo needs to be detached
		return; // If not exit the function
	if (millis() - detachServoTimestamp > 1000) { // Check if it's time to detach the servo
		Serial.println("Detaching the servo...");
		servoMotor.detach(); // Detach the servo
		detachServoTimestamp = 0; // Reset the timestamp
	}
}

// Create the scrolling message
void createStockDataMessage(JsonDocument doc) {
	Serial.println("Creating message...");
	const byte MAX_NUMBER_SIZE = 30; // Max length for the numbers
	char tempString[MAX_NUMBER_SIZE]; // Temporary string 1
	char tempString2[MAX_NUMBER_SIZE]; // Temporary string 2
	double tempVal; // Temporary variable 1
	double tempVal2; // Temporary variable 2

	// Price
	tempVal = doc[0]["price"].as<double>();
	tempVal2 = doc[0]["changesPercentage"].as<double>();
	if (servoMotorEnabled) // Check if the servo motor is enabled
		moveServoMotor(tempVal2); // Move the servo
	formatCurrency(tempVal, tempString, MAX_NUMBER_SIZE); // Format number
	formatCurrency(tempVal2, tempString2, MAX_NUMBER_SIZE); // Format number
	snprintf(stripMessagePrice, BUF_SIZE, " BTC: $ %s (%s%%)", tempString, tempString2);

	// Daily Change
	tempVal = doc[0]["change"].as<double>();
	formatCurrency(tempVal, tempString, MAX_NUMBER_SIZE); // Format number
	snprintf(stripMessageDailyChange, BUF_SIZE, "Daily Change: $ %s", tempString);

	// Market cap
	tempVal = doc[0]["marketCap"].as<double>();
	formatCurrency(tempVal, tempString, MAX_NUMBER_SIZE); // Format number
	snprintf(stripMessageMarketCap, BUF_SIZE, "Market Cap: $ %s", tempString);

	// Daily High/Low
	tempVal = doc[0]["dayHigh"].as<double>();
	tempVal2 = doc[0]["dayLow"].as<double>();
	formatCurrency(tempVal, tempString, MAX_NUMBER_SIZE); // Format number
	formatCurrency(tempVal2, tempString2, MAX_NUMBER_SIZE); // Format number
	snprintf(stripMessageDailyHighLow, BUF_SIZE, "Daily High: $ %s  -  Daily Low: $ %s", tempString, tempString2);

	// Year High/Low
	tempVal = doc[0]["yearHigh"].as<double>();
	tempVal2 = doc[0]["yearLow"].as<double>();
	formatCurrency(tempVal, tempString, MAX_NUMBER_SIZE); // Format number
	formatCurrency(tempVal2, tempString2, MAX_NUMBER_SIZE); // Format number
	snprintf(stripMessageYearHighLow, BUF_SIZE, "Year High: $ %s  -  Year Low: $ %s", tempString, tempString2);

	// Open
	tempVal = doc[0]["open"].as<double>();
	formatCurrency(tempVal, tempString, MAX_NUMBER_SIZE); // Format number
	snprintf(stripMessageOpen, BUF_SIZE, "Open: $ %s", tempString);

	// Volume
	tempVal = doc[0]["volume"].as<double>();
	formatCurrency(tempVal, tempString, MAX_NUMBER_SIZE); // Format number
	snprintf(stripMessageVolume, BUF_SIZE, "Volume: $ %s", tempString);

	// Bitcoin mined
	tempVal = doc[0]["sharesOutstanding"].as<double>();
	tempVal2 = tempVal / 21000000 * 100; // Current supply percentage
	formatCurrency(tempVal, tempString, MAX_NUMBER_SIZE); // Format number
	formatCurrency(tempVal2, tempString2, MAX_NUMBER_SIZE); // Format number
	snprintf(stripMessageBitcoinMined, BUF_SIZE, "Total Bitcoin mined: %s (%s%%)", tempString, tempString2);
}

// Getting Bitcoin data
bool getStockDataAPI() {
    char url[100]; // URL modificato per utilizzare HTTP
    sprintf(url, "http://financialmodelingprep.com/api/v3/quote/BTCUSD?apikey=%s", apiKey);
    http.begin(client, url); // Inizia una chiamata HTTP all'URL specificato
    if (http.GET() == HTTP_CODE_OK) {
        Serial.println("Response body: " + http.getString());
        JsonDocument doc; // Create the JSON object
		DeserializationError error = deserializeJson(doc, http.getString()); // Deserialize the JSON object
		if (error) { // Error while parsing the JSON
			Serial.printf("Error while parsing the JSON: %s\n", error.c_str());
			http.end();
			return false;
		}
		createStockDataMessage(doc); // Create the scrolling message
		http.end(); // Close connection
		return true;
    } else {
        Serial.printf("HTTP call error: %d\n", http.GET());
        http.end();
        return false;
    }
}

// Connecting to WiFi
bool connectToWiFi() {
	WiFi.begin(wiFiSSID, wiFiPassword); // Connecting to the WiFi
	byte maxTry = 50; // Maximum number of attempts to connect to WiFi
	byte count = 0; // Counter
	Serial.print("Connecting to WiFi");
	while (WiFi.status() != WL_CONNECTED) {
		if (count >= maxTry) {
			wiFiSSID[0] = '\0'; // Reset network SSID
			wiFiPassword[0] = '\0'; // Reset network password
			return false; // Connection failed
		}
		count++;
		Serial.print(".");
		delay(250);
	}
	Serial.println(" connected!");
	writeEEPROM(); // Save the network credentials
	return true; // Connection success
}

// Setup delle route
void setupRoutes() {
	server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html"); // Serve web page
	server.onNotFound([](AsyncWebServerRequest *request) { // Page not found
		request->send(404);
	});

	// Connect to WiFi
	server.on("/connect", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (!request->hasParam("ssid") || !request->hasParam("password")) { // Check required fields
			request->send(400, "application/json", "{\"status\":\"KO\"}"); // Response
			return;
		}
		stringCopy(wiFiSSID, request->getParam("ssid")->value().c_str(), 35); // Save the SSID
		stringCopy(wiFiPassword, request->getParam("password")->value().c_str(), 70); // Save the password
		Serial.print("SSID: ");
		Serial.println(wiFiSSID);
		Serial.print("Password: ");
		Serial.println(wiFiPassword);
		wiFiConnectionStatus = WIFI_TRY; // Trying to connect
		char jsonResponse[20]; // JSON response
		snprintf(jsonResponse, sizeof(jsonResponse), "{\"status\":\"%d\"}", wiFiConnectionStatus); // Create response
		request->send(200, "application/json", jsonResponse); // Response
		newDataToSaveToEEPROM = true; // New credentials
	});

	// Check the WiFi connection status
	server.on("/checkConnection", HTTP_GET, [](AsyncWebServerRequest *request) {
		char jsonResponse[20]; // JSON response
		snprintf(jsonResponse, sizeof(jsonResponse), "{\"status\":\"%d\"}", wiFiConnectionStatus);
		switch(wiFiConnectionStatus) {
			case WIFI_TRY:
				request->send(200, "application/json", jsonResponse); // Response
				break;
			case WIFI_OK:
				disableAccessPoint = true; // I need to disable the access point
				request->send(200, "application/json", jsonResponse); // Response
				break;
			case WIFI_KO:
				request->send(400, "application/json", jsonResponse); // Response
				break;
		}
	});

	// Send the list of networks
	server.on("/networks", HTTP_GET, [](AsyncWebServerRequest *request) {
		WiFi.scanNetworksAsync([request](int numNetworks) {
			JsonDocument doc; // JSON object
			JsonArray networks = doc["networks"].to<JsonArray>();
			for (int i = 0; i < numNetworks; i++) {
				JsonObject network = networks.add<JsonObject>();
				network["ssid"] = WiFi.SSID(i);
				network["signal"] = WiFi.RSSI(i);
			}
			size_t jsonLength = measureJson(doc) + 1; // Get the size of the JSON object
			char json[jsonLength];
			serializeJson(doc, json, jsonLength);
			request->send(200, "application/json", json); // Send the JSON object
		});
    });

	// Save the API key
	server.on("/apiKey", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (!request->hasParam("apiKey")) { // Check if the API key is missing
        	request->send(400, "application/json", "{\"status\":\"KO\"}"); // Send the JSON object
        	return;
    	}
		stringCopy(apiKey, request->getParam("apiKey")->value().c_str(), 35); // Save the API key
		request->send(200, "application/json", "{\"status\":\"OK\"}"); // Send the JSON object
		Serial.println("API key changed");
		newDataToSaveToEEPROM = true; // New API key
	});

	// Get the values visibility settings
	server.on("/valuesSettings", HTTP_GET, [](AsyncWebServerRequest *request) {
		JsonDocument doc; // JSON object
		doc["currentPrice"] = currentPriceVisible ? "Y" : "N";
		doc["priceChange"] = priceChangeVisible ? "Y" : "N";
		doc["marketCap"] = marketCapVisible ? "Y" : "N";
		doc["dailyHighLow"] = dailyHighLowVisible ? "Y" : "N";
		doc["yearHighLow"] = yearHighLowVisible ? "Y" : "N";
		doc["openPrice"] = openPriceVisible ? "Y" : "N";
		doc["volume"] = volumeVisible ? "Y" : "N";
		doc["bitcoinMined"] = bitcoinMinedVisible ? "Y" : "N";
		doc["formatType"] = formatType == FORMAT_US ? "US" : "EU";
		size_t jsonLength = measureJson(doc) + 1; // Get the size of the JSON object
		char json[jsonLength];
		serializeJson(doc, json, jsonLength);
		request->send(200, "application/json", json); // Send the JSON object
	});

	// Save the values visibility settings
	server.on("/saveValuesSettings", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (request->hasParam("currentPrice"))
			currentPriceVisible = request->getParam("currentPrice")->value() == "Y";
		if (request->hasParam("priceChange"))
			priceChangeVisible = request->getParam("priceChange")->value() == "Y";
		if (request->hasParam("marketCap"))
			marketCapVisible = request->getParam("marketCap")->value() == "Y";
		if (request->hasParam("dailyHighLow"))
			dailyHighLowVisible = request->getParam("dailyHighLow")->value() == "Y";
		if (request->hasParam("yearHighLow"))
			yearHighLowVisible = request->getParam("yearHighLow")->value() == "Y";
		if (request->hasParam("openPrice"))
			openPriceVisible = request->getParam("openPrice")->value() == "Y";
		if (request->hasParam("volume"))
			volumeVisible = request->getParam("volume")->value() == "Y";
		if (request->hasParam("bitcoinMined"))
			bitcoinMinedVisible = request->getParam("bitcoinMined")->value() == "Y";
		if (request->hasParam("formatType"))
			formatType = request->getParam("formatType")->value() == "US" ? FORMAT_US : FORMAT_EU;
		if (request->hasParam("priceScrollPause"))
			priceScrollPause = request->getParam("priceScrollPause")->value().toInt();
		request->send(200, "application/json", "{\"status\":\"OK\"}");
		Serial.println("Values settings changed");
		newDataToSaveToEEPROM = true; // New settings
		writeEEPROM(); // Save the settings
	});
}

// Setting up the access point
bool setupAccessPoint() {
	if (accessPointEnabled) // Check if already enabled
		return true; // If enabled exit the function
	accessPointEnabled = WiFi.softAP(accessPointSSID); // Start the access point
	if (!accessPointEnabled) // Check if enabled
		return false; // If not enabled exit the function
	return true; // Access point enabled
}

// Manage WiFi connection
bool manageWiFiConnection() {
	if (wiFiConnectionStatus == WIFI_TRY) { // Check if already trying to connect
		if (connectToWiFi()) { // Connecting to WiFi
			wiFiConnectionStatus = WIFI_OK; // Update connection status
			return true; // Connection success
		} else {
			wiFiConnectionStatus = WIFI_KO; // Update connection status
			return false; // Connection failed
		}
	}

	// Every 2 seconds
	if (millis() - timestampWiFiConnection > 2000) {
		timestampWiFiConnection = millis(); // Save timestamp
		if (disableAccessPoint && accessPointEnabled) { // Check if I need to disable the access point
			accessPointEnabled = !WiFi.softAPdisconnect(); // Disable access point
			if (!accessPointEnabled) // Check if disabled
				disableAccessPoint = false; // Mark as disabled
		}
		if (WiFi.status() == WL_CONNECTED) // Check if connected to WiFi
			return true; // If connected exit the function
		if (wiFiSSID[0] != '\0' && wiFiPassword[0] != '\0') { // Check if credentials are already present
			if (connectToWiFi()) // Connecting to WiFi
				return true; // Connection success
			else
				setupAccessPoint(); // Setup access point
		} else {
			setupAccessPoint(); // Setup access point
		}
		return false; // Connection failed
	}
	return true; // Connection success
}

// Check if connected to WiFi
bool checkWifiConnection() {
	if (WiFi.status() != WL_CONNECTED) {
		const char errorMessage[] = "Not connected to Wi-Fi. Use the 'Bitcoin-Ticker' access point to enter the Wi-Fi credentials.";
		Serial.println(errorMessage);
		printOnLedMatrix(errorMessage, sizeof(errorMessage)); // Print the message on the matrix
		return false; // Not connected
	}
	return true; // Connected
}

// Call the API to get the data
bool callAPI() {
	currentMillis = millis();
	if (currentMillis - timestampStockData > 360000 || timestampStockData == 0) { // Call the API every 6 minutes (To limit usage)
		if (apiKey[0] == '\0') { // Check if API key is present
			const char errorMessageApi[] = "API key is not present. Use the web page to insert the key and try again.";
			Serial.println(errorMessageApi);
			printOnLedMatrix(errorMessageApi, sizeof(errorMessageApi)); // Print the message on the matrix
			return false; // If error, return false
		}
		Serial.println("Calling the API");
		if (!getStockDataAPI()) { // Getting the data
			const char errorMessageServer[] = "Error while calling the API. Retrying...";
			Serial.println(errorMessageServer);
			printOnLedMatrix(errorMessageServer, sizeof(errorMessageServer)); // Print the message on the matrix
			return false; // If error, return false
		}
		writeEEPROM(); // Save the apiKey to the EEPROM
		timestampStockData = currentMillis; // Save timestamp
	}
	return true; // If no error, return true
}

// Manage the LED matrix
void manageLedMatrix() {
	detachServo(); // Check if I need to detach the servo
	if (!P.displayAnimate())
		return; // If scrolling, exit the function
	if (!checkWifiConnection()) // Check if connected to WiFi
		return; // If not connected, exit the function
	if (!callAPI()) // Call the API
		return; // If error, exit the function

	// Print messagges
	switch (switchText) {
		case PRINT_PRICE:
			Serial.println("Section: PRICE");
			if (currentPriceVisible) // Check if current price is visible
				printOnLedMatrix(stripMessagePrice, BUF_SIZE, priceScrollPause); // Print the message on the matrix
			switchText = PRINT_CHANGE;
			break;

		case PRINT_CHANGE:
			Serial.println("Section: CHANGE");
			if (priceChangeVisible) // Check if price change is visible
				printOnLedMatrix(stripMessageDailyChange, BUF_SIZE); // Print the message on the matrix
			switchText = PRINT_MARKET_CAP;
			break;
		
		case PRINT_MARKET_CAP:
			Serial.println("Section: MARKET CAP");
			if (marketCapVisible) // Check if market cap is visible
				printOnLedMatrix(stripMessageMarketCap, BUF_SIZE); // Print the message on the matrix
			switchText = PRINT_DAILY_HIGH_LOW;
			break;

		case PRINT_DAILY_HIGH_LOW:
			Serial.println("Section: DAILY HIGHLOW");
			if (dailyHighLowVisible) // Check if daily high/low is visible
				printOnLedMatrix(stripMessageDailyHighLow, BUF_SIZE); // Print the message on the matrix
			switchText = PRINT_YEAR_HIGH_LOW;
			break;

		case PRINT_YEAR_HIGH_LOW:
			Serial.println("Section: YEAR HIGHLOW");
			if (yearHighLowVisible) // Check if year high/low is visible
				printOnLedMatrix(stripMessageYearHighLow, BUF_SIZE); // Print the message on the matrix
			switchText = PRINT_OPEN;
			break;

		case PRINT_OPEN:
			Serial.println("Section: OPEN");
			if (openPriceVisible) // Check if open price is visible
				printOnLedMatrix(stripMessageOpen, BUF_SIZE); // Print the message on the matrix
			switchText = PRINT_VOLUME;
			break;
			
		case PRINT_VOLUME:
			Serial.println("Section: VOLUME");
			if (volumeVisible) // Check if volume is visible
				printOnLedMatrix(stripMessageVolume, BUF_SIZE); // Print the message on the matrix
			switchText = PRINT_BITCOIN_MINED;
			break;
		
		case PRINT_BITCOIN_MINED:
			Serial.println("Section: MINED");
			if (bitcoinMinedVisible) // Check if bitcoin mined is visible
				printOnLedMatrix(stripMessageBitcoinMined, BUF_SIZE); // Print the message on the matrix
			switchText = PRINT_PRICE;
			break;
	}
}

// Setup the web client
void setupWebClient() {
    http.setTimeout(5000); // Set timeout
}

// Setup the LED matrix
void setupLedMatrix() {
	P.begin(); // Start the LED matrix
	printOnLedMatrix("Initializing...", BUF_SIZE); // Print the message on the matrix
}

// Setup LittleFS
bool setupLittleFS() {
	if (!LittleFS.begin()) { // Check if LittleFS is mounted
		Serial.println("An Error has occurred while mounting LittleFS");
		return false;
	} else {
		return true;
	}
}

// Setup server
void setupServer() {
	setupRoutes(); // Setup routes
	server.begin(); // Start the server
}

// read the already saved settings from the EEPROM
void readSavedSetting(JsonDocument doc) {
	if (doc.containsKey("apiKey") && !doc["apiKey"].isNull()) // Check if the API key is present
		stringCopy(apiKey, doc["apiKey"], sizeof(apiKey));
	if (doc.containsKey("ssid") && !doc["ssid"].isNull()) // Check if the WiFi SSID is present
		stringCopy(wiFiSSID, doc["ssid"], sizeof(wiFiSSID));
	if (doc.containsKey("password") && !doc["password"].isNull()) // Check if the WiFi password is present
		stringCopy(wiFiPassword, doc["password"], sizeof(wiFiPassword));
	if (doc.containsKey("currentPrice")) // Check if the current price is present
		currentPriceVisible = doc["currentPrice"].as<bool>();
	if (doc.containsKey("priceChange")) // Check if the price change is present
		priceChangeVisible = doc["priceChange"].as<bool>();
	if (doc.containsKey("marketCap")) // Check if the market cap is present
		marketCapVisible = doc["marketCap"].as<bool>();
	if (doc.containsKey("dailyHighLow")) // Check if the daily high/low is present
		dailyHighLowVisible = doc["dailyHighLow"].as<bool>();
	if (doc.containsKey("yearHighLow")) // Check if the year high/low is present
		yearHighLowVisible = doc["yearHighLow"].as<bool>();
	if (doc.containsKey("openPrice")) // Check if the open price is present
		openPriceVisible = doc["openPrice"].as<bool>();
	if (doc.containsKey("volume")) // Check if the volume is present
		volumeVisible = doc["volume"].as<bool>();
	if (doc.containsKey("bitcoinMined")) // Check if the bitcoin mined is present
		bitcoinMinedVisible = doc["bitcoinMined"].as<bool>();
	if (doc.containsKey("formatType") && !doc["formatType"].isNull()) // Check if the format type is present
		formatType = doc["formatType"].as<formatNum>();
	if (doc.containsKey("priceScrollPause") && !doc["priceScrollPause"].isNull()) // Check if the price scroll pause is present
		priceScrollPause = doc["priceScrollPause"].as<uint16_t>();
}

// Setup EEPROM
void setupEEPROM() {
	EEPROM.begin(EEPROM_SIZE); // Start the EEPROM
	JsonDocument doc; // JSON object
	if (!readEEPROM(doc)) // Read the EEPROM
		return; // If error, exit the function
	readSavedSetting(doc); // Read the saved settings
	Serial.println("EEPROM data loaded");
}

// Setup
void setup() {
	Serial.begin(9600); // Start serial
	setupLittleFS(); // Setup LittleFS
	setupEEPROM(); // Setup EEPROM
	setupServer(); // Setup server
	manageWiFiConnection(); // Manage WiFi connection
	setupWebClient(); // Setup web client
	setupLedMatrix(); // Setup LED matrix
}

// Loop
void loop() {
	manageWiFiConnection(); // Manage WiFi connection
	manageLedMatrix(); // Manage the LED matrix
}