#include "EtherCard_AOG.h"
#include <IPAddress.h>

//Program counter reset
void(* resetFunc) (void) = 0;

// ========== DEFINITION DES BROCHES ARDUINO ==========

#define CS_Pin 10 //Broche CS Arduino Nano pour carte Ethernet ENC28J60
#define CAPTEUR_1_PIN 2
#define CAPTEUR_2_PIN 3
#define CAPTEUR_3_PIN 4

// ========== CONFIG CARTE RESEAU ==========

static uint8_t myip[] = { 192, 168, 1, 123 };// Adresse IP de la carte machine
static uint8_t gwip[] = { 192, 168, 1, 1 };// Adresse de la passerelle
static uint8_t myDNS[] = { 8, 8, 8, 8 };// DNS
static uint8_t mask[] = { 255, 255, 255, 0 };// Masque de sous réseau
uint16_t portMy = 5123;// Port de la carte machine

//Adresse Ip et port d'AOG
static uint8_t ipDestination[] = {192, 168, 1, 255};
uint16_t portDestination = 9999;

// ethernet mac address - must be unique on your network
static uint8_t mymac[] = { 0x00, 0x00, 0x56, 0x00, 0x00, 0x7B };

uint8_t Ethernet::buffer[200]; // udp send and receive buffer

//========== CONFIG de L'INO ==========

// Gestion du temps en millisecondes
const uint8_t LOOP_TIME = 200; // 5hz
uint32_t lastTime = LOOP_TIME;
uint32_t currentTime = LOOP_TIME;
//uint32_t fifthTime = 0;
uint16_t count = 0;

// Gestion du temps de communication entre AOG et la carte machine
uint8_t watchdogTimer = 20; // Max 20*200ms = 4s (LOOP TIME) si plus de 20 déclaration de perte de communication avec le module
uint8_t serialResetTimer = 0; // Vidange de la mémoire tampon


// Parsing PGN
uint8_t PGN[] = { 128, 129, 123, 234, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0xCC };
int8_t PGN_Size = sizeof(PGN) - 1;

// hello from AgIO
uint8_t helloFromMachine[] = {128, 129, 123, 123, 2, 1, 1, 71};

void setup() {
  Serial.begin(38400);  //set up communication

  if (ether.begin(sizeof Ethernet::buffer, mymac, CS_Pin) == 0)
    Serial.println(F("Failed to access Ethernet controller"));

  //set up connection
  ether.staticSetup(myip, gwip, myDNS, mask);
  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);
  ether.printIp("DNS: ", ether.dnsip);

  //register to port 8888
  ether.udpServerListenOnPort(&udpSteerRecv, 8888);

  Serial.println("Setup complete, waiting for AgOpenGPS");
}

void loop() {
  currentTime = millis(); // Temps maintenant

  if (currentTime - lastTime >= LOOP_TIME)
  {
    lastTime = currentTime;
    //If connection lost to AgOpenGPS, the watchdog will count up
    if (watchdogTimer++ > 250) watchdogTimer = 20;
    //clean out serial buffer to prevent buffer overflow
    if (serialResetTimer++ > 20)
    {
      while (Serial.available() > 0) Serial.read();
      serialResetTimer = 0;
    }

    if (watchdogTimer > 20)
    {
      //section = 0;
    }
    //section = 7;
    PGN[9]=7;
    PGN[10] = 248;
    PGN[11] = 255;
    PGN[12]=0;
    
    //checksum
    int16_t CK_A = 0;
    for (uint8_t i = 2; i < PGN_Size; i++)
    {
      CK_A = (CK_A + PGN[i]);
    }
    PGN[PGN_Size] = CK_A;

    //off to AOG
    ether.sendUdp(PGN, sizeof(PGN), portMy, ipDestination, portDestination);
  }
  delay(1);

  //Doit être appelé pour que les fonctions de la carte Ethernet fonctionnent. Appelle udpSteerRecv() défini ci-dessous.
  ether.packetLoop(ether.packetReceive());
}

void udpSteerRecv(uint16_t dest_port, uint8_t src_ip[IP_LEN], uint16_t src_port, uint8_t* udpData, uint16_t len)
{
  if (udpData[0] == 0x80 && udpData[1] == 0x81 && udpData[2] == 0x7F) //Data
  {
    //========== FAIS COUCOU A AOG POUR DIRE QUE LA CARTE EST LA ET PASSER L'ICONE AU VERT
    if (udpData[3] == 200) // Hello from AgIO
    {
      if (udpData[7] == 1)
      {
        //relay = relay - 255;
        watchdogTimer = 0;
      }

      //byte section = 0;
      //helloFromMachine[5] = section;
      //helloFromMachine[6] = relay;

      ether.sendUdp(helloFromMachine, sizeof(helloFromMachine), portMy, ipDestination, portDestination);
    }
  }
}
