/**
 * Marlin 3D Printer Firmware
 *
 * Copyright (c) 2019 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 * Copyright (c) 2016 Bob Cousins bobcousins42@googlemail.com
 * Copyright (c) 2015-2016 Nico Tonnhofer wurstnase.reprap@gmail.com
 * Copyright (c) 2016 Victor Perez victor_pv@hotmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#if defined(ARDUINO_ARCH_STM32) && !defined(STM32GENERIC)

#include "../../inc/MarlinConfig.h"

#if ENABLED(EEPROM_SETTINGS) && EITHER(FLASH_EEPROM_EMULATION,SRAM_EEPROM_EMULATION)

#include "../shared/persistent_store_api.h"

#if NONE(EEPROM_EMULATED_WITH_SRAM, SPI_EEPROM, I2C_EEPROM)
  #include <EEPROM.h>
  static bool eeprom_data_written = false;
#endif

bool PersistentStore::access_start() {
  #if NONE(EEPROM_EMULATED_WITH_SRAM, SPI_EEPROM, I2C_EEPROM)
    eeprom_buffer_fill();
  #endif
  return true;
}

bool PersistentStore::access_finish() {
  #if NONE(EEPROM_EMULATED_WITH_SRAM, SPI_EEPROM, I2C_EEPROM)
    if (eeprom_data_written) {
      eeprom_buffer_flush();
      eeprom_data_written = false;
    }
  #endif
  return true;
}

bool PersistentStore::write_data(int &pos, const uint8_t *value, size_t size, uint16_t *crc) {
  while (size--) {
    uint8_t v = *value;

    // Save to either external EEPROM, program flash or Backup SRAM
    #if EITHER(SPI_EEPROM, I2C_EEPROM)
      // EEPROM has only ~100,000 write cycles,
      // so only write bytes that have changed!
      uint8_t * const p = (uint8_t * const)pos;
      if (v != eeprom_read_byte(p)) {
        eeprom_write_byte(p, v);
        if (eeprom_read_byte(p) != v) {
          SERIAL_ECHO_MSG(MSG_ERR_EEPROM_WRITE);
          return true;
        }
      }
    #elif DISABLED(EEPROM_EMULATED_WITH_SRAM)
      eeprom_buffered_write_byte(pos, v);
    #else
      *(__IO uint8_t *)(BKPSRAM_BASE + (uint8_t * const)pos) = v;
    #endif

    crc16(crc, &v, 1);
    pos++;
    value++;
  };
  #if NONE(EEPROM_EMULATED_WITH_SRAM, SPI_EEPROM, I2C_EEPROM)
    eeprom_data_written = true;
  #endif

  return false;
}

bool PersistentStore::read_data(int &pos, uint8_t* value, size_t size, uint16_t *crc, const bool writing) {
  do {
    // Read from either external EEPROM, program flash or Backup SRAM
    const uint8_t c = (
      #if EITHER(SPI_EEPROM, I2C_EEPROM)
        eeprom_read_byte((uint8_t*)pos)
      #elif DISABLED(EEPROM_EMULATED_WITH_SRAM)
        eeprom_buffered_read_byte(pos)
      #else
        (*(__IO uint8_t *)(BKPSRAM_BASE + ((uint8_t*)pos)))
      #endif
    );

    if (writing) *value = c;
    crc16(crc, &c, 1);
    pos++;
    value++;
  } while (--size);
  return false;
}

size_t PersistentStore::capacity() {
  #if EITHER(SPI_EEPROM, I2C_EEPROM)
    return E2END + 1;
  #elif DISABLED(EEPROM_EMULATED_WITH_SRAM)
    return E2END + 1;
  #else
    return 4096; // 4kB
  #endif
}

#endif // EEPROM_SETTINGS
#endif // ARDUINO_ARCH_STM32 && !STM32GENERIC
