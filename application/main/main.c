  // GENERAL INCLUDES
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// AUDIO INCLUDES
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "audio_sys.h"
#include "filter_resample.h"

// encoder/decoder?
#include "wav_encoder.h"
#include "wav_decoder.h"

// FILE INCLUDEs (mp3, fat filesystem, etc.)
#include "fatfs_stream.h"
#include "mp3_decoder.h"
#include "sdcard_list.h"
#include "sdcard_scan.h"

// interfaces/peripherals includes
#include "i2s_stream.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_touch.h"
#include "periph_button.h"
#include "periph_adc_button.h"
#include "board.h"

// maybe dont need
#include "input_key_service.h"

static const char *TAG = "BAND_BOX";

// global pipeline and i2s audio reader/writer handles, encoder/decoder
audio_pipeline_handle_t pipeline;
audio_element_handle_t i2s_stream_reader, i2s_stream_writer, wav_encoder, wav_decoder;

void app_main(void) {
	// set logging levels
	esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "app_main function invocation");
	
	// audio settings?
	int channel_format = I2S_CHANNEL_TYPE_ONLY_RIGHT;
	int sample_rate = 16000;

	
	ESP_LOGI(TAG, "start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
	// BOTH encoding and decoding audio signal
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "create audio pipeline for recording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);
	
	ESP_LOGI(TAG, "create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_cfg.chan_cfg.id = 1;

	i2s_stream_set_channel_type(&i2s_cfg, channel_format);
    i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = sample_rate;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

	ESP_LOGI(TAG, "create audio encoder to handle data");
    wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();
    wav_encoder = wav_encoder_init(&wav_cfg);

	ESP_LOGI(TAG, "register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline, wav_encoder, "wav");

}
