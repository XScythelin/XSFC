#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <EEPROM.h>
#include <Xsfc.h>
#include <Kalman.h>
#include <Madgwick.h>
#include <Mahony.h>
#include <printf.h>
#include <blackbox/blackbox.h>
#include <EscDriver.h>
#include <EspWire.h>
#include <Gps.hpp>
#if defined(XSFC_ESPNOW)
#include <EspNowRcLink/Receiver.h>
#endif
#ifdef XSFC_WIFI
#include <WiFi.h>
#endif
#include "Debug_Xsfc.h"
#include "Target/GyroTimer.h"

#ifdef ESP32
void IRAM_ATTR serialEventRun(void) {}
#endif

Xsfc::Xsfc xsfc;

#if defined(XSFC_MULTI_CORE)
  #if defined(XSFC_FREE_RTOS)

    // ESP32 multicore
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
    #if defined(ESP32P4)
      #include <driver/gptimer.h>
      #include <esp_task_wdt.h>
    #else
      #include <driver/timer.h>
    #endif

    TaskHandle_t gyroTaskHandle = NULL;
    TaskHandle_t pidTaskHandle = NULL;
    #if defined(ESP32P4)
    static gptimer_handle_t gyroTimer = nullptr;
    #else
    static const timer_group_t TIMER_GROUP = TIMER_GROUP_0;
    static const timer_idx_t TIMER_IDX = TIMER_0;
    #endif

    bool IRAM_ATTR gyroTimerIsr(void* args)
    {
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      vTaskNotifyGiveFromISR(gyroTaskHandle, &xHigherPriorityTaskWoken);
      return xHigherPriorityTaskWoken == pdTRUE;
    }

    #if defined(ESP32P4)
    bool IRAM_ATTR gyroGptimerIsr(gptimer_handle_t, const gptimer_alarm_event_data_t*, void*)
    {
      return gyroTimerIsr(nullptr);
    }
    #endif

    void gyroTimerInit(bool (*isrCb)(void* args), int interval)
    {
    #if defined(ESP32P4)
      gptimer_config_t config = {};
      config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
      config.direction = GPTIMER_COUNT_UP;
      config.resolution_hz = 1000000;
      ESP_ERROR_CHECK(gptimer_new_timer(&config, &gyroTimer));
      gptimer_event_callbacks_t callbacks = {};
      (void)isrCb;
      callbacks.on_alarm = gyroGptimerIsr;
      ESP_ERROR_CHECK(gptimer_register_event_callbacks(gyroTimer, &callbacks, nullptr));
      gptimer_alarm_config_t alarm = {};
      alarm.alarm_count = interval;
      alarm.flags.auto_reload_on_alarm = true;
      ESP_ERROR_CHECK(gptimer_set_alarm_action(gyroTimer, &alarm));
      ESP_ERROR_CHECK(gptimer_enable(gyroTimer));
      ESP_ERROR_CHECK(gptimer_start(gyroTimer));
    #else
      timer_config_t config = {
        .alarm_en = TIMER_ALARM_EN,
        .counter_en = TIMER_PAUSE,
        .intr_type = TIMER_INTR_LEVEL,
        .counter_dir = TIMER_COUNT_UP,
        .auto_reload = TIMER_AUTORELOAD_EN,
        .divider = 80,
      };
      timer_init(TIMER_GROUP, TIMER_IDX, &config);
      timer_set_counter_value(TIMER_GROUP, TIMER_IDX, 0);
      timer_set_alarm_value(TIMER_GROUP, TIMER_IDX, interval);
      timer_isr_callback_add(TIMER_GROUP, TIMER_IDX, isrCb, nullptr, ESP_INTR_FLAG_IRAM);
      timer_enable_intr(TIMER_GROUP, TIMER_IDX);
      timer_start(TIMER_GROUP, TIMER_IDX);
    #endif
    }

    void gyroTimerStop()
    {
    #if defined(ESP32P4)
      if(gyroTimer) ESP_ERROR_CHECK(gptimer_stop(gyroTimer));
    #else
      timer_pause(TIMER_GROUP, TIMER_IDX);
    #endif
    }

    void gyroTask(void *pvParameters)
    {
      xsfc.begin();
      // The consumer must not touch queues/state while begin() initializes
      // devices and performs cross-core flash/cache operations.
      xTaskNotifyGive(pidTaskHandle);
      gyroTimerInit(gyroTimerIsr, xsfc.getGyroInterval());
      while(true)
      {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // wait for timer isr notification
        xsfc.update(true);
      }
    }

    void pidTask(void *pvParameters)
    {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      while(true)
      {
        xsfc.updateOther();
      }
    }

    void setup()
    {
    #if defined(ESP32P4)
      // IDF 5 also registers an idle hook. Deleting only the idle task's WDT
      // entry leaves that hook calling reset() for an unsubscribed task.
      // Use reconfigure() to unsubscribe the idle task AND remove its hook.
      esp_task_wdt_config_t watchdog = {};
      watchdog.timeout_ms = CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000;
      watchdog.idle_core_mask = 0; // PID task intentionally occupies core 0.
      #if defined(CONFIG_ESP_TASK_WDT_PANIC)
      watchdog.trigger_panic = true;
      #endif
      ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&watchdog));
    #else
      disableCore0WDT();
    #endif
      // internal task priorities
      // PRO(0): hi-res timer(22), timer(1), event-loop(20), lwip(18/any), wifi(23), wpa(2/any), BT/vhci(23), NimBle(21), BT/other(19,20,22), Eth(15), Mqtt(5/any)
      // APP(1): free
      xsfc.load();
      xTaskCreateUniversal(pidTask,  "pidTask",  8192, NULL,  1, &pidTaskHandle,  0);
      xTaskCreateUniversal(gyroTask, "gyroTask", 8192, NULL, 24, &gyroTaskHandle, 1);
      vTaskDelete(NULL); // delete arduino loop task
    }

    void loop()
    {
    }

  #else
    #error "No RTOS defined for multicore board"
  #endif

#else

  // single core
  void setup()
  {
    xsfc.load();
    xsfc.begin();
  }
  void loop()
  {
    xsfc.update();
    xsfc.updateOther();
  }

#endif
