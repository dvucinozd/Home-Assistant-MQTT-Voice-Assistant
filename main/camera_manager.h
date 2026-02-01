/**
 * Camera Manager
 * OV5647 camera initialization and capture via MIPI-CSI
 */

#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Camera configuration
 */
typedef struct {
  int i2c_scl_pin; // SCCB I2C SCL pin (default: 8)
  int i2c_sda_pin; // SCCB I2C SDA pin (default: 7)
  int width;       // Frame width (default: 1280)
  int height;      // Frame height (default: 720)
  int fps;         // Frame rate (default: 30)
} camera_config_t;

/**
 * @brief JPEG frame buffer
 */
typedef struct {
  uint8_t *buf;      // JPEG data buffer
  size_t len;        // JPEG data length
  uint32_t width;    // Frame width
  uint32_t height;   // Frame height
  int64_t timestamp; // Capture timestamp (microseconds)
} camera_frame_t;

/**
 * @brief Initialize camera with default configuration
 * @return ESP_OK on success
 */
esp_err_t camera_manager_init(void);

/**
 * @brief Initialize camera with custom configuration
 * @param config Camera configuration
 * @return ESP_OK on success
 */
esp_err_t camera_manager_init_with_config(const camera_config_t *config);

/**
 * @brief Deinitialize camera
 * @return ESP_OK on success
 */
esp_err_t camera_manager_deinit(void);

/**
 * @brief Check if camera is initialized
 * @return true if initialized
 */
bool camera_manager_is_initialized(void);

/**
 * @brief Capture a JPEG frame
 * @param frame Pointer to frame buffer structure (will be filled)
 * @return ESP_OK on success
 * @note Caller must call camera_manager_return_frame() when done
 */
esp_err_t camera_manager_capture_jpeg(camera_frame_t *frame);

/**
 * @brief Return a captured frame buffer
 * @param frame Frame to return
 */
void camera_manager_return_frame(camera_frame_t *frame);

/**
 * @brief Get camera status string
 * @return Status string ("OK", "NOT_INIT", "ERROR")
 */
const char *camera_manager_get_status(void);

#ifdef __cplusplus
}
#endif

#endif // CAMERA_MANAGER_H
