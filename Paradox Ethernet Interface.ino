/*
   Ser2Net Server Sketch Created by Dimitris Mylonakis, jim.milonakis@gmail.com
   This sketch transports Serial RS232 Interface to Telnet Session
   Supports up to 3 concurrent Telent Sessions (port 23) while
   One more Telnet Session (port 24) enabled for Command Line Interface.
   Last but not least one more session can be held by USBtoSerial Integrated onBoard Adapter.

*/

// to store settings in memory
#include <EEPROM.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>



//disable debug by commenting the follow define Debug Line
#define DEBUG
// Port Command Telnet Server
#define CMD_PORT 23
// Port Control Telnet Server
#define CONTROL_PORT 24
// description buffer size
#define LBLSIZE 64
#define SCREEN_WIDTH 128
//#define SCREEN_HEIGHT 32 // remove in case of 128X32 OLED Display
#define SCREEN_HEIGHT 64 // remove in case of 128X64 OLED Display



Adafruit_SSD1306
display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
// replace this with your mac address
byte mac[6] = { 0x90, 0xA2, 0x00, 0x00, 0x00, 0x00 };
char macstr[18];
char buffIP[32]; //buffer for IP address
char buffSUB[32]; //buffer for Subnet
char buffGW[32]; //buffer for Gateway
unsigned long startMillis;
unsigned long currentMillis;
const unsigned long period = 500;  //start Serial after 1000 milliseconds
unsigned long timeOfLastActivity; //time in milliseconds of last activity
unsigned long allowedConnectTime = 300000; // 300 seconds - 5 minutes

IPAddress ipS(EEPROM.read(50), EEPROM.read(51), EEPROM.read(52), EEPROM.read(53));
IPAddress subnetS(EEPROM.read(54), EEPROM.read(55), EEPROM.read(56), EEPROM.read(57));
IPAddress gatewayS(EEPROM.read(58), EEPROM.read(59), EEPROM.read(60), EEPROM.read(61));

// structure to store settings of the serial port
struct ComSettings {
  char hostname[LBLSIZE];
  long baudrate;
  char parity;
  long wordlength;
  long stopbits;
};
ComSettings settings;

// default serial port configuration
ComSettings defaults = {"Undefined", 9600, 'N', 8, 1};

long serialSettings(struct ComSettings s) {
  // this function returns serial configuration for Serial library
  long conf = 0;
  long wl = 3;
  if (s.wordlength >= 5 && s.wordlength <= 8) {
    wl = s.wordlength - 5;
  }
  long stp = 0;
  if (s.stopbits == 1 || s.stopbits == 2) {
    stp = s.stopbits - 1;
  }
  long p = 0;
  if (s.parity == 'E') {
    p = 2;
  }
  if (s.parity == 'O') {
    p = 3;
  }
  conf = (p << 4) | (stp << 3) | (wl << 1);
  return conf;
}

bool alreadyConnected = false;
bool closesession = false;
bool serport = EEPROM.read(30);
bool dhcp = EEPROM.read(40);
String inputString = "";         // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete
bool initserial = false;  // whether the serial is init

EthernetServer cmdServer(CMD_PORT);
EthernetServer controlServer(CONTROL_PORT);
EthernetClient clients[8];
String cmd = "";
String ctrl = "";

void(* resetFunc) (void) = 0;

