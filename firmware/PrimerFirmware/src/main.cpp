#include <Arduino.h>
#include "DHT.h" //se declara la libreria

#define DHTPIN 2 //defino GPO2
#define DHTTYPE DHT22 //defino el tipo de sensor DHT (DHT11 o DHT22)
DHT dht(2, DHT22); //defino el pin y el tipo de sensor

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); //inicializar el puerto serial
  Serial.println(F("Testeando Sensor DHT22...")); //Lo que va a mostrar en la pantalla
   
   dht.begin(); //inicializar el sensor DHT22
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(2000);// tiempo entre muestreo
  
  float h = dht.readHumidity(); //se imprime Humedad
  float t = dht.readTemperature();//se imprime temperatura en celcius

   Serial.print(F("Hum: "));
  Serial.print(h);
  Serial.println(F("% ")); 
  Serial.print(F("%  Temp: ")); 
  Serial.print(t);
  Serial.print(F("°C "));
}

