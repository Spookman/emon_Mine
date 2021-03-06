/*
 EmonTx CT123 + Voltage example
 
 An example sketch for the emontx module for
 CT and AC voltage sample electricity monitoring. Enables real power and Vrms calculations.
 
 Part of the openenergymonitor.org project
 Licence: GNU GPL V3
 
 Authors: Glyn Hudson, Trystan Lea
 Builds upon JeeLabs RF12 library and Arduino
 
 emonTx documentation: http://openenergymonitor.org/emon/modules/emontxshield/
 emonTx firmware code explination: http://openenergymonitor.org/emon/modules/emontx/firmware
 emonTx calibration instructions: http://openenergymonitor.org/emon/modules/emontx/firmware/calibration
 
 THIS SKETCH REQUIRES:
 
 Libraries in the standard arduino libraries folder:
 	- JeeLib		https://github.com/jcw/jeelib
 	- EmonLib		https://github.com/openenergymonitor/EmonLib.git
 
 Other files in project directory (should appear in the arduino tabs above)
 	- emontx_lib.ino
 
 */

/*Recommended node ID allocation
 ------------------------------------------------------------------------------------------------------------
 -ID-	-Node Type- 
 0	- Special allocation in JeeLib RFM12 driver - reserved for OOK use
 1-4     - Control nodes 
 5-10	- Energy monitoring nodes
 11-14	--Un-assigned --
 15-16	- Base Station & logging nodes
 17-30	- Environmental sensing nodes (temperature humidity etc.)
 31	- Special allocation in JeeLib RFM12 driver - Node31 can communicate with nodes on any network group
 -------------------------------------------------------------------------------------------------------------
 */

#define FILTERSETTLETIME 5000                                           //  Time (ms) to allow the filters to settle before sending data

//CT 1 is always enabled
const int CT2 = 1;                                                      // Set to 1 to enable CT channel 2
const int CT3 = 1;                                                      // Set to 1 to enable CT channel 3

const int PV_gen_offset=20;         // When generation drops below this level generation will be set to zero - used to force generation level to zero at night

#define freq RF12_433MHZ                                                // Frequency of RF12B module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.
const int nodeID = 10;                                                  // emonTx RFM12B node ID
const int networkGroup = 210;                                           // emonTx RFM12B wireless network group - needs to be same as emonBase and emonGLCD needs to be same as emonBase and emonGLCD

const int UNO = 1;                                                      // Set to 0 if your not using the UNO bootloader (i.e using Duemilanove) - All Atmega's shipped from OpenEnergyMonitor come with Arduino Uno bootloader
#include <avr/wdt.h>                                                    // the UNO bootloader 

#include <JeeLib.h>                                                     // Download JeeLib: http://github.com/jcw/jeelib
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

#include "EmonLib.h"
EnergyMonitor ct1,ct2,ct3;                                              // Create  instances for each CT channel

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 4                                                  // Data wire is plugged into port 2 on the Arduino
OneWire oneWire(ONE_WIRE_BUS);                                          // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature sensors(&oneWire);                                    // Pass our oneWire reference to Dallas Temperature.
 
// By using direct addressing its possible to make sure that as you add temperature sensors
// To find the addresses of your temperature sensors use the: **temperature_search sketch**
DeviceAddress address_T1 = { 0x28, 0x42, 0xDD, 0x27, 0x05, 0x00, 0x00, 0x74 };
DeviceAddress address_T2 = { 0x28, 0x46, 0xD9, 0x09, 0x05, 0x00, 0x00, 0xD2 };
DeviceAddress address_T3 = { 0x28, 0xC6, 0xE8, 0x08, 0x05, 0x00, 0x00, 0xFE };
//DeviceAddress address_T4 = { 0x28, 0x13, 0xB6, 0x27, 0x05, 0x00, 0x00, 0x0C };
//DeviceAddress address_T5 = { 0x28, 0xDC, 0xD8, 0x27, 0x05, 0x00, 0x00, 0x04 };


typedef struct { 
               int power1;                                              // Grid Power Supply
               int power2;                                              // Solar Power Supply
               int power3;                                              // Not in use
               int Vrms;                                                // Voltage
               int powerFactor;                                         // Power Factor
               int Irms;                                                // Current
               int T1;                                                  // Tempreture Probe 1
               int T2;                                                  // Tempreture Probe 2
               int T3;                                                  // Tempreture Probe 3
//               int T4;                                                  // Tempreture Probe 4
//               int T5;                                                  // Tempreture Probe 5
} 
PayloadTX;         // neat way of packaging data for RF comms
PayloadTX emontx;