void parseCmd(String s, EthernetClient client) {
  bool changed = false;
  bool knowncommand = false;

  if (s == "help") {
    help (client);
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }

  if (s == "?") {
    help (client);
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }


  if (s == "save") {
    EEPROM.put(10, settings);
    EEPROM.write(30, serport);
    EEPROM.write(40, dhcp);
    client.println("Saved!");
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }

  if (s == "load") {
    EEPROM.get(10, settings);
    serport = EEPROM.read(30);
    dhcp = EEPROM.read(40);
    client.println("Loaded!");
    client.print(settings.hostname); client.print(">");
    changed = true;
    knowncommand = true;
    timeOfLastActivity = millis();
  }

  if (s == "show conf") {
    client.println("");
    client.print("Hostname:        ");
    client.println(settings.hostname);
    client.print("BaudRate:        ");
    client.println(settings.baudrate);
    client.print("Parity:          ");
    client.println(settings.parity);
    client.print("Wordlength:      ");
    client.println(settings.wordlength);
    client.print("Stopbits:        ");
    client.println(settings.stopbits);
    client.print("USB Port:        ");
    if (serport)
      client.println("Enabled");
    else
      client.println("Disabled");
    client.println("");
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }

  if (s == "show info") {
    client.println("");
    client.println (" ********** Network Status **********");
    client.print("MAC Address:    ");
    client.print(mac[0], HEX);
    client.print("-");
    client.print(mac[1], HEX);
    client.print("-");
    client.print(mac[2], HEX);
    client.print("-");
    client.print(mac[3], HEX);
    client.print("-");
    client.print(mac[4], HEX);
    client.print("-");
    client.println(mac[5], HEX);
    client.print("IP Address:     ");
    client.println(Ethernet.localIP());
    client.print("NetMask:        ");
    client.println(Ethernet.subnetMask());
    client.print("IP Gateway:     ");
    client.println(Ethernet.gatewayIP());
    client.println("");
    client.println (" ******* Network Configuration *******");
    client.print("DHCP Status:    ");
    if (dhcp)
      client.println("Disabled");
    else
      client.println("Enabled");
    client.println("");
    sprintf(buffIP, "%d.%d.%d.%d", EEPROM.read(50), EEPROM.read(51), EEPROM.read(52), EEPROM.read(53));
    client.print("IP Address (static):");
    client.println (buffIP);
    sprintf(buffSUB, "%d.%d.%d.%d", EEPROM.read(54), EEPROM.read(55), EEPROM.read(56), EEPROM.read(57));
    client.print("NetMask (static):   ");
    client.println (buffSUB);
    sprintf(buffGW, "%d.%d.%d.%d", EEPROM.read(58), EEPROM.read(59), EEPROM.read(60), EEPROM.read(61));
    client.print("Gateway (static)    ");
    client.println (buffGW);
    client.println("");
    client.println (" ********** Session Status **********");
    client.print("Session Timeout:    ");
    client.println("300 Seconds");
    client.print("Active Session:     ");
    client.print((millis() - timeOfLastActivity)/1000);
    client.println(" Seconds");
    client.println("");
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }

  if (s == "USB enable") {
    serport = true ;
    Serial.begin(settings.baudrate, serialSettings(settings));
    client.println("USB Port Enabled !");
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }

  if (s == "USB disable") {
    serport = false;
    Serial.end();
    client.println("USB Port Disabled !");
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }

  if (s == "dhcp enable") {
    dhcp = false ;
    client.println("DHCP Enabled !");
    client.println ("Please Save for configuration be Effective on next reboot. ");
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }

  if (s == "dhcp disable") {
    dhcp = true;
    client.println("DHCP Disabled !");
    client.println ("Please Save for configuration be Effective on next reboot. ");
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }

  if (s == "show debug") {
    client.print("Debug Info InputString: ");
    client.println(inputString);
    client.print("StringComplete: ");
    client.println(stringComplete);
    client.print("InitSerial: ");
    client.println(initserial);
    client.print("Debug Info serial1 read: ");
    client.println(Serial1.read());
    client.print("Debug Info serial1 available: ");
    client.println(Serial1.available());
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }



  if (s == "exit") {
    client.println("\nBye.\n");
    client.stop();
    knowncommand = true;
  }

  if (s == "reboot") {
    client.println("\n Ser2Net Server Reboots ...\n");
    client.stop();
    closesession = true;
    knowncommand = true;
  }

  if (s == "show ver") {
    client.println("");
    client.println("Paradox Ethernet Interface Version: 1.51 August 30, 2022, designed by Dimitris Mylonakis, jim.milonakis@gmail.com ");
    client.println("");
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }

  int l = s.length();
  if (s.startsWith("hostname")) {
    if (l > 6) {
      s.substring(9).toCharArray(settings.hostname, LBLSIZE);
      changed = true;
    }
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }
  if (s.startsWith("baudrate")) {
    if (l > 9) {
      settings.baudrate = s.substring(9).toInt();
      changed = true;
    }
    client.print ("BaudRate : ");
    client.println(settings.baudrate);
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }
  if (s.startsWith("parity")) {
    if (l > 7) {
      settings.parity = s.charAt(7);
      changed = true;
    }
    client.print ("Parity : ");
    client.println(settings.parity);
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }
  if (s.startsWith("wordlength")) {
    if (l > 11) {
      settings.wordlength = s.substring(11).toInt();
      changed = true;
    }
    client.print ("WordLength : ");
    client.println(settings.wordlength);
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }
  if (s.startsWith("stopbits")) {
    if (l > 9) {
      settings.stopbits = s.substring(9).toInt();
      changed = true;
    }
    client.print("StopBits: ");
    client.println(settings.stopbits);
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }

  if (s.startsWith("ip address")) {
    int delimiter, delimiter_1, delimiter_2, delimiter_3, delimiter_4, delimiter_5;
    delimiter = s.indexOf(" ");
    delimiter_1 = s.indexOf(" ", delimiter + 1);
    delimiter_2 = s.indexOf(".", delimiter_1 + 1);
    delimiter_3 = s.indexOf(".", delimiter_2 + 1);
    delimiter_4 = s.indexOf(".", delimiter_3 + 1);
    delimiter_5 = s.indexOf(".", delimiter_4 + 1);
    String first = s.substring(delimiter + 1, delimiter_1);
    String ip1 = s.substring(delimiter_1 + 1, delimiter_2);
    String ip2 = s.substring(delimiter_2 + 1, delimiter_3);
    String ip3 = s.substring(delimiter_3 + 1, delimiter_4);
    String ip4 = s.substring(delimiter_4 + 1, delimiter_5);
    byte byte1 = ip1.toInt();
    byte byte2 = ip2.toInt();
    byte byte3 = ip3.toInt();
    byte byte4 = ip4.toInt();
    EEPROM.write(50, byte1 );
    EEPROM.write(51, byte2 );
    EEPROM.write(52, byte3 );
    EEPROM.write(53, byte4 );
    client.println("");
    client.print("IP Address: ");
    client.print (ip1 );
    client.print (".");
    client.print (ip2);
    client.print (".");
    client.print (ip3);
    client.print (".");
    client.println (ip4);
    client.println ("Saved... Configuration will be Effective on next reboot ");
    client.println ("");
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }


  if (s.startsWith("netmask")) {
    int delimiter, delimiter_1, delimiter_2, delimiter_3, delimiter_4;
    delimiter = s.indexOf(" ");
    delimiter_1 = s.indexOf(".", delimiter + 1);
    delimiter_2 = s.indexOf(".", delimiter_1 + 1);
    delimiter_3 = s.indexOf(".", delimiter_2 + 1);
    delimiter_4 = s.indexOf(".", delimiter_3 + 1);
    String ip1 = s.substring(delimiter + 1, delimiter_1);
    String ip2 = s.substring(delimiter_1 + 1, delimiter_2);
    String ip3 = s.substring(delimiter_2 + 1, delimiter_3);
    String ip4 = s.substring(delimiter_3 + 1, delimiter_4);
    byte byte1 = ip1.toInt();
    byte byte2 = ip2.toInt();
    byte byte3 = ip3.toInt();
    byte byte4 = ip4.toInt();
    EEPROM.write(54, byte1 );
    EEPROM.write(55, byte2 );
    EEPROM.write(56, byte3 );
    EEPROM.write(57, byte4 );
    client.println("");
    client.print("NetMAsk: ");
    client.print (ip1 );
    client.print (".");
    client.print (ip2);
    client.print (".");
    client.print (ip3);
    client.print (".");
    client.println (ip4);
    client.println ("Saved... Configuration will be Effective on next reboot ");
    client.println ("");
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }

  if (s.startsWith("gateway")) {
    int delimiter, delimiter_1, delimiter_2, delimiter_3, delimiter_4;
    delimiter = s.indexOf(" ");
    delimiter_1 = s.indexOf(".", delimiter + 1);
    delimiter_2 = s.indexOf(".", delimiter_1 + 1);
    delimiter_3 = s.indexOf(".", delimiter_2 + 1);
    delimiter_4 = s.indexOf(".", delimiter_3 + 1);
    String ip1 = s.substring(delimiter + 1, delimiter_1);
    String ip2 = s.substring(delimiter_1 + 1, delimiter_2);
    String ip3 = s.substring(delimiter_2 + 1, delimiter_3);
    String ip4 = s.substring(delimiter_3 + 1, delimiter_4);
    byte byte1 = ip1.toInt();
    byte byte2 = ip2.toInt();
    byte byte3 = ip3.toInt();
    byte byte4 = ip4.toInt();
    EEPROM.write(58, byte1 );
    EEPROM.write(59, byte2 );
    EEPROM.write(60, byte3 );
    EEPROM.write(61, byte4 );
    client.println("");
    client.print("Gateway: ");
    client.print (ip1 );
    client.print (".");
    client.print (ip2);
    client.print (".");
    client.print (ip3);
    client.print (".");
    client.println (ip4);
    client.println ("Saved... Configuration will be Effective on next reboot ");
    client.println ("");
    client.print(settings.hostname); client.print(">");
    knowncommand = true;
    timeOfLastActivity = millis();
  }
  if (s == "") {
    client.print(settings.hostname); client.print(">");
    timeOfLastActivity = millis();
  }
  else {
    if (!knowncommand) {
      client.println("\n Incomplete , Error or Unknown Command ");
      client.println(" Press 'help or ?'  for available commands \n");
      client.print(settings.hostname); client.print(">");
      timeOfLastActivity = millis();
    }
  }

  if (changed) {
    reopenSerial();
  }
}



