#include "mic_array.h"
#include <stddef.h>
#include <stdio.h>
#include <printf.h>
#include "audio_bf.h"
#include "config.h"
#include "i2s_capture.h"

#ifdef INCLUDE_MIC_ARRAY
#if AUDIO_BF_FFT_ENABLE
uint32_t AUDIO_BF_DIR_FFT_BUFFER[AUDIO_BF_DIR_CHANNEL_MAX]
				[AUDIO_BF_DIR_CHANNEL_SIZE]
	__attribute__((aligned(128)));
uint32_t AUDIO_BF_VOC_FFT_BUFFER[AUDIO_BF_VOC_CHANNEL_SIZE]
	__attribute__((aligned(128)));
#else
int16_t AUDIO_BF_DIR_BUFFER[AUDIO_BF_DIR_CHANNEL_MAX][AUDIO_BF_DIR_CHANNEL_SIZE]
	__attribute__((aligned(128)));
int16_t AUDIO_BF_VOC_BUFFER[AUDIO_BF_VOC_CHANNEL_SIZE]
	__attribute__((aligned(128)));
#endif

static void mic_array_detect_and_set_dir(void)
{
//	printf("%s\n", __func__);
	uint32_t dir_sum = 0;
	int32_t dir_max = 0;
	uint16_t contex = 0;

	for (size_t ch = 0; ch < AUDIO_BF_DIR_CHANNEL_MAX; ch++) { //
            dir_sum = 0;
			for (size_t i = 0; i < AUDIO_BF_DIR_CHANNEL_SIZE; i++) { //
				 dir_sum += (AUDIO_BF_DIR_BUFFER[ch][i] * AUDIO_BF_DIR_BUFFER[ch][i]) >> 5;
			}
			if(dir_sum > dir_max){
				dir_max = dir_sum;
				contex = ch;
			}
			//printf("%d	", dir_sum);
		}
		//printf("   %d\r\n", contex);
		//printf("\r\n");
    audio_bf_voc_set_direction(contex);
	audio_bf_dir_enable();
	return;
}

/*total buffer*/
#define SPEECH_REGCOGNIZE_MAX_SAMPLES (AUDIO_BF_DIR_CHANNEL_MAX*CONFIG_I2S_INTERRUPT_FRAME_LEN)
int16_t g_speech_samples_buffer[SPEECH_REGCOGNIZE_MAX_SAMPLES];


int mic_array_int_audio_bf(void *ctx)
{
	struct audio_bf_int_stat_t rdy_reg = audio_bf->bf_int_stat_reg;

	if (rdy_reg.dir_search_data_rdy) {
		audio_bf_dir_clear_int_state();

#if AUDIO_BF_FFT_ENABLE
		static int ch;

		ch = (ch + 1) % 16;
		for (uint32_t i = 0; i < 512; i++) { //
			uint32_t data = audio_bf->sobuf_dma_rdata;

			AUDIO_BF_DIR_FFT_BUFFER[ch][i] = data;
		}
#else
		for (uint32_t ch = 0; ch < AUDIO_BF_DIR_CHANNEL_MAX; ch++) {
			for (uint32_t i = 0; i < 256; i++) { //
				uint32_t data = audio_bf->sobuf_dma_rdata;

				AUDIO_BF_DIR_BUFFER[ch][i * 2 + 0] =
					data & 0xffff;
				AUDIO_BF_DIR_BUFFER[ch][i * 2 + 1] =
					(data >> 16) & 0xffff;
			}
		}
#endif
        mic_array_detect_and_set_dir();

	} else if (rdy_reg.voc_buf_data_rdy) {
		audio_bf_voc_clear_int_state();

#if AUDIO_BF_FFT_ENABLE
		for (uint32_t i = 0; i < 512; i++) { //
			uint32_t data = audio_bf->vobuf_dma_rdata;

			AUDIO_BF_VOC_FFT_BUFFER[i] = data;
		}
#else
		for (uint32_t i = 0; i < 256; i++) { //
			uint32_t data = audio_bf->vobuf_dma_rdata;

			AUDIO_BF_VOC_BUFFER[i * 2 + 0] = data & 0xffff;
			AUDIO_BF_VOC_BUFFER[i * 2 + 1] = (data >> 16) & 0xffff;
		}

        i2s_capture(AUDIO_BF_VOC_BUFFER,AUDIO_BF_VOC_CHANNEL_SIZE);
#endif
	} else { //
		printf("[waring]: unknown %s interrupt cause.\r\n", __func__);
	}
	return 0;
}