const int LEDpin = 9;                                                   // On-board emonTx LED 

boolean settled = false;

void setup() 
{
  Serial.begin(9600);
  Serial.println("Custom emonTX CT123 Voltage & Tempreture");
  Serial.println("OpenEnergyMonitor.org");
  Serial.print("Node: "); 
  Serial.print(nodeID); 
  Serial.print(" Freq: "); 
  if (freq == RF12_433MHZ) Serial.print("433Mhz");
  if (freq == RF12_868MHZ) Serial.print("868Mhz");
  if (freq == RF12_915MHZ) Serial.print("915Mhz"); 
  Serial.print(" Network: "); 
  Serial.println(networkGroup);

  ct1.voltageTX(224.06, 1.7);                                         // ct.voltageTX(calibration, phase_shift) - make sure to select correct calibration for AC-AC adapter  http://openenergymonitor.org/emon/modules/emontx/firmware/calibration. Default is set for Ideal Power voltage adapter. 
  ct1.currentTX(1, 111.1);                                            // Setup emonTX CT channel (channel (1,2 or 3), calibration)
                                                                      // CT Calibration factor = CT ratio / burden resistance
  ct2.voltageTX(224.06, 1.7);                                         // CT Calibration factor = (100A / 0.05A) x 18 Ohms
  ct2.currentTX(2, 111.1);

  ct3.voltageTX(224.06, 1.7);
  ct3.currentTX(3, 111.1);

  sensors.begin();

  rf12_initialize(nodeID, freq, networkGroup);                          // initialize RF
  rf12_sleep(RF12_SLEEP);

  pinMode(LEDpin, OUTPUT);                                              // Setup indicator LED
  digitalWrite(LEDpin, HIGH);

  if (UNO) wdt_enable(WDTO_8S);                                         // Enable anti crash (restart) watchdog if UNO bootloader is selected. Watchdog does not work with duemilanove bootloader                                                             // Restarts emonTx if sketch hangs for more than 8s
}

void loop() 
{ 
  ct1.calcVI(20,2000);                                                  // Calculate all. No.of crossings, time-out 
  emontx.power1 = ct1.realPower;
  Serial.print(emontx.power1); 

  emontx.Vrms = ct1.Vrms*100;                                          // AC Mains rms voltage 
  emontx.powerFactor = ct1.powerFactor*100;
  emontx.Irms = ct1.Irms*100;

  if (CT2) {  
    ct2.calcVI(20,2000);                                               //ct.calcVI(number of crossings to sample, time out (ms) if no waveform is detected)                                         
    emontx.power2 = ct2.realPower;
    if (emontx.power2<PV_gen_offset) emontx.power2=0;
    Serial.print(" "); 
    Serial.print(emontx.power2);
  }

  if (CT3) {
    ct3.calcVI(20,2000);
    emontx.power3 = ct3.realPower;
    Serial.print(" "); 
    Serial.print(emontx.power3);
  }

  Serial.print(" "); 
  Serial.print(ct1.Vrms);
  Serial.print(" "); 
  Serial.print(ct1.powerFactor);
  Serial.print(" ");   
  Serial.print(ct1.Irms);
   
 sensors.requestTemperatures();                                        // Send the command to get temperatures
  
  emontx.T1 = sensors.getTempC(address_T1) * 100;
  if (emontx.T1<0) emontx.T1=0;
  emontx.T2 = sensors.getTempC(address_T2) * 100;
  if (emontx.T2<0) emontx.T2=0;
  emontx.T3 = sensors.getTempC(address_T3) * 100;
  if (emontx.T3<0) emontx.T3=0;
//  emontx.T4 = sensors.getTempC(address_T4) * 100;
//  emontx.T5 = sensors.getTempC(address_T5) * 100;

  Serial.print(" ");  Serial.print(emontx.T1);
  Serial.print(" ");  Serial.print(emontx.T2);
  Serial.print(" ");  Serial.print(emontx.T3);
//  Serial.print(" ");  Serial.print(emontx.T4);
//  Serial.print(" ");  Serial.print(emontx.T5);
  Serial.println();
  delay(100);

  // because millis() returns to zero after 50 days ! 
  if (!settled && millis() > FILTERSETTLETIME) settled = true;

  if (settled)                                                            // send data only after filters have settled
  { 
    send_rf_data();                                                       // *SEND RF DATA* - see emontx_lib
    digitalWrite(LEDpin, HIGH); delay(2); digitalWrite(LEDpin, LOW);      // flash LED
    emontx_sleep(5);                                                      // sleep or delay in seconds - see emontx_lib
  }
}