void reopenSerial() {
  Serial1.begin(settings.baudrate, serialSettings(settings));
  controlServer.println("Settings changed:");
  controlServer.print(settings.hostname);
  controlServer.print(",");
  controlServer.print(settings.baudrate);
  controlServer.print(",");
  controlServer.print(settings.parity);
  controlServer.print(",");
  controlServer.print(settings.wordlength);
  controlServer.print(",");
  controlServer.println(settings.stopbits);
  if (serport)
    Serial.begin(settings.baudrate, serialSettings(settings));
  else
    Serial.end();
  printConfig ();
  displaySettings ();
}


void help(EthernetClient client) {
  client.println("                        Telnet to Serial Command Line Interface");
  client.println("");
  client.println("**************************************************************************************");
  client.println(" show conf     shows Ser2Net Server Current Settings");
  client.println(" show info     shows Device MAC Address , IP Addres, NetMASK & IP Gateway  Settings");
  client.println(" show ver      Shows Ser2Net Server Firmware Version");
  client.println(" show debug    Shows debug Info regarding Ser2Net Severity level");
  client.println(" hostname      Ser2Net Server Hostname (up to 32 characters), e.g. 'hostname ser2net'");
  client.println(" baudrate      Set baudrate from 300 to 115200 ,e.g. 'baurate 9600'");
  client.println(" parity        Set parity (N, E, O), e.g 'parity N' ");
  client.println(" wordlength    Set Wordlength (5, 6, 7, 8), e.g. 'wordlength 8' ");
  client.println(" stopbits      Set stopbits (1, 2), e,g, 'stopbit 1' ");
  client.println(" dhcp          Enable or Disable DHCP (Applicable upon Reboot), e,g, 'dhcp enable' ");
  client.println(" ip address    Set IP Address (Applicable upon Reboot), e.g. 'ip address 192.168.1.101' ");
  client.println(" netmask       Set Subnet Mask (Applicable upon Reboot), e.g. 'netmask 255.255.255.0' ");
  client.println(" gateway       Set Gateway (Applicable upon Reboot), e.g. 'gateway 192.168.1.1' ");
  client.println(" USB           Enable or Disble USB Port as Serial Adapter, e.g. USB enable' ");
  client.println(" save          saves current settings to EEPROM memory");
  client.println(" load          loads settings from EEPROM memory");
  client.println(" help          Shows Ser2Net CLI command set , similar to ? ");
  client.println(" reboot        Reboots Ser2Net Server ");
  client.println(" exit          Closes this Control telnet Session (port:24)");
  client.println("**************************************************************************************");
  client.println("");

}


