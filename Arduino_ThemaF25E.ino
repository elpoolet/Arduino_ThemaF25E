
/*
 Thema F25E
 
 Gestion des mises en route d'une chaudière Saunier Duval Thema F25E
 
 La mise en route se fait par commutation d'un relais
 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * LM35 sur la pin A3
 * relay sur Digital pin : 2
 
 
  0.fr.pool.ntp.org
  -- 62.210.207.122
  -- 92.243.6.5
  -- 188.138.75.207
  -- 95.81.173.8

 */

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <lm35library.h>



// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 
  0x00, 0x51, 0x09, 0x11, 0x10, 0x5A };
IPAddress dnsserver(192, 168, 1, 254);
IPAddress gateway(192, 168, 1, 254);
IPAddress netmask(255, 255, 255, 0);
IPAddress localip(192,168,1,251);



// Variables pour récupérer une heure UTC
// --- Structure UTCtime
struct UTCtime {
  int hh;
  int mm;
  int ss;
  bool retOK;
};
// --- Variable globale pour garder l'heure active
UTCtime globaltime;

// --- local port to listen for UDP packets
unsigned int localPort = 8888;
// --- List of timeservers
IPAddress timeServer1(212, 83, 174, 163);
IPAddress timeServer2(62, 210, 207, 122);
// --- NTP time stamp is in the first 48 bytes of the message
const int NTP_PACKET_SIZE= 48;
// --- buffer to hold incoming and outgoing packets 
byte packetBuffer[ NTP_PACKET_SIZE]; 
// --- Instanciation d'une variable de type EthernetUDP
EthernetUDP udp;


// 
// Variables pour la gestion des temperatures
LM35 tempsensor;

int pin_relay = 2;

unsigned long lastgetutcmillis = 0 ;
unsigned long curmillis = 0;


// Initialize the Ethernet server library
// with the IP address and port you want to use 
// (port 80 is default for HTTP):
EthernetServer server(80);


void setup() {
 // Open serial communications and wait for port to open:
  Serial.begin(115200);
  /* while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  */
  

  // Application de la configuration Ethernet
  Ethernet.begin(mac, localip, dnsserver, gateway, netmask);
  // Démarrage su serveur sur le port 80
  server.begin();
  // Démarrage de la partie UDP pour le NTP
  udp.begin(localPort);
  // attente 3 secondes
  delay(3000);
  
  tempsensor.begin(3);
  
  
  Serial.print("Adresse IP : ");
  Serial.println(Ethernet.localIP());
  
  Serial.println("Recup d'une heure (obligatoire pour démarrer)...");
  globaltime = getUTCtime();
  lastgetutcmillis = millis() ;
  while (globaltime.retOK == false) {
    Serial.println("Impossible d'obtenir une heure.. attente 5 secondes et nouvel essai.");
    delay(5000);
    globaltime = getUTCtime();
    lastgetutcmillis = millis() ;
  }
  // on a récupéré une heure...
  Serial.print("Heure UTC de lancement: ");
  Serial.print(globaltime.hh);
  Serial.print("h ");
  Serial.print(globaltime.mm);
  Serial.print("min ");
  Serial.print(globaltime.ss);
  Serial.println("s. ");
  
  curmillis = millis();
  
  pinMode(pin_relay,OUTPUT);
  
}