#if AUDIO_BF_DMA_ENABLE

static void dmac_chanel_interrupt_clear(dmac_channel_number_t channel_num)
{
    writeq(0xffffffff, &dmac->channel[channel_num].intclear);
}

int int_audio_bf_dir_dma(void *ctx)
{
	uint64_t chx_intstatus =
		dmac->channel[AUDIO_BF_DIR_DMA_CHANNEL].intstatus;
	if (chx_intstatus & 0x02) {
		dmac_chanel_interrupt_clear(AUDIO_BF_DIR_DMA_CHANNEL);

#if AUDIO_BF_FFT_ENABLE
		static int ch;

		ch = (ch + 1) % 16;
		dmac->channel[AUDIO_BF_DIR_DMA_CHANNEL].dar =
			(uint64_t)AUDIO_BF_DIR_FFT_BUFFER[ch];
#else
		dmac->channel[AUDIO_BF_DIR_DMA_CHANNEL].dar =
			(uint64_t)AUDIO_BF_DIR_BUFFER;
#endif

		dmac->chen = 0x0101 << AUDIO_BF_DIR_DMA_CHANNEL;
	} else {
		printf("[warning] unknown dma interrupt. %lx %lx\r\n",
		       dmac->intstatus, dmac->com_intstatus);
		printf("dir intstatus: %lx\r\n", chx_intstatus);

		dmac_chanel_interrupt_clear(AUDIO_BF_DIR_DMA_CHANNEL);
	}
	return 0;
}


int int_audio_bf_voc_dma(void *ctx)
{
	uint64_t chx_intstatus =
		dmac->channel[AUDIO_BF_VOC_DMA_CHANNEL].intstatus;

	if (chx_intstatus & 0x02) {
		dmac_chanel_interrupt_clear(AUDIO_BF_VOC_DMA_CHANNEL);

#if AUDIO_BF_FFT_ENABLE
		dmac->channel[AUDIO_BF_VOC_DMA_CHANNEL].dar =
			(uint64_t)AUDIO_BF_VOC_FFT_BUFFER;
#else
		dmac->channel[AUDIO_BF_VOC_DMA_CHANNEL].dar =
			(uint64_t)AUDIO_BF_VOC_BUFFER;
#endif

		dmac->chen = 0x0101 << AUDIO_BF_VOC_DMA_CHANNEL;
	} else {
		printf("[warning] unknown dma interrupt. %lx %lx\r\n",
		       dmac->intstatus, dmac->com_intstatus);
		printf("voc intstatus: %lx\r\n", chx_intstatus);

		dmac_chanel_interrupt_clear(AUDIO_BF_VOC_DMA_CHANNEL);
	}
	return 0;
}
#endif

static void mic_array_init_fpioa(void)
{
	printf("init fpioa.\r\n");
	//fpioa_init();
	//fpioa_set_function(47, FUNC_GPIOHS4);
	fpioa_set_function(CONFIG_GPIO_MIC_ARRAY_D0_PIN, CONFIG_GPIO_MIC_ARRAY_IN_D0_FUN);
	fpioa_set_function(CONFIG_GPIO_MIC_ARRAY_D1_PIN, CONFIG_GPIO_MIC_ARRAY_IN_D1_FUN);
	fpioa_set_function(CONFIG_GPIO_MIC_ARRAY_D2_PIN, CONFIG_GPIO_MIC_ARRAY_IN_D2_FUN);
	fpioa_set_function(CONFIG_GPIO_MIC_ARRAY_D3_PIN, CONFIG_GPIO_MIC_ARRAY_IN_D3_FUN);
	fpioa_set_function(CONFIG_GPIO_MIC_ARRAY_WS_PIN, CONFIG_GPIO_MIC_ARRAY_WS_FUN);
	fpioa_set_function(CONFIG_GPIO_MIC_ARRAY_SCLK_PIN, CONFIG_GPIO_MIC_ARRAY_SCLK_FUN);
#ifdef INCLUDE_PLAYBACK
            /*playback*/
    fpioa_set_function(CONFIG_GPIO_PLAYBACK_D1_PIN, CONFIG_GPIO_PLAYBACK_D1_FUN);
    fpioa_set_function(CONFIG_GPIO_PLAYBACK_SCLK_PIN, CONFIG_GPIO_PLAYBACK_SCLK_FUN);
    fpioa_set_function(CONFIG_GPIO_PLAYBACK_WS_PIN, CONFIG_GPIO_PLAYBACK_WS_FUN);
#endif
}


