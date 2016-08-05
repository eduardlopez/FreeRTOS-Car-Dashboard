#include <Arduino_FreeRTOS.h>
#include <croutine.h>
#include <event_groups.h>
#include <FreeRTOSConfig.h>
#include <FreeRTOSVariant.h>
#include <list.h>
#include <mpu_wrappers.h>
#include <portable.h>
#include <portmacro.h>
#include <projdefs.h>
#include <queue.h>
#include <semphr.h>
#include <StackMacros.h>
#include <task.h>
#include <timers.h>

#include <LiquidCrystal.h>
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

/*
// define two tasks for Blink & AnalogRead
void TaskBlink( void *pvParameters );
void TaskAnalogRead( void *pvParameters );
*/



SemaphoreHandle_t xSerialSemaphore;



const byte ledPinISR = 13;
const byte pinISR = 21;
byte ledPinISRstate = LOW;

int pulses = 0;

void ISRpulses() {
  pulses++;
  Serial.println(pulses);
  ledPinISRstate = !ledPinISRstate;
  digitalWrite(ledPinISR, ledPinISRstate);
}


static QueueHandle_t xqueue_DistanceCalcPulse_Input = NULL; 
static QueueHandle_t xqueue_SpeedCalcPulse_Input = NULL;
static QueueHandle_t xqueue_DistanceCalc_OutputToFuelCalc = NULL;

static QueueHandle_t xqueue_FuelCalc_Input = NULL; 
static QueueHandle_t xqueue_FuelCalc_OutpuToLed = NULL;

static QueueHandle_t xQueueToCtrl = NULL;

typedef struct{
    float value;
    char  source;
} xDataToCtrt;



static void prvPulseCounter_task(void *pvParameters){

    TickType_t xNextWakeTime;
    xNextWakeTime = xTaskGetTickCount();
    
    for (;;) {       
        vTaskDelayUntil(&xNextWakeTime, 500/portTICK_PERIOD_MS);       
        xQueueSend(xqueue_DistanceCalcPulse_Input, &pulses, 0);
        xQueueSend(xqueue_SpeedCalcPulse_Input, &pulses, 0);
        pulses = 0;         
    }
}




static void prvDistanceCalc_task(void *pvParameters){
    float distanceC = 0; 

    int partialPulses;
    double partial_distance = 0; 
    float pulsesD;
    
    typedef struct{
        float value;
        char  source;
    } xDataToCtrt;

    xDataToCtrt xStructToSendToCtrl =    { 0.0 ,  'd' };

    for (;;) {
        
        // Se bloquea por "siempre" (portMAX_DELAY) hasta que haya algo en la cola
        if (xQueueReceive( xqueue_DistanceCalcPulse_Input, &partialPulses, portMAX_DELAY) == pdPASS) {
            pulsesD = (float) partialPulses;
            partial_distance =   ( (pulsesD*(2*3.141516*159.1549431)) / (100000) );  // 20" = 50.8 cm         
            distanceC = distanceC + partial_distance;
    
            xStructToSendToCtrl.value = distanceC;
            xStructToSendToCtrl.source = 'd';
            //Serial.println(distanceC, DEC);
            xQueueSend(xQueueToCtrl, &xStructToSendToCtrl, 0);
            xQueueSendToFront(xqueue_DistanceCalc_OutputToFuelCalc, &distanceC, 0);
        }
    }

}


static void prvSpeedCalc_task(void *pvParameters){

    //xDataToCtrt *datatmp =  (struct xDataToCtrt*) pvParameters;

    unsigned int partialPulses;
    float tiempo;
    float speed; 
    float distance;
     // 500 ms in hours ~ 1.389 * 10^4
    tiempo =  0.000138888; // 1/7200;
    float pulsesD;

    typedef struct{
        float value;
        char  source;
    } xDataToCtrt;

    xDataToCtrt xStructToSendToCtrl =   {  0.0 ,   's' };
    for (;;) {       
        // Se bloquea por "siempre" (portMAX_DELAY) hasta que haya algo en la cola
        if (xQueueReceive( xqueue_SpeedCalcPulse_Input, &partialPulses, portMAX_DELAY) == pdPASS) {
            pulsesD = (float) partialPulses;
            distance =  ( (pulsesD*(2*3.141516*159.1549431) ) / (100000) ); // in km
            tiempo =  0.000138888;
            speed = distance / tiempo; // 20" = 50.8 cm
     
            xStructToSendToCtrl.value = speed;
            xStructToSendToCtrl.source = 's';
            //Serial.println(speed, DEC);
            
            xQueueSendToFront(xQueueToCtrl, &xStructToSendToCtrl, 0);
        }
    }
}




