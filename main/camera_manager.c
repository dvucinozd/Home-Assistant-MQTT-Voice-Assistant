/**
 * Camera Manager Implementation
 * OV5647 camera via MIPI-CSI using esp_video component
 */

#include "camera_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "esp_video_device.h"
#include "esp_video_init.h"
#include "linux/videodev2.h"


static const char *TAG = "camera_mgr";

// Camera state
static bool s_initialized = false;
static int s_cam_fd = -1;  // Camera capture device
static int s_jpeg_fd = -1; // JPEG encoder device
static SemaphoreHandle_t s_mutex = NULL;

// Buffer configuration
#define CAM_BUFFER_COUNT 2
static uint8_t *s_cam_buffer[CAM_BUFFER_COUNT] = {NULL};
static uint8_t *s_jpeg_buffer = NULL;
static size_t s_jpeg_buffer_size = 0;

// Default configuration
static camera_config_t s_config = {.i2c_scl_pin = 8,
                                   .i2c_sda_pin = 7,
                                   .width = 1280,
                                   .height = 720,
                                   .fps = 30};

// Forward declarations
static esp_err_t init_camera_device(void);
static esp_err_t init_jpeg_encoder(void);
static void cleanup_resources(void);

esp_err_t camera_manager_init(void) {
  return camera_manager_init_with_config(NULL);
}

