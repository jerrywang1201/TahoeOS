/* Private includes -----------------------------------------------------------*/
// includes
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

#include "main.h"
#include "stm32f4xx_it.h"
#include "rtc.h"
#include "usart.h"

#include "user_TasksInit.h"
#include "user_MessageSendTask.h"

#include "ui.h"
#include "ui_EnvPage.h"
#include "ui_HRPage.h"
#include "ui_SPO2Page.h"
#include "ui_HomePage.h"
#include "ui_DateTimeSetPage.h"
#include "ui_OffTimePage.h"

#include "HWDataAccess.h"
#include "version.h"

/* Private define ------------------------------------------------------------*/
#define BLE_PROTO_ERR_OK 0U
#define BLE_PROTO_ERR_FORMAT 1U
#define BLE_PROTO_ERR_LENGTH 2U
#define BLE_PROTO_ERR_CRC 3U
#define BLE_PROTO_ERR_CMD 4U
#define BLE_PROTO_ERR_PAYLOAD 5U

#define BLE_PROTO_CMD_MAX 24U
#define BLE_PROTO_PAYLOAD_MAX 96U

/* Private variables ---------------------------------------------------------*/
typedef struct
{
  RTC_DateTypeDef nowdate;
  RTC_TimeTypeDef nowtime;
  int8_t humi;
  int8_t temp;
  uint8_t HR;
  uint8_t SPO2;
  uint16_t stepNum;
} BLEMessage_t;

typedef struct
{
  uint8_t seq;
  char cmd[BLE_PROTO_CMD_MAX];
  char payload[BLE_PROTO_PAYLOAD_MAX];
} BleProtoReq_t;

static BLEMessage_t BLEMessage;

static struct
{
  RTC_DateTypeDef nowdate;
  RTC_TimeTypeDef nowtime;
} TimeSetMessage;

/* Private function prototypes -----------------------------------------------*/
static char *BleProto_Trim(char *text);
static uint8_t BleProto_ParseU32(const char *text, uint32_t *out);
static uint8_t BleProto_ParseHex16(const char *text, uint16_t *out);
static uint16_t BleProto_Crc16Ccitt(const uint8_t *data, uint16_t len);
static void BleProto_SendFrame(uint8_t seq, uint8_t ack, uint8_t code, const char *payload);
static uint8_t BleProto_ParseFrame(char *frame_text, BleProtoReq_t *req, uint8_t *error_code, uint8_t *error_seq);
static uint8_t BleProto_GetBleEnabled(void);
static void BleProto_BuildStatusPayload(char *payload, uint16_t payload_size);
static uint8_t BleProto_HandleSetCfg(const char *payload, char *resp_payload, uint16_t resp_size);
static void BleProto_HandleFrame(char *frame_text);
static void Legacy_HandleCommand(char *cmd_text);

void StrCMD_Get(uint8_t *str, uint8_t *cmd)
{
  uint8_t i = 0;
  if (str == NULL || cmd == NULL)
  {
    return;
  }
  while (str[i] != '\0' && str[i] != '=')
  {
    cmd[i] = str[i];
    i++;
  }
  cmd[i] = '\0';
}

// set time // OV+ST=20230629125555
uint8_t TimeFormat_Get(uint8_t *str)
{
  TimeSetMessage.nowdate.Year = (str[8] - '0') * 10 + str[9] - '0';
  TimeSetMessage.nowdate.Month = (str[10] - '0') * 10 + str[11] - '0';
  TimeSetMessage.nowdate.Date = (str[12] - '0') * 10 + str[13] - '0';
  TimeSetMessage.nowtime.Hours = (str[14] - '0') * 10 + str[15] - '0';
  TimeSetMessage.nowtime.Minutes = (str[16] - '0') * 10 + str[17] - '0';
  TimeSetMessage.nowtime.Seconds = (str[18] - '0') * 10 + str[19] - '0';
  if (TimeSetMessage.nowdate.Year > 0 && TimeSetMessage.nowdate.Year < 99 && TimeSetMessage.nowdate.Month > 0 &&
      TimeSetMessage.nowdate.Month <= 12 && TimeSetMessage.nowdate.Date > 0 && TimeSetMessage.nowdate.Date <= 31 &&
      TimeSetMessage.nowtime.Hours >= 0 && TimeSetMessage.nowtime.Hours <= 23 &&
      TimeSetMessage.nowtime.Minutes >= 0 && TimeSetMessage.nowtime.Minutes <= 59 &&
      TimeSetMessage.nowtime.Seconds >= 0 && TimeSetMessage.nowtime.Seconds <= 59)
  {
    RTC_SetDate(TimeSetMessage.nowdate.Year, TimeSetMessage.nowdate.Month, TimeSetMessage.nowdate.Date);
    RTC_SetTime(TimeSetMessage.nowtime.Hours, TimeSetMessage.nowtime.Minutes, TimeSetMessage.nowtime.Seconds);
    printf("TIMESETOK\r\n");
    return 1;
  }
  return 0;
}