static void mic_array_init_i2s(void)
{
	printf("init i2s.\r\n");

	/* I2s init */
    i2s_init(CONFIG_MIC_ARRAY_I2S_DEVICE, I2S_RECEIVER, 0x3);

    i2s_rx_channel_config(CONFIG_MIC_ARRAY_I2S_DEVICE, I2S_CHANNEL_0,
            RESOLUTION_16_BIT, SCLK_CYCLES_32,
            TRIGGER_LEVEL_4, STANDARD_MODE);
    i2s_rx_channel_config(CONFIG_MIC_ARRAY_I2S_DEVICE, I2S_CHANNEL_1,
            RESOLUTION_16_BIT, SCLK_CYCLES_32,
            TRIGGER_LEVEL_4, STANDARD_MODE);
    i2s_rx_channel_config(CONFIG_MIC_ARRAY_I2S_DEVICE, I2S_CHANNEL_2,
            RESOLUTION_16_BIT, SCLK_CYCLES_32,
            TRIGGER_LEVEL_4, STANDARD_MODE);
    i2s_rx_channel_config(CONFIG_MIC_ARRAY_I2S_DEVICE, I2S_CHANNEL_3,
            RESOLUTION_16_BIT, SCLK_CYCLES_32,
            TRIGGER_LEVEL_4, STANDARD_MODE);

    i2s_set_sample_rate(CONFIG_MIC_ARRAY_I2S_DEVICE, CONFIG_I2S_SAMLPLE_RATE);
#ifdef INCLUDE_PLAYBACK
    i2s_init(CONFIG_I2S_PLAYBACK_DEVICE, I2S_TRANSMITTER, 0xC);

    i2s_tx_channel_config(CONFIG_I2S_PLAYBACK_DEVICE, I2S_CHANNEL_1,
        RESOLUTION_16_BIT, SCLK_CYCLES_32,
        TRIGGER_LEVEL_4,
        RIGHT_JUSTIFYING_MODE
        );
    i2s_set_sample_rate(CONFIG_I2S_PLAYBACK_DEVICE, CONFIG_I2S_SAMLPLE_RATE);
#endif
}