esp_err_t camera_manager_init_with_config(const camera_config_t *config) {
  if (s_initialized) {
    ESP_LOGW(TAG, "Camera already initialized");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Initializing camera manager...");

  // Apply custom config if provided
  if (config) {
    s_config = *config;
  }

  // Create mutex
  s_mutex = xSemaphoreCreateMutex();
  if (!s_mutex) {
    ESP_LOGE(TAG, "Failed to create mutex");
    return ESP_ERR_NO_MEM;
  }

  // Initialize esp_video subsystem
  esp_video_init_csi_config_t csi_config = {
      .sccb_config =
          {
              .init_sccb = true,
              .i2c_config =
                  {
                      .port = 0,
                      .scl_pin = s_config.i2c_scl_pin,
                      .sda_pin = s_config.i2c_sda_pin,
                  },
              .freq = 100000,
          },
      .reset_pin = -1,
      .pwdn_pin = -1,
  };

  esp_video_init_config_t cam_config = {
      .csi = &csi_config,
  };

  esp_err_t ret = esp_video_init(&cam_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init esp_video: %s", esp_err_to_name(ret));
    cleanup_resources();
    return ret;
  }

  // Initialize camera device
  ret = init_camera_device();
  if (ret != ESP_OK) {
    cleanup_resources();
    return ret;
  }

  // Initialize JPEG encoder
  ret = init_jpeg_encoder();
  if (ret != ESP_OK) {
    cleanup_resources();
    return ret;
  }

  s_initialized = true;
  ESP_LOGI(TAG, "Camera manager initialized (OV5647 %dx%d @ %dfps)",
           s_config.width, s_config.height, s_config.fps);

  return ESP_OK;
}

static esp_err_t init_camera_device(void) {
  ESP_LOGI(TAG, "Opening MIPI-CSI camera device...");

  s_cam_fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY);
  if (s_cam_fd < 0) {
    ESP_LOGE(TAG, "Failed to open camera device");
    return ESP_FAIL;
  }

  // Query capabilities
  struct v4l2_capability cap;
  if (ioctl(s_cam_fd, VIDIOC_QUERYCAP, &cap) != 0) {
    ESP_LOGE(TAG, "Failed to query camera capabilities");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Camera: %s, driver: %s", cap.card, cap.driver);

  // Set format
  struct v4l2_format fmt = {0};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = s_config.width;
  fmt.fmt.pix.height = s_config.height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;

  if (ioctl(s_cam_fd, VIDIOC_S_FMT, &fmt) != 0) {
    ESP_LOGE(TAG, "Failed to set camera format");
    return ESP_FAIL;
  }

  // Request buffers
  struct v4l2_requestbuffers req = {0};
  req.count = CAM_BUFFER_COUNT;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (ioctl(s_cam_fd, VIDIOC_REQBUFS, &req) != 0) {
    ESP_LOGE(TAG, "Failed to request buffers");
    return ESP_FAIL;
  }

  // Map buffers
  for (int i = 0; i < CAM_BUFFER_COUNT; i++) {
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (ioctl(s_cam_fd, VIDIOC_QUERYBUF, &buf) != 0) {
      ESP_LOGE(TAG, "Failed to query buffer %d", i);
      return ESP_FAIL;
    }

    s_cam_buffer[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                           s_cam_fd, buf.m.offset);
    if (!s_cam_buffer[i]) {
      ESP_LOGE(TAG, "Failed to mmap buffer %d", i);
      return ESP_FAIL;
    }

    // Queue buffer
    if (ioctl(s_cam_fd, VIDIOC_QBUF, &buf) != 0) {
      ESP_LOGE(TAG, "Failed to queue buffer %d", i);
      return ESP_FAIL;
    }
  }

  // Start streaming
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(s_cam_fd, VIDIOC_STREAMON, &type) != 0) {
    ESP_LOGE(TAG, "Failed to start camera streaming");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Camera device initialized, streaming started");
  return ESP_OK;
}

static esp_err_t init_jpeg_encoder(void) {
  ESP_LOGI(TAG, "Opening JPEG encoder device...");

  s_jpeg_fd = open(ESP_VIDEO_JPEG_DEVICE_NAME, O_RDONLY);
  if (s_jpeg_fd < 0) {
    ESP_LOGE(TAG, "Failed to open JPEG encoder device");
    return ESP_FAIL;
  }

  // Query capabilities
  struct v4l2_capability cap;
  if (ioctl(s_jpeg_fd, VIDIOC_QUERYCAP, &cap) != 0) {
    ESP_LOGE(TAG, "Failed to query JPEG capabilities");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "JPEG encoder: %s, driver: %s", cap.card, cap.driver);

  // Set output format (input to encoder)
  struct v4l2_format fmt = {0};
  fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  fmt.fmt.pix.width = s_config.width;
  fmt.fmt.pix.height = s_config.height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;

  if (ioctl(s_jpeg_fd, VIDIOC_S_FMT, &fmt) != 0) {
    ESP_LOGE(TAG, "Failed to set JPEG input format");
    return ESP_FAIL;
  }

  // Request output buffer
  struct v4l2_requestbuffers req = {0};
  req.count = 1;
  req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  req.memory = V4L2_MEMORY_USERPTR;

  if (ioctl(s_jpeg_fd, VIDIOC_REQBUFS, &req) != 0) {
    ESP_LOGE(TAG, "Failed to request JPEG output buffer");
    return ESP_FAIL;
  }

  // Set capture format (JPEG output)
  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = s_config.width;
  fmt.fmt.pix.height = s_config.height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;

  if (ioctl(s_jpeg_fd, VIDIOC_S_FMT, &fmt) != 0) {
    ESP_LOGE(TAG, "Failed to set JPEG output format");
    return ESP_FAIL;
  }

  // Request capture buffer
  memset(&req, 0, sizeof(req));
  req.count = 1;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (ioctl(s_jpeg_fd, VIDIOC_REQBUFS, &req) != 0) {
    ESP_LOGE(TAG, "Failed to request JPEG capture buffer");
    return ESP_FAIL;
  }

  // Query and map capture buffer
  struct v4l2_buffer buf = {0};
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.index = 0;

  if (ioctl(s_jpeg_fd, VIDIOC_QUERYBUF, &buf) != 0) {
    ESP_LOGE(TAG, "Failed to query JPEG buffer");
    return ESP_FAIL;
  }

  s_jpeg_buffer = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                       s_jpeg_fd, buf.m.offset);
  if (!s_jpeg_buffer) {
    ESP_LOGE(TAG, "Failed to mmap JPEG buffer");
    return ESP_FAIL;
  }
  s_jpeg_buffer_size = buf.length;

  // Queue buffer
  if (ioctl(s_jpeg_fd, VIDIOC_QBUF, &buf) != 0) {
    ESP_LOGE(TAG, "Failed to queue JPEG buffer");
    return ESP_FAIL;
  }

  // Start streams
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(s_jpeg_fd, VIDIOC_STREAMON, &type) != 0) {
    ESP_LOGE(TAG, "Failed to start JPEG capture");
    return ESP_FAIL;
  }

  type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  if (ioctl(s_jpeg_fd, VIDIOC_STREAMON, &type) != 0) {
    ESP_LOGE(TAG, "Failed to start JPEG output");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "JPEG encoder initialized");
  return ESP_OK;
}