void loop() {
  float temp=0 ;
    
  
  boolean chauffer = false ;
  // on va chercher la température courante
  temp = tempsensor.lm35read();
  // debug
  Serial.print("Temp lue : ");
  Serial.print(temp);
  Serial.println();
  
  // mise à jour d' l'heure 'calculee'
  curmillis = millis();
  // on fait un check au serveur de temps toutes les 30 minutes
  unsigned long millisdiff = 0 ;
  unsigned long thirtyMinutes = 30 * 60 * 1000 ;
  millisdiff = curmillis - lastgetutcmillis ;
  Serial.print("MillisDiff : ");
  Serial.print(millisdiff);
  Serial.print(" - Soit : ");
  Serial.print(millisdiff/60000);
  Serial.println(" minutes");
  if (millisdiff/60000 > thirtyMinutes) {
    // On fait un retrive du temps UTC
    Serial.println("30 minutes ecoulees - Execution 'getUTCtime()'");
    globaltime = getUTCtime();
    lastgetutcmillis = millis() ;
    curmillis = lastgetutcmillis ;
  } else {
    int mm = 0;
    int hh = 0;
    // --- On met à jour un horaire "théorique"
    Serial.println("Mise a jour horaire theorique");
    globaltime.ss = 0 ;
    mm = globaltime.mm ;
    hh = globaltime.hh ;
    mm = mm + millisdiff/60000;
    if (mm > 59) {
      hh = hh + 1;
      mm = mm - 59 ;
      if (hh > 23) {
        hh = 0;
      }
    }
    globaltime.hh = hh;
    globaltime.mm = mm;
  }
  Serial.print("horaire en cours : ");
  Serial.print(globaltime.hh);
  Serial.print("h ");
  Serial.print(globaltime.mm);
  Serial.print("min ");
  Serial.print(globaltime.ss);
  Serial.println("sec.");
  
  
  
  // Est-ce qu'on doit chauffer
  chauffer = doit_on_chauffer(temp) ;
  if (chauffer == true) {
    Serial.println("Allumage de la chaudiere");
    chaudiere_allume();
  } else {
    Serial.println("Extinction de la chaudiere");
    chaudiere_eteint();
  }
  
  
  
  // Réduction de la charge
  for (int ii=0; ii<10000 ; ii++) {
    // Accepter les connexions Ethernet et se débrancher dans une procédure (lisibilité)
    EthernetClient client = server.available();
    if (client) {
      httpHandleClient(client, temp, chauffer) ;
    }
    delay(1);
  }
}


void chaudiere_allume() {
  digitalWrite(pin_relay,HIGH);
}

void chaudiere_eteint() {
  digitalWrite(pin_relay,LOW);
}



// Fonction pour déterminer si on doit chauffer
boolean doit_on_chauffer(float temperature_actuelle) {
  // Définition des plages horaires de chauffe : 
  // Plage de chauffe : 
  // 00h00 -> 07h00
  // 10h00 -> 12h
  // 14h30 -> 17h00
  // 21h30 > 00h00
  int h_chauffe_start[4] = {0000, 1000, 1430, 2130} ;
  int h_chauffe_stop[4]  = {0700, 1200, 1700, 2359} ;
  boolean il_faut_chauffer = false ;
  // --- Consigne Ideale
  float consigne_ideale = 21.5;
  // --- Température minimale à maintenir
  float consigne_mini = 17.0 ;
  float consigne_basse = consigne_ideale - 0.5 ;
  float consigne_haute = consigne_ideale + 0.5 ;
  boolean bHoraireOK = false ;
  int hh = 0;
  int mm = 0;
  int hhmm = 0;
  int i_start = 0;
  int i_stop = 0;
  int i =0;
  
  hh = globaltime.hh;
  mm = globaltime.mm;
  hhmm = hh*100+mm;
  
  Serial.print("Horaire servant de calcul :");
  Serial.println(hhmm);
  
  // on determine la plage actuelle
  // --- plage start
  if (hhmm > h_chauffe_start[3]) {
    i_start = 3;
  } else {
    if (hhmm > h_chauffe_start[2]) {
      i_start = 2;
    } else {
      if (hhmm > h_chauffe_start[1]) {
        i_start = 1;
      } else {
        if (hhmm > h_chauffe_start[0]) {
          i_start = 0;
        }
      }
    }
  }
  
  // --- plage stop
  if (hhmm < h_chauffe_stop[0]) {
    i_stop = 0;
  } else {
    if (hhmm < h_chauffe_stop[1]) {
      i_stop = 1;
    } else {
      if (hhmm < h_chauffe_stop[2]) {
        i_stop = 2;
      } else {
        if (hhmm > h_chauffe_stop[3]) {
          i_stop = 3;
        }
      }
    }
  }
  
  Serial.print("Indices des horaires : Start:");
  Serial.print(i_start);
  Serial.print(" - Stop:");
  Serial.println(i_stop);
  
  // les deux indices concordent, l'horaire est prêt pour la chauffe
  if (i_start == i_stop) {
    bHoraireOK = true;
  }
  
  
  if (temperature_actuelle <= consigne_mini) {
    il_faut_chauffer = true;
  } else {
    if (bHoraireOK) {
      if (temperature_actuelle <= consigne_basse){
        il_faut_chauffer = true ;
      }
      if (temperature_actuelle <= consigne_ideale) {
        il_faut_chauffer = true ;
      }
    }
  }
  
  return il_faut_chauffer ;
}




















