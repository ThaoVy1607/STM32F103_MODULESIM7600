/*
 * sim7600.c
 *
 *  Created on: Apr 13, 2023
 *      Author: ADMIN
 */
#include <sim7600.hpp>
#include "main.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

extern UART_HandleTypeDef huart1;
uint8_t uart_rx[512];

int AT_Getstring_index(char *des,char *scr,char *key,int index)
{
   char *p;
   if(!(p = strstr(scr,key))) //tim kiem key
      return -1;
   uint16_t len = strlen(p);
   char *tmp = (char *)malloc(len);
   len = sprintf(tmp,",%s",p+2);
   uint8_t cnt=0;
   uint16_t i=0;
   //printf("%s\n",tmp);
   for(i=0;i<len;i++)
   {
      if(tmp[i] == ',')
      {
         if(cnt == index)
            break;
         cnt++;
      }
   }
   char *start= (tmp+i+1);
   if(i != len)
   {
      //printf("find match\n");
      uint8_t tot;
      if(!(p = strstr(start,",")))
      {
         if(!(p = strstr(start,"\r")))
         {
            tot = strlen(start);
         }
         else tot = strlen(start)-strlen(p);
      }
      else
        tot = strlen(start)-strlen(p);
      memcpy(des,start,tot);
      des[tot]=0;
      free(tmp);
      return 1;
   }
   free(tmp);
   printf("not find index\n" );
   return -3;
}
int AT_Getint_index(int *res,char *src,char *key,int index)
{
   char des[20];
   if(AT_Getstring_index(des,src,key,index)<0)
   {
      return-1;
   }
   *res = atoi(des);
   return *res;
}


void SIM7600_TURN_ON()
{
	printf("turn on module sim\r\n");
	HAL_GPIO_WritePin(SIM_PWR_GPIO_Port, SIM_PWR_Pin, GPIO_PIN_RESET);
	HAL_Delay(500);
	HAL_GPIO_WritePin(SIM_PWR_GPIO_Port, SIM_PWR_Pin, GPIO_PIN_SET);
}

void SIM7600_TURN_OFF()
{
	printf("turn off module sim\r\n");
	HAL_GPIO_WritePin(SIM_PWR_GPIO_Port, SIM_PWR_Pin, GPIO_PIN_RESET);
	HAL_Delay(4000);
	HAL_GPIO_WritePin(SIM_PWR_GPIO_Port, SIM_PWR_Pin, GPIO_PIN_SET);
}
int At_Command_response(char *cmd ,char *RSP1,char* rsp,int *len,uint32_t timeout)
{
	int res =-1;
	printf("TX: %s\n",cmd);
	memset(uart_rx,0,512);
	HAL_UART_Transmit(&huart1, (uint8_t *)cmd,strlen(cmd), 1000);
	uart_rx[0] = huart1.Instance->DR; // free rx buffer
	HAL_UART_Receive_DMA(&huart1, uart_rx, 512);
	timeout += HAL_GetTick();
	while(HAL_GetTick() < timeout)
	{
		if(strstr((char *)uart_rx,RSP1))
		{
			res = 1;
			break;
		}
	}
	uint32_t tt = HAL_GetTick() +3;
	uint32_t old_cnt = huart1.hdmarx->Instance->CNDTR;
	while(HAL_GetTick() < tt)
	{
		if(old_cnt != huart1.hdmarx->Instance->CNDTR)
		{
			old_cnt = huart1.hdmarx->Instance->CNDTR;
			tt = HAL_GetTick()+3;
		}
	}
	*len = 512 - huart1.hdmarx->Instance->CNDTR;
	memcpy(rsp,uart_rx,*len);
	HAL_UART_DMAStop(&huart1);
	if(res == 1){
		printf("RX=> %s=>OK\n",(char *)uart_rx);
	}
	else{
		printf("RX=> %s=>FAIL\n",(char *)uart_rx);
	}
	return res;
}
int At_Command(char *cmd ,char *RSP1,uint32_t timeout)
{
	int res =-1;
	printf("TX: %s\n",cmd);
	memset(uart_rx,0,512);
	HAL_UART_Transmit(&huart1, (uint8_t *)cmd,strlen(cmd), 1000);
	uart_rx[0] = huart1.Instance->DR; // free rx buffer
	HAL_UART_Receive_DMA(&huart1, uart_rx, 512);
	timeout += HAL_GetTick();
	while(HAL_GetTick() < timeout)
	{
		if(strstr((char *)uart_rx,RSP1))
		{
			res = 1;
			break;
		}
		else if(strstr((char *)uart_rx,"ERROR\r\n"))
		{
			break;
		}
	}
	uint32_t tt = HAL_GetTick() +3;
	uint32_t old_cnt = huart1.hdmarx->Instance->CNDTR;
	while(HAL_GetTick() < tt)
	{
		if(old_cnt != huart1.hdmarx->Instance->CNDTR)
		{
			old_cnt = huart1.hdmarx->Instance->CNDTR;
			tt = HAL_GetTick()+3;
		}
	}
	HAL_UART_DMAStop(&huart1);
	if(res == 1){
		printf("RX=> %s=>OK\n",(char *)uart_rx);
	}
	else{
		printf("RX=> %s=>FAIL\n",(char *)uart_rx);
	}
	return res;
}