static char *BleProto_Trim(char *text)
{
  char *start = text;
  char *end;
  if (start == NULL)
  {
    return NULL;
  }
  while (*start == ' ' || *start == '\r' || *start == '\n' || *start == '\t')
  {
    start++;
  }
  end = start + strlen(start);
  while (end > start && (end[-1] == ' ' || end[-1] == '\r' || end[-1] == '\n' || end[-1] == '\t'))
  {
    end--;
  }
  *end = '\0';
  return start;
}

static uint8_t BleProto_ParseU32(const char *text, uint32_t *out)
{
  char *endptr;
  unsigned long value;
  if (text == NULL || out == NULL || text[0] == '\0')
  {
    return 0;
  }
  value = strtoul(text, &endptr, 10);
  if (*endptr != '\0')
  {
    return 0;
  }
  *out = (uint32_t)value;
  return 1;
}

static uint8_t BleProto_ParseHex16(const char *text, uint16_t *out)
{
  char *endptr;
  unsigned long value;
  if (text == NULL || out == NULL || text[0] == '\0')
  {
    return 0;
  }
  value = strtoul(text, &endptr, 16);
  if (*endptr != '\0' || value > 0xFFFFUL)
  {
    return 0;
  }
  *out = (uint16_t)value;
  return 1;
}

static uint16_t BleProto_Crc16Ccitt(const uint8_t *data, uint16_t len)
{
  uint16_t crc = 0xFFFFU;
  uint16_t i;
  uint8_t bit;
  for (i = 0; i < len; i++)
  {
    crc ^= (uint16_t)data[i] << 8;
    for (bit = 0; bit < 8; bit++)
    {
      if (crc & 0x8000U)
      {
        crc = (crc << 1) ^ 0x1021U;
      }
      else
      {
        crc <<= 1;
      }
    }
  }
  return crc;
}

static void BleProto_SendFrame(uint8_t seq, uint8_t ack, uint8_t code, const char *payload)
{
  char body[200];
  char frame[240];
  const char *ack_text = ack ? "ACK" : "NACK";
  uint16_t body_len;
  uint16_t crc;
  int n;
  if (payload == NULL)
  {
    payload = "";
  }

  n = snprintf(body, sizeof(body), "%u|%s|%u|%s", (unsigned int)seq, ack_text, (unsigned int)code, payload);
  if (n < 0)
  {
    return;
  }
  body[sizeof(body) - 1] = '\0';
  body_len = (uint16_t)strlen(body);
  crc = BleProto_Crc16Ccitt((const uint8_t *)body, body_len);

  n = snprintf(frame, sizeof(frame), "#%03u|%s|%04X\r\n", (unsigned int)body_len, body, (unsigned int)crc);
  if (n < 0)
  {
    return;
  }
  frame[sizeof(frame) - 1] = '\0';
  HAL_UART_Transmit(&huart1, (uint8_t *)frame, (uint16_t)strlen(frame), 0xFFFFU);
}

