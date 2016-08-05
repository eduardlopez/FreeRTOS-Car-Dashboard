#include <stdio.h>
#include "FreeRTOS.h"

#include <signal.h>
#include <unistd.h>

#include "task.h"
#include "queue.h"
#include "semphr.h"


#define mainQUEUE_RECEIVE_TASK_PRIORITY         ( tskIDLE_PRIORITY + 2 )
#define mainQUEUE_SEND_TASK_PRIORITY            ( tskIDLE_PRIORITY + 1 )
#define mainQUEUE_LENGTH                        ( 1 )
#define mainQUEUE_SEND_FREQUENCY_MS             ( 5000 / portTICK_RATE_MS )

//static void prvQueueReceiveTask(void *pvParameters);
//static void prvQueueSendTask(void *pvParameters);
static void prvPulseCounter_task(void *pvParameters);
static void prvSpeedCalc_task(void *pvParameters);
static void prvDistanceCalc_task(void *pvParameters);
static void prvFuelLevel_task(void *pvParameters);
static void prvFuelCalc_task(void *pvParameters);
static void prvDisplaycCrl_task(void *pvParameters);
static void prvLEDctrl_task(void *pvParameters);
//static xQueueHandle xQueue = NULL;

int pulses = 0;

/* kill -HUP xxx */
void sig_handler_A(int signo)
{
     if (signo == SIGHUP) {
         //printf("received SIGHUP\n");
        printf("\n\n-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*- ISR | pulses++ -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-\n\n");
        pulses++;
     }
}


/* kill -IO xxx */
void sig_handler_B(int signo)
{
    if (signo == SIGPOLL) {
        printf("received SIGPOLL\n");
        //printf("-*-*-*-*-*-*-*-*-*- ISR | pulses++ -*-*-*-*-*-*-*-*-*-\n");
    }
}

xSemaphoreHandle xSerialSemaphore;
//SemaphoreHandle_t xSerialSemaphore;


static xQueueHandle xqueue_SpeedCalcPulse_Input = NULL;
static xQueueHandle xqueue_DistanceCalcPulse_Input = NULL;
static xQueueHandle xqueue_FuelCalc_Input = NULL;

    //static xQueueHandle xqueue_SpeedCalc_Output = NULL; 
    //static xQueueHandle xqueue_DistanceCalc_OutputToDisplayCtrl = NULL;
static xQueueHandle xqueue_DistanceCalc_OutputToFuelCalc = NULL;

static xQueueHandle xqueue_FuelCalc_OutpuToLed = NULL;
   // static xQueueHandle xqueue_FuelCalc_OutpuToDisplatCtrl = NULL;


static xQueueHandle xQueueToCtrl = NULL;

//static xQueueHandle queueDistanceCalcPulseInput = NULL;
//static xQueueHandle queueDisplayClrtInput = NULL;

typedef struct{
    float value;
    char  source;
} xDataToCtrt;

//static const xDataToCtrt xStructsToSendToCtrl [3] = 
xDataToCtrt xStructsToSendToCtrl [3] = 
{
    { 1.2 ,   's'},
    { 0 ,   'd'},
    { 0 ,   'f'}
};