void checkControl() {
  // check for any new client connecting, and say hello (before any incoming data)
  EthernetClient newClient = controlServer.accept();

  if (newClient) {
    for (byte i = 0; i < 8; i++) {
      if (!clients[i]) {
        newClient.println("**************************************************");
        newClient.println("***                                            ***");
        newClient.println("***                  dMSystems                 ***");
        newClient.println("***          Paradox Ethetnet Interface        ***");
        newClient.println("***                                            ***");
        newClient.println("***          This session is monitored         ***");
        newClient.println("***        If you do not like this policy      ***");
        newClient.println("***            please disconnect now           ***");
        newClient.println("***     Contact: contact@dmsystems.gr          ***");
        newClient.println("***                                            ***");
        newClient.println("***                                            ***");
        newClient.println("***             |#|                |#|         ***");
        newClient.println("***           |#| |#|            |#| |#|       ***");
        newClient.println("***         |#|     |#|        |#|     |#|     ***");
        newClient.println("***       |# Dimitris  #|    |# Mylonakis #|   ***");
        newClient.println("***         |#|     |#|        |#|     |#|     ***");
        newClient.println("***           |#| |#|            |#| |#|       ***");
        newClient.println("***             |#|                |#|         ***");
        newClient.println("***                                            ***");
        newClient.println("***            Telnet to Serial Server         ***");
        newClient.println("***                                            ***");
        newClient.println("***   Press  'help' or '?' to list commands    ***");
        newClient.println("*** or Press 'show conf' for current settings  ***");
        newClient.println("***                                            ***");
        newClient.println("**************************************************");
        newClient.println(" ");
        newClient.print("Local Server Port: ");
        newClient.println(newClient.localPort());
        newClient.print("Remote Host  Port: ");
        newClient.println(newClient.remotePort());
        newClient.print("Remote Host IP address: ");
        newClient.println(newClient.remoteIP());
        newClient.println(" ");
        newClient.print(settings.hostname); newClient.print(">");
        timeOfLastActivity = millis();
        // Once we "accept", the client is no longer tracked by EthernetServer
        // so we must store it into our list of clients
        clients[i] = newClient;
        break;
      }
    }
  }

  // check for incoming data from all clients
  for (byte i = 0; i < 8; i++) {
    while (clients[i] && clients[i].available() > 0) {
      // read incoming data from the client
      char c = clients[i].read();
      if (c == '\n') {
        parseCmd(cmd, clients[i]);
        cmd = "";
      } else {
        if (c != '\r') { // ignoring \r
          cmd += c;
        }
      }

    }
  }

  // stop any clients which disconnect
  for (byte i = 0; i < 8; i++) {
    if (clients[i] && !clients[i].connected()) {
      clients[i].stop();
    }
    if (millis() - timeOfLastActivity > allowedConnectTime) {
      clients[i].println();
      clients[i].println();
      clients[i].println("    Session Timeout disconnect.");
      clients[i].println("Please Reconnect Whenever is Needed.");
      clients[i].println("             Bye Bye.");
      clients[i].println("");
      clients[i].stop();
    }
  }
}


