/* Private includes -----------------------------------------------------------*/
//includes
#include "string.h"
#include "stdio.h"

#include "main.h"
#include "stm32f4xx_it.h"
#include "rtc.h"

#include "user_TasksInit.h"
#include "user_MessageSendTask.h"

#include "ui.h"
#include "ui_EnvPage.h"
#include "ui_HRPage.h"
#include "ui_SPO2Page.h"
#include "ui_HomePage.h"
#include "ui_DateTimeSetPage.h"

#include "HWDataAccess.h"
#include "version.h"
#include "lcd.h"
#include "lcd_init.h"
#include "CST816.h"
#include "key.h"
#include "power.h"
#include "AHT21.h"
#include "SPL06_001.h"
#include "LSM303.h"
#include "em70x8.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
struct
{
	RTC_DateTypeDef nowdate;
	RTC_TimeTypeDef nowtime;
	int8_t humi;
	int8_t temp;
	uint8_t HR;
	uint8_t SPO2;
	uint16_t stepNum;
}BLEMessage;

struct
{
	RTC_DateTypeDef nowdate;
	RTC_TimeTypeDef nowtime;
}TimeSetMessage;

/* Private function prototypes -----------------------------------------------*/

static void Strip_CRLF(char *str)
{
  char *p = str;
  while (*p != '\0')
  {
    if (*p == '\r' || *p == '\n')
    {
      *p = '\0';
      break;
    }
    p++;
  }
}

static void ToUppercase(char *str)
{
  char *p = str;
  while (*p != '\0')
  {
    if (*p >= 'a' && *p <= 'z')
    {
      *p = (char)(*p - ('a' - 'A'));
    }
    p++;
  }
}

static void Test_SendResult(const char *name, const char *status, const char *detail)
{
  if (detail && detail[0] != '\0')
  {
    printf("TEST:%s:%s:%s\r\n", name, status, detail);
  }
  else
  {
    printf("TEST:%s:%s\r\n", name, status);
  }
}

static void Test_PrintList(void)
{
  printf("TEST:LIST:LCD,BACKLIGHT,TOUCH,KEY,IMU,STEP,ENV,COMPASS,BARO,HR,RTC,BAT,BLE,BUZZER,VIB,ALL\r\n");
}

static void Test_LCD(void)
{
  Test_SendResult("LCD", "START", "");
  LCD_Fill(0, 0, LCD_W, LCD_H, RED);
  HAL_Delay(150);
  LCD_Fill(0, 0, LCD_W, LCD_H, GREEN);
  HAL_Delay(150);
  LCD_Fill(0, 0, LCD_W, LCD_H, BLUE);
  HAL_Delay(150);
  LCD_Fill(0, 0, LCD_W, LCD_H, WHITE);
  LCD_ShowString(10, 10, (uint8_t *)"LCD TEST", BLACK, WHITE, 24, 0);
  HAL_Delay(200);
  Test_SendResult("LCD", "PASS", "");
}

static void Test_Backlight(void)
{
  uint8_t prev = ui_LightSliderValue;
  Test_SendResult("BACKLIGHT", "START", "");
  HWInterface.LCD.SetLight(0);
  HAL_Delay(150);
  HWInterface.LCD.SetLight(20);
  HAL_Delay(150);
  HWInterface.LCD.SetLight(60);
  HAL_Delay(150);
  HWInterface.LCD.SetLight(100);
  HAL_Delay(150);
  HWInterface.LCD.SetLight(prev);
  Test_SendResult("BACKLIGHT", "PASS", "");
}

static void Test_Touch(void)
{
  uint32_t start = HAL_GetTick();
  uint8_t id = 0;
  Test_SendResult("TOUCH", "START", "TOUCH_SCREEN");
  CST816_Init();
  CST816_Wakeup();
  id = CST816_Get_ChipID();
  if (id == 0x00 || id == 0xFF)
  {
    Test_SendResult("TOUCH", "FAIL", "NO_ID");
    return;
  }
  while (HAL_GetTick() - start < 5000)
  {
    if (CST816_Get_FingerNum() > 0)
    {
      Test_SendResult("TOUCH", "PASS", "");
      return;
    }
    osDelay(20);
  }
  Test_SendResult("TOUCH", "FAIL", "TIMEOUT");
}

static void Test_Key(void)
{
  uint32_t start = HAL_GetTick();
  Test_SendResult("KEY", "START", "PRESS_KEY");
  while (HAL_GetTick() - start < 5000)
  {
    if (!KEY1)
    {
      Test_SendResult("KEY", "PASS", "KEY1");
      return;
    }
    if (KEY2)
    {
      Test_SendResult("KEY", "PASS", "KEY2");
      return;
    }
    osDelay(20);
  }
  Test_SendResult("KEY", "FAIL", "TIMEOUT");
}

static void Test_IMU(void)
{
  uint8_t ret = HWInterface.IMU.Init();
  if (ret == 0)
  {
    Test_SendResult("IMU", "PASS", "");
  }
  else
  {
    Test_SendResult("IMU", "FAIL", "INIT");
  }
}

