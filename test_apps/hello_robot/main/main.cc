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
#include "tensorflow/lite/micro/micro_allocator.h"
#include "tensorflow/lite/micro/micro_resource_variable.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "frontend.h"
#include "frontend_util.h"
#include "board.h"
#include "audio_service.h"
#include "pcm_stream.h"

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
  ESP_LOGI(TAG, "HELLO ROBOT V1 LIVE_MIC FRONTEND + INFERENCE DIAGNOSTIC");
  ESP_LOGI(TAG, "model bytes=%u", (unsigned)(model_end - model_start));
  ESP_LOGI(TAG, "main task stack high-water mark at entry=%u bytes",
           (unsigned)uxTaskGetStackHighWaterMark(nullptr));
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
  // Stateful microWakeWord exports rely on VAR_HANDLE / READ_VARIABLE /
  // ASSIGN_VARIABLE / CALL_ONCE. The interpreter does not create resource
  // variables implicitly: give them their own persistent allocator.
  // Keep both arenas in PSRAM and alive for the interpreter's entire life.
  constexpr size_t kVariableArenaBytes = 128 * 1024;
  constexpr int kMaxResourceVariables = 32;
  uint8_t *variable_arena = (uint8_t *)heap_caps_malloc(
      kVariableArenaBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!variable_arena) {
    ESP_LOGE(TAG, "PSRAM resource variable arena allocation failed");
    return;
  }
  auto *variable_allocator = tflite::MicroAllocator::Create(
      variable_arena, kVariableArenaBytes);
  if (!variable_allocator) {
    ESP_LOGE(TAG, "MicroAllocator::Create(resource variables) failed");
    return;
  }
  auto *resource_variables = tflite::MicroResourceVariables::Create(
      variable_allocator, kMaxResourceVariables);
  if (!resource_variables) {
    ESP_LOGE(TAG, "MicroResourceVariables::Create failed");
    return;
  }
  ESP_LOGI(TAG, "resource variables ready: slots=%d arena=%u bytes",
           kMaxResourceVariables, (unsigned)kVariableArenaBytes);
  static tflite::MicroInterpreter *interpreter;
  interpreter = new tflite::MicroInterpreter(
      model, resolver, arena, kArenaBytes, resource_variables);
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
  ESP_LOGI(TAG, "initializing Waveshare V2 board / single-owner microphone");
  if (board_init() != ESP_OK || audio_service_init() != ESP_OK ||
      !audio_service_capture_ready()) {
    ESP_LOGE(TAG, "board or ES7210 capture unavailable; no fake input fallback");
    return;
  }
  ESP_LOGI(TAG, "LIVE_MIC: ES7210 24kHz stereo -> 16kHz mono; no Hi ESP reader");

  // Match the standard microWakeWord micro_speech frontend rather than
  // sending arbitrary feature bytes. No live microphone is connected yet.
  FrontendConfig frontend_cfg = {};
  FrontendState frontend_state = {};
  FrontendFillConfigWithDefaults(&frontend_cfg);
  frontend_cfg.window.size_ms = 30;
  frontend_cfg.window.step_size_ms = 10;
  frontend_cfg.filterbank.num_channels = 40;
  frontend_cfg.filterbank.lower_band_limit = 125.0f;
  frontend_cfg.filterbank.upper_band_limit = 7500.0f;
  frontend_cfg.noise_reduction.smoothing_bits = 10;
  frontend_cfg.noise_reduction.even_smoothing = 0.025f;
  frontend_cfg.noise_reduction.odd_smoothing = 0.06f;
  frontend_cfg.noise_reduction.min_signal_remaining = 0.05f;
  frontend_cfg.pcan_gain_control.enable_pcan = true;
  frontend_cfg.pcan_gain_control.strength = 0.95f;
  frontend_cfg.pcan_gain_control.offset = 80.0f;
  frontend_cfg.pcan_gain_control.gain_bits = 21;
  frontend_cfg.log_scale.enable_log = true;
  frontend_cfg.log_scale.scale_shift = 6;
  if (!FrontendPopulateState(&frontend_cfg, &frontend_state, 16000)) {
    ESP_LOGE(TAG, "micro_speech FrontendPopulateState failed");
    return;
  }
  ESP_LOGI(TAG, "micro_speech frontend ready: 16kHz 30ms window 10ms hop 40 bins");
  uint64_t total_us=0, max_us=0;
  uint32_t frames=0;
  int8_t group[120] = {};
  int16_t source[HELLO_AUDIO_SOURCE_FRAMES * HELLO_AUDIO_CHANNELS] = {};
  int16_t mono[HELLO_AUDIO_TARGET_FRAMES] = {};
  uint32_t read_failures=0, peak=0, detections=0, total_features=0;
  uint32_t above_threshold=0;
  uint64_t sum_abs=0;
  // Experimental detection threshold only, NOT validated for this model.
  constexpr uint8_t kScoreThreshold=180;
  for (int i=0; i<2000; ++i) {
    for (int part=0; part<3; ++part) {
      // A 10ms PCM buffer may not immediately yield one 30ms feature frame.
      // The frontend retains its 20ms overlap between calls.
      bool produced = false;
      for (int feed=0; feed<4 && !produced; ++feed) {
        size_t consumed=0;
        if (audio_service_capture_read(source, sizeof(source)) != ESP_OK) {
          ++read_failures;
          ESP_LOGE(TAG, "LIVE_MIC: capture read failed (%u)", (unsigned)read_failures);
          FrontendFreeStateContents(&frontend_state);
          return;
        }
        hello_pcm_24k_to_16k_left(source, mono);
        for (int x=0;x<HELLO_AUDIO_TARGET_FRAMES;++x) {
          int32_t sample=mono[x];
          uint32_t absolute=(uint32_t)(sample<0?-sample:sample);
          sum_abs+=absolute;
          if(absolute>peak)peak=absolute;
        }
        FrontendOutput feature = FrontendProcessSamples(
            &frontend_state, mono, HELLO_AUDIO_TARGET_FRAMES, &consumed);
        if (feature.size == 40) {
          for (size_t k=0; k<40; ++k) {
            // ESPHome's microWakeWord integer feature mapping:
            // frontend uint16 -> int8, matching 0..26 training range.
            int32_t v = ((int32_t)feature.values[k] * 256 + 333) / 666 - 128;
            if (v < -128) v=-128;
            if (v > 127) v=127;
            group[part*40+k] = (int8_t)v;
          }
          ++frames;
          ++total_features;
          produced = true;
        } else if (feature.size != 0 || consumed == 0) {
          ESP_LOGE(TAG, "frontend stalled: size=%u consumed=%u",
                   (unsigned)feature.size, (unsigned)consumed);
          FrontendFreeStateContents(&frontend_state);
          return;
        }
      }
      if (!produced) {
        ESP_LOGE(TAG, "frontend did not produce 40 features at part=%d", part);
        FrontendFreeStateContents(&frontend_state);
        return;
      }
    }
    memcpy(input->data.int8, group, sizeof(group));
    int64_t t0=esp_timer_get_time();
    if (interpreter->Invoke()!=kTfLiteOk) {
      ESP_LOGE(TAG, "Invoke failed on iteration=%d", i);
      FrontendFreeStateContents(&frontend_state);
      return;
    }
    uint64_t delta=(uint64_t)(esp_timer_get_time()-t0);
    total_us+=delta;
    if(delta>max_us)max_us=delta;
    const uint8_t score=output->data.uint8[0];
    above_threshold = score >= kScoreThreshold ? above_threshold+1 : 0;
    if (above_threshold >= 3) {
      ++detections;
      ESP_LOGW(TAG, "HELLO ROBOT CANDIDATE DETECTED: score=%u count=%u (UNCALIBRATED)",
               (unsigned)score, (unsigned)detections);
      above_threshold=0;
    }
    if(i<10||i%30==29) {
      ESP_LOGI(TAG,"LIVE_MIC: iteration=%d score=%u probability=%.3f inference_us=%llu rms_proxy=%u peak=%u read_errors=%u",
        i,(unsigned)score,score/256.0f,(unsigned long long)delta,
        (unsigned)(sum_abs/(total_features ? (uint64_t)total_features*160ULL : 1ULL)),
        (unsigned)peak,(unsigned)read_failures);
      peak=0;
    }
    vTaskDelay(1);
  }
  FrontendFreeStateContents(&frontend_state);
  ESP_LOGI(TAG, "LIVE_MIC: generated %u frames, candidates=%u, read_errors=%u",
           (unsigned)frames,(unsigned)detections,(unsigned)read_failures);
  ESP_LOGI(TAG,"PASS: 2000 LIVE_MIC Invokes; avg_us=%llu max_us=%llu (uncalibrated; not proof of accurate wake recognition)",
      (unsigned long long)(total_us/2000),(unsigned long long)max_us);
  ESP_LOGI(TAG, "main task stack high-water mark at finish=%u bytes",
           (unsigned)uxTaskGetStackHighWaterMark(nullptr));
  memory("finished");
}
