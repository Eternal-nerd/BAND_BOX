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
	
	
	ESP_LOGI(TAG, "[ 1 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();

    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[ 2 ] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[3.1] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to read data from codec chip");
    i2s_stream_cfg_t i2s_cfg_read = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg_read.type = AUDIO_STREAM_READER;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg_read);

    ESP_LOGI(TAG, "[3.3] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s_read");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s_write");

    ESP_LOGI(TAG, "[3.4] Link it together [codec_chip]-->i2s_stream_reader-->i2s_stream_writer-->[codec_chip]");
    const char *link_tag[2] = {"i2s_read", "i2s_write"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);

    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    ESP_LOGI(TAG, "[ 6 ] Listen for all pipeline events");
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        /* Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) i2s_stream_writer
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }

    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, i2s_stream_reader);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(i2s_stream_writer);
	
	/*
	
	// audio settings?
	int RECORD_TIME_SECONDS = 30;
	int channel_format = I2S_CHANNEL_TYPE_ONLY_RIGHT;
	int sample_rate = 16000;

	
	ESP_LOGI(TAG, "start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
	// BOTH encoding and decoding audio signal
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "create audio pipeline for recording/playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);
	
	// read audio data
	ESP_LOGI(TAG, "create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg_read = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg_read.type = AUDIO_STREAM_READER;
	//i2s_cfg_read.multi_out_num = 1;
    //i2s_cfg_read.task_core = 1;
    i2s_cfg_read.chan_cfg.id = CODEC_ADC_I2S_PORT;
    //i2s_cfg_read.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    //i2s_cfg_read.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    i2s_cfg_read.std_cfg.clk_cfg.sample_rate_hz = sample_rate;
	i2s_stream_set_channel_type(&i2s_cfg_read, channel_format);
    i2s_stream_reader = i2s_stream_init(&i2s_cfg_read);

	// encode to wav
	ESP_LOGI(TAG, "create audio encoder to handle data");
    wav_encoder_cfg_t wav_cfg_encode = DEFAULT_WAV_ENCODER_CONFIG();
    wav_encoder = wav_encoder_init(&wav_cfg_encode);
	
	// NO WRITING FOR NOW
	// decode from wav
	ESP_LOGI(TAG, "create wav decoder to decode wav data");
    wav_decoder_cfg_t wav_cfg_decode = DEFAULT_WAV_DECODER_CONFIG();
    wav_decoder = wav_decoder_init(&wav_cfg_decode);

	// write audio data
	ESP_LOGI(TAG, "create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg_write = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg_write.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg_write);
    i2s_stream_set_clk(i2s_stream_writer, sample_rate, 16, 1);


	ESP_LOGI(TAG, "register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s_read");
    audio_pipeline_register(pipeline, wav_encoder, "wav_encode");
    //audio_pipeline_register(pipeline, wav_decoder, "wav_decode");
	//audio_pipeline_register(pipeline, i2s_stream_writer, "i2s_write");
	
	ESP_LOGI(TAG, "link it together [codec_chip]-->i2s_stream-->wav_encoder-->wav_decoder-->resample-->i2s_stream-->[codec_chip]");
    const char *link_tag[4] = {"i2s_read", "wav_encode", "wav_decode", "i2s_write"};
    audio_pipeline_link(pipeline, &link_tag[0], 4);
	
	ESP_LOGI(TAG, "link it together [codec_chip]-->i2s_stream-->wav_encoder-->LOGGING");
    const char *link_tag[2] = {"i2s_read", "wav_encode"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);
	
	//ESP_LOGI(TAG, "create ringbuf to link i2s");
    //ringbuf_handle_t rb = audio_element_get_output_ringbuf(el_raw_reader);
    //audio_element_set_multi_output_ringbuf(i2s_stream_reader, rb, 0);
	
	ESP_LOGI(TAG, "set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "listening event from pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "start audio_pipeline");
    audio_pipeline_run(pipeline);

	// FIXME TESTING
	ESP_LOGI(TAG, "passthrough audio for all pipeline events, record for %d Seconds", RECORD_TIME_SECONDS);
    int second_recorded = 0;
    while (1) {
		
		ESP_LOGW(TAG, "DATA FROM i2s reader element: %d", *audio_element_getdata(i2s_stream_reader));
		
		
        audio_event_iface_msg_t msg;
        if (audio_event_iface_listen(evt, &msg, 1000 / portTICK_RATE_MS) != ESP_OK) {
            second_recorded++;
            ESP_LOGI(TAG, "[ * ] Recording ... %d", second_recorded);
            if (second_recorded >= RECORD_TIME_SECONDS) {
                ESP_LOGI(TAG, "Finishing recording");
                audio_element_set_ringbuf_done(i2s_stream_reader);
            }
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)i2s_stream_reader
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }
	
    ESP_LOGI(TAG, "stop audio_pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, i2s_stream_reader);
    audio_pipeline_unregister(pipeline, wav_encoder);
    //audio_pipeline_unregister(pipeline, wav_decoder);
    //audio_pipeline_unregister(pipeline, i2s_stream_writer);

    //audio_pipeline_unregister(pipeline, resample);



    audio_pipeline_remove_listener(pipeline);


    audio_event_iface_destroy(evt);

    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(wav_encoder);
    //audio_element_deinit(wav_decoder);
    //audio_element_deinit(i2s_stream_writer);
    //audio_element_deinit(resample);
	*/
}