static uint8_t BleProto_ParseFrame(char *frame_text, BleProtoReq_t *req, uint8_t *error_code, uint8_t *error_seq)
{
  char *len_sep;
  char *crc_sep;
  char *body;
  char *seq_sep;
  char *cmd_sep;
  char *seq_text;
  char *cmd_text;
  char *payload_text;
  uint32_t frame_len_u32 = 0;
  uint32_t seq_u32 = 0;
  uint16_t frame_crc = 0;
  uint16_t calc_crc = 0;
  uint8_t seq_found = 0;

  *error_code = BLE_PROTO_ERR_FORMAT;
  *error_seq = 0;
  if (frame_text == NULL || req == NULL)
  {
    return 0;
  }
  if (frame_text[0] != '#')
  {
    return 0;
  }

  len_sep = strchr(frame_text, '|');
  if (len_sep == NULL)
  {
    return 0;
  }
  *len_sep = '\0';
  if (!BleProto_ParseU32(frame_text + 1, &frame_len_u32) || frame_len_u32 > (HARDINT_RX_BUF_SIZE - 1U))
  {
    return 0;
  }

  body = len_sep + 1;
  crc_sep = strrchr(body, '|');
  if (crc_sep == NULL)
  {
    return 0;
  }
  *crc_sep = '\0';
  if (!BleProto_ParseHex16(crc_sep + 1, &frame_crc))
  {
    return 0;
  }

  seq_sep = strchr(body, '|');
  if (seq_sep != NULL)
  {
    char seq_buf[8];
    uint16_t seq_len = (uint16_t)(seq_sep - body);
    if (seq_len < sizeof(seq_buf))
    {
      memcpy(seq_buf, body, seq_len);
      seq_buf[seq_len] = '\0';
      if (BleProto_ParseU32(seq_buf, &seq_u32) && seq_u32 <= 255U)
      {
        *error_seq = (uint8_t)seq_u32;
        seq_found = 1;
      }
    }
  }

  if (frame_len_u32 != strlen(body))
  {
    *error_code = BLE_PROTO_ERR_LENGTH;
    return 0;
  }

  calc_crc = BleProto_Crc16Ccitt((const uint8_t *)body, (uint16_t)strlen(body));
  if (calc_crc != frame_crc)
  {
    *error_code = BLE_PROTO_ERR_CRC;
    return 0;
  }

  seq_sep = strchr(body, '|');
  if (seq_sep == NULL)
  {
    return 0;
  }
  cmd_sep = strchr(seq_sep + 1, '|');
  if (cmd_sep == NULL)
  {
    return 0;
  }

  *seq_sep = '\0';
  *cmd_sep = '\0';
  seq_text = body;
  cmd_text = seq_sep + 1;
  payload_text = cmd_sep + 1;

  if (!BleProto_ParseU32(seq_text, &seq_u32) || seq_u32 > 255U)
  {
    return 0;
  }

  if (strlen(cmd_text) >= BLE_PROTO_CMD_MAX || strlen(payload_text) >= BLE_PROTO_PAYLOAD_MAX)
  {
    *error_code = BLE_PROTO_ERR_LENGTH;
    if (!seq_found)
    {
      *error_seq = (uint8_t)seq_u32;
    }
    return 0;
  }

  req->seq = (uint8_t)seq_u32;
  strcpy(req->cmd, cmd_text);
  strcpy(req->payload, payload_text);
  *error_seq = req->seq;
  *error_code = BLE_PROTO_ERR_OK;
  return 1;
}

static uint8_t BleProto_GetBleEnabled(void)
{
  return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) == GPIO_PIN_SET ? 1U : 0U;
}

static void BleProto_BuildStatusPayload(char *payload, uint16_t payload_size)
{
  RTC_DateTypeDef nowdate;
  RTC_TimeTypeDef nowtime;
  HAL_RTC_GetTime(&hrtc, &nowtime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &nowdate, RTC_FORMAT_BIN);
  snprintf(payload, payload_size,
           "ver=%u.%u.%u,bat=%u,chg=%u,ble=%u,wrist=%u,app=%u,lt=%u,tt=%u,li=%u,st=%u,hr=%u,o2=%u,tp=%d,hm=%d,t=%02u%02u%02u,d=%02u%02u",
           (unsigned int)watch_version_major(), (unsigned int)watch_version_minor(),
           (unsigned int)watch_version_patch(), (unsigned int)HWInterface.Power.power_remain,
           (unsigned int)ChargeCheck(), (unsigned int)BleProto_GetBleEnabled(),
           (unsigned int)HWInterface.IMU.wrist_is_enabled, (unsigned int)ui_APPSy_EN,
           (unsigned int)ui_LTimeValue, (unsigned int)ui_TTimeValue, (unsigned int)ui_LightSliderValue,
           (unsigned int)HWInterface.IMU.Steps, (unsigned int)HWInterface.HR_meter.HrRate,
           (unsigned int)HWInterface.HR_meter.SPO2, (int)HWInterface.AHT21.temperature,
           (int)HWInterface.AHT21.humidity, (unsigned int)nowtime.Hours, (unsigned int)nowtime.Minutes,
           (unsigned int)nowtime.Seconds, (unsigned int)nowdate.Month, (unsigned int)nowdate.Date);
}