void printConfig() {
  Serial.println(" ******** Device Info ******** ");
  Serial.print("MAC Address: ");
  Serial.print(mac[0], HEX);
  Serial.print("-");
  Serial.print(mac[1], HEX);
  Serial.print("-");
  Serial.print(mac[2], HEX);
  Serial.print("-");
  Serial.print(mac[3], HEX);
  Serial.print("-");
  Serial.print(mac[4], HEX);
  Serial.print("-");
  Serial.println(mac[5], HEX);
  Serial.print("IP-address: ");
  Serial.println(Ethernet.localIP());
  Serial.print("NetMask: ");
  Serial.println(Ethernet.subnetMask());
  Serial.print("IP Gateway: ");
  Serial.println(Ethernet.gatewayIP());
  Serial.print ("DHCP status: ");
  if (dhcp)
    Serial.println("Disabled");
  else
    Serial.println("Enabled");
  Serial.println("");
  Serial.println(" ******** Server Settings ******** ");
  if (!Serial1) {
    Serial.println("Serial port is closed");
  } else {
    Serial.println("Serial port is opened");
  }
  Serial.println("Serial configuration:");
  Serial.print("Hostname: ");
  Serial.println(settings.hostname);
  Serial.print("Baudrate: ");
  Serial.println(settings.baudrate);
  Serial.print("Parity: ");
  Serial.println(settings.parity);
  Serial.print("Wordlength: ");
  Serial.println(settings.wordlength);
  Serial.print("Stopbits: ");
  Serial.println(settings.stopbits);
  Serial.print("USB Port: ");
  if (serport)
    Serial.println("Enabled");
  else
    Serial.println("Disabled");
}
void displaySettings() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("  Paradox Interface ");
  display.println(""); // remove in case of 128X32 OLED Display
  display.println(" Dimitris Mylonakis ");
  display.println(""); // remove in case of 128X32 OLED Display
  display.print(" IP: ");
  display.println(Ethernet.localIP());
  display.println(""); // remove in case of 128X32 OLED Display
  display.print(" Serial: ");
  display.print(settings.baudrate);
  display.print(",");
  display.print(settings.wordlength);
  display.print(",");
  display.print(settings.parity);
  display.print(",");
  display.print(settings.stopbits);
  display.display();
}