static void prvFuelLevel_task(void *pvParameters){

    TickType_t xNextWakeTime;
    xNextWakeTime = xTaskGetTickCount();

    /*unsigned long fuel;
    fuel = 1000;*/

    int fuelSensorValue = 0;
    
    for (;;) {
          
        vTaskDelayUntil(&xNextWakeTime, 500/portTICK_PERIOD_MS);

        // Mira a ver si puede obtener o "take" el semaforo.
        // Si el semaforo no esta disponible, espera 2 ticks del scheduler para ver si luego este estara libre
        if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 2 ) == pdTRUE )
        {
          fuelSensorValue = analogRead(A0);
          xSemaphoreGive( xSerialSemaphore );   // Liberar semaforo
        } 
        
        fuelSensorValue = map( fuelSensorValue, 200, 1000, 0, 100);
        if(fuelSensorValue>100){ fuelSensorValue=100; }
        else if( fuelSensorValue<0 ){ fuelSensorValue=0; }
        //Serial.println(fuelSensorValue);
        
        xQueueSend(xqueue_FuelCalc_Input, &fuelSensorValue, 0);   
    }
}




static void prvFuelCalc_task(void *pvParameters){

    int fuel0 = 0;
    int fuel1 = 0;
    
    float distance0 = 0;
    float distance1 = 0;
    
    float fuelRatio = 1;

    typedef struct{
        float value;
        char  source;
    } xDataToCtrt;

    xDataToCtrt xStructToSendToCtrl  =  {  0.0 ,   's' };

    for (;;) {
        
        // Se bloquea por "siempre" (portMAX_DELAY) hasta que haya algo en la cola
        if (xQueueReceive( xqueue_FuelCalc_Input, &fuel1, portMAX_DELAY) == pdPASS) {
  //          printf("\nDesde FUEL CALC @@@ | fuel = %lu\n", fuel );
             //Serial.println("Hola 1\n");
        }
        if (xQueueReceive( xqueue_DistanceCalc_OutputToFuelCalc, &distance1, portMAX_DELAY) == pdPASS) {
            //Serial.println("Hola 2\n");
 //           printf("\nDesde FUEL CALC @@@ | distance = %f\n", distance );
        }

        //if(fuel==0 && distance==0){ fuelRatio = 1;}
        //else{ fuelRatio = fuel/distance; } // Para evitar Excepción de coma flotante (`core' generado)
        fuelRatio = ( (float)fuel0 - (float)fuel1 ) / ( (float)distance1 - (float)distance0 );

        xStructToSendToCtrl.value = fuelRatio;
        xStructToSendToCtrl.source = 'f';
/*
        Serial.println(fuel1);
        Serial.println(distance1);
        Serial.println((float)fuel0 - (float)fuel1, DEC);
        Serial.println(fuelRatio, DEC);
        Serial.println("");
*/
        fuel0 = fuel1;
        distance0 = distance1;

        xQueueSend(xqueue_FuelCalc_OutpuToLed, &fuel1, 0);
        xQueueSendToFront(xQueueToCtrl, &xStructToSendToCtrl, 0);
    }
}



static void prvDisplaycCrl_task(void *pvParameters){
    float fuelratio;
    float distance;
    float speed;

    TickType_t xNextWakeTime;
    xNextWakeTime = xTaskGetTickCount();

    xDataToCtrt xStructsToCtrl ;

    Serial.println("11111111111");
    
    for (;;) {
      Serial.println("2222222222222");
        vTaskDelayUntil(&xNextWakeTime, 500/portTICK_PERIOD_MS);
            int i=0;
            for (i=0; i<3; i++){
                 if (xQueueReceive( xQueueToCtrl, &xStructsToCtrl, 1) == pdPASS) {
                    if(xStructsToCtrl.source == 's') { 
                        speed = xStructsToCtrl.value; 
                        Serial.println("speed = ");
                        Serial.print(speed);
                        Serial.println("");
                    }
                    if(xStructsToCtrl.source == 'd') { 
                        distance = xStructsToCtrl.value; 
                        Serial.println("distance = ");
                        Serial.print(distance);
                        Serial.println("");
                    }
                    if(xStructsToCtrl.source == 'f') { 
                        fuelratio = xStructsToCtrl.value; 
                        Serial.println("fuelratio = ");
                        Serial.print(fuelratio);
                        Serial.println("");
                    }
                 }    
            }
            Serial.println("");Serial.println("");Serial.println("");

            //lcd.clear(); //se limpia el display
    
            lcd.setCursor(0,0); //nos situamos en la fila 0 columna 0
            lcd.print("Dist: "); //mostramos el texto fijo 
            lcd.setCursor(5,0); //nos situamos en la fila 1 columna 0
            lcd.print(distance); //se muestra la variable del contador
        
            lcd.setCursor(0,1);
            lcd.print("Vel: ");
            lcd.setCursor(4,1);
            lcd.print(speed);
        
            lcd.setCursor(9,1);
            lcd.print("F: ");
            lcd.setCursor(11,1);
            lcd.print(fuelratio);
            //delay(800);
      }
      
      
}




