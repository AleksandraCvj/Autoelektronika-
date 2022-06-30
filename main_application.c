/* Standard includes. */
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "extint.h"

/* Hardware simulator utility functions */
#include "HW_access.h"
void vApplicationIdleHook(void);

/* SERIAL SIMULATOR CHANNEL TO USE */
#define COM_CH_0 (0) 
#define COM_CH_1 (1)

/* TASK PRIORITIES */
#define TASK_SERIAL_REC_PRI		(3 + tskIDLE_PRIORITY )                                                  
#define	TASK_SERIAL_SEND_PRI		(2 + tskIDLE_PRIORITY )
#define	SERVICE_TASK_PRI		(1 + tskIDLE_PRIORITY )

/* TASKS */
static void SerialReceiveTask_0(void* pvParameters); 
static void SerialSendTask_0(void* pvParameters);
static void SerialReceiveTask_1(void* pvParameters);
static void Obrada_podataka(const void* pvParameters);
static void Led_bar(const void* pvParameters);

/* VARIABLES */
#define R_BUF_SIZE (32)
static char r_buffer[R_BUF_SIZE];
static uint16_t vrata;
uint64_t idleHookCounter;

/* 7-SEG NUMBER DATABASE - ALL HEX DIGITS */
static const uint8_t hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
								0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };
/* GLOBAL OS-HANDLES */
static SemaphoreHandle_t RXC_BS_0;
static SemaphoreHandle_t RXC_BS_1;
static QueueHandle_t serial_queue;
static QueueHandle_t max_brzina;
static QueueHandle_t broj_vrata;
static QueueHandle_t broj_vrata1;

/* RXC - RECEPTION COMPLETE - INTERRUPT HANDLER */
static uint32_t prvProcessRXCInterrupt(void)
{
	BaseType_t xHigherPTW = pdFALSE;

	if (get_RXC_status(0) != 0)
	{
		xSemaphoreGiveFromISR(RXC_BS_0, &xHigherPTW);
	}

	if (get_RXC_status(1) != 0) 
	{
		xSemaphoreGiveFromISR(RXC_BS_1, &xHigherPTW);
	}

	portYIELD_FROM_ISR((uint32_t)xHigherPTW);
}

/* Task za prijem karaktera sa kanala COM0 */
static void SerialReceiveTask_0(void* pvParameters)
{
	static uint8_t cc;
	static uint8_t z = 0;

	while(1)
	{
		if (xSemaphoreTake(RXC_BS_0, portMAX_DELAY) != pdTRUE)
		{
			printf("Greska pri preuzimanju semafora.\n");
		}

		if (get_serial_character(COM_CH_0, &cc) != 0)
		{
			printf("Greska.\n");
		}

		if (cc != (uint8_t)43)
		{ 
			r_buffer[z] = (char)cc;
			z++;
		}
		else 
		{
			r_buffer[z] = '\0';	
			z = 0;

			printf("CH0: %s\n", r_buffer);

			if (xQueueSend(serial_queue, &r_buffer, 10) != pdTRUE)
			{
				printf("Neuspesno slanje u red.\n");
			}
		}
	}
}

/* Task za ispis na serijsku */
static void SerialSendTask_0(const void* pvParameters)
{
	uint8_t c = (uint8_t)'s';

	while(1)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));

		if (send_serial_character(COM_CH_0, c) != 0)
		{
			printf("Greska prilikom slanja.");
		}
	}
}

/* Task za prijem karaktera sa kanala COM1 */
static void SerialReceiveTask_1(const void* pvParameters)
{
	static uint16_t bafer1[200];
	static uint16_t index1;
	static uint8_t cc;
	static uint16_t max_brz;
	static uint16_t stotine;
	static uint16_t desetice;
	static uint16_t jedinice;

	while(1)
	{
		if (xSemaphoreTake(RXC_BS_1, portMAX_DELAY) != pdTRUE)
		{
			printf("Greska pri preuzimanju semafora.\n");
		}

		if (get_serial_character(COM_CH_1, &cc) != 0)
		{
			printf("Greska.\n");
		}
		
		if (cc == (uint8_t)0x00)
		{
			index1 = 0;
		}
		else if (cc == (uint8_t)0x0d)
		{
			if ((bafer1[0] == (uint16_t)'M') && (bafer1[1] == (uint16_t)'A') && (bafer1[2] == (uint16_t)'X') && (bafer1[3] == (uint16_t)'B') && (bafer1[4] == (uint16_t)'R') && (bafer1[5] == (uint16_t)'Z'))
			{
				stotine = (uint16_t)bafer1[6] - (uint16_t)48;
				desetice = (uint16_t)bafer1[7] - (uint16_t)48;
				jedinice = (uint16_t)bafer1[8] - (uint16_t)48;
				max_brz = (stotine * (uint16_t)100) + (desetice * (uint16_t)10) + jedinice; 
				
				xQueueSend(max_brzina, &max_brz, 10);

				max_brz = (uint16_t)0;
			}
			else 
			{
				printf("Pogresan format pri upisu na COM1.\n");
			}

			static uint8_t m = 0;
			for (m = (uint8_t)0; m < (uint8_t)10; m++)
			{
				bafer1[m] = (uint16_t)'\0';
			}
		}
		else
		{
			bafer1[index1] = (uint8_t)cc;
			index1++;
		}
	}
}