void mic_array_init_bf(void)
{
	printf("init bf.\r\n");
	uint16_t fir_prev_t[] = {
		0x020b, 0x0401, 0xff60, 0xfae2, 0xf860, 0x0022,
		0x10e6, 0x22f1, 0x2a98, 0x22f1, 0x10e6, 0x0022,
		0xf860, 0xfae2, 0xff60, 0x0401, 0x020b,
	};
	uint16_t fir_post_t[] = {
		0xf649, 0xe59e, 0xd156, 0xc615, 0xd12c, 0xf732,
		0x2daf, 0x5e03, 0x7151, 0x5e03, 0x2daf, 0xf732,
		0xd12c, 0xc615, 0xd156, 0xe59e, 0xf649,
	};

	uint16_t fir_neg_one[] = {
		0x8000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	};

	uint16_t fir_common[] = {
		0x03c3, 0x03c3, 0x03c3, 0x03c3, 0x03c3, 0x03c3,
		0x03c3, 0x03c3, 0x03c3, 0x03c3, 0x03c3, 0x03c3,
		0x03c3, 0x03c3, 0x03c3, 0x03c3, 0x03c3,
	};
//3cm
	uint8_t offsets[16][8] = {
		{0, 3, 7, 7, 3, 4, 0, 0},{0, 1, 6, 7, 4, 4, 0, 0},
		{1, 0, 4, 7, 5, 3, 0, 0},{2, 0, 3, 7, 7, 4, 0, 0},
		{4, 0, 1, 6, 7, 4, 0, 0},{5, 0, 0, 4, 7, 3, 0, 0},
		{7, 2, 0, 3, 7, 4, 0, 0},{7, 3, 0, 2, 6, 4, 0, 0},
		{7, 4, 0, 0, 4, 3, 0, 0},{7, 6, 2, 0, 3, 4, 0, 0},
		{7, 7, 3, 0, 2, 4, 0, 0},{5, 7, 4, 0, 0, 3, 0, 0},
		{4, 7, 6, 1, 0, 4, 0, 0},{2, 7, 7, 3, 0, 4, 0, 0},
		{1, 5, 7, 4, 0, 3, 0, 0},{0, 4, 7, 6, 1, 4,  0, 0}
	};
/*//5cm
	uint8_t offsets[16][8] = {
		{0, 4, 12, 12, 4, 4, 0, 0},	{0, 2, 9, 12, 7, 3, 0, 0},
		{1, 0, 7, 12, 9, 3, 0, 0},	{4, 0, 5, 12, 11, 4, 0, 0},
		{6, 0, 2, 10, 12, 4, 0, 0},	{8, 1, 0, 7, 12, 3, 0, 0},
		{11, 3, 0, 5, 12, 4, 0, 0},	{12, 6, 0, 3, 11, 4, 0, 0},
		{12, 7, 0, 0, 7, 3, 0, 0},	{12, 11, 3, 0, 6, 4, 0, 0},
		{11, 12, 5, 0, 3, 4, 0, 0},	{8, 12, 7, 0, 1, 3, 0, 0},
		{6, 12, 10, 2, 0, 4, 0, 0},	{4, 11, 12, 5, 0, 4, 0, 0},
		{1, 9, 12, 7, 0, 3, 0, 0},	{0, 7, 12, 9, 2, 3, 0, 0}
	};*/
//7cm
/*	uint8_t offsets[16][8] = {
		{0, 6, 16, 16, 6, 4, 0, 0},	{0, 2, 13, 17, 9, 3, 0, 0},
		{2, 0, 10, 17, 12, 3, 0, 0},{6, 0, 7, 17, 16, 4, 0, 0},
		{9, 0, 3, 14, 17, 3, 0, 0},	{11, 1, 0, 10, 17, 3, 0, 0},
		{15, 5, 0, 8, 17, 4, 0, 0},	{17, 8, 0, 4, 15, 4, 0, 0},
		{16, 10, 0, 0, 10, 2, 0, 0},{17, 15, 4, 0, 8, 4, 0, 0},
		{15, 17, 8, 0, 5, 4, 0, 0},	{11, 17, 10, 0, 1, 3, 0, 0},
		{9, 17, 14, 3, 0, 3, 0, 0},	{6, 16, 17, 7, 0, 4, 0, 0},
		{2, 12, 17, 10, 0, 3, 0, 0},{0, 9, 17, 13, 2, 3, 0, 0}
	};*/

	audio_bf_dir_set_prev_fir(fir_common);
	audio_bf_dir_set_post_fir(fir_common);
	audio_bf_voc_set_prev_fir(fir_common);
	audio_bf_voc_set_post_fir(fir_common);

	for (size_t i = 0; i < 16; i++) { //
		audio_bf_set_direction_delay(i, offsets[i]);
	}

	audio_bf_set_smpl_shift(AUDIO_BF_SMPL_SHIFT);
	audio_bf_voc_set_saturation_limit(AUDIO_BF_SATURATION_VPOS_DEBUG,
					  AUDIO_BF_SATURATION_VNEG_DEBUG);
	audio_bf_set_audio_gain(AUDIO_BF_AUDIO_GAIN_TEST);
	audio_bf_voc_set_direction(0);
	audio_bf_set_channel_enabled(0x3f);
	audio_bf_set_down_size(0, 0);

#if AUDIO_BF_FFT_ENABLE
	audio_bf_set_fft_shift_factor(1, 0xaa);
#else
	audio_bf_set_fft_shift_factor(0, 0);
#endif

	audio_bf_set_interrupt_mask(AUDIO_BF_DMA_ENABLE, AUDIO_BF_DMA_ENABLE);
#if AUDIO_BF_DIR_ENABLE
	audio_bf_dir_enable();
#endif
#if AUDIO_BF_VOC_ENABLE
	audio_bf_voc_enable(1);
#else
	audio_bf_voc_enable(0);
#endif
}

#if AUDIO_BF_DMA_ENABLE
static void mic_array_init_dma(void)
{
#if 0
	printf("%s\r\n", __func__);
	// dmac enable dmac and interrupt
	dmac_cfg_u_t dmac_cfg;

	dmac_cfg.data = readq(&dmac->cfg);
	dmac_cfg.cfg.dmac_en = 1;
	dmac_cfg.cfg.int_en = 1;
	writeq(dmac_cfg.data, &dmac->cfg);
#endif
	sysctl_dma_select(SYSCTL_DMA_CHANNEL_0 + AUDIO_BF_DIR_DMA_CHANNEL,
			  SYSCTL_DMA_SELECT_I2S0_BF_DIR_REQ);
	sysctl_dma_select(SYSCTL_DMA_CHANNEL_0 + AUDIO_BF_VOC_DMA_CHANNEL,
			  SYSCTL_DMA_SELECT_I2S0_BF_VOICE_REQ);
}
#endif

