/*
  u8g2_stm32.c
  for NUCLEO-STM32F446 board
*/

#include "main.h"
#include "u8x8.h"

#ifdef HAL_SPI_MODULE_ENABLED
/*
 * HW SPI
 * PB4: CS(D5)
 * PB5: DC(D4)
 * PB6: RST(D10)
 * PA7: MOSI(D11)
 * PA5: SCK(D13)
 */
#define CS_GPIO_Port GPIOB
#define DC_GPIO_Port GPIOB
#define RST_GPIO_Port GPIOB
#define CS_Pin GPIO_PIN_4
#define DC_Pin GPIO_PIN_5
#define RST_Pin GPIO_PIN_6
extern SPI_HandleTypeDef hspi1;
#endif

#ifdef HAL_I2C_MODULE_ENABLED
/*
 * HW I2C
 * PB9: SDA(D14)
 * PB5: SCL(D15)
 */
extern I2C_HandleTypeDef hi2c1;
#endif

/*
 * SW I2C
 * PB4: Clock (D5)
 * PB5: Data (D4)
 */
#define SWI2C_CLK_ENABLE __HAL_RCC_GPIOB_CLK_ENABLE
#define SWI2C_GPIO_Port GPIOB
#define SWI2C_SDA_Pin GPIO_PIN_5
#define SWI2C_SCL_Pin GPIO_PIN_4

void u8g_delay_10us_stm32(uint8_t us);

#ifdef HAL_SPI_MODULE_ENABLED
uint8_t u8x8_gpio_and_delay_stm32_hw_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  switch(msg)
  {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
      /* no operation */
      break;
    case U8X8_MSG_DELAY_MILLI:
      HAL_Delay(arg_int);
      break;
    case U8X8_MSG_GPIO_CS:
      HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, arg_int);
      break;
    case U8X8_MSG_GPIO_DC:
      HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin, arg_int);
      break;
    case U8X8_MSG_GPIO_RESET:
      HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, arg_int);
      break;
    default:
      u8x8_SetGPIOResult(u8x8, 1);
      break;
  }
  return 1;
}

uint8_t u8x8_byte_stm32_hw_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  switch (msg)
  {
  case U8X8_MSG_BYTE_SEND:
    HAL_SPI_Transmit(&hspi1, (uint8_t *) arg_ptr, arg_int, 10000);
    break;
  case U8X8_MSG_BYTE_INIT:
    break;
  case U8X8_MSG_BYTE_SET_DC:
    u8x8_gpio_SetDC(u8x8, arg_int);
    break;
  case U8X8_MSG_BYTE_START_TRANSFER:
    u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_enable_level);
    HAL_Delay(1);
    break;
  case U8X8_MSG_BYTE_END_TRANSFER:
    HAL_Delay(1);
    u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
    break;
  default:
    return 0;
  }
  return 1;
}
#endif

#ifdef HAL_I2C_MODULE_ENABLED
uint8_t u8x8_gpio_and_delay_stm32_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  switch(msg)
  {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
      /* no operation */
      break;
    case U8X8_MSG_DELAY_MILLI:
      HAL_Delay(arg_int);
      break;
    case U8X8_MSG_DELAY_I2C:
      //HAL_Delay(1);
      /* arg_int is 1 or 4: 100KHz (5us) or 400KHz (1.25us) */
      //delay_micro_seconds(arg_int<=2?5:1);
      //u8g_delay_10us_stm32(arg_int);
      break;
    default:
      u8x8_SetGPIOResult(u8x8, 1);
      break;
  }
  return 1;
}

uint8_t u8x8_byte_stm32_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  static uint8_t buffer[32];	/* u8g2/u8x8 will never send more than 32 bytes between START_TRANSFER and END_TRANSFER */
  static uint8_t buf_idx;
  uint8_t *data;

  switch(msg)
  {
  case U8X8_MSG_BYTE_SEND:
    data = (uint8_t *)arg_ptr;
    while( arg_int > 0 )
    {
      buffer[buf_idx++] = *data;
      data++;
      arg_int--;
    }
    break;
  case U8X8_MSG_BYTE_INIT:
    /* add your custom code to init i2c subsystem */
    break;
  case U8X8_MSG_BYTE_SET_DC:
    /* ignored for i2c */
    break;
  case U8X8_MSG_BYTE_START_TRANSFER:
    buf_idx = 0;
    break;
  case U8X8_MSG_BYTE_END_TRANSFER:
    HAL_I2C_Master_Transmit(&hi2c1, u8x8_GetI2CAddress(u8x8), buffer, buf_idx, 10000);
    break;
  default:
    return 0;
  }
  return 1;
}
#endif

