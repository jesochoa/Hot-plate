/*
Leo la termocupla y escribo con la libreria de la pantalla OLED
Libreria es de Rinky-Dink http://www.rinkydinkelectronics.com/library.php?id=79
Es la libreria OLED_I2C que ocupa poco y puedo meterla en un arduino NANO 168
Utilizo el programa de Electgpl y el diseño con unas pocas variaciones
La rutina de leer la termocupla es creada por mi
*/

#include <Arduino.h>
#include <OLED_I2C.h> //Libreria para la pantalla OLED

// Configuro SPI
#define SCLK PIN_SPI_SCK  //  Reloj SPI D13
#define MISO PIN_SPI_MISO //  Lee del dispositivo SPI D12
#define CS PIN_SPI_SS     //  Habilita leer los datos D10

//Entradas y salidas
#define MENU A2      // Pin A2 para el menu MANUAL/REFLOW
#define START A3     // Pin A3 para encender 1 apagado
#define TSETPOINT A0 // Pin A0 para el potenciometro
#define TRIAC PD2    // Pin 2 donde se conecta el TRIAC

//Variables globales para la temperatura
int tSeteo = 0;    // Temperatura que se programa
int tSensor = 0;   // Temperatura del plato
int hysteresis = 2;  // variable del hysteresis
int profile[4] = {100,150, 230, 20};// Rampa del REFLOW

//Variables boleanas para el menu
bool blMENU = true;  // true es manual false reflow

// True es cuando queremos que funcione la resistencia y funcione el REFLOW
bool blSTART = false;


// Genero la clase pantalla
OLED pantalla(A4, A5); // SDL y SCL

// Defino como globales las fuentes a utilizar 
extern uint8_t BigNumbers[];
extern uint8_t MediumNumbers[];
extern uint8_t SmallFont[];

//Variable gloval para controlar el tiempo para leer la sonda
unsigned long Tiempo1 = 0 ;


// Funcion para leer del MAX6675 y devolver la temperatura
float MAX6675_read(void)
{
   uint16_t v = 0;
   int i;
   // pone en low CS para empezar a leer
   digitalWrite(CS, LOW);
   delayMicroseconds(10);
   // rutina para leer los 16 bits y dejarlos bien ordenados
   //  el primer dato leido que sea la posición 16
   for (i = 15; i >= 0; i--)
   {
      digitalWrite(SCLK, LOW);
      delayMicroseconds(10);
      if (digitalRead(MISO))
      {
         v |= (1 << i);
      }
      digitalWrite(SCLK, HIGH);
      delayMicroseconds(10);
   }
   // CS en alto para no leer mas
   digitalWrite(CS, HIGH);

   // corro 3 bit para tener el valor de lectura y eliminar los 3 primeros
   v >>= 3;
   return v * 0.25 ; 
}



void setup()
{
   pantalla.begin(SSD1306_128X32); // declaro la pantalla
   pantalla.clrScr();              // Borro la pantalla
   pantalla.setBrightness(100);    // Ajusto el brillo no se nota mucho

   pinMode(TRIAC, OUTPUT);

   // Configuro SPI para leer el MAX6675
   pinMode(CS, OUTPUT);
   pinMode(SCLK, OUTPUT);
   pinMode(MISO, INPUT);
   digitalWrite(CS, HIGH); // En alto no lee

   // Botones de entrada configurados PULLUP interno
   pinMode(MENU, INPUT_PULLUP);  // Elije MANUAL/REFLOW
   pinMode(START, INPUT_PULLUP); // APAGADO 1 ENCENDIDO 0

   tSensor = MAX6675_read();//Lee el sensor de temperatura de la placa
   delay(300);
   Tiempo1 = millis();//Leo el tiempo
}

//miro si se pulsa START y no sale hasta que deje de pulsarlo
boolean pulsoSTART(){
   if (!digitalRead(START))
   {
      while (!digitalRead(START)){}// Se mantiene en el bucle mientras se MANTENGA pulsado
      return true;
   }else{ return false;}
}

// controla la salida del Triac mas la Histeresis
//Velocidad es el tiempo en ms que esta encendido
void ajustaTRIAC(int velocidad)
{
   if (tSeteo >= (tSensor + hysteresis))
   {
      digitalWrite(TRIAC, HIGH); // Activo la resistencia
      delay(velocidad); //Velocidad es tiempo que esta la resistencia encendida
      digitalWrite(TRIAC, LOW); // Apago la resistencia
      delay(100); 
   }

   if (tSeteo <= (tSensor - hysteresis))  digitalWrite(TRIAC, LOW); // Apago la resistencia si esta en temp. con la hysteresis  
}


// Visualiza la temperatura de la placa y el SETEO
// modo en que esta el reflow
void oledUpdate(int modo)
{
   pantalla.clrScr();
   pantalla.setFont(SmallFont);
   if (blMENU)
   { // MANUAL
      pantalla.print("Temp. PLACA   SETEADA", 0, 0);
      pantalla.print("MANUAL", 0, 14);
      if (blSTART)
      {
         pantalla.print(" ON", 0, 25);
      }
      else
      {
         pantalla.print(" OFF", 0, 25);
      }
   }
   else
   { // REFLOW
      pantalla.print("Temp. PLACA   SETEADA", 0, 0);
      pantalla.print("REFLOW", 0, 10);
      if (!blSTART)
      {
         pantalla.print(" OFF", 0, 25); 
      }
      else
      {
      if(modo==0) pantalla.print("RAMPA ", 0, 25);   
      if(modo==1) pantalla.print("PRECAL", 0, 25);  
      if(modo==2) pantalla.print("REFLOW", 0, 25);
      if(modo==3) pantalla.print("ENFRIA", 0, 25);
      }
   }

   pantalla.setFont(MediumNumbers);
   pantalla.printNumF(tSensor, 0, 43, 13);
   pantalla.printNumF(tSeteo, 0, 85, 13);
   pantalla.update();
}

