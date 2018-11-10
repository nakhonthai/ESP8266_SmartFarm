/* ESP8266 Programmable Moisture Control IoT

Version 1.0  1/11/2018  Version 1.0   Base on Damon Borgnino
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <FS.h> // FOR SPIFFS
#include <ctype.h> // for isNumber check

#define RELAY_CH1 12
#define RELAY_CH2 13

//กำหนดค่าไวไฟเร้าเตอร์ที่จะให้ไปเชื่อมต่อ
const char* ssid = "APRSTH";
const char* password = "aprsthnetwork";
boolean Client_Connect();

//กำหนดค่าความชื้นเริ่มต้น
int moistureOn = 700;
int moistureOff = 400;

String relayState = "OFF";
const static String fName = "prop.txt";
const static String dfName = "data.txt";
int dataLines = 0;
int maxFileData = 20;

//ประกาศเว็บเซิร์ฟเวอร์พอร์ต 80
ESP8266WebServer server(80);

int moisture;
String webString = "";   // String to display
String webMessage = "";
// Generally, you should use "unsigned long" for variables that hold time to handle rollover
unsigned long previousMillis = 0;        // will store last temp was read
long interval = 20000;              // interval at which to read sensor


// อ่านข้อมูลจากเซ็นเซอร์วัดความชื้นดิน Capacitive Soil Moisture V1.2
void getmoisture() {

	// อ่านสัญญาณอนาล๊อคแรงดัน 0-1.0V แปลงเป็นข้อมูลดิจิตอลขนาด 10บิต จะได้ข้อมูล 0-1023 ค่า
	int sensorValue = analogRead(A0);

	moisture = sensorValue&0x3FF;

	// ทำงานตามเงื่อนไข
	if (moisture >= moistureOn)
	{
		//ค่าที่วัดได้มีค่ามากกว่าหรือเท่ากับค่าที่ตั้งไว้ให้ เปิดไฟ
		digitalWrite(RELAY_CH1, HIGH);
		relayState = "ON";
	}
	else if (moisture <= moistureOff)
	{
		//ค่าที่วัดได้มีค่าน้อยกว่าหรือเท่ากับค่าที่ตั้งไว้ให้ ปิดไฟ
		digitalWrite(RELAY_CH1, LOW);
		relayState = "OFF";
	}
}

/////////////////////////////////////////////////

void clearDataFile() // deletes all the stored data
{
	File f = SPIFFS.open(dfName, "w");
	if (!f) {
		Serial.println("data file open to clear failed");
	}
	else
	{
		Serial.println("-- Data file cleared =========");
		f.close();
	}
}

///////////////////////////////////////////////////////////////////////////////////
// removes the first line of a file by writing all data except the first line
// to a new file. The old file is deleted and new file is renamed.
///////////////////////////////////////////////////////////////////////////////////

void removeFileLine(String fi)
{
	File original = SPIFFS.open(fi, "r");
	if (!original) {

		Serial.println("original data file open failed");
	}

	File temp = SPIFFS.open("tempfile.txt", "w");
	if (!temp) {

		Serial.println("temp data file open failed");
	}

	Serial.println("------ Removing Data Lines ------");

	//Lets read line by line from the file
	for (int i = 0; i < maxFileData; i++) {

		String str = original.readStringUntil('\n'); // read a line
		if (i > 0) { // skip writing first line to the temp file

			temp.println(str);
			//  Serial.println(str); // uncomment to view the file data in the serial console
		}
	}

	int origSize = original.size();
	int tempSize = temp.size();
	temp.close();
	original.close();

	Serial.print("size orig: "); Serial.print(origSize); Serial.println(" bytes");
	Serial.print("size temp: "); Serial.print(tempSize); Serial.println(" bytes");

	if (!SPIFFS.remove(dfName))
		Serial.println("Remove file failed");


	if (!SPIFFS.rename("tempfile.txt", dfName))
		Serial.println("Rename file failed");
	// dataLines--;
}

//////////////////////////////////////////////////////////////////////
// writes the most recent variable values to the data file          //
//////////////////////////////////////////////////////////////////////

void updateDataFile()
{
	Serial.println("dataLines: ");
	Serial.println(dataLines);
	if (dataLines >= maxFileData)
	{
		removeFileLine(dfName);
	}
	///////
	File f = SPIFFS.open(dfName, "a");
	if (!f) {

		Serial.println("data file open failed");
	}
	else
	{
		Serial.println("====== Writing to data file =========");

		f.print(relayState); f.print(":");
		f.println(moisture);

		Serial.println("Data file updated");
		f.close();
	}

	Serial.print("millis: ");
	Serial.println(millis());

}

//////////////////////////////////////////////////////////////////////////////
// reads the data and formats it so that it can be used by google charts    //
//////////////////////////////////////////////////////////////////////////////

String readDataFile()
{
	String returnStr = "";
	File f = SPIFFS.open(dfName, "r");

	if (!f)
	{
		Serial.println("Data File Open for read failed.");

	}
	else
	{
		Serial.println("----Reading Data File-----");
		dataLines = 0;

		while (f.available()) {

			//Lets read line by line from the file
			dataLines++;
			String str = f.readStringUntil('\n');
			String switchState = str.substring(0, str.indexOf(":"));

			returnStr += ",['" + switchState + "'," + str.substring(str.indexOf(":") + 1) + "]";
		}
		f.close();
	}

	return returnStr;

}

////////////////////////////////////////////////////////////////////////////////////
//      creates the HTML string to be sent to the client                          //
////////////////////////////////////////////////////////////////////////////////////

void setHTML()
{
	webString = "<html><head>\n";
	webString += "<meta http-equiv=\"refresh\" content=\"60;url=http://" + WiFi.localIP().toString() + "\"> \n";

	///////////// google charts script
	webString += "<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script> \n";
	webString += "   <script type=\"text/javascript\"> \n";
	webString += "    google.charts.load('current', {'packages':['corechart','gauge']}); \n";
	webString += "    google.charts.setOnLoadCallback(drawTempChart); \n";
	//webString += "    google.charts.setOnLoadCallback(drawHumidChart); \n";
	webString += "    google.charts.setOnLoadCallback(drawChart); \n ";

	// draw temp guage
	webString += "   function drawTempChart() { \n";
	webString += "      var data = google.visualization.arrayToDataTable([ \n";
	webString += "        ['Label', 'Value'], ";
	webString += "        ['Moisture',  ";
	webString += moisture;
	webString += " ], ";
	webString += "       ]); \n";
	// setup the google chart options here
	webString += "    var options = {";
	webString += "      width: 250, height: 150,";
	webString += "      min: 0, max: 1024,";
	webString += "      greenFrom: 0, greenTo: " + String(moistureOff) + ",";
	webString += "      yellowFrom: " + String(moistureOff) + ", yellowTo: " + String(moistureOn) + ",";
	webString += "      redFrom: " + String(moistureOn) + ", redTo: 1024,";
	webString += "      minorTicks: 5";
	webString += "    }; \n";
	webString += "   var chart = new google.visualization.Gauge(document.getElementById('chart_divTemp')); \n";
	webString += "  chart.draw(data, options); \n";
	webString += "  } \n";


	// draw main graph
	webString += "    function drawChart() { \n";

	webString += "     var data = google.visualization.arrayToDataTable([ \n";
	webString += "       ['Hit', 'Moisture'] ";


	webString += readDataFile();
	// open file for reading

	webString += "     ]); \n";
	webString += "     var options = { \n";
	webString += "        title: 'Moisture Activity', ";
	webString += "        curveType: 'function', \n";

	webString += "  series: { \n";
	webString += "         0: {targetAxisIndex: 0} \n";
	//webString += "         1: {targetAxisIndex: 1} \n";
	webString += "       }, \n";

	webString += "  vAxes: { \n";
	webString += "         // Adds titles to each axis. \n";
	webString += "         0: {title: 'Moisture'} \n";
	//webString += "         1: {title: 'Humidity %'} \n";
	webString += "       }, \n";

	webString += "  hAxes: { \n";
	webString += "         // Adds titles to each axis. \n";
	webString += "         0: {title: 'Moisture On/Off'} \n";
	//webString += "         1: {title: ''} \n";
	webString += "       }, \n";

	webString += "         legend: { position: 'bottom' } \n";
	webString += "       }; \n";

	webString += "       var chart = new google.visualization.LineChart(document.getElementById('curve_chart')); \n";

	webString += "       chart.draw(data, options); \n";
	webString += "      } \n";

	////////////
	webString += "    </script> \n";
	webString += "</head><body>\n";
	webString += "<form action=\"http://" + WiFi.localIP().toString() + "/submit\" method=\"POST\">";
	webString += "<h1>Smart Farm Moisture Control</h1>\n";
	webString += "<div style=\"color:red\">" + webMessage + "</div>\n";

	webString += "<table style=\"width:800px;\"><tr><td>";
	webString += "<div>Moisture On Setting: &ge; " + String((int)moistureOn) + "</div>\n";
	webString += "<div>Moisture Off Setting: &le; " + String((int)moistureOff) + "</div>\n";
	webString += "</td><td style=\"text-align:right\">";
	webString += "Moisture On: &ge; <input type='text' value='" + String((int)moistureOn) + "' name='moistureOn' maxlength='3' size='2'><br>\n";
	webString += "Moisture Off: &le; <input type='text' value='" + String((int)moistureOff) + "' name='moistureOff' maxlength='3' size='2'><br>\n";
	webString += "</td><td style=\"text-align:right\">";
	webString += "Sample Rate (seconds): <input type='text' value='" + String((long)interval / 1000) + "' name='sRate' maxlength='3' size='2'><br>\n";
	webString += "Maximum Chart Data: <input type='text' value='" + String((long)maxFileData) + "' name='maxData' maxlength='3' size='2'><br>\n";

	webString += "</td><td>";
	webString += "<input type='submit' value='Submit' >";
	webString += "</td></tr></table>\n";

	webString += "<table style=\"width:800px;\"><tr><td>";
	webString += "<div style=\"width:300px\"><b>Last Readings</b></div>\n";
	webString += "<div>Moisture: " + String(moisture) + "</div>\n";
	//webString += "<div>Humidity: " + String((int)humidity) + "%</div> \n";

	if (digitalRead(RELAY_CH1) == LOW)
		webString += "<div>Moisture is OFF</div>";
	else
		webString += "<div>Moisture is ON</div>";

	webString += "<div>Data Lines: " + String((int)dataLines) + "</div> \n";
	//  webString += "<div>Total On Time: " + String( ((onCount * interval) / 1000) / 60 ) + " minutes</div> \n";
	webString += "<div>Sample Rate: " + String(interval / 1000) + " seconds</div> \n";

	webString += "<div><a href=\"http://" + WiFi.localIP().toString() + "\">Refresh</a></div>\n";
	webString += "</td><td>";
	webString += "<div id=\"chart_divTemp\" style=\"width: 250px;\"></div>\n";
	webString += "</td><td>";
	//webString += "<div id=\"chart_divHumid\" style=\"width: 250px;\"></div>\n";
	webString += "</td></tr></table>\n";
	webString += "<div id=\"curve_chart\" style=\"width: 1000px; height: 500px\"></div> \n";
	webString += "<div><a href=\"/clear\">Clear Data</a></div> \n";

	webString += "</body></html>\n";
}

//////////////////////////////////
///  used for error checking   ///
//////////////////////////////////

boolean isValidNumber(String str) {
	for (byte i = 0; i < str.length(); i++)
	{
		if (isDigit(str.charAt(i))) return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////
// handler for web server request: http://IpAddress/submit        //
////////////////////////////////////////////////////////////////////

void handle_submit() {

	webMessage = "";
	int tempON = 0;
	int tempOFF = 0;
	long sRate = 0;
	int maxData = 0;

	if (server.args() > 0) {
		for (uint8_t i = 0; i < server.args(); i++) {

			if (server.argName(i) == "moistureOn") {

				if (server.arg(i) != "")
				{
					if (isValidNumber(server.arg(i)))
						tempON = server.arg(i).toInt();
					else
						webMessage += "Moisture On must be a number<br>";
				}
				else
					webMessage += "Moisture On is required<br>";
			}

			if (server.argName(i) == "moistureOff") {

				if (server.arg(i) != "")
				{
					if (isValidNumber(server.arg(i)))
						tempOFF = server.arg(i).toInt();
					else
						webMessage += "Moisture Off must be a number<br>";

				}
				else
					webMessage += "Moisture Off is required<br>";
			}


			if (server.argName(i) == "sRate") {

				if (server.arg(i) != "")
				{
					if (isValidNumber(server.arg(i)))
						sRate = server.arg(i).toInt();
					else
						webMessage += "Sample Rate must be a number<br>";
				}
				else
					webMessage += "Sample Rate is required<br>";
			}

			if (server.argName(i) == "maxData") {

				if (server.arg(i) != "")
				{
					if (isValidNumber(server.arg(i)))
						maxData = server.arg(i).toInt();
					else
						webMessage += "Max Chart Data must be a number<br>";
				}
				else
					webMessage += "Max Chart Data is required<br>";
			}

		}

		if (sRate < 10)
			webMessage += "Sample Rate must be greater than or equal to 10<br>";

		if (maxData < 10 || maxData > 300)
			webMessage += "Max Chart Data must be between 10 and 300<br>";

		if (webMessage == "")
		{
			moistureOn = tempON;
			moistureOff = tempOFF;
			interval = sRate * 1000;
			maxFileData = maxData;
			///////
			File f = SPIFFS.open(fName, "w");
			if (!f) {

				Serial.println("file open for properties failed");
			}
			else
			{
				Serial.println("====== Writing to config file =========");

				f.print(moistureOn); f.print(","); f.print(moistureOff);
				f.print("~"); f.print(sRate);
				f.print(":"); f.println(maxData);
				Serial.println("Properties file updated");
				f.close();
			}

		}
	}


	if (webMessage == "") {
		webMessage = "Settings Updated";
	}

	getmoisture();

	setHTML();
	server.send(200, "text/html", webString);            // send to someones browser when asked

	delay(100);
}


////////////////////////////////////////////////////////////
// handler for web server request: http://IpAddress/      //
////////////////////////////////////////////////////////////

void handle_root() {

	getmoisture();       // read sensor
	webMessage = "";
	setHTML();
	server.send(200, "text/html", webString);            // send to someones browser when asked

	delay(100);
}

///////////////////////////////////////
// first funtion to run at power up //
//////////////////////////////////////
int status = WL_IDLE_STATUS;

void setup(void)
{
	int timeout = 0;
	// You can open the Arduino IDE Serial Monitor window to see what the code is doing
	Serial.begin(115200);  // Serial connection from ESP-01 via 3.3v console cable

	pinMode(RELAY_CH1, OUTPUT);
	digitalWrite(RELAY_CH1, LOW);

	//// check for the presence of the shield
	//if (WiFi.status() == WL_NO_SHIELD) {
	//	Serial.println("WiFi shield not present");
	//	while (true); // don't continue
	//}

	//Serial.print("Attempting to start AP ");
	//Serial.println(ssid);

	//// start access point
	//status = WiFi.softAP(ssid, password);

	//// Connect to WiFi network
	////WiFi.begin(ssid, password);
	////WiFi.config(IPAddress(192, 168, 1, 201), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
	//Serial.print("Access Point \"");
	//Serial.print(ssid);
	//Serial.println("\" started");

	//Serial.print("IP address:\t");
	//Serial.println(WiFi.softAPIP());         // Send the IP address of the ESP8266 to the computer



	//Serial.print("\n\r \n\rWorking to connect");

	//// Wait for connection
	////while (WiFi.status() != WL_CONNECTED) {
	////	delay(500);
	////	Serial.print(".");
	////}
	//Serial.println("");
	//Serial.println("DHT Server");
	//Serial.print("Connected to ");
	//Serial.println(ssid);
	//Serial.print("IP address: ");
	//Serial.println(WiFi.localIP());

	// Connect to WiFi network
	WiFi.begin(ssid, password);

	Serial.print("\n\r \n\rWorking to connect");

	// Wait for connection
	timeout = 0;
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
		if (timeout++ > 60) break;
	}
	if (timeout > 60) {
		Serial.println("\n\rWiFi connection fail.");
	}

	SPIFFS.begin();
	delay(10);
	///////////////////
	// SPIFFS.format(); // uncomment to completely clear data


	File f = SPIFFS.open(fName, "r");

	if (!f) {
		// no file exists so lets format and create a properties file
		Serial.println("Please wait 30 secs for SPIFFS to be formatted");

		SPIFFS.format();

		Serial.println("Spiffs formatted");

		f = SPIFFS.open(fName, "w");
		if (!f) {

			Serial.println("properties file open failed");
		}
		else
		{
			// write the defaults to the properties file
			Serial.println("====== Writing to properties file =========");

			f.print(moistureOn); f.print(","); f.print(moistureOff);
			f.print("~"); f.print(interval / 1000);
			f.print(":"); f.println(maxFileData);
			Serial.println("Properties file created");
			dataLines = 1;
			f.close();
		}

	}
	else
	{
		// if the properties file exists on startup,  read it and set the defaults
		Serial.println("Properties file exists. Reading.");

		while (f.available()) {

			//Lets read line by line from the file
			String str = f.readStringUntil('\n');

			String loSet = str.substring(0, str.indexOf(","));
			String hiSet = str.substring(str.indexOf(",") + 1, str.indexOf("~"));
			String sampleRate = str.substring(str.indexOf("~") + 1, str.indexOf(":"));
			String maxData = str.substring(str.indexOf(":") + 1);

			Serial.println(loSet);
			Serial.println(hiSet);
			Serial.println(sampleRate);
			Serial.println(maxData);

			moistureOn = loSet.toInt();
			moistureOff = hiSet.toInt();
			interval = sampleRate.toInt() * 1000;
			maxFileData = maxData.toInt();
		}

		f.close();
	}

	// now lets read the data file mainly to set the number of lines
	readDataFile();
	// now read the DHT and set the temp and humidity variables
	getmoisture();
	// update the datafile to start a new session
	updateDataFile();

	// web client handlers
	server.on("/", handle_root);

	server.on("/submit", handle_submit);

	server.on("/clear", []() {
		// handler for http://iPaddress/clear

		// deletes all the stored data for temp and humidity
		clearDataFile();

		webMessage = "Data Cleared";

		// read the DHT and use new values to start new file data
		getmoisture();
		updateDataFile();
		setHTML(); // this will set the webString varialbe with HTML
		server.send(200, "text/html", webString);            // send to someones browser when asked
		delay(100);

	});

	// start the web server
	server.begin();
	Serial.println("HTTP server started");
	// you're connected now, so print out the status:
	printWifiStatus();
}

void printWifiStatus() {
	// print the SSID of the network you're attached to:
	Serial.print("SSID: ");
	Serial.println(WiFi.SSID());

	// print your WiFi shield's IP address:
	IPAddress ip = WiFi.localIP();
	Serial.print("IP Address: ");
	Serial.println(ip);

	// print the received signal strength:
	long rssi = WiFi.RSSI();
	Serial.print("signal strength (RSSI):");
	Serial.print(rssi);
	Serial.println(" dBm");
}

////////////////////////////////////////////////////////////////////////////////

void loop(void)
{

	// check timer to see if it's time to update the data file with another DHT reading
	unsigned long currentMillis = millis();

	// cast to unsigned long to handle rollover
	if ((unsigned long)(currentMillis - previousMillis) >= interval)
	{
		// save the last time you read the sensor
		previousMillis = currentMillis;

		getmoisture();

		Serial.print("Moisture: ");
		Serial.println(moisture);

		updateDataFile();
		readDataFile();
	}

	if (WiFi.status() != WL_CONNECTED) {
			//WiFi Disconnect
			WiFi.begin(ssid, password);
			delay(5000);
			Serial.println("WiFi ReConnect");
	}
	else {
		server.handleClient();
	}

}