static void Test_Step(void)
{
  char buf[32];
  uint16_t steps = HWInterface.IMU.GetSteps();
  snprintf(buf, sizeof(buf), "STEPS=%u", steps);
  Test_SendResult("STEP", "PASS", buf);
}

static void Test_Env(void)
{
  float humi = 0.0f;
  float temp = 0.0f;
  char buf[48];
  AHT_Init();
  if (AHT_Read(&humi, &temp) == 0)
  {
    snprintf(buf, sizeof(buf), "H=%.1f,T=%.1f", humi, temp);
    Test_SendResult("ENV", "PASS", buf);
  }
  else
  {
    Test_SendResult("ENV", "FAIL", "READ");
  }
}

static void Test_Compass(void)
{
  uint8_t ret = LSM303DLH_Init();
  if (ret == 0)
  {
    Test_SendResult("COMPASS", "PASS", "");
  }
  else
  {
    Test_SendResult("COMPASS", "FAIL", "INIT");
  }
}

static void Test_Baro(void)
{
  uint8_t ret = SPL_init();
  uint8_t id = SPL_GetID();
  if (ret == 0 && id == SPL_CHIP_ID)
  {
    Test_SendResult("BARO", "PASS", "");
  }
  else
  {
    Test_SendResult("BARO", "FAIL", "INIT");
  }
}

static void Test_HR(void)
{
  uint8_t id = EM7028_Get_ID();
  uint8_t ret = EM7028_hrs_init();
  if (id == EM7028_ID && ret == 0)
  {
    Test_SendResult("HR", "PASS", "");
  }
  else
  {
    Test_SendResult("HR", "FAIL", "INIT");
  }
}

static void Test_RTC(void)
{
  RTC_DateTypeDef nowdate;
  RTC_TimeTypeDef nowtime;
  char buf[48];
  HAL_RTC_GetTime(&hrtc, &nowtime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &nowdate, RTC_FORMAT_BIN);
  snprintf(buf, sizeof(buf), "%02d-%02d %02d:%02d:%02d",
           nowdate.Month, nowdate.Date, nowtime.Hours, nowtime.Minutes, nowtime.Seconds);
  Test_SendResult("RTC", "PASS", buf);
}

static void Test_Bat(void)
{
  char buf[24];
  uint8_t bat = PowerCalculate();
  snprintf(buf, sizeof(buf), "PCT=%u", bat);
  Test_SendResult("BAT", "PASS", buf);
}

static void Test_BLE(void)
{
  Test_SendResult("BLE", "START", "");
  HWInterface.BLE.Init();
  HWInterface.BLE.Enable();
  HAL_Delay(200);
  HWInterface.BLE.Disable();
  Test_SendResult("BLE", "PASS", "");
}

static void Test_Unsupported(const char *name)
{
  Test_SendResult(name, "UNSUPPORTED", "");
}

static void Test_RunAll(void)
{
  Test_LCD();
  Test_Backlight();
  Test_Touch();
  Test_Key();
  Test_IMU();
  Test_Step();
  Test_Env();
  Test_Compass();
  Test_Baro();
  Test_HR();
  Test_RTC();
  Test_Bat();
  Test_BLE();
  Test_Unsupported("BUZZER");
  Test_Unsupported("VIB");
}

void StrCMD_Get(uint8_t * str,uint8_t * cmd)
{
	uint8_t i=0;
  while(str[i]!='=')
  {
      cmd[i] = str[i];
      i++;
  }
}

//set time//OV+ST=20230629125555
uint8_t TimeFormat_Get(uint8_t * str)
{
	TimeSetMessage.nowdate.Year = (str[8]-'0')*10+str[9]-'0';
	TimeSetMessage.nowdate.Month = (str[10]-'0')*10+str[11]-'0';
	TimeSetMessage.nowdate.Date = (str[12]-'0')*10+str[13]-'0';
	TimeSetMessage.nowtime.Hours = (str[14]-'0')*10+str[15]-'0';
	TimeSetMessage.nowtime.Minutes = (str[16]-'0')*10+str[17]-'0';
	TimeSetMessage.nowtime.Seconds = (str[18]-'0')*10+str[19]-'0';
	if(TimeSetMessage.nowdate.Year>0 && TimeSetMessage.nowdate.Year<99
		&& TimeSetMessage.nowdate.Month>0 && TimeSetMessage.nowdate.Month<=12
		&& TimeSetMessage.nowdate.Date>0 && TimeSetMessage.nowdate.Date<=31
		&& TimeSetMessage.nowtime.Hours>=0 && TimeSetMessage.nowtime.Hours<=23
		&& TimeSetMessage.nowtime.Minutes>=0 && TimeSetMessage.nowtime.Minutes<=59
		&& TimeSetMessage.nowtime.Seconds>=0 && TimeSetMessage.nowtime.Seconds<=59)
	{
		RTC_SetDate(TimeSetMessage.nowdate.Year, TimeSetMessage.nowdate.Month,TimeSetMessage.nowdate.Date);
		RTC_SetTime(TimeSetMessage.nowtime.Hours,TimeSetMessage.nowtime.Minutes,TimeSetMessage.nowtime.Seconds);
		printf("TIMESETOK\r\n");
	}

}

