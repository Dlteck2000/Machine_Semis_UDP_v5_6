// Controleur de semis pour AgOpenGPS - Seed monitor for AgOpenGPS
// Author: Damien Loisel
// Copyright: GPL V2

#include "EtherCard_AOG.h"
#include <IPAddress.h>

// ========== PARAMETRES UTILSATEUR - USER SETTINGS ==========

//Broche CS Arduino Nano pour carte Ethernet ENC28J60 - Arduino Nano CS Pin for ENC28J60 Ethernet Board
#define CS_Pin 10

//Nombre de capteurs (16 MAX) - Number of sensors (16 MAX)
#define NB_Capteur 3

//Broches d'entrée des capteurs - Sensor input pins
const uint8_t capteurPinArray[] = { 2, 3, 4};


// ========== CONFIGURATION CARTE RESEAU - NETWORK CARD CONFIGURATION ==========

static uint8_t myip[] = { 192, 168, 1, 123 };// Adresse IP de la carte machine - Machine board IP address
static uint8_t gwip[] = { 192, 168, 1, 1 };// Adresse de la passerelle - Gateway address
static uint8_t myDNS[] = { 8, 8, 8, 8 };// DNS
static uint8_t mask[] = { 255, 255, 255, 0 };// Masque de sous réseau - subnet mask
uint16_t portMy = 5123;// Port de la carte machine - Machine board port

//Adresse Ip et port d'AOG - Ip address and port of AOG
static uint8_t ipDestination[] = {192, 168, 1, 255};
uint16_t portDestination = 9999;

// Adresse mac de la carte, doit être unique sur votre réseau - Mac address of the card, must be unique on your network
static uint8_t mymac[] = { 0x00, 0x00, 0x56, 0x00, 0x00, 0x7B };

uint8_t Ethernet::buffer[200]; // Tampon d'envoi et de réception udp - Udp send and receive buffer

//========== CONFIGURATION de L'INO - INO CONFIGURATION ==========

// Gestion du temps en millisecondes - Time management in milliseconds
const uint8_t LOOP_TIME = 200; // 5hz
uint32_t lastTime = LOOP_TIME;
uint32_t currentTime = LOOP_TIME;

// Gestion du temps de communication entre AOG et la carte machine
uint8_t watchdogTimer = 20; // Max 20*200ms = 4s (LOOP TIME) si plus de 20 déclaration de perte de communication avec le module
uint8_t serialResetTimer = 0; // Vidange de la mémoire tampon


// Parsing PGN
uint8_t PGN_234[] = { 128, 129, 123, 234, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0xCC };
int8_t PGN_234_Size = sizeof(PGN_234) - 1;

// hello from AgIO
uint8_t helloFromMachine[] = {128, 129, 123, 123, 2, 1, 1, 71};

uint8_t capteurOn_Grp_0 = 0, capteurOn_Grp_1 = 0, capteurOff_Grp_0 = 0, capteurOff_Grp_1 = 0;

//Program counter reset
void(* resetFunc) (void) = 0;

void setup() {
  Serial.begin(38400);  //set up communication
  
  for (int8_t i=0; i < NB_Capteur; i++){
	  pinMode(capteurPinArray[i], INPUT_PULLUP);
  }

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
	
	// Collecte de l'état les capteurs
	
	for (int8_t i = 0; i < NB_Capteur && i < 8; i++ ){
		if (!digitalRead(capteurPinArray[i])){
			bitSet(capteurOn_Grp_0, i);
			bitClear(capteurOff_Grp_0, i);
		}
		else {
			bitSet(capteurOff_Grp_0, i);
			bitClear(capteurOn_Grp_0, i);
		}
	}
	
	for (int8_t i = 0; i < NB_Capteur && i < 8; i++ ){
		if (digitalRead(capteurPinArray[i] + 8)){
			bitSet(capteurOn_Grp_1, i + 8);
			bitClear(capteurOff_Grp_1, i+8);
		}
		else {
			bitSet(capteurOff_Grp_1, i+8);
			bitClear(capteurOn_Grp_1, i+8);
		}
	}
	
    PGN_234[9] = capteurOn_Grp_0;
    PGN_234[10] = capteurOff_Grp_0;
    PGN_234[11] = capteurOn_Grp_1;
    PGN_234[12] = capteurOff_Grp_1;
    
    //checksum
    int16_t CK_A = 0;
    for (uint8_t i = 2; i < PGN_234_Size; i++)
    {
      CK_A = (CK_A + PGN_234[i]);
    }
    PGN_234[PGN_234_Size] = CK_A;

    ether.sendUdp(PGN_234, sizeof(PGN_234), portMy, ipDestination, portDestination);
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