static uint8_t BleProto_HandleSetCfg(const char *payload, char *resp_payload, uint16_t resp_size)
{
  char payload_buf[BLE_PROTO_PAYLOAD_MAX];
  char *token;
  uint8_t has_update = 0;
  uint8_t need_save = 0;
  uint32_t value_u32 = 0;
  uint8_t new_ltime = ui_LTimeValue;
  uint8_t new_ttime = ui_TTimeValue;

  if (payload == NULL || payload[0] == '\0' || strlen(payload) >= sizeof(payload_buf))
  {
    return BLE_PROTO_ERR_PAYLOAD;
  }

  strcpy(payload_buf, payload);
  token = strtok(payload_buf, ",;");
  while (token != NULL)
  {
    char *eq = strchr(token, '=');
    char *key;
    char *value;
    if (eq == NULL)
    {
      return BLE_PROTO_ERR_PAYLOAD;
    }
    *eq = '\0';
    key = BleProto_Trim(token);
    value = BleProto_Trim(eq + 1);
    if (key == NULL || value == NULL || key[0] == '\0' || value[0] == '\0')
    {
      return BLE_PROTO_ERR_PAYLOAD;
    }
    if (!strcmp(key, "ble"))
    {
      if (!BleProto_ParseU32(value, &value_u32) || value_u32 > 1U)
      {
        return BLE_PROTO_ERR_PAYLOAD;
      }
      if (value_u32 == 1U)
      {
        HWInterface.BLE.Enable();
      }
      else
      {
        HWInterface.BLE.Disable();
      }
      has_update = 1;
    }
    else if (!strcmp(key, "wrist"))
    {
      if (!BleProto_ParseU32(value, &value_u32) || value_u32 > 1U)
      {
        return BLE_PROTO_ERR_PAYLOAD;
      }
      if (value_u32 == 1U)
      {
        HWInterface.IMU.WristEnable();
      }
      else
      {
        HWInterface.IMU.WristDisable();
      }
      has_update = 1;
      need_save = 1;
    }
    else if (!strcmp(key, "app"))
    {
      if (!BleProto_ParseU32(value, &value_u32) || value_u32 > 1U)
      {
        return BLE_PROTO_ERR_PAYLOAD;
      }
      ui_APPSy_EN = (uint8_t)value_u32;
      has_update = 1;
      need_save = 1;
    }
    else if (!strcmp(key, "light"))
    {
      if (!BleProto_ParseU32(value, &value_u32) || value_u32 > 100U)
      {
        return BLE_PROTO_ERR_PAYLOAD;
      }
      ui_LightSliderValue = (uint8_t)value_u32;
      HWInterface.LCD.SetLight(ui_LightSliderValue);
      has_update = 1;
    }
    else if (!strcmp(key, "ltoff"))
    {
      if (!BleProto_ParseU32(value, &value_u32) || value_u32 < 1U || value_u32 > 59U)
      {
        return BLE_PROTO_ERR_PAYLOAD;
      }
      new_ltime = (uint8_t)value_u32;
      has_update = 1;
    }
    else if (!strcmp(key, "ttoff"))
    {
      if (!BleProto_ParseU32(value, &value_u32) || value_u32 < 2U || value_u32 > 60U)
      {
        return BLE_PROTO_ERR_PAYLOAD;
      }
      new_ttime = (uint8_t)value_u32;
      has_update = 1;
    }
    else
    {
      return BLE_PROTO_ERR_PAYLOAD;
    }
    token = strtok(NULL, ",;");
  }

  if (!has_update || new_ltime >= new_ttime)
  {
    return BLE_PROTO_ERR_PAYLOAD;
  }

  ui_LTimeValue = new_ltime;
  ui_TTimeValue = new_ttime;

  if (need_save)
  {
    uint8_t save_msg = 1;
    osMessageQueuePut(DataSave_MessageQueue, &save_msg, 0, 1);
  }

  snprintf(resp_payload, resp_size, "ble=%u,wrist=%u,app=%u,lt=%u,tt=%u,li=%u", (unsigned int)BleProto_GetBleEnabled(),
           (unsigned int)HWInterface.IMU.wrist_is_enabled, (unsigned int)ui_APPSy_EN, (unsigned int)ui_LTimeValue,
           (unsigned int)ui_TTimeValue, (unsigned int)ui_LightSliderValue);
  return BLE_PROTO_ERR_OK;
}

