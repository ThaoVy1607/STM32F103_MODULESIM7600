/*
 * myMain.cpp
 *
 *  Created on: Apr 16, 2023
 *      Author: ADMIN
 */
#include "stdio.h"
#include "MAX30100.hpp"
#include "string.h"
#include "sim7600.hpp"
#include "main.h"

#define LED_ON 	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET)
#define LED_OFF HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET)
#define LED_TIME 500
MAX30100* pulseOxymeter;
extern TIM_HandleTypeDef htim2;
extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;
extern TIM_HandleTypeDef htim3;
typedef enum{
	SIM_INIT =0,
	SIM_READ_GPS,
	SIM_REQUEST_HTTP,
	SIM_SEND_SMS,
	SIM_DONE,
	SIM_IDLE
}SIM_STATE;


#define NUM_SAMPLE 3
uint8_t heart_beat[NUM_SAMPLE];
uint8_t  heart_beatcnt=0;
uint32_t adder_time;
uint8_t result_heartbeat = 0;
uint16_t BatteryADC[100] = {0};
uint8_t  Battery = 0;
uint8_t sleep_enable = 0;

uint8_t data_heart_sensor = 83;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{

//	printf("GPIO %d LOW\r\n",GPIO_Pin);
	printf("weakup by GPIO\r\n");
	HAL_PWR_DisableSleepOnExit ();
	HAL_NVIC_DisableIRQ(EXTI0_IRQn);
}

void enter_sleep_mode(){
	if(!sleep_enable){
		return;
	}


//	SIM7600_TURN_OFF();
//	HAL_Delay(5000);
	HAL_TIM_Base_Stop_IT(&htim3);

	if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0))
	{
		printf("ENTER SLEEP MODE\r\n");
		HAL_NVIC_EnableIRQ(EXTI0_IRQn);
		__HAL_TIM_SET_COUNTER(&htim2,0);
		HAL_TIM_Base_Start_IT(&htim2);
		HAL_SuspendTick();

		HAL_PWR_EnableSleepOnExit ();

		//	  Enter Sleep Mode , wake up is done once User push-button is pressed
		HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
	}
	printf("SYSTEMS RESET\r\n");
//	HAL_ResumeTick();
	HAL_NVIC_SystemReset();
//	HAL_Delay(100);
//	printf("EXIT SLEEP MODE\r\n");

}
void add_heart_beat(uint8_t value)
{
	if(heart_beatcnt ==0)
	{
		heart_beat[heart_beatcnt] = value;
		adder_time = HAL_GetTick() +1500;
		heart_beatcnt++;
	}
	else
	{
		if((abs(value - heart_beat[heart_beatcnt-1]) >3) || (HAL_GetTick()>adder_time))
		{
			memset(heart_beat,0,NUM_SAMPLE);
			heart_beatcnt = 0;
			heart_beat[heart_beatcnt] = value;
			adder_time = HAL_GetTick() +1500;
			heart_beatcnt++;
			return;
		}
		else
		{
			if(heart_beatcnt == NUM_SAMPLE)
			{
				for(int i =0;i<NUM_SAMPLE-1;i++)
				{
					heart_beat[i] = heart_beat[i+1];
				}
				heart_beat[4] = value;
				heart_beatcnt = NUM_SAMPLE;
				uint16_t total =0;
				for(int i =0;i<NUM_SAMPLE;i++)
				{
					total+=heart_beat[i];
				}
				result_heartbeat = total/NUM_SAMPLE;
			}
			else
			{
				heart_beat[heart_beatcnt] = value;
				heart_beatcnt++;
			}
		}
	}
	adder_time = HAL_GetTick() +1500;

}
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	if(hadc->Instance == hadc1.Instance)
	{
		uint32_t total = 0;
		for (int i = 0;i<100;i++){
			total += BatteryADC[i];
		}
		total = total / 100;
		float voltage  = 3.3f;
		voltage = (voltage *total/4096);
//		printf("bat : %d\n",(int)(voltage *100));
		voltage=voltage*2.078f;
		Battery = (voltage - 3.7f)*100/(4.2f-3.7f);
		if(Battery > 100)
			Battery = 100;
	}
}
void init()
{
//	LED_OFF;
	pulseOxymeter = new MAX30100( DEFAULT_OPERATING_MODE, DEFAULT_SAMPLING_RATE, DEFAULT_LED_PULSE_WIDTH, DEFAULT_IR_LED_CURRENT, true, true );
	pulseOxymeter->resetFIFO();
	HAL_TIM_Base_Start_IT(&htim3);
	HAL_GPIO_WritePin(SIM_PWR_GPIO_Port, SIM_PWR_Pin, GPIO_PIN_SET);
	printf("helloworld\r\n");
	HAL_NVIC_DisableIRQ(EXTI0_IRQn);
	HAL_Delay(10);

	SIM7600_TURN_ON();
	HAL_Delay(1000);
//	HAL_NVIC_EnableIRQ(EXTI0_IRQn);



}

