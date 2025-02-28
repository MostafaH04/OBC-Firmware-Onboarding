#include "lm75bd.h"
#include "i2c_io.h"
#include "errors.h"

#include <stdint.h>
#include <string.h>
#include <math.h>

// Personal Defines ------------------
typedef enum{
  TEMP_REG  = 0x00U,
  CONF_REG  = 0x01U,
  THYST_REG = 0x02U,
  TOS_REG   = 0x03U
} pointer_value_type_t;

// ratio between temp value and temp
#define TEMP_CONVERSION 0.125f

// Personal Defines End --------------

/* LM75BD Registers (p.8) */
#define LM75BD_REG_CONF 0x01U  /* Configuration Register (R/W) */

error_code_t lm75bdInit(lm75bd_config_t *config) {
  error_code_t errCode;

  if (config == NULL) return ERR_CODE_INVALID_ARG;

  errCode = writeConfigLM75BD(config->devAddr, config->osFaultQueueSize, config->osPolarity,
                                         config->osOperationMode, config->devOperationMode);
  
  if (errCode != ERR_CODE_SUCCESS) return errCode;

  // Assume that the overtemperature and hysteresis thresholds are already set
  // Hysteresis: 75 degrees Celsius
  // Overtemperature: 80 degrees Celsius

  return ERR_CODE_SUCCESS;
}

error_code_t readTempLM75BD(uint8_t devAddr, float *temp) {
  /* Implement this driver function */
  // From datasheet:
  // First write over i2c pointer byte then restart
  // now read over i2c starting MSByte to LSByte
  
  // write:
  uint8_t pointerBuf = TEMP_REG;
  error_code_t i2c_error;
  i2c_error = i2cSendTo(devAddr, &pointerBuf, 1);
  if (i2c_error != ERR_CODE_SUCCESS)
  {
    return i2c_error;
  }

  // read:
  uint8_t tempBuf[2];
  i2c_error = i2cReceiveFrom(devAddr, &tempBuf, 2);
  if (i2c_error != ERR_CODE_SUCCESS)
  {
    return i2c_error;
  }
  tempBuf[1] = (tempBuf[1] & 0xE0U); // masking useless bits, cuz y not :)
  int16_t tempIn = tempBuf[0] << 8 | tempBuf[1];
  tempIn >>= 5;
   
  *temp = (float)tempIn * TEMP_CONVERSION;

  return ERR_CODE_SUCCESS;
}

#define CONF_WRITE_BUFF_SIZE 2U
error_code_t writeConfigLM75BD(uint8_t devAddr, uint8_t osFaultQueueSize, uint8_t osPolarity,
                                   uint8_t osOperationMode, uint8_t devOperationMode) {
  error_code_t errCode;

  // Stores the register address and data to be written
  // 0: Register address
  // 1: Data
  uint8_t buff[CONF_WRITE_BUFF_SIZE] = {0};

  buff[0] = LM75BD_REG_CONF;

  uint8_t osFaltQueueRegData = 0;
  switch (osFaultQueueSize) {
    case 1:
      osFaltQueueRegData = 0;
      break;
    case 2:
      osFaltQueueRegData = 1;
      break;
    case 4:
      osFaltQueueRegData = 2;
      break;
    case 6:
      osFaltQueueRegData = 3;
      break;
    default:
      return ERR_CODE_INVALID_ARG;
  }

  buff[1] |= (osFaltQueueRegData << 3);
  buff[1] |= (osPolarity << 2);
  buff[1] |= (osOperationMode << 1);
  buff[1] |= devOperationMode;

  errCode = i2cSendTo(LM75BD_OBC_I2C_ADDR, buff, CONF_WRITE_BUFF_SIZE);
  if (errCode != ERR_CODE_SUCCESS) return errCode;

  return ERR_CODE_SUCCESS;
}