/**
  * @brief  send the message via BLE, use uart
  * @param  argument: Not used
  * @retval None
  */
void MessageSendTask(void *argument)
{
	while(1)
	{
		if(HardInt_uart_flag)
		{
			HardInt_uart_flag = 0;
			uint8_t IdleBreakstr = 0;
			osMessageQueuePut(IdleBreak_MessageQueue,&IdleBreakstr,NULL,1);
      HardInt_receive_str[24] = '\0';
      Strip_CRLF((char *)HardInt_receive_str);
      ToUppercase((char *)HardInt_receive_str);
			printf("RecStr:%s\r\n",HardInt_receive_str);
			if(!strcmp((char *)HardInt_receive_str,"OV"))
			{
				printf("OK\r\n");
			}
			else if(!strcmp((char *)HardInt_receive_str,"OV+VERSION"))
			{
				printf("VERSION=V%d.%d.%d\r\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
			}
			else if(!strcmp((char *)HardInt_receive_str,"OV+SEND"))
			{
				HAL_RTC_GetTime(&hrtc,&(BLEMessage.nowtime),RTC_FORMAT_BIN);
				HAL_RTC_GetDate(&hrtc,&BLEMessage.nowdate,RTC_FORMAT_BIN);
				BLEMessage.humi = HWInterface.AHT21.humidity;
				BLEMessage.temp = HWInterface.AHT21.temperature;
				BLEMessage.HR = HWInterface.HR_meter.HrRate;
				BLEMessage.SPO2 = HWInterface.HR_meter.SPO2;
				BLEMessage.stepNum = HWInterface.IMU.Steps;

				printf("data:%2d-%02d\r\n",BLEMessage.nowdate.Month,BLEMessage.nowdate.Date);
				printf("time:%02d:%02d:%02d\r\n",BLEMessage.nowtime.Hours,BLEMessage.nowtime.Minutes,BLEMessage.nowtime.Seconds);
				printf("humidity:%d%%\r\n",BLEMessage.humi);
				printf("temperature:%d\r\n",BLEMessage.temp);
				printf("Heart Rate:%d%%\r\n",BLEMessage.HR);
				printf("SPO2:%d%%\r\n",BLEMessage.SPO2);
				printf("Step today:%d\r\n",BLEMessage.stepNum);
			}
      else if(!strncmp((char *)HardInt_receive_str,"OV+TEST=",8))
      {
        char test_cmd[16];
        memset(test_cmd, 0, sizeof(test_cmd));
        strncpy(test_cmd, (char *)HardInt_receive_str + 8, sizeof(test_cmd) - 1);
        if (!strcmp(test_cmd, "LIST") || !strcmp(test_cmd, "HELP"))
        {
          Test_PrintList();
        }
        else if (!strcmp(test_cmd, "ALL"))
        {
          Test_RunAll();
        }
        else if (!strcmp(test_cmd, "LCD"))
        {
          Test_LCD();
        }
        else if (!strcmp(test_cmd, "BACKLIGHT"))
        {
          Test_Backlight();
        }
        else if (!strcmp(test_cmd, "TOUCH"))
        {
          Test_Touch();
        }
        else if (!strcmp(test_cmd, "KEY"))
        {
          Test_Key();
        }
        else if (!strcmp(test_cmd, "IMU"))
        {
          Test_IMU();
        }
        else if (!strcmp(test_cmd, "STEP"))
        {
          Test_Step();
        }
        else if (!strcmp(test_cmd, "ENV"))
        {
          Test_Env();
        }
        else if (!strcmp(test_cmd, "COMPASS"))
        {
          Test_Compass();
        }
        else if (!strcmp(test_cmd, "BARO"))
        {
          Test_Baro();
        }
        else if (!strcmp(test_cmd, "HR"))
        {
          Test_HR();
        }
        else if (!strcmp(test_cmd, "RTC"))
        {
          Test_RTC();
        }
        else if (!strcmp(test_cmd, "BAT"))
        {
          Test_Bat();
        }
        else if (!strcmp(test_cmd, "BLE"))
        {
          Test_BLE();
        }
        else if (!strcmp(test_cmd, "BUZZER"))
        {
          Test_Unsupported("BUZZER");
        }
        else if (!strcmp(test_cmd, "VIB"))
        {
          Test_Unsupported("VIB");
        }
        else
        {
          Test_SendResult(test_cmd, "UNKNOWN", "");
        }
      }
			//set time//OV+ST=20230629125555
			else if(strlen((char *)HardInt_receive_str)==20)
			{
				uint8_t cmd[10];
				memset(cmd,0,sizeof(cmd));
				StrCMD_Get(HardInt_receive_str,cmd);
				if(ui_APPSy_EN && !strcmp(cmd,"OV+ST"))
				{
					TimeFormat_Get(HardInt_receive_str);
				}
			}
			memset(HardInt_receive_str,0,sizeof(HardInt_receive_str));
		}
		osDelay(1000);
	}
}