/* Task za obradu podataka */
static void Obrada_podataka(const void* pvParameters)
{
	static uint16_t maksimalna_brzina;
	static uint16_t trenutna_brzina;
	static uint16_t stotine;
	static uint16_t desetice;
	static uint16_t jedinice;

	while (1)
	{
		xQueueReceive(serial_queue, &r_buffer, portMAX_DELAY);
		xQueueReceive(max_brzina, &maksimalna_brzina, portMAX_DELAY);
		
		if (r_buffer[0] == '0')
		{
			printf("Vrata su zatvorena.\n");
		}
		else
		{
			printf("Vrata su otvorena.\n");
		}
		
		vrata = (uint16_t)r_buffer[2] - (uint16_t)48;

		switch(vrata)
		{
			case 1:
				printf("PREDNJA LEVA\n");
				break;
			case 2:
				printf("PREDNJA DESNA\n");
				break; 
			case 3:
				printf("ZADNJA LEVA\n");
				break; 
			case 4:
				printf("ZADNJA DESNA\n"); 
				break; 
			case 5:
				printf("GEPEK\n");
				break; 
			default:
				printf("GRESKA\n");
				break;
		}

		stotine = (uint16_t)r_buffer[4] - (uint16_t)48;
		desetice = (uint16_t)r_buffer[5] - (uint16_t)48;
		jedinice = (uint16_t)r_buffer[6] - (uint16_t)48;
		trenutna_brzina = (stotine * (uint16_t)100) + (desetice * (uint16_t)10) + jedinice;
		printf("Trenutna brzina je %d\n", trenutna_brzina);

		if ((trenutna_brzina > maksimalna_brzina) && r_buffer[0] == '1')
		{
			if (xQueueSend(broj_vrata, &vrata, 10) != pdTRUE) 
			{
				printf("Neuspesno slanje u red.\n");
			}
			if (xQueueSend(broj_vrata1, &vrata, 10) != pdTRUE)
			{
				printf("Neuspesno slanje u red.\n");
			}
		}
	}
}

/* Task za blinkanje dioda */
static void Led_bar(const void* pvParameters)
{
	while(1)
	{
		static uint8_t taster;
		static uint16_t vrata1;

		xQueueReceive(broj_vrata1, &vrata1, portMAX_DELAY);

		if (vrata1 == 5)
		{
			get_LED_BAR(2, &taster);

			if ((taster & 0x01) != 0)
			{
				for (uint16_t i = 0; i < 5; i++) 
				{
					set_LED_BAR(0, 0xff);
					set_LED_BAR(1, 0xff);
					vTaskDelay(pdMS_TO_TICKS(500));

					set_LED_BAR(0, 0x00);
					set_LED_BAR(1, 0x00);
					vTaskDelay(pdMS_TO_TICKS(500));
				}
			}
		}
		else if (vrata1 == 4 || vrata1 == 3 || vrata1 == 2 || vrata1 == 1)
		{
			for (uint16_t i = 0; i < 5; i++)
			{
				set_LED_BAR(0, 0xff);
				set_LED_BAR(1, 0xff);
				vTaskDelay(pdMS_TO_TICKS(500));

				set_LED_BAR(0, 0x00);
				set_LED_BAR(1, 0x00);
				vTaskDelay(pdMS_TO_TICKS(500));
			}
		}
	}
}