// 
// --- Fonction getUTCtime : retourne l'heure UTC dans une structure
struct UTCtime getUTCtime() {
  UTCtime utctime ;
  bool bRet ;
  int i;
  // send an NTP packet to a time server
  // Clear received data from possible stray received packets
  udp.flush();
  sendNTPpacket(timeServer1); 

  // wait to see if a reply is available
  bRet = false;
  i=0;
  while (bRet == false) {
    delay(1000);
    bRet = udp.parsePacket();
    i++;
    if (i > 5) { 
      utctime.hh=0;
      utctime.mm=0;
      utctime.ss=0;
      utctime.retOK = false;
      return utctime;
    }
  }
  
  
  // arrivé ici, on a forcément une réponse  
  // We've received a packet, read the data from it
  udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer

  //the timestamp starts at byte 40 of the received packet and is four bytes,
  // or two words, long. First, esxtract the two words:

  unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  
  // combine the four bytes (two words) into a long integer
  // this is NTP time (seconds since Jan 1 1900):
  unsigned long secsSince1900 = highWord << 16 | lowWord;  
  //Serial.print("Seconds since Jan 1 1900 = " );
  //Serial.println(secsSince1900);               

  // now convert NTP time into everyday time:
  //Serial.print("Unix time = ");
  // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
  const unsigned long seventyYears = 2208988800UL;     
  // subtract seventy years:
  unsigned long epoch = secsSince1900 - seventyYears;  
  // print Unix time:
  //Serial.println(epoch);                               


  // print the hour, minute and second:
  Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
  utctime.hh = (epoch  % 86400L) / 3600;
  Serial.print(utctime.hh); // print the hour (86400 equals secs per day)
  Serial.print(':'); 
  utctime.mm = ((epoch % 3600) / 60);
  if ( utctime.mm < 10 ) {
    // In the first 10 minutes of each hour, we'll want a leading '0'
    Serial.print('0');
  }
  Serial.print(utctime.mm); // print the minute (3600 equals secs per minute)
  Serial.print(':'); 
  utctime.ss = (epoch % 60);
  if ( utctime.ss < 10 ) {
    // In the first 10 seconds of each minute, we'll want a leading '0'
    Serial.print('0');
  }
  Serial.println(utctime.ss); // print the second
  
  utctime.retOK = true;
  return utctime;
}













// send an NTP request to the time server at the given address 
unsigned long sendNTPpacket(IPAddress& address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE); 
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49; 
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp: 		   
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer,NTP_PACKET_SIZE);
  udp.endPacket(); 
}








//
//
// -- Procédure de gestion d'un client http
void httpHandleClient(EthernetClient httpclient, float temp, boolean chaudiereallumee) {
  Serial.println("new http client");
  Serial.print("HTTP Request : ");
  // an http request ends with a blank line
  boolean currentLineIsBlank = true;
  while (httpclient.connected()) {
    if (httpclient.available()) {
      char c = httpclient.read();
      Serial.write(c);
      // if you've gotten to the end of the line (received a newline
      // character) and the line is blank, the http request has ended,
      // so you can send a reply
      if (c == '\n' && currentLineIsBlank) {
        // send a standard http response header
        httpclient.println("HTTP/1.1 200 OK");
        httpclient.println("Content-Type: text/html");
        httpclient.println("Connection: close");  // the connection will be closed after completion of the response
	httpclient.println("Refresh: 15");  // refresh the page automatically every 55 sec
        httpclient.println();
        httpclient.println("<!DOCTYPE HTML>");
        httpclient.println("<html>");
        httpclient.println("<head>");
        httpclient.print("<meta http-equiv=""Content-Type"" content=""text/html; charset=iso-8859-1"" />");
        httpclient.println("</head>");          
        
        // --- Temperature
        httpclient.print("Temperature : ");
        httpclient.print(temp);
        httpclient.print(" degres C");
        httpclient.println("<br />");  
        
        // --- chauffe en cours
        if (chaudiereallumee == true) {
          httpclient.print("Chauffage en cours ...");
        } else {
          httpclient.print("Chauffage non activ&eacute");
        }
        
                  
        httpclient.println("</html>");
        break;
      }
  if (c == '\n') {
        // you're starting a new line
        currentLineIsBlank = true;
      } 
      else if (c != '\r') {
        // you've gotten a character on the current line
        currentLineIsBlank = false;
      }
    }
  }
  // give the web browser time to receive the data
  delay(10);
  // close the connection:
  httpclient.stop();
  Serial.println("client disconnected");
}
