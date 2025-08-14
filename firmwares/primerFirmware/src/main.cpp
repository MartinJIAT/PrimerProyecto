#include <Arduino.h>
#include "DHT.h" //Se declara libreria dHT

#define DHTPIN 2  //Defino el GPIO 2

#define DHTTYPE DHT22 //Defino tipo de Sensor DHT (DHT11 O DHT22)

DHT dht(DHTPIN, DHTTYPE); //Defino objeto

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); //Inicializar el PUERTO SERIAL 
  Serial.println(F("Testeando Sensor DHT22...!"));

  dht.begin(); //Inicializar el Sensor DHT22
  
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(2000); //Tiempo entre muestreo

  float h = dht.readHumidity(); //Se imprime Humedad

  float t = dht.readTemperature(); //Se imprime Temp en Celsius

  Serial.print(F("Hum: "));
  Serial.print(h);
  Serial.println(F("%"));
  Serial.print(F("Temp: "));
  Serial.print(t);
  Serial.print(F("°C "));
}