uint8_t u8x8_gpio_and_delay_stm32_sw_i2c(u8x8_t *u8x8, uint8_t msg,
        uint8_t arg_int, void *arg_ptr)
{
  GPIO_InitTypeDef gpio = {0};
  switch(msg)
  {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
      /* only support for software I2C*/
      SWI2C_CLK_ENABLE();
      __NOP();
      gpio.Pin = SWI2C_SDA_Pin|SWI2C_SCL_Pin;
      gpio.Mode = GPIO_MODE_OUTPUT_PP;
      gpio.Pull = GPIO_NOPULL;
      gpio.Speed = GPIO_SPEED_LOW;
      HAL_GPIO_Init(SWI2C_GPIO_Port, &gpio);
      break;
    case U8X8_MSG_DELAY_NANO:
      /* not required for SW I2C */
      break;
    case U8X8_MSG_DELAY_10MICRO:
      /* not used at the moment */
      break;
    case U8X8_MSG_DELAY_100NANO:
      /* not used at the moment */
      break;
    case U8X8_MSG_DELAY_MILLI:
      HAL_Delay(arg_int);
      break;
    case U8X8_MSG_DELAY_I2C:
      //HAL_Delay(1);
      /* arg_int is 1 or 4: 100KHz (5us) or 400KHz (1.25us) */
      //delay_micro_seconds(arg_int<=2?5:1);
      //u8g_delay_10us_stm32(arg_int);
      break;
    
    case U8X8_MSG_GPIO_I2C_CLOCK:
      if ( arg_int == 0 )
      {
        HAL_GPIO_WritePin(SWI2C_GPIO_Port, SWI2C_SCL_Pin, GPIO_PIN_RESET);
      }
      else
      {
        HAL_GPIO_WritePin(SWI2C_GPIO_Port, SWI2C_SCL_Pin, GPIO_PIN_SET);
      }
      break;
    case U8X8_MSG_GPIO_I2C_DATA:
      if ( arg_int == 0 )
      {
        HAL_GPIO_WritePin(SWI2C_GPIO_Port, SWI2C_SDA_Pin, GPIO_PIN_RESET);
      }
      else
      {
        HAL_GPIO_WritePin(SWI2C_GPIO_Port, SWI2C_SDA_Pin, GPIO_PIN_SET);
      }
      break;
    case U8X8_MSG_GPIO_MENU_SELECT:
      u8x8_SetGPIOResult(u8x8, HAL_GPIO_ReadPin(BUTTON_PLAY_GPIO_Port, BUTTON_PLAY_Pin));
      break;
    case U8X8_MSG_GPIO_MENU_DOWN:
      u8x8_SetGPIOResult(u8x8, HAL_GPIO_ReadPin(BUTTON_DOWN_GPIO_Port, BUTTON_DOWN_Pin));
      break;
    case U8X8_MSG_GPIO_MENU_UP:
      u8x8_SetGPIOResult(u8x8, HAL_GPIO_ReadPin(BUTTON_UP_GPIO_Port, BUTTON_UP_Pin));
      break;
    case U8X8_MSG_GPIO_MENU_HOME:
      u8x8_SetGPIOResult(u8x8, HAL_GPIO_ReadPin(BUTTON_STOP_GPIO_Port, BUTTON_STOP_Pin));
      break;
    default:
      u8x8_SetGPIOResult(u8x8, 1);
      break;
  }
  return 1;
}

void u8g_delay_10us_stm32(uint8_t us)
{
  // Core @90 MHZ: 11ns per instruction.
  // Divide ns / 22 (extra instruction for jump back to beginning of the loop) for loop cycles.
  // 10000 / 22 / 11 = 41.3

  for (uint32_t i = 0; i < (us * 41); i++)
  {
    __NOP();
  }
}