void Sim7600_init()
{
	for(uint8_t i=0;i<10;i++)
	{
	  if(At_Command((char *)"ATE0\r\n",(char *)"OK\r\n",5000)>0)
		break;
	  HAL_Delay(20);
	}
	for(uint8_t i=0;i<10;i++)
	{
	  if(At_Command((char *)"AT+CSCS=\"GSM\"\r\n", (char *)"OK\r\n", 5000)>0)
		break;
	  HAL_Delay(20);
	}
	At_Command((char *)"AT+CGSOCKCONT=1,\"IP\",\"CMNET\"\r\n", (char *)"OK\r\n",1000);
//	At_Command((char *)"AT+CGPSURL=\"111.222.333.444:8888\"\r\n", (char *)"OK\r\n",1000);
//	At_Command((char *)"AT+CGPSSSL=0\r\n", (char *)"OK\r\n",1000);
	At_Command((char *)"AT+CGPS=1\r\n", (char *)"OK\r\n",1000);



	At_Command((char *)"AT+CMGF=1\r\n", (char *)"OK\r\n",1000);
	At_Command((char *)"AT+CREG?\r\n", (char *)"OK\r\n",1000);
	At_Command((char *)"AT+CSQ\r\n", (char *)"OK\r\n",1000);
	At_Command((char *)"AT+CGREG?\r\n", (char *)"OK\r\n",1000);

	At_Command((char *)"AT+CGPADDR\r\n", (char *)"OK\r\n",1000);
}
int AT_Sms_Send(char* input_number,char* msg)
{
  char aux_string[256] = {0};
  sprintf(aux_string,"AT+CMGS=\"%s\"\r\n", input_number);
  At_Command(aux_string,(char *)">",5000);     // Send the SMS number
  sprintf(aux_string,"%s%c",msg,26);
  printf("send content\n");
  return  At_Command(aux_string,(char *)"OK\r\n",180000);     // Send the SMS number
  return -1;
}

int AT_SIM7600_HTTP_Get(char * request_url,char *rsp,uint16_t *sizess)
{
	char buf[1024];
	int buflen;
//	char *para;
	int Status_Code = 0;


	At_Command((char *)"AT+HTTPINIT\r\n",(char *)"OK\r\n",10000);
	sprintf(buf,"AT+HTTPDATA=%d,1000\r\n",strlen(request_url));
	if(At_Command(buf,(char *)"DOWNLOAD",3000)!=1)
	{
		At_Command((char *)"AT+HTTPTERM\r\n",(char *)"OK\r\n",10000);
		return Status_Code;
	}
	At_Command(request_url,(char *)"OK\r\n",10000);
	At_Command((char *)"AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"\r\n",(char *)"OK\r\n",10000);
	sprintf(buf,"AT+HTTPPARA=\"URL\",\"http://iires.tech/data/form\"\r\n");
	At_Command(buf,(char *)"OK\r\n",10000);
	memset(buf,0,1024);
	At_Command_response((char *)"AT+HTTPACTION=1\r\n",(char *)"+HTTPACTION",buf,&buflen,10000);
	AT_Getint_index(&Status_Code,buf,(char *)": ",1);
	At_Command((char *)"AT+HTTPREAD=0,500\r\n",(char *)"OK\r\n",10000);
	At_Command((char *)"AT+HTTPTERM\r\n",(char *)"OK\r\n",10000);
	return Status_Code;
}
//+CGPSINFO: 16°04'58.6327"N 108°08'98.5204"E,170423,130954.0,34.9,0.0,0.0
//           16°04'35.0"N 108°08'59.8"E
int extractFloatValues(const char* sentence, float* value1, float* value2)
{
    const char* start = strchr(sentence, ':');  // Find the first occurrence of ':'
    if (start == NULL) {
        // fprintf(stderr, "Error: Invalid sentence format\n");
        return -1;
    }
    start++;  // Move past the ':' character

    char* end;
    *value1 = strtof(start, &end);  // Convert the first substring to float
    if (start == end) {
        // fprintf(stderr, "Error: Failed to extract float value 1\n");
        return -1;
    }

    start = strchr(end + 1, ',');  // Find the next occurrence of ','
    if (start == NULL) {
//        fprintf(stderr, "Error: Failed to extract float value 2\n");
        return -1;
    }
    start++;  // Move past the ',' character

    *value2 = strtof(start, &end);  // Convert the second substring to float
    if (start == end) {
//        fprintf(stderr, "Error: Failed to extract float value 2\n");
        return -1;
    }
    return 1;
}
#define FLT(x)       ((float)(x))
static float prv_parse_lat_long(float ll) {
    float  deg, min;

    deg = FLT((int)((int)ll / 100));       /* Get absolute degrees value, interested in integer part only */
    min = ll - (deg * FLT(100));           /* Get remaining part from full number, minutes */
    ll = deg + (min / FLT(60.0));          /* Calculate latitude/longitude */

    return ll;
}
int  SIM_7600_read_GNSS(char *Location)
{

	char rsp[200]={0};
	int len;
	if(At_Command_response((char*)"AT+CGPSINFO\r\n",(char*)"OK\r\n",rsp,&len, 2000)){
		char *start = strstr(rsp,": ");
		if(start)
		{
			float plat,plong=0.0f;
			if(extractFloatValues(rsp,&plat,&plong) == 1)
			{
				plat = prv_parse_lat_long(plat);
				plong = prv_parse_lat_long(plong);
				sprintf(Location,"%f,%f",plat,plong);
				return 1;
			}
		}
	 }
	return 0;
}
//int  SIM_7600_read_GNSS(char *Location)
//{
//	char rsp[200]={0};
//	int len;
//	if(At_Command_response((char*)"AT+CGPSINFO\r\n",(char*)"OK\r\n",rsp,&len, 2000)){
//		char *start = strstr(rsp,": ");
//		if(start)
//		{
//			float plat,plong=0.0f;
//			if(extractFloatValues(rsp,&plat,&plong) == 1)
//			{
//				sprintf(Location,"%f, %f",plat,plong);
//				return 1;
//			}
//		}
//	 }
//	return 0;
//}
