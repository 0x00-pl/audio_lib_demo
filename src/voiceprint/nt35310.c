/* Copyright 2018 Canaan Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "nt35310.h"
#include "gpiohs.h"
#include "spi.h"
#include "fpioa.h"
#include "config.h"

static void  init_dcx(void)
{
    gpiohs_set_drive_mode(CONFIG_GPIO_LCD_SPI_DC_FUN-FUNC_GPIOHS0, GPIO_DM_OUTPUT);
    gpiohs_set_pin(CONFIG_GPIO_LCD_SPI_DC_FUN-FUNC_GPIOHS0, GPIO_PV_HIGH);
}

static void set_dcx_control(void)
{
    gpiohs_set_pin(CONFIG_GPIO_LCD_SPI_DC_FUN-FUNC_GPIOHS0, GPIO_PV_LOW);
}

static void set_dcx_data(void)
{
    gpiohs_set_pin(CONFIG_GPIO_LCD_SPI_DC_FUN-FUNC_GPIOHS0, GPIO_PV_HIGH);
}

void tft_hard_init(void)
{
    init_dcx();
    spi_init(CONFIG_LCD_SPI_CHANNEL, SPI_WORK_MODE_0, SPI_FF_OCTAL, 8, 0);
    spi_set_clk_rate(CONFIG_LCD_SPI_CHANNEL, 25000000);
}

void tft_write_command(uint8_t cmd)
{
    set_dcx_control();
    spi_init(CONFIG_LCD_SPI_CHANNEL, SPI_WORK_MODE_0, SPI_FF_OCTAL, 8, 0);
    spi_init_non_standard(CONFIG_LCD_SPI_CHANNEL, 8/*instrction length*/, 0/*address length*/, 0/*wait cycles*/,
                          SPI_AITM_AS_FRAME_FORMAT/*spi address trans mode*/);
    spi_send_data_normal_dma(LCD_DMA, CONFIG_LCD_SPI_CHANNEL, CONFIG_LCD_SPI_SLAVE_SELECT, (uint8_t *)(&cmd), 1,SPI_TRANS_CHAR);
}

void tft_write_byte(uint8_t *data_buf, uint32_t length)
{
    set_dcx_data();
    spi_init(CONFIG_LCD_SPI_CHANNEL, SPI_WORK_MODE_0, SPI_FF_OCTAL, 8, 0);
    spi_init_non_standard(CONFIG_LCD_SPI_CHANNEL, 8/*instrction length*/, 0/*address length*/, 0/*wait cycles*/,
                          SPI_AITM_AS_FRAME_FORMAT/*spi address trans mode*/);
    spi_send_data_normal_dma(LCD_DMA, CONFIG_LCD_SPI_CHANNEL, CONFIG_LCD_SPI_SLAVE_SELECT, data_buf, length, SPI_TRANS_CHAR);
}

void tft_write_half(uint16_t *data_buf, uint32_t length)
{
    set_dcx_data();
    spi_init(CONFIG_LCD_SPI_CHANNEL, SPI_WORK_MODE_0, SPI_FF_OCTAL, 16, 0);
    spi_init_non_standard(CONFIG_LCD_SPI_CHANNEL, 16/*instrction length*/, 0/*address length*/, 0/*wait cycles*/,
                          SPI_AITM_AS_FRAME_FORMAT/*spi address trans mode*/);
    spi_send_data_normal_dma(LCD_DMA, CONFIG_LCD_SPI_CHANNEL, CONFIG_LCD_SPI_SLAVE_SELECT,data_buf, length, SPI_TRANS_SHORT);
}

void tft_write_word(uint32_t *data_buf, uint32_t length, uint32_t flag)
{
    set_dcx_data();
    spi_init(CONFIG_LCD_SPI_CHANNEL, SPI_WORK_MODE_0, SPI_FF_OCTAL, 32, 0);

    spi_init_non_standard(CONFIG_LCD_SPI_CHANNEL, 0/*instrction length*/, 32/*address length*/, 0/*wait cycles*/,
                          SPI_AITM_AS_FRAME_FORMAT/*spi address trans mode*/);
    spi_send_data_normal_dma(LCD_DMA, CONFIG_LCD_SPI_CHANNEL, CONFIG_LCD_SPI_SLAVE_SELECT,data_buf, length, SPI_TRANS_INT);
}

void tft_fill_data(uint32_t *data_buf, uint32_t length)
{
    set_dcx_data();
    spi_init(CONFIG_LCD_SPI_CHANNEL, SPI_WORK_MODE_0, SPI_FF_OCTAL, 32, 0);
    spi_init_non_standard(CONFIG_LCD_SPI_CHANNEL, 0/*instrction length*/, 32/*address length*/, 0/*wait cycles*/,
                          SPI_AITM_AS_FRAME_FORMAT/*spi address trans mode*/);
    spi_fill_data_dma(LCD_DMA, CONFIG_LCD_SPI_CHANNEL, CONFIG_LCD_SPI_SLAVE_SELECT,data_buf, length);
}