int main(int argc, char* argv[]) {

    if ( xSerialSemaphore == NULL )  
    {
        xSerialSemaphore = xSemaphoreCreateMutex();  
        if ( ( xSerialSemaphore ) != NULL ){
          xSemaphoreGive( ( xSerialSemaphore ) ); // xSemaphoreGive libera el semaforo  
        }
    }


    printf("\n\n\ntskIDLE_PRIORITY = %d \n\n\n\n", tskIDLE_PRIORITY);

    if (signal(SIGHUP, sig_handler_A) == SIG_ERR) {  printf("\ncan't catch SIGHUP\n");  }
    if (signal(SIGPOLL, sig_handler_B) == SIG_ERR) {  printf("\ncan't catch SIGPOLL\n");  }

    // QUEUE CREATION
    xqueue_SpeedCalcPulse_Input = xQueueCreate( 1, sizeof( unsigned long ) ); // 1 element queue
    xqueue_DistanceCalcPulse_Input = xQueueCreate( 1, sizeof( unsigned long ) );
    xqueue_FuelCalc_Input = xQueueCreate( 1, sizeof( unsigned long ) );

        //xqueue_SpeedCalc_Output = xQueueCreate( 1, sizeof( unsigned long ) );
        //xqueue_DistanceCalc_OutputToDisplayCtrl = xQueueCreate( 1, sizeof( unsigned long ) );
    xqueue_DistanceCalc_OutputToFuelCalc = xQueueCreate( 1, sizeof( unsigned long ) );


    xqueue_FuelCalc_OutpuToLed = xQueueCreate( 1, sizeof( unsigned long ) );
      //  xqueue_FuelCalc_OutpuToDisplatCtrl = xQueueCreate( 1, sizeof( unsigned long ) );


    xQueueToCtrl = xQueueCreate( 3, sizeof( xDataToCtrt ) );



    //queueDisplayClrtInput = xQueueCreate( lenQueuesPulseInput, sizeof( unsigned long ) );

    //xQueueSend(queueSpeedCalcPulseInput, 1, portMAX_DELAY);

    //ls();
    if (xqueue_SpeedCalcPulse_Input != NULL) {
        /* Start the two tasks as described in the accompanying application
         note. */
        
        
        xTaskCreate( prvPulseCounter_task, ( signed char * ) "PulseCounter_task",
                configMINIMAL_STACK_SIZE, NULL, mainQUEUE_RECEIVE_TASK_PRIORITY+1,
                NULL);

        xTaskCreate( prvSpeedCalc_task, ( signed char * ) "prvSpeedCalc_task",
                configMINIMAL_STACK_SIZE, &(xStructsToSendToCtrl[0]), mainQUEUE_RECEIVE_TASK_PRIORITY,
                NULL);

        xTaskCreate( prvDistanceCalc_task, ( signed char * ) "prvDistanceCalc_task",
                configMINIMAL_STACK_SIZE, &(xStructsToSendToCtrl[1]), mainQUEUE_RECEIVE_TASK_PRIORITY,
                NULL);


        xTaskCreate( prvFuelLevel_task, ( signed char * ) "prvFuelLevel_task",
                configMINIMAL_STACK_SIZE, NULL, mainQUEUE_RECEIVE_TASK_PRIORITY+1,
                NULL);
        
        xTaskCreate( prvFuelCalc_task, ( signed char * ) "prvFuelCalc_task",
                configMINIMAL_STACK_SIZE, &(xStructsToSendToCtrl[2]), mainQUEUE_RECEIVE_TASK_PRIORITY,
                NULL); 


        xTaskCreate( prvDisplaycCrl_task, ( signed char * ) "prvDisplaycCrl_task",
                configMINIMAL_STACK_SIZE, NULL, mainQUEUE_SEND_TASK_PRIORITY,
                NULL);

        xTaskCreate( prvLEDctrl_task, ( signed char * ) "prvLEDctrl_task",
                configMINIMAL_STACK_SIZE, NULL, mainQUEUE_SEND_TASK_PRIORITY,
                NULL);


        printf("Hollal\n");

        /* Start the tasks running. */
        vTaskStartScheduler();
    } else {
        printf("queue null\n");
    }

    return 0;
}