/* Task za ispis na displej */
static void Displej(const void* pvParameters)
{
	while(1)
	{
		xQueueReceive(broj_vrata, &vrata, portMAX_DELAY);

		configASSERT(!select_7seg_digit(0));
		configASSERT(!set_7seg_digit(0X5E));
		configASSERT(!select_7seg_digit(1));
		configASSERT(!set_7seg_digit(0X5C));
		configASSERT(!select_7seg_digit(2));
		configASSERT(!set_7seg_digit(0X5C));
		configASSERT(!select_7seg_digit(3));
		configASSERT(!set_7seg_digit(0X50));

		switch(vrata)
		{
		case 1:
			configASSERT(!select_7seg_digit(4));
			configASSERT(!set_7seg_digit(0X06));
			break;
		case 2:
			configASSERT(!select_7seg_digit(4));
			configASSERT(!set_7seg_digit(0x5B));
			break;
		case 3:
			configASSERT(!select_7seg_digit(4));
			configASSERT(!set_7seg_digit(0x4F));
			break;
		case 4:
			configASSERT(!select_7seg_digit(4));
			configASSERT(!set_7seg_digit(0x66));
			break;
		case 5:
			configASSERT(!select_7seg_digit(4));
			configASSERT(!set_7seg_digit(0x6D));
			break;
		default:
			printf("Vrata su zatvorena\n");
			break;
		}
	}
}

/* Main application */
void main_demo(void)
{

	BaseType_t status;
	
	if (init_LED_comm() != 0) {
		printf("Neuspesna inicijalizacija\n");
	}

	if (init_7seg_comm() != 0) {
		
		printf("Neuspesna inicijalizacija\n");
	}

	if (init_serial_uplink(COM_CH_0) != 0) {
		printf("Neuspesna inicijalizacija\n");
	}

	if (init_serial_downlink(COM_CH_0) != 0) {
		
		printf("Neuspesna inicijalizacija\n");
	}

	if (init_serial_uplink(COM_CH_1) != 0) {
		printf("Neuspesna inicijalizacija\n");
	}

	if (init_serial_downlink(COM_CH_1) != 0) {
		
		printf("Neuspesna inicijalizacija\n");
	}

	vPortSetInterruptHandler(portINTERRUPT_SRL_RXC, prvProcessRXCInterrupt);

	RXC_BS_0 = xSemaphoreCreateBinary();
	if (RXC_BS_0 == NULL)
	{
		printf("Greska prilikom kreiranja semafora.\n");
	}

	RXC_BS_1 = xSemaphoreCreateBinary();
	if (RXC_BS_1 == NULL)
	{
		printf("Greska prilikom kreiranja semafora.\n");
	}

	status = xTaskCreate(SerialReceiveTask_0, "SR0", configMINIMAL_STACK_SIZE, NULL, (UBaseType_t)TASK_SERIAL_REC_PRI, NULL);
	if (status != pdPASS)
	{
		printf("Greska prilikom kreiranja task-a.\n");
	}

	status = xTaskCreate(SerialSendTask_0, "ST0", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAL_SEND_PRI, NULL);
	if (status != pdPASS)
	{
		printf("Greska prilikom kreiranja task-a.\n");
	}

	status = xTaskCreate(SerialReceiveTask_1, "SR1", configMINIMAL_STACK_SIZE, NULL, (UBaseType_t)TASK_SERIAL_REC_PRI, NULL);
	if (status != pdPASS)
	{
		printf("Greska prilikom kreiranja task-a.\n");
	}

	status = xTaskCreate(Obrada_podataka, "Obrada", configMINIMAL_STACK_SIZE, NULL, (UBaseType_t)SERVICE_TASK_PRI, NULL);
	if (status != pdPASS)
	{
		printf("Greska prilikom kreiranja task-a.\n");
	}

	status = xTaskCreate(Displej, "7seg", configMINIMAL_STACK_SIZE, NULL, (UBaseType_t)SERVICE_TASK_PRI, NULL);
	if (status != pdPASS)
	{
		printf("Greska prilikom kreiranja task-a.\n");
	}

	status = xTaskCreate(Led_bar, "led", configMINIMAL_STACK_SIZE, NULL, (UBaseType_t)SERVICE_TASK_PRI, NULL);
	if (status != pdPASS)
	{
		printf("Greska prilikom kreiranja task-a.\n");
	}
	
	serial_queue = xQueueCreate(150, sizeof(uint16_t));
	if (serial_queue == NULL)
	{
		printf("GRESKA.\n");
	}

	max_brzina = xQueueCreate(150, sizeof(uint16_t));
	if (max_brzina == NULL)
	{
		printf("GRESKA.\n");
	}

	broj_vrata = xQueueCreate(150, sizeof(uint16_t));
	if (broj_vrata == NULL)
	{
		printf("GRESKA.\n");
	}

	broj_vrata1 = xQueueCreate(150, sizeof(uint16_t));
	if (broj_vrata1 == NULL)
	{
		printf("GRESKA.\n");
	}

	vTaskStartScheduler();

	for (;;);
}

void vApplicationIdleHook(void) {

	idleHookCounter++;
}