static void BleProto_HandleFrame(char *frame_text)
{
  BleProtoReq_t req;
  uint8_t parse_err;
  uint8_t err_seq;
  char resp_payload[BLE_PROTO_PAYLOAD_MAX];
  uint8_t cfg_ret;

  if (!BleProto_ParseFrame(frame_text, &req, &parse_err, &err_seq))
  {
    BleProto_SendFrame(err_seq, 0, parse_err, "parse_error");
    return;
  }

  if (!strcmp(req.cmd, "GET_STATUS"))
  {
    BleProto_BuildStatusPayload(resp_payload, sizeof(resp_payload));
    BleProto_SendFrame(req.seq, 1, BLE_PROTO_ERR_OK, resp_payload);
  }
  else if (!strcmp(req.cmd, "SET_CFG"))
  {
    cfg_ret = BleProto_HandleSetCfg(req.payload, resp_payload, sizeof(resp_payload));
    if (cfg_ret == BLE_PROTO_ERR_OK)
    {
      BleProto_SendFrame(req.seq, 1, BLE_PROTO_ERR_OK, resp_payload);
    }
    else
    {
      BleProto_SendFrame(req.seq, 0, cfg_ret, "bad_payload");
    }
  }
  else
  {
    BleProto_SendFrame(req.seq, 0, BLE_PROTO_ERR_CMD, "unknown_cmd");
  }
}

static void Legacy_HandleCommand(char *cmd_text)
{
  if (!strcmp(cmd_text, "OV"))
  {
    printf("OK\r\n");
  }
  else if (!strcmp(cmd_text, "OV+VERSION"))
  {
    printf("VERSION=V%d.%d.%d\r\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
  }
  else if (!strcmp(cmd_text, "OV+SEND"))
  {
    HAL_RTC_GetTime(&hrtc, &(BLEMessage.nowtime), RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &BLEMessage.nowdate, RTC_FORMAT_BIN);
    BLEMessage.humi = HWInterface.AHT21.humidity;
    BLEMessage.temp = HWInterface.AHT21.temperature;
    BLEMessage.HR = HWInterface.HR_meter.HrRate;
    BLEMessage.SPO2 = HWInterface.HR_meter.SPO2;
    BLEMessage.stepNum = HWInterface.IMU.Steps;

    printf("data:%2d-%02d\r\n", BLEMessage.nowdate.Month, BLEMessage.nowdate.Date);
    printf("time:%02d:%02d:%02d\r\n", BLEMessage.nowtime.Hours, BLEMessage.nowtime.Minutes, BLEMessage.nowtime.Seconds);
    printf("humidity:%d%%\r\n", BLEMessage.humi);
    printf("temperature:%d\r\n", BLEMessage.temp);
    printf("Heart Rate:%d%%\r\n", BLEMessage.HR);
    printf("SPO2:%d%%\r\n", BLEMessage.SPO2);
    printf("Step today:%d\r\n", BLEMessage.stepNum);
  }
  // set time // OV+ST=20230629125555
  else if (strlen(cmd_text) == 20U)
  {
    uint8_t cmd[10];
    memset(cmd, 0, sizeof(cmd));
    StrCMD_Get((uint8_t *)cmd_text, cmd);
    if (ui_APPSy_EN && !strcmp((char *)cmd, "OV+ST"))
    {
      TimeFormat_Get((uint8_t *)cmd_text);
    }
  }
}

/**
  * @brief  send the message via BLE, use uart
  * @param  argument: Not used
  * @retval None
  */
void MessageSendTask(void *argument)
{
  while (1)
  {
    if (HardInt_uart_flag)
    {
      uint8_t idle_break = 0;
      uint16_t rx_len;
      char rx_buf[HARDINT_RX_BUF_SIZE + 1];
      char *cmd_text;

      HardInt_uart_flag = 0;
      rx_len = HardInt_receive_len;
      if (rx_len >= HARDINT_RX_BUF_SIZE)
      {
        rx_len = HARDINT_RX_BUF_SIZE - 1U;
      }
      memcpy(rx_buf, HardInt_receive_str, rx_len);
      rx_buf[rx_len] = '\0';
      memset(HardInt_receive_str, 0, sizeof(HardInt_receive_str));
      HardInt_receive_len = 0;

      osMessageQueuePut(IdleBreak_MessageQueue, &idle_break, 0, 1);

      cmd_text = BleProto_Trim(rx_buf);
      if (cmd_text[0] == '\0')
      {
        osDelay(10);
        continue;
      }

      if (cmd_text[0] == '#')
      {
        BleProto_HandleFrame(cmd_text);
      }
      else
      {
        printf("RecStr:%s\r\n", cmd_text);
        Legacy_HandleCommand(cmd_text);
      }
    }
    osDelay(1000);
  }
}