static void prvPulseCounter_task(void *pvParameters){

    portTickType xNextWakeTime;
    xNextWakeTime = xTaskGetTickCount();


    for (;;) {
        
        vTaskDelayUntil(&xNextWakeTime, 500/portTICK_RATE_MS);

        //if( pulses != 0){
            xQueueSend(xqueue_SpeedCalcPulse_Input, &pulses, 0);
            xQueueSend(xqueue_DistanceCalcPulse_Input, &pulses, 0);
        //}
 //       printf("Desde Pulse Counter | poniendo en cola %d pulses from wheels in 500ms\n", pulses);

        pulses = 0;    
        
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

    //printf("datatmp.value = %f \n", datatmp[1]->value );
    //printf("datatmp.source = %c \n", datatmp->source );
    //datatmp->value = 3;
    //printf("datatmp.value = %f \n", datatmp->value );
/*
    printf("pvParameters = %p \n", pvParameters);
    printf("pvParameters = %p \n", &xStructsToSendToCtrl[0].value);
    printf("pvParameters = %f \n", xStructsToSendToCtrl[0].value );
    */

    typedef struct{
        float value;
        char  source;
    } xDataToCtrt;

    xDataToCtrt xStructToSendToCtrl = 
    {
        { 0.0 ,   's'}
    };


    for (;;) {
        
        // Se bloquea por "siempre" (portMAX_DELAY) hasta que haya algo en la cola
        if (xQueueReceive( xqueue_SpeedCalcPulse_Input, &partialPulses, portMAX_DELAY) == pdPASS) {
            //if(partialPulses==0){ speed = 0; }
            //else { 
            distance =  ( (partialPulses*(2*3.141516*159.1549431) ) / (1000*100) ); // in km
            tiempo =  0.000138888;
            speed = distance / tiempo; // 20" = 50.8 cm
            //}
            /*
            printf("\n\nDesde Speed | partialPulses = %d pulses from wheels in 500ms \n", partialPulses );
            printf("Desde Speed | distance = %f\n", distance );
            printf("Desde Speed | tiempo = %f\n", tiempo );
            printf("Desde Speed | speed = %f\n\n", speed );
            */
            xStructToSendToCtrl.value = speed;
            xStructToSendToCtrl.source = 's';
            //printf("Desde Speed | xStructToSendToCtrl.value = %f\n\n", xStructToSendToCtrl.value );

            xQueueSendToFront(xQueueToCtrl, &xStructToSendToCtrl, 0);
        }
    }

}



static void prvDistanceCalc_task(void *pvParameters){
    float distanceC = 0; 

    unsigned int partialPulses;
    float partial_distance; 

    typedef struct{
        float value;
        char  source;
    } xDataToCtrt;

    xDataToCtrt xStructToSendToCtrl = 
    {
        { 0 ,   "d"}
    };

    for (;;) {
        
        // Se bloquea por "siempre" (portMAX_DELAY) hasta que haya algo en la cola
        if (xQueueReceive( xqueue_DistanceCalcPulse_Input, &partialPulses, portMAX_DELAY) == pdPASS) {
             
            partial_distance =   ( (partialPulses*(2*3.141516*159.1549431)) / (1000*100) );  // 20" = 50.8 cm         
            distanceC = distanceC + partial_distance;
/*
            printf("\nDesde Distance | partialPulses = %d pulses from wheels in 500ms \n", partialPulses );
            printf("Desde Distance | distance = %f km \n\n", distanceC );
*/
            xStructToSendToCtrl.value = distanceC;
            xStructToSendToCtrl.source = 'd';

            xQueueSend(xQueueToCtrl, &xStructToSendToCtrl, 0);
            xQueueSendToFront(xqueue_DistanceCalc_OutputToFuelCalc, &distanceC, 0);
        }
    }

}





static void prvFuelLevel_task(void *pvParameters){

    portTickType xNextWakeTime;
    xNextWakeTime = xTaskGetTickCount();

    unsigned long fuel;
    fuel = 1000;

    for (;;) {
        
        vTaskDelayUntil(&xNextWakeTime, 500/portTICK_RATE_MS);
        
        // Mira a ver si puede obtener o "take" el semaforo.
        // Si el semaforo no esta disponible, espera 2 ticks del scheduler para ver si luego este estara libre
        if ( xSemaphoreTake( xSerialSemaphore, 1/portTICK_RATE_MS ) == pdTRUE )
        {
            fuel -= 1;
            xSemaphoreGive( xSerialSemaphore ); 
        }   // Liberar semaforo
        

        xQueueSend(xqueue_FuelCalc_Input, &fuel, 0);

        //printf("\nDesde Fuel Level| ################################################\n");
 //       printf("\n\nDesde Fuel Level| poniendo en cola %lu fuel level en 5000 ms\n\n", fuel);
        //printf("\nDesde Fuel Level| ################################################\n\n");
        
    }

}



static void prvFuelCalc_task(void *pvParameters){

    unsigned long fuel1;
    unsigned long fuel0;

    float distance1;
    float distance0;

    float fuelRatio = 1;

    typedef struct{
        float value;
        char  source;
    } xDataToCtrt;

    xDataToCtrt xStructToSendToCtrl = 
    {
        { 0 ,   'f'}
    };

    for (;;) {
        
        // Se bloquea por "siempre" (portMAX_DELAY) hasta que haya algo en la cola
        if (xQueueReceive( xqueue_FuelCalc_Input, &fuel1, portMAX_DELAY) == pdPASS) {
  //          printf("\nDesde FUEL CALC @@@ | fuel = %lu\n", fuel );
        }
        if (xQueueReceive( xqueue_DistanceCalc_OutputToFuelCalc, &distance1, portMAX_DELAY) == pdPASS) {
 //           printf("Hola\n");
 //           printf("\nDesde FUEL CALC @@@ | distance = %f\n", distance );
        }

        //if(fuel==0 && distance==0){ fuelRatio = 1;}
        //else{ fuelRatio = fuel/distance; } // Para evitar Excepción de coma flotante (`core' generado)
        //fuelRatio = (float)fuel / (float)distance;
        fuelRatio = ( (float)fuel0 - (float)fuel1 ) / ( (float)distance1 - (float)distance0 );


//        printf("\nDesde Fuel ###### | fuelRatio = %f\n", fuelRatio );

        xStructToSendToCtrl.value = (float) fuelRatio;
        xStructToSendToCtrl.source = 'f';

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

    portTickType xNextWakeTime;
    xNextWakeTime = xTaskGetTickCount();

    xDataToCtrt xStructsToCtrl ;

    for (;;) {

        //if( uxQueueMessagesWaiting(xQueueToCtrl) != 3 ){
            //printf("\nDesde Display Crl | Cola vaciaa ¡¡!!");
        //}else{
            //printf("\nDesde Display Crl | Cola llena ¡¡!!");
        vTaskDelayUntil(&xNextWakeTime, 500/portTICK_RATE_MS);
            int i=0;
            //printf("\n\n\n------------------------------------------------------\n");
            for (i=0; i<3; i++){
                 if (xQueueReceive( xQueueToCtrl, &xStructsToCtrl, 1) == pdPASS) {
                    //printf("source = %c, value = %f \n", xStructsToCtrl.source, xStructsToCtrl.value);
                    if(xStructsToCtrl.source == 's') { 
                        speed = xStructsToCtrl.value; 
                        printf("Desde Display Crl | speed = %.2f\n", speed );
                    }
                    if(xStructsToCtrl.source == 'd') { 
                        distance = xStructsToCtrl.value; 
                        printf("Desde Display Crl | distance = %.2f\n", distance );
                    }
                    if(xStructsToCtrl.source == 'f') { 
                        fuelratio = xStructsToCtrl.value; 
                        printf("Desde Display Crl | fuelRatio = %.2f\n", fuelratio );
                    }
                 }    
            }

           /* printf("\nDesde Display Crl | speed = %f\n", speed );
            printf("Desde Display Crl | distance = %f\n", distance );
             printf("Desde Display Crl | fuel = %f\n", fuelratio );*/
            //printf("------------------------------------------------------ todo ok CRTL\n\n\n");

        }
    //}



      //  printf("bien ------------------------------------------------------\n");
/*
        if (xQueueReceive( xQueueToCtrl, &xStructsToCtrl, 0) == pdPASS) 
            printf("\nDesde Display Crl | speed = %f\n", xStructsToCtrl.value );
        }
*/
        /*
  //      printf(" HOLAAAAAA\n");
        vTaskDelayUntil(&xNextWakeTime, 5000/portTICK_RATE_MS);
        
        // Se bloquea por "siempre" (portMAX_DELAY) hasta que haya algo en la cola
        if (xQueueReceive( xqueue_SpeedCalc_Output, &speed, 1) == pdPASS) {
            printf("\nDesde Display Crl | speed = %f\n", speed );
        }else{}

        if (xQueueReceive( xqueue_DistanceCalc_OutputToDisplayCtrl, &distance, 1) == pdPASS) {
            printf("\nDesde Display Crl | distance = %f\n", distance );
        }else{}

        if (xQueueReceive( xqueue_FuelCalc_OutpuToDisplatCtrl, &fuelratio, 1) == pdPASS) {
            printf("\nDesde Display Crl | fuel = %f\n", fuelratio );
        }else{}
        */

/*          
        printf("\n\n\n¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬\n");
        printf("¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬\n");
        printf("¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬\n");
        printf(" DESDE DISPLAY |  DISTANCE = %f\n", distance);
        printf(" DESDE DISPLAY |  FUEL RATIO= %f\n", fuelratio);
        
        printf(" DESDE DISPLAY |  SPEED = %f\n", speed);
        printf("¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬\n");
        printf("¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬\n");
        printf("¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬¬\n\n\n");
*/

    

}








static void prvLEDctrl_task(void *pvParameters){


    unsigned long  fuel;


    for (;;) {
        
        // Se bloquea por "siempre" (portMAX_DELAY) hasta que haya algo en la cola
        if (xQueueReceive( xqueue_FuelCalc_OutpuToLed, &fuel, 500/portMAX_DELAY) == pdPASS) {
            printf("\n @@@@@@@@@@@@@@@@@@@@@@@@@ LED CTRL | fuel = %.2f  @@@@@@@@@@@@@@@@@@@@@@@@@ \n", (float)fuel );
        }
    }

}




