/**
 * Wake Prompt Player Implementation
 * Plays a pre-recorded MP3 audio prompt from SD card when wake word is detected
 */

#include "wake_prompt.h"
#include "bsp_board_extra.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "mp3dec.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "wake_prompt";

#define WAKE_PROMPT_PATH "/sdcard/sounds/wake_prompt.mp3"
#define MAX_AUDIO_SIZE (64 * 1024) // 64KB max for wake prompt
#define PCM_BUFFER_SIZE (MAX_NCHAN * MAX_NSAMP * 2)

static uint8_t *audio_buffer = NULL;
static size_t audio_size = 0;
static HMP3Decoder mp3_decoder = NULL;
static bool is_initialized = false;

esp_err_t wake_prompt_init(void) {
  if (is_initialized) {
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Initializing wake prompt player...");

  // Try to load audio file from SD card
  FILE *f = fopen(WAKE_PROMPT_PATH, "rb");
  if (f == NULL) {
    ESP_LOGW(TAG, "Wake prompt file not found: %s", WAKE_PROMPT_PATH);
    ESP_LOGW(TAG, "Will use beep tone as fallback");
    return ESP_ERR_NOT_FOUND;
  }

  // Get file size
  fseek(f, 0, SEEK_END);
  audio_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (audio_size == 0 || audio_size > MAX_AUDIO_SIZE) {
    ESP_LOGE(TAG, "Invalid audio file size: %d bytes (max: %d)", audio_size,
             MAX_AUDIO_SIZE);
    fclose(f);
    return ESP_ERR_INVALID_SIZE;
  }

  // Allocate buffer
  audio_buffer = (uint8_t *)malloc(audio_size);
  if (audio_buffer == NULL) {
    ESP_LOGE(TAG, "Failed to allocate audio buffer");
    fclose(f);
    return ESP_ERR_NO_MEM;
  }

  // Read file
  size_t read = fread(audio_buffer, 1, audio_size, f);
  fclose(f);

  if (read != audio_size) {
    ESP_LOGE(TAG, "Failed to read audio file: %d/%d bytes", read, audio_size);
    free(audio_buffer);
    audio_buffer = NULL;
    return ESP_FAIL;
  }

  // Initialize MP3 decoder
  mp3_decoder = MP3InitDecoder();
  if (mp3_decoder == NULL) {
    ESP_LOGE(TAG, "Failed to initialize MP3 decoder");
    free(audio_buffer);
    audio_buffer = NULL;
    return ESP_ERR_NO_MEM;
  }

  is_initialized = true;
  ESP_LOGI(TAG, "Wake prompt loaded: %d bytes from %s", audio_size,
           WAKE_PROMPT_PATH);
  return ESP_OK;
}

esp_err_t wake_prompt_play(void) {
  if (!is_initialized || audio_buffer == NULL) {
    ESP_LOGW(TAG, "Wake prompt not initialized, using fallback beep");
    // Fallback to beep - caller should handle this
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Playing wake prompt...");

  // Allocate PCM buffer
  int16_t *pcm_buffer = (int16_t *)malloc(PCM_BUFFER_SIZE);
  if (pcm_buffer == NULL) {
    ESP_LOGE(TAG, "Failed to allocate PCM buffer");
    return ESP_ERR_NO_MEM;
  }

  // Ensure codec is unmuted
  bsp_extra_codec_mute_set(false);

  uint8_t *read_ptr = audio_buffer;
  int bytes_left = audio_size;
  int total_samples = 0;
  bool codec_configured = false;

  // Decode and play MP3 frames
  while (bytes_left > 0) {
    // Find sync word
    int offset = MP3FindSyncWord(read_ptr, bytes_left);
    if (offset < 0) {
      break;
    }

    read_ptr += offset;
    bytes_left -= offset;

    // Decode one MP3 frame
    int err = MP3Decode(mp3_decoder, &read_ptr, &bytes_left, pcm_buffer, 0);

    if (err == ERR_MP3_NONE) {
      // Get frame info
      MP3FrameInfo frame_info;
      MP3GetLastFrameInfo(mp3_decoder, &frame_info);

      // Configure codec on first frame
      if (!codec_configured) {
        ESP_LOGI(TAG, "Audio: %d Hz, %d ch", frame_info.samprate,
                 frame_info.nChans);
        bsp_extra_codec_set_fs(frame_info.samprate, 16,
                               (i2s_slot_mode_t)frame_info.nChans);
        codec_configured = true;
      }

      // Write PCM data to I2S
      size_t pcm_bytes = frame_info.outputSamps * sizeof(int16_t);
      size_t bytes_written = 0;
      esp_err_t ret =
          bsp_extra_i2s_write(pcm_buffer, pcm_bytes, &bytes_written, 0);

      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
        free(pcm_buffer);
        return ret;
      }

      total_samples += frame_info.outputSamps;

    } else if (err == ERR_MP3_INDATA_UNDERFLOW) {
      break;
    } else {
      ESP_LOGW(TAG, "MP3 decode error: %d", err);
      if (bytes_left > 0) {
        read_ptr++;
        bytes_left--;
      }
    }
  }

  free(pcm_buffer);
  ESP_LOGI(TAG, "Wake prompt playback complete: %d samples", total_samples);
  return ESP_OK;
}

bool wake_prompt_is_available(void) { return is_initialized; }
