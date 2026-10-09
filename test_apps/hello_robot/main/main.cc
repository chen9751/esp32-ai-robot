// Standalone Hello Robot V1 streaming TFLite Micro diagnostic.
// This is NOT the production Hi ESP firmware and does not drive the microphone.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

extern "C" void app_main(void);
extern const uint8_t model_start[] asm("_binary_hello_robot_v1_tflite_start");
extern const uint8_t model_end[] asm("_binary_hello_robot_v1_tflite_end");

static const char *TAG = "hello_robot_test";
static void memory(const char *where) {
  ESP_LOGI(TAG, "%s internal=%u largest=%u psram=%u",
           where,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
extern "C" void app_main(void) {
  ESP_LOGI(TAG, "HELLO ROBOT V1 independent streaming inference SMOKE TEST");
  ESP_LOGI(TAG, "model bytes=%u", (unsigned)(model_end - model_start));
  if (model_end-model_start != 62200) { ESP_LOGE(TAG, "model size mismatch"); return; }
  auto *model = tflite::GetModel(model_start);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    ESP_LOGE(TAG, "schema mismatch model=%d library=%d", model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }
  // Register only the operators found in the verified Hello Robot V1 FlatBuffer.
  static tflite::MicroMutableOpResolver<16> resolver;
  // Exact builtin operator inventory from the verified FlatBuffer.
  // Variable op support is version-dependent in esp-tflite-micro.
  if (resolver.AddVarHandle() != kTfLiteOk ||
      resolver.AddReadVariable() != kTfLiteOk ||
      resolver.AddReshape() != kTfLiteOk ||
      resolver.AddAssignVariable() != kTfLiteOk ||
      resolver.AddConv2D() != kTfLiteOk ||
      resolver.AddDepthwiseConv2D() != kTfLiteOk ||
      resolver.AddConcatenation() != kTfLiteOk ||
      resolver.AddCallOnce() != kTfLiteOk ||
      resolver.AddStridedSlice() != kTfLiteOk ||
      resolver.AddSplitV() != kTfLiteOk ||
      resolver.AddFullyConnected() != kTfLiteOk ||
      resolver.AddLogistic() != kTfLiteOk ||
      resolver.AddQuantize() != kTfLiteOk) {
    ESP_LOGE(TAG, "operator registration failed");
    return;
  }
  constexpr size_t kArenaBytes = 768 * 1024;
  memory("before arena");
  uint8_t *arena = (uint8_t*)heap_caps_malloc(kArenaBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!arena) { ESP_LOGE(TAG, "PSRAM arena allocation failed"); return; }
  static tflite::MicroInterpreter *interpreter;
  interpreter = new tflite::MicroInterpreter(model, resolver, arena, kArenaBytes);
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    ESP_LOGE(TAG, "AllocateTensors failed: inspect missing/unsupported ops and variables");
    return;
  }
  auto *input = interpreter->input(0);
  auto *output = interpreter->output(0);
  if (!input || !output || input->type != kTfLiteInt8 || output->type != kTfLiteUInt8 ||
      input->bytes != 120 || output->bytes != 1) {
    ESP_LOGE(TAG, "unexpected IO: input type=%d bytes=%u, output type=%d bytes=%u",
      input ? input->type : -1, input ? (unsigned)input->bytes : 0,
      output ? output->type : -1, output ? (unsigned)output->bytes : 0);
    return;
  }
  ESP_LOGI(TAG, "IO OK; arena used=%u of %u bytes", (unsigned)interpreter->arena_used_bytes(), (unsigned)kArenaBytes);
  memory("after arena");
  uint64_t total_us=0, max_us=0;
  for (int i=0; i<200; ++i) {
    // -128 represents a quantized feature zero (input zero point -128).
    // It is not a trained speech sample and cannot establish accuracy.
    memset(input->data.int8, -128, input->bytes);
    int64_t t0=esp_timer_get_time();
    if (interpreter->Invoke()!=kTfLiteOk) {
      ESP_LOGE(TAG, "Invoke failed on iteration=%d", i);
      return;
    }
    uint64_t delta=(uint64_t)(esp_timer_get_time()-t0);
    total_us+=delta;
    if(delta>max_us)max_us=delta;
    if(i<5||i%50==49)
      ESP_LOGI(TAG,"iteration=%d score_uint8=%u inference_us=%llu",i,(unsigned)output->data.uint8[0],(unsigned long long)delta);
    vTaskDelay(1);
  }
  ESP_LOGI(TAG,"PASS: 200 Invokes; avg_us=%llu max_us=%llu (no microphone, no accuracy verdict)",
      (unsigned long long)(total_us/200),(unsigned long long)max_us);
  memory("finished");
}