void checkSerial1() {
  while (Serial1.available()) {
    char inChar = (char)Serial1.read();
    // add it to the inputString:
    inputString += inChar;
    stringComplete = true; // after init enable this parameter
  }
}

void setup() {
  // Ranomize MAC Address in case that is not HW set
  if (EEPROM.read(1) == '#') {
    for (int i = 2; i < 6; i++) {
      mac[i] = EEPROM.read(i);
    }
  } else {
    randomSeed(analogRead(0));
    for (int i = 2; i < 6; i++) {
      mac[i] = random(0, 255);
      EEPROM.write(i, mac[i]);
    }
    EEPROM.write(1, '#');
  }
  snprintf(macstr, 18, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    //Serial.println(F("SSD1306 Allocation Failed"));
    for (;;);
    resetFunc();
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(" Starting");

  //DHCP Enabled by only Used MAC ADDRESS
  if (dhcp)
    Ethernet.begin(mac, ipS, gatewayS, gatewayS, subnetS);
  else
    Ethernet.begin(mac);

  EEPROM.get(10, settings);
  // a kind of baudrate check
  if (settings.baudrate < 300) {
    EEPROM.put(10, defaults);
    EEPROM.get(10, settings);
  }

  cmdServer.begin();
  controlServer.begin();
  Serial1.begin(settings.baudrate, serialSettings(settings));
  if (serport)
    Serial.begin(settings.baudrate, serialSettings(settings));
  else
    Serial.end();
  inputString.reserve(200);


  displaySettings ();
#ifdef DEBUG
  printConfig ();
#endif

}

void loop() {
  checkSerial1();
  // wait for a new Telnet 2 Serial client:
  EthernetClient client = cmdServer.available();
  // Function to enable Control Server
  checkControl();

  // transfer all bytes from client to Serial1
  char d;
  if (client) {
    if (!alreadyConnected) {
      startMillis = millis();
      // clean out the input buffer:
      client.flush();
      alreadyConnected = true;
    }
    currentMillis = millis();
    if (currentMillis - startMillis >= period)
      initserial = true ;
    if (client.available() > 0) {
      if (initserial) {
        d = client.read();
        Serial1.write(d);
      }
      else  d = client.read();  //clean out Telnet Protocol Negotiation.

      //If you want the serial to send ASCII, use Serial.print / Serial.println instead of Serial.write ...
      //enable only for debug purposes as end Serial Device performs echo back
      //#ifdef DEBUG
      //      Serial.write(d);
      //#endif
    }
  }
  // transfer all bytes to the cmdServer under serialEvent1


  if (stringComplete) {
    cmdServer.print(inputString);
#ifdef DEBUG
    Serial.print(inputString);
#endif
    // clear the string:
    inputString = "";
    stringComplete = false;
  }
  if (closesession) {
    client.println("\n Ser2Net Server Reboots ...\n");
    client.stop();
    resetFunc();
  }

  // read from port 0, send to port 1:
#ifdef DEBUG
  if (Serial.available() > 0) {
    int inByte = Serial.read();
    Serial1.write(inByte);
  }
#endif

}