//Funcion para leer el sensor si a pasado un tiempo
//definido Tespera
void leeSENSOR()
{
   //lee tiempo para saber la diferencia del tiempo para poder leer el sensor
   unsigned long Tespera = 300; //300ms tiempo de espera

   if(millis() > (Tiempo1 + Tespera)){
      Tiempo1 = millis(); //vuelvo a reiniciar el tiempo
      // lee la temperatura si a pasado el tiempo programado
      tSensor = int(MAX6675_read());//Paso float a int con int() 
   }
}


// Control de la placa en manual
void manual()
{
   // Mira el valor del potenciometro para ajustar la temperatura
   tSeteo = map(analogRead(TSETPOINT), 0, 1023, 0, 300); // map se ajusta de 0 a 300 grados
   
   leeSENSOR();//lee el senor si a pasado un tiempo
   
   //Si esta en ON enciende la placa
   if (blSTART) ajustaTRIAC(50); // manda 50 ms de encendido
}


// Control en Reflow
// Aplica el perfil de la rampa de REFLOW
void reflow()
{
   long tiempoESTADO;// tiempo que tiene que estar en cada etapa
   leeSENSOR(); // lee el sensor de temperatura

   //empieza el REFLOW si blSTART esta a 1
   while (blSTART)
   {
      //Pongo la temperatura de RAMPA y tiempo de RAMPA: profile[0]
      tSeteo = profile[0];
      tiempoESTADO = millis() + 120000; //Es el tiempo de RAMPA 120 segundos
      //no sale porque no llega a la temperatura de rampa o no a llegado al tiempo
      while (tSeteo > tSensor || tiempoESTADO > long(millis()))
      {
         oledUpdate(0); //le doy 0 para decirle que esta en rampa
         leeSENSOR(); // lee el sensor de temperatura
         ajustaTRIAC(50); //Pongo ms de encendido estaba en 50ms
         if (pulsoSTART()) break;  //miro si se pulsa START para salir
         if (tiempoESTADO < long(millis())) break;  // Si a llegado al tiempo sale
      }
 
      //Pongo la temperatura de PRECALENTAMIENTO: profile[1]
      tSeteo = profile[1];
      tiempoESTADO = millis() + 120000; //Es el tiempo de PRECALENTAMIENTO 120 segundos
      //no sale porque no llega a la temperatura de precalentamiento o no a llegado al tiempo
      while (tSeteo > tSensor || tiempoESTADO > long(millis()))
      {
         oledUpdate(1);//le doy 1 para decirle que esta en precalentamiento
         leeSENSOR(); // lee el sensor de temperatura
         ajustaTRIAC(50); //Pongo ms de encendido         
         if (pulsoSTART()) break;  //miro si se pulsa START para salir
         if (tiempoESTADO < long(millis())) break;  // Si a llegado al tiempo sale
      }
      //Pongo la temperatura de REFLOW: profile[2]
      tSeteo = profile[2];
      tiempoESTADO = millis() + 75000; //Es el tiempo de REFLOW segundos 75 segundo
      //no sale porque no llega a la temperatura de reflow o el tiempo  o no a llegado al tiempo
      while (tSeteo > tSensor || tiempoESTADO > long(millis()))
      {
         oledUpdate(2);//le doy 2 para decirle que esta en reflow
         leeSENSOR(); // lee el sensor de temperatura
         ajustaTRIAC(400); //Pongo ms de encendido
         if (pulsoSTART()) break;  //miro si se pulsa START para salir
         if (tiempoESTADO < long(millis())) break;  // Si a llegado al tiempo sale
      }
      //Pongo la temperatura de ENFRIAMIENTO: profile[3]
      tSeteo = profile[3];
      while (tSeteo > tSensor)//no sale porque no llega a la temperatura de ENFRIAMIENTO
      {
         oledUpdate(3);//le doy 3 para decirle que esta en enfriamiento
         leeSENSOR(); // lee el sensor de temperatura
         if (pulsoSTART()) break;  //miro si se pulsa START para salir
      }
      //Sale del reflow poniendo blSTART en falso
      blSTART = !blSTART;
   }
   
}


void loop()
{
   oledUpdate(0);

   // Cambia blMENU cada vez que se pulsa MENU
   if (!digitalRead(MENU))
   {
      blMENU = !blMENU;
      // Se mantiene en el bucle mientras se MANTENGA pulsado
      while (!digitalRead(MENU)){}
   }
   //Comprueba si se pulsa START y cambia blSTART
   if (pulsoSTART()) blSTART = !blSTART;  
   
  //Va a la funcion de manual o reflow
  if (blMENU){
   manual();
  }else{
   reflow();
  }
}