static void mic_array_init_dma_ch(int ch, volatile uint32_t *src_reg, void *buffer,
		 size_t size_of_byte)
{
	printf("%s %d\r\n", __func__, ch);

	dmac->channel[ch].sar = (uint64_t)src_reg;
	dmac->channel[ch].dar = (uint64_t)buffer;
	dmac->channel[ch].block_ts = (size_of_byte / 4) - 1;
	dmac->channel[ch].ctl =
		(((uint64_t)1 << 47) | ((uint64_t)15 << 48)
		 | ((uint64_t)1 << 38) | ((uint64_t)15 << 39)
		 | ((uint64_t)3 << 18) | ((uint64_t)3 << 14)
		 | ((uint64_t)2 << 11) | ((uint64_t)2 << 8) | ((uint64_t)0 << 6)
		 | ((uint64_t)1 << 4) | ((uint64_t)1 << 2) | ((uint64_t)1));
	/*
	 * dmac->channel[ch].ctl = ((  wburst_len_en  ) |
	 *                        (    wburst_len   ) |
	 *                        (  rburst_len_en  ) |
	 *                        (    rburst_len   ) |
	 *                        (one transaction:d) |
	 *                        (one transaction:s) |
	 *                        (    dst width    ) |
	 *                        (    src width   ) |
	 *                        (    dinc,0 inc  )|
	 *                        (  sinc:1,no inc ));
	 */

	dmac->channel[ch].cfg = (((uint64_t)1 << 49) | ((uint64_t)ch << 44)
				 | ((uint64_t)ch << 39) | ((uint64_t)2 << 32));
	/*
	 * dmac->channel[ch].cfg = ((     prior       ) |
	 *                         (      dst_per    ) |
	 *                         (     src_per     )  |
	 *           (    peri to mem  ));
	 *  01: Reload
	 */

	dmac->channel[ch].intstatus_en = 0x2; // 0xFFFFFFFF;
	dmac->channel[ch].intclear = 0xFFFFFFFF;

	dmac->chen = 0x0101 << ch;
}


static void mic_array_init_interrupt(void)
{
	plic_init();
	// bf
	plic_set_priority(IRQN_I2S0_INTERRUPT, 4);
	plic_irq_enable(IRQN_I2S0_INTERRUPT);
	plic_irq_register(IRQN_I2S0_INTERRUPT, mic_array_int_audio_bf, NULL);

#if AUDIO_BF_DMA_ENABLE
	// dma
	plic_set_priority(IRQN_DMA0_INTERRUPT + AUDIO_BF_DIR_DMA_CHANNEL, 4);
	plic_irq_register(IRQN_DMA0_INTERRUPT + AUDIO_BF_DIR_DMA_CHANNEL,
			  int_audio_bf_dir_dma, NULL);
	plic_irq_enable(IRQN_DMA0_INTERRUPT + AUDIO_BF_DIR_DMA_CHANNEL);
	// dma
	plic_set_priority(IRQN_DMA0_INTERRUPT + AUDIO_BF_VOC_DMA_CHANNEL, 4);
	plic_irq_register(IRQN_DMA0_INTERRUPT + AUDIO_BF_VOC_DMA_CHANNEL,
			  int_audio_bf_voc_dma, NULL);
	plic_irq_enable(IRQN_DMA0_INTERRUPT + AUDIO_BF_VOC_DMA_CHANNEL);
#endif
}

void mic_array_init(void)
{
	mic_array_init_fpioa();
	mic_array_init_interrupt();
	mic_array_init_i2s();
	mic_array_init_bf();

	if (AUDIO_BF_DMA_ENABLE) {
		#if AUDIO_BF_DMA_ENABLE
		mic_array_init_dma();
		#endif
#if AUDIO_BF_FFT_ENABLE
		mic_array_init_dma_ch(AUDIO_BF_DIR_DMA_CHANNEL,
			    &audio_bf->sobuf_dma_rdata,
			    AUDIO_BF_DIR_FFT_BUFFER[0], 512 * 4);
		mic_array_init_dma_ch(AUDIO_BF_VOC_DMA_CHANNEL,
			    &audio_bf->vobuf_dma_rdata, AUDIO_BF_VOC_FFT_BUFFER,
			    512 * 4);
#else
		mic_array_init_dma_ch(AUDIO_BF_DIR_DMA_CHANNEL,
			    &audio_bf->sobuf_dma_rdata, AUDIO_BF_DIR_BUFFER,
			    512 * 16 * 2);
		mic_array_init_dma_ch(AUDIO_BF_VOC_DMA_CHANNEL,
			    &audio_bf->vobuf_dma_rdata, AUDIO_BF_VOC_BUFFER,
			    512 * 2);
#endif
	}
}
#endif