int ledPins[] = {
 22, 24, 26, 28, 30, 32, 34, 36, 38, 40
};       
int pinCount = 10;       

static void prvLEDctrl_task(void *pvParameters){

    int fuel;

    for (;;) {
        
        // Se bloquea por "siempre" (portMAX_DELAY) hasta que haya algo en la cola
        if (xQueueReceive( xqueue_FuelCalc_OutpuToLed, &fuel, 500/portMAX_DELAY) == pdPASS) {
            //printf("\n @@@@@@@@@@@@@@@@@@@@@@@@@ LED CTRL | fuelRatio = %f  @@@@@@@@@@@@@@@@@@@@@@@@@ \n", fuelRatio );
            fuel = map( fuel, 0, 100, 0, 10);

            for (int thisPin = 0; thisPin < pinCount; thisPin++) {

              if(thisPin < fuel ){
                digitalWrite(ledPins[thisPin], HIGH);
              }else{
                digitalWrite(ledPins[thisPin], LOW);
              } 
            }

            Serial.println(fuel);
        }
    }

}





 
void setup() {

   
  if ( xSerialSemaphore == NULL )  
  {
    xSerialSemaphore = xSemaphoreCreateMutex();  
    if ( ( xSerialSemaphore ) != NULL )
      xSemaphoreGive( ( xSerialSemaphore ) ); // xSemaphoreGive libera el semaforo  
  }

  
  for (int thisPin = 0; thisPin < pinCount; thisPin++) {
    pinMode(ledPins[thisPin], OUTPUT);
  }
  
  lcd.begin(16, 2);
  lcd.clear();// se limpia el display

  typedef struct{
    float value;
    char  source;
  } xDataToCtrt1;

  Serial.begin(9600);  
  pinMode(ledPinISR, OUTPUT);
  pinMode(pinISR, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pinISR), ISRpulses, CHANGE  );
 
 
  xqueue_DistanceCalcPulse_Input = xQueueCreate( 1, sizeof( unsigned long ) );
  xqueue_SpeedCalcPulse_Input = xQueueCreate( 1, sizeof( unsigned long ) );
  xqueue_DistanceCalc_OutputToFuelCalc = xQueueCreate( 1, sizeof( unsigned long ) );

   
  xqueue_FuelCalc_Input = xQueueCreate( 1, sizeof( unsigned int ) );  
  xqueue_FuelCalc_OutpuToLed = xQueueCreate( 1, sizeof( unsigned int ) );
  
  xQueueToCtrl = xQueueCreate( 3, sizeof( xDataToCtrt ) );

  

  xTaskCreate(
    prvPulseCounter_task
    ,  (const portCHAR *) "prvPulseCounter_task"   // A name just for humans
    ,  128  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  1  // Priority, with 1 being the highest, and 4 being the lowest.
    ,  NULL );
    
  xTaskCreate(
    prvDistanceCalc_task
    ,  (const portCHAR *) "prvDistanceCalc_task"   // A name just for humans
    ,  128  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  2  // Priority, with 1 being the highest, and 4 being the lowest.
    ,  NULL );

    xTaskCreate(
    prvSpeedCalc_task
    ,  (const portCHAR *) "prvSpeedCalc_task"   // A name just for humans
    ,  128  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  2  // Priority, with 1 being the highest, and 4 being the lowest.
    ,  NULL );




    xTaskCreate(
    prvFuelLevel_task
    ,  (const portCHAR *) "prvFuelLevel_task"   // A name just for humans
    ,  128  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  1  // Priority, with 1 being the highest, and 4 being the lowest.
    ,  NULL );

    xTaskCreate(
    prvFuelCalc_task
    ,  (const portCHAR *) "prvFuelLevel_task"   // A name just for humans
    ,  128  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  2  // Priority, with 1 being the highest, and 4 being the lowest.
    ,  NULL ); 




    xTaskCreate(
    prvDisplaycCrl_task
    ,  (const portCHAR *) "prvDisplaycCrl_task"   // A name just for humans
    ,  128  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  3  // Priority, with 1 being the highest, and 4 being the lowest.
    ,  NULL );

    xTaskCreate(
    prvLEDctrl_task
    ,  (const portCHAR *) "prvLEDctrl_task"   // A name just for humans
    ,  128  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  1  // Priority, with 1 being the highest, and 4 being the lowest.
    ,  NULL );
    

    
  // Now the task scheduler, which takes over control of scheduling individual tasks, is automatically started.
}
 
void loop()
{
  // Empty. Things are done in Tasks.
}