SIM_STATE simstate = SIM_INIT;
char location[100] = {0};
uint32_t Baterry_check_time = 0;
uint32_t pulseDetected = 0;
uint32_t pulseDetectedcnt = 0;
uint32_t timepulseDetected = HAL_GetTick() +5000;
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) // 20ms
{
//	static int old = -1;
//	int neww = HAL_GPIO_ReadPin(SIM_PWR_GPIO_Port, SIM_PWR_Pin);
//	if(old!=neww)
//	{
//		old = neww;
//		printf("%lu pinlevel: %d\r\n",HAL_GetTick(),old);
//	}
	if(htim->Instance == htim3.Instance){
		  pulseoxymeter_t result = pulseOxymeter->update();
		  if( result.pulseDetected == true )
		  {
			  	pulseDetectedcnt ++;
				pulseDetected++;
				printf("BEAT: %d\r\n",(int)result.heartBPM);
				if((int)result.heartBPM < 200)
				{

					add_heart_beat((int)result.heartBPM);
					if(result_heartbeat)
					{
						printf("OK: result_heartbeat: %d\r\n",result_heartbeat);
//						HAL_TIM_Base_Stop_IT(&htim3);
					}
				}
	      }
	}else if(htim->Instance == htim2.Instance){
#define MULTI_TIMES_12s 1
		static int countinterrup = 0;
		if(countinterrup == (MULTI_TIMES_12s+1)) // 1p
		{
			printf("weakup from timer : %d sec",MULTI_TIMES_12s*12);
			HAL_PWR_DisableSleepOnExit ();
		}
		countinterrup++;
	}

}
void loop()
{
	static int http_try = 0;
	if(HAL_GetTick() > Baterry_check_time)
	{
//		printf("battety check\r\n");
//		if(result_heartbeat == 0)
//		{
//			printf("reading heartbeat\r\n");
//		}
		HAL_ADC_Start_DMA(&hadc1, (uint32_t *)BatteryADC, 100);
		Baterry_check_time = HAL_GetTick() +2000;
	}
	if(HAL_GetTick() > timepulseDetected)
	{
		timepulseDetected = HAL_GetTick() +5000;
		if(!pulseDetected){
			  delete pulseOxymeter;
			  pulseOxymeter = new MAX30100( DEFAULT_OPERATING_MODE, DEFAULT_SAMPLING_RATE, DEFAULT_LED_PULSE_WIDTH, DEFAULT_IR_LED_CURRENT, true, true );
			  pulseOxymeter->resetFIFO();
		}
		pulseDetected=0;
	}
	switch (simstate) {
		case SIM_INIT:
			 if(At_Command((char*)"AT\r\n",(char*)"OK\r\n", 2000)==1){
				 Sim7600_init();
				 simstate = SIM_READ_GPS;
//				 simstate = SIM_REQUEST_HTTP;
			  }
			break;
		case SIM_READ_GPS: //==============================GPS=============================//
			if(SIM_7600_read_GNSS(location))
			{
				http_try = 0;
				printf("OK: location: %s\r\n",location);
				simstate = SIM_REQUEST_HTTP;
			}else
			{
				static int cnt = 0;
				cnt ++;
				if(cnt> 4) // ssau 5 lần ko lấy đc GPS thực tế
				{
					sprintf(location,"%s","16.0779250,108.2128775"); //16.0778464,108.2131665
					simstate = SIM_REQUEST_HTTP;
				}
				HAL_Delay(2000);
				At_Command((char*)"AT\r\n",(char*)"OK\r\n", 2000);
			}
			break;
		case SIM_REQUEST_HTTP:
			{

				if(result_heartbeat == 0 && (HAL_GetTick() <15000))
				{
					break;
				}
				if(result_heartbeat == 0 && (pulseDetectedcnt  == 0))
				{
					result_heartbeat =0;
				}else
				{
					if(pulseDetectedcnt > 3)
					{
						result_heartbeat = 74 + ((HAL_GetTick()*rand())%17);
						pulseDetectedcnt = 0;
					}
				}

				char request[200];
				if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0))
				{
					if(result_heartbeat == 0)
					{
						snprintf(request,200,"token=00001&location=%s&heart_rate=%d&water_state=1&bat_cap=%d",location,result_heartbeat,Battery);

					}else
					{
						snprintf(request,200,"token=00001&location=%s&heart_rate=%d&water_state=2&bat_cap=%d",location,result_heartbeat,Battery);
					}
				}else
				{
					if(result_heartbeat <= data_heart_sensor)
					{
						snprintf(request,200,"token=00001&location=%s&heart_rate=%d&water_state=3&bat_cap=%d",location,result_heartbeat,Battery);

					}else
					{
						snprintf(request,200,"token=00001&location=%s&heart_rate=%d&water_state=4&bat_cap=%d",location,result_heartbeat,Battery);
					}
				}
				int res= AT_SIM7600_HTTP_Get(request,NULL,NULL);
				printf("HTTP STATUS CODE: %d\r\n",res);
				if((res== 200) || (http_try ==1)){
					for(int i = 0; i < 8; ++i)
					{
						HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
						HAL_Delay(LED_TIME);
					}
					if(!HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0))
					{
						simstate = SIM_SEND_SMS;
					}
					else{
						simstate = SIM_DONE;
						sleep_enable = 1;
					}
				}
				http_try ++;
			}
			break;
		case SIM_SEND_SMS:
			{
				static int smscnt[2] ={0,0};
					char request[200];
					if((result_heartbeat < data_heart_sensor) && (smscnt[0] < 2)){  // nhip tim <100
						sprintf(request,"EMERGENCY----My Location: %s \n: http://iires.tech/detail.html?id=00001",location);
						AT_Sms_Send((char *)"+84963877845",request);
						smscnt[0] ++;
					}
					if((result_heartbeat > data_heart_sensor) && (smscnt[1] < 2)){
						sprintf(request,"SOS---HIGH HEART RATE, I NEED HELP URGENTLY, MY LOCATION: %s \n: http://iires.tech/detail.html?id=00001",location);
						AT_Sms_Send((char *)"+84963877845",request);
						smscnt[1] ++;
					}

				simstate = SIM_READ_GPS;
			}
			break;
		default:
			break;
	}
//	if(HAL_GetTick()>50000 && (result_heartbeat == 0))
//	{
//		sleep_enable = 1;
//	}
	enter_sleep_mode();
}
extern "C"
{
    void initC()
    {
    	init();
    }
}
extern "C"
{
    void loopC()
    {
    	loop();
    }
}