esp_err_t camera_manager_deinit(void) {
  if (!s_initialized) {
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Deinitializing camera manager...");
  cleanup_resources();
  s_initialized = false;

  return ESP_OK;
}

static void cleanup_resources(void) {
  // Stop streaming
  if (s_cam_fd >= 0) {
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(s_cam_fd, VIDIOC_STREAMOFF, &type);
    close(s_cam_fd);
    s_cam_fd = -1;
  }

  if (s_jpeg_fd >= 0) {
    int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ioctl(s_jpeg_fd, VIDIOC_STREAMOFF, &type);
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(s_jpeg_fd, VIDIOC_STREAMOFF, &type);
    close(s_jpeg_fd);
    s_jpeg_fd = -1;
  }

  // Buffers are unmapped automatically on close

  if (s_mutex) {
    vSemaphoreDelete(s_mutex);
    s_mutex = NULL;
  }
}

bool camera_manager_is_initialized(void) { return s_initialized; }

esp_err_t camera_manager_capture_jpeg(camera_frame_t *frame) {
  if (!s_initialized || !frame) {
    return ESP_ERR_INVALID_STATE;
  }

  if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  esp_err_t ret = ESP_FAIL;

  // Dequeue camera buffer
  struct v4l2_buffer cam_buf = {0};
  cam_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  cam_buf.memory = V4L2_MEMORY_MMAP;

  if (ioctl(s_cam_fd, VIDIOC_DQBUF, &cam_buf) != 0) {
    ESP_LOGE(TAG, "Failed to dequeue camera buffer");
    goto done;
  }

  // Send to JPEG encoder
  struct v4l2_buffer jpeg_out = {0};
  jpeg_out.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  jpeg_out.memory = V4L2_MEMORY_USERPTR;
  jpeg_out.index = 0;
  jpeg_out.m.userptr = (unsigned long)s_cam_buffer[cam_buf.index];
  jpeg_out.length = cam_buf.bytesused;

  if (ioctl(s_jpeg_fd, VIDIOC_QBUF, &jpeg_out) != 0) {
    ESP_LOGE(TAG, "Failed to queue JPEG input");
    ioctl(s_cam_fd, VIDIOC_QBUF, &cam_buf);
    goto done;
  }

  // Dequeue JPEG output
  struct v4l2_buffer jpeg_cap = {0};
  jpeg_cap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  jpeg_cap.memory = V4L2_MEMORY_MMAP;

  if (ioctl(s_jpeg_fd, VIDIOC_DQBUF, &jpeg_cap) != 0) {
    ESP_LOGE(TAG, "Failed to dequeue JPEG output");
    ioctl(s_cam_fd, VIDIOC_QBUF, &cam_buf);
    goto done;
  }

  // Re-queue camera buffer
  ioctl(s_cam_fd, VIDIOC_QBUF, &cam_buf);

  // Dequeue JPEG output buffer (to complete the cycle)
  ioctl(s_jpeg_fd, VIDIOC_DQBUF, &jpeg_out);

  // Fill frame info
  frame->buf = s_jpeg_buffer;
  frame->len = jpeg_cap.bytesused;
  frame->width = s_config.width;
  frame->height = s_config.height;
  frame->timestamp = esp_timer_get_time();

  ESP_LOGD(TAG, "Captured JPEG: %zu bytes", frame->len);
  ret = ESP_OK;

done:
  xSemaphoreGive(s_mutex);
  return ret;
}

void camera_manager_return_frame(camera_frame_t *frame) {
  if (!s_initialized || !frame) {
    return;
  }

  if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return;
  }

  // Re-queue JPEG capture buffer
  struct v4l2_buffer buf = {0};
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.index = 0;

  ioctl(s_jpeg_fd, VIDIOC_QBUF, &buf);

  xSemaphoreGive(s_mutex);
}

const char *camera_manager_get_status(void) {
  if (!s_initialized) {
    return "NOT_INIT";
  }
  if (s_cam_fd < 0 || s_jpeg_fd < 0) {
    return "ERROR";
  }
  return "OK";
}
