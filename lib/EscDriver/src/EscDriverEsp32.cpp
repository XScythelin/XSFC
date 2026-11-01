#if defined(ESP32)

#include <Arduino.h>
#include "EscDriverEsp32.h"
//#include "Debug_Xsfc.h"
#include "soc/rmt_periph.h"
#include "hal/rmt_ll.h"
#include "hal/gpio_ll.h"
#include "esp_rom_gpio.h"

#if defined(ESP32P4)
extern "C" rmt_mem_t RMTMEM;
// IDF 5.x consolidated the per-event LL helpers into event-mask operations.
// Keep the IDF 4.x path used by ESP32/S3 intact.
#define rmt_ll_enable_tx_end_interrupt(dev, ch, en) rmt_ll_enable_interrupt(dev, RMT_LL_EVENT_TX_DONE(ch), en)
#define rmt_ll_clear_tx_end_interrupt(dev, ch) rmt_ll_clear_interrupt_status(dev, RMT_LL_EVENT_TX_DONE(ch))
#define rmt_ll_enable_rx_end_interrupt(dev, ch, en) rmt_ll_enable_interrupt(dev, RMT_LL_EVENT_RX_DONE(ch), en)
#define rmt_ll_clear_rx_end_interrupt(dev, ch) rmt_ll_clear_interrupt_status(dev, RMT_LL_EVENT_RX_DONE(ch))
#endif

static constexpr size_t DURATION_CLOCK = 25; // [ns] doubled value to increase precision
#define TO_INTERVAL_US(v) (1 * 1000 * 1000 / (v)) // [us]

#define RMT_RX_CHANNEL_ENCODING_START (SOC_RMT_CHANNELS_PER_GROUP - SOC_RMT_TX_CANDIDATES_PER_GROUP)
#define RMT_ENCODE_RX_CHANNEL(encode_chan) ((encode_chan + RMT_RX_CHANNEL_ENCODING_START))

#define XSFC_RMT_RX_BUFFER_SIZE 128

// faster esc response, but unsafe (no task synchronisation)
// set to 0 in case of issues
#define XSFC_RMT_BYPASS_WRITE_SYNC 1

#if XSFC_RMT_BYPASS_WRITE_SYNC
IRAM_ATTR static esp_err_t _rmt_fill_tx_items(rmt_channel_t channel, const rmt_item32_t* item, uint16_t item_num, uint16_t mem_offset)
{
  //rmt_ll_enable_mem_access(&RMT, true);
#if defined(ESP32P4)
  volatile uint32_t* dest = &RMTMEM.chan[channel].data32[mem_offset].val;
  for(uint16_t i = 0; i < item_num; i++) dest[i] = item[i].val;
#else
  rmt_ll_write_memory(&RMTMEM, channel, item, item_num, mem_offset);
#endif
  rmt_ll_tx_reset_pointer(&RMT, channel);
#if !defined(ESP32P4)
  rmt_ll_rx_set_mem_owner(&RMT, channel, RMT_MEM_OWNER_TX);
#endif
  return ESP_OK;
}
IRAM_ATTR static esp_err_t _rmt_tx_start(rmt_channel_t channel, bool tx_idx_rst)
{
  if(tx_idx_rst)
  {
    rmt_ll_tx_reset_pointer(&RMT, channel); // BC
  }
#if !defined(ESP32P4)
  rmt_ll_rx_set_mem_owner(&RMT, channel, RMT_MEM_OWNER_TX); // moved to _rmt_fill_tx_items
#endif
  rmt_ll_clear_tx_end_interrupt(&RMT, channel);
  rmt_ll_tx_start(&RMT, channel);
  return ESP_OK;
}
#else
#define _rmt_tx_start rmt_tx_start
#define _rmt_fill_tx_items rmt_fill_tx_items
#endif

#if !defined(ESP32P4)
static void IRAM_ATTR _rmt_rx_zero_mem(rmt_channel_t channel, size_t len)
{
  volatile rmt_item32_t *data = (rmt_item32_t *)RMTMEM.chan[RMT_ENCODE_RX_CHANNEL(channel)].data32;
  for (size_t idx = 0; idx < len; idx++)
  {
    data[idx].val = 0;
  }
}
#endif

// using map() in isr cause crash
static long IRAM_ATTR _map(long x, long in_min, long in_max, long out_min, long out_max)
{
  const long in_range = in_max - in_min;
  if(in_range == 0) return out_min;
  const long out_range = out_max - out_min;
  const long delta = x - in_min;
  return (delta * out_range) / in_range + out_min;
}

bool EscDriverEsp32::_tx_end_installed = false;
#if defined(ESP32P4)
rmt_isr_handle_t EscDriverEsp32::_interruptHandle = nullptr;
bool EscDriverEsp32::_interruptAllocationAttempted = false;

void IRAM_ATTR EscDriverEsp32::rmtInterrupt(void*)
{
  // Direct-register TX must not enter the legacy driver's FreeRTOS queues.
  // Clear before callback: the callback may start the next asynchronous TX.
  uint32_t pending = rmt_ll_get_tx_end_interrupt_status(&RMT);
  uint32_t receive = 0;
  uint32_t completed = 0;
  for(uint32_t i = 0; i < ESC_CHANNEL_COUNT; ++i)
  {
    if(!(pending & (1u << i))) continue;
    auto* instance = instances[i];
    if(instance && instance->_digital && instance->_dshot_tlm && !instance->_async)
    {
      receive |= 1u << i;
      completed |= RMT_LL_EVENT_TX_DONE(i);
    }
  }
  if(completed)
  {
    // These non-looping packets have already stopped at their end marker.
    // A second TX_STOP/config-update and the generic callback only delay RX.
    // Acknowledge together, arm every receiver, then update diagnostics.
    rmt_ll_enable_interrupt(&RMT, completed, false);
    rmt_ll_clear_interrupt_status(&RMT, completed);
    for(uint32_t i = 0; i < ESC_CHANNEL_COUNT; ++i)
      if(receive & (1u << i)) instances[i]->enableRx(static_cast<rmt_channel_t>(i));
    for(uint32_t i = 0; i < ESC_CHANNEL_COUNT; ++i)
      if(receive & (1u << i)) instances[i]->_telemetryStats[i].txDone++;
    pending &= ~receive;
  }
  for(uint32_t i = 0; i < ESC_CHANNEL_COUNT; ++i)
  {
    if(!(pending & (1u << i))) continue;
    rmt_ll_enable_tx_end_interrupt(&RMT, i, false);
    rmt_ll_clear_tx_end_interrupt(&RMT, i);
    txDoneCallback(static_cast<rmt_channel_t>(i), nullptr);
  }
}
#endif

EscDriverEsp32 *EscDriverEsp32::instances[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

EscDriverEsp32::EscDriverEsp32() : _protocol(ESC_PROTOCOL_PWM), _async(true), _rate(50), _digital(false), _dshot_tlm(false), _channel_mask(0)
{
  for (size_t i = 0; i < ESC_CHANNEL_COUNT; i++)
  {
    _channel[i].pin = gpio_num_t(-1);
  }
}

void EscDriverEsp32::end()
{
  for (size_t i = 0; i < ESC_CHANNEL_COUNT; i++)
  {
    if (!_channel[i].attached()) continue;
    if (_protocol == ESC_PROTOCOL_DISABLED) continue;
    // TODO: handle splitted case
#if defined(ESP32P4)
    rmt_ll_enable_tx_end_interrupt(&RMT, i, false);
    rmt_ll_tx_stop(&RMT, i);
    disableRx(static_cast<rmt_channel_t>(i));
    instances[i] = nullptr;
#else
    rmt_driver_uninstall((rmt_channel_t)i);
#endif
  }
  _protocol = ESC_PROTOCOL_DISABLED;
}

int EscDriverEsp32::begin(const EscConfig& conf)
{
  _protocol = ESC_PROTOCOL_SANITIZE(conf.protocol);
  _async = _protocol == ESC_PROTOCOL_BRUSHED ? true : conf.async; // force async for brushed
  _rate = constrain(conf.rate, 50, 8000);
  _interval = TO_INTERVAL_US(_rate);
  _digital = isDigital(_protocol);
  _dshot_tlm = conf.dshotTelemetry && (_protocol == ESC_PROTOCOL_DSHOT300 || _protocol == ESC_PROTOCOL_DSHOT600);
  // Bidirectional packets need a receive window; continuous async TX cannot
  // provide it. Keep the existing synchronous mixer schedule for this mode.
#if defined(ESP32P4)
  if(_dshot_tlm) _async = false;
#endif

  return 1;
}

int EscDriverEsp32::attach(size_t channel, int pin, int pulse)
{
  if (channel < 0 || channel >= ESC_CHANNEL_COUNT) return 0;
  initChannel(channel, (gpio_num_t)pin, pulse);
  return 1;
}

int EscDriverEsp32::beep(size_t channel, uint8_t tone)
{
  if(!_digital || channel >= ESC_CHANNEL_COUNT || tone < 1 || tone > 5) return 0;
  // Negative sentinel is internal only; normal write() replaces it next cycle.
  _channel[channel].pulse = -static_cast<int>(tone);
  return 1;
}

int IRAM_ATTR EscDriverEsp32::write(size_t channel, int pulse)
{
  if (channel < 0 || channel >= ESC_CHANNEL_COUNT) return 0;
  _channel[channel].pulse = pulse;
  return 1;
}

void IRAM_ATTR EscDriverEsp32::apply()
{
  if (_protocol == ESC_PROTOCOL_DISABLED) return;
  if (_async) return;
#if defined(ESP32P4)
  // ESP-IDF binds the interrupt to the allocating core. initChannel() runs
  // on the gyro/I2C core, but synchronous outputs run on the mixer core.
  // Allocate here once, before the first TX, so turnaround does not contend
  // with gyro timer and I2C interrupts on the other core.
  if(_dshot_tlm && !_interruptHandle)
  {
    if(!_interruptAllocationAttempted)
    {
      _interruptAllocationAttempted = true;
      const esp_err_t error = rmt_isr_register(rmtInterrupt, nullptr, ESP_INTR_FLAG_IRAM, &_interruptHandle);
      if(error != ESP_OK)
        for(size_t i = 0; i < ESC_CHANNEL_COUNT; ++i)
          if(_channel[i].attached()) _telemetryStats[i].initError = error;
    }
    if(!_interruptHandle) return; // Do not drive bidirectional TX without turnaround.
  }
#endif
  transmitAll();
}

int EscDriverEsp32::pin(size_t channel) const
{
  if (channel < 0 || channel >= ESC_CHANNEL_COUNT) return -1;
  return _channel[channel].pin;
}

uint32_t IRAM_ATTR EscDriverEsp32::telemetry(size_t channel) const
{
  if (channel < 0 || channel >= ESC_CHANNEL_COUNT) return 0;
  return _channel[channel].telemetryValue;
}

EscDriverEsp32::TelemetryStats EscDriverEsp32::telemetryStats(size_t channel) const
{
  // Diagnostic snapshot only: counters may advance independently on the other core.
  if(channel >= ESC_CHANNEL_COUNT) return TelemetryStats{};
  return _telemetryStats[channel];
}

void EscDriverEsp32::collectTelemetry()
{
#if defined(ESP32P4)
  readTelemetry();
#endif
}

bool EscDriverEsp32::telemetryAttempted(size_t channel) const
{
  if(channel >= ESC_CHANNEL_COUNT) return false;
#if defined(ESP32P4)
  return _responseAttempted[channel];
#else
  return true; // Preserve legacy ESP32/S3 scheduling.
#endif
}

void EscDriverEsp32::initChannel(int i, gpio_num_t pin, int pulse)
{
  if (pin == -1) return;

  _telemetryStats[i] = TelemetryStats{};
  _channel[i].telemetryValue = 0;

  pinMode(pin, OUTPUT);
  digitalWrite(pin, _dshot_tlm ? HIGH : LOW);

  _channel[i].pin = pin;
  _channel[i].protocol = _protocol;
  _channel[i].pulse = pulse;
  _channel[i].divider = getClockDivider();
  _channel[i].pulse_min = getPulseMin();
  _channel[i].pulse_max = getPulseMax();
  _channel[i].pulse_space = nsToTicks(_interval * 1000);

  // specification 0:37%, 1:75% of 1670ns for dshot600
  //_channel[i].dshot_t0h = getDshotPulse(625);
  //_channel[i].dshot_t0l = getDshotPulse(1045);
  //_channel[i].dshot_t1h = getDshotPulse(1250);
  //_channel[i].dshot_t1l = getDshotPulse(420);

  // betaflight 0:35%, 1:70% of 1670ns for dshot600
  _channel[i].dshot_t0h = getDshotPulse(584);
  _channel[i].dshot_t0l = getDshotPulse(1086);
  _channel[i].dshot_t1h = getDshotPulse(1170);
  _channel[i].dshot_t1l = getDshotPulse(500);
  _channel[i].dshot_tlm_bit_len = (_channel[i].dshot_t0h + _channel[i].dshot_t0l) * 4 / 5;

  instances[i] = this;

  rmt_channel_t rx_ch = (rmt_channel_t)RMT_ENCODE_RX_CHANNEL(i);
  bool splitted = i != rx_ch; // rx and tx are separated

  if(_digital && _dshot_tlm)
  {
    // setup RX
    rmt_config_t rxconf = RMT_DEFAULT_CONFIG_RX(pin, rx_ch);
    rxconf.clk_div = _channel[i].divider;
    rxconf.rx_config.filter_en = true;
    rxconf.rx_config.idle_threshold = nsToTicks(30000); // max bit len, (30us)
    rxconf.rx_config.filter_ticks_thresh = 40; // min bit len, 80 ticks = 1us for div = 1, max value = 255
    const esp_err_t rxConfigError = rmt_config(&rxconf);
    if(rxConfigError != ESP_OK) _telemetryStats[i].initError = rxConfigError;
#if defined(ESP32P4)
    // One bounded frame per request, not a ping-pong stream.
    rmt_ll_rx_enable_wrap(&RMT, i, false);
    // rmt_driver_install() used to do this in rmt_hal_rx_channel_reset().
    // Direct-register operation must release/reset the channel divider too.
    rmt_ll_rx_reset_channels_clock_div(&RMT, 1u << i);
#endif
    gpio_pullup_en((gpio_num_t)pin); // needed ?

    if(splitted)
    {
#if !defined(ESP32P4)
      const esp_err_t rxInstallError = rmt_driver_install((rmt_channel_t)rx_ch, XSFC_RMT_RX_BUFFER_SIZE, ESP_INTR_FLAG_IRAM);
      if(rxInstallError != ESP_OK) _telemetryStats[i].initError = rxInstallError;
#endif
    }
  }

  // setup TX
  rmt_config_t txconf = RMT_DEFAULT_CONFIG_TX(pin, (rmt_channel_t)i);
  txconf.clk_div = _channel[i].divider;
  txconf.tx_config.idle_level = _dshot_tlm ? RMT_IDLE_LEVEL_HIGH : RMT_IDLE_LEVEL_LOW;
  const esp_err_t txConfigError = rmt_config(&txconf);
  if(txConfigError != ESP_OK) _telemetryStats[i].initError = txConfigError;
#if defined(ESP32P4)
  if(_digital && _dshot_tlm)
  {
    // Dedicated TX/RX channels keep the same GPIO matrix routes throughout
    // operation. rmt_config(TX) above disables the pad input, so restore it
    // AFTER both configurations. RX itself remains disabled during TX.
    esp_rom_gpio_connect_in_signal(pin, rmt_periph_signals.groups[0].channels[rx_ch].rx_sig, false);
    gpio_ll_input_enable(&GPIO, pin);
    gpio_ll_set_output_enable_ctrl(&GPIO, pin, false, false);
  }
  rmt_ll_tx_reset_channels_clock_div(&RMT, 1u << i);
  // Synchronous bidirectional TX registers from apply() on the output core.
  // Preserve init-time registration for asynchronous/non-telemetry outputs.
  if(!_interruptHandle && !_dshot_tlm)
  {
    const esp_err_t error = rmt_isr_register(rmtInterrupt, nullptr, ESP_INTR_FLAG_IRAM, &_interruptHandle);
    if(error != ESP_OK) _telemetryStats[i].initError = error;
  }
#else
  const esp_err_t txInstallError = rmt_driver_install((rmt_channel_t)i, splitted ? 0 : XSFC_RMT_RX_BUFFER_SIZE, ESP_INTR_FLAG_IRAM);
  if(txInstallError != ESP_OK) _telemetryStats[i].initError = txInstallError;
#endif

#if 0 && SOC_RMT_SUPPORT_TX_SYNCHRO
  // sync all channels
  if(!_async)
  {
    rmt_ll_tx_enable_sync(&RMT, true);
    _channel_mask |= (1 << i);
    rmt_ll_tx_add_to_sync_group(&RMT, i);
    rmt_ll_tx_reset_channels_clock_div(&RMT, _channel_mask);
  }
#endif

  // install tx_end callback if async pwm or bidir dshot
  if(_async || (_digital && _dshot_tlm))
  {
    // install only once
    if(!_tx_end_installed)
    {
      _tx_end_installed = true;
#if !defined(ESP32P4)
      rmt_register_tx_end_callback(txDoneCallback, NULL);
#endif
    }
    rmt_ll_enable_tx_end_interrupt(&RMT, i, true);
    if (_async)
    {
      txDoneCallback((rmt_channel_t)i, NULL); // start generating pulses
    }
  }
}

void IRAM_ATTR EscDriverEsp32::modeTx(rmt_channel_t channel)
{
  disableRx(channel);
  enableTx(channel);
}

void IRAM_ATTR EscDriverEsp32::modeRx(rmt_channel_t channel)
{
  disableTx(channel);
  enableRx(channel);
}

void IRAM_ATTR EscDriverEsp32::enableTx(rmt_channel_t channel)
{
  gpio_num_t gpio_num = (gpio_num_t)_channel[(size_t)channel].pin;

  //gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT);
#if defined(ESP32P4)
  // Matrix routes and input enable are permanent on P4. Only the pad output
  // enable changes; the RX channel was disabled by modeTx().
  gpio_ll_output_enable(&GPIO, gpio_num);
#else
  gpio_ll_input_disable(&GPIO, gpio_num);
  gpio_ll_output_enable(&GPIO, gpio_num);
  esp_rom_gpio_connect_out_signal(gpio_num, rmt_periph_signals.groups[0].channels[channel].tx_sig, false, false);
#endif
  rmt_ll_enable_tx_end_interrupt(&RMT, channel, true);
}

void IRAM_ATTR EscDriverEsp32::disableTx(rmt_channel_t channel)
{
  rmt_ll_tx_stop(&RMT, channel);
}

void IRAM_ATTR EscDriverEsp32::enableRx(rmt_channel_t channel)
{
  // NOTE: time critical function, execution must not exceed 5-6us
  gpio_num_t gpio_num = (gpio_num_t)_channel[(size_t)channel].pin;

  //gpio_set_direction(gpio_num, GPIO_MODE_INPUT);
#if defined(ESP32P4)
  // Release immediately; do not reprogram the GPIO matrix in the turnaround
  // ISR. Input is already enabled and routed to this dedicated RX channel.
  gpio_ll_output_disable(&GPIO, gpio_num);
#else
  rmt_channel_t rx_ch = (rmt_channel_t)RMT_ENCODE_RX_CHANNEL(channel);
  gpio_ll_input_enable(&GPIO, gpio_num);
  gpio_ll_output_disable(&GPIO, gpio_num);
  esp_rom_gpio_connect_in_signal(gpio_num, rmt_periph_signals.groups[0].channels[rx_ch].rx_sig, false);
#endif

  //rmt_rx_start((rmt_channel_t)i, true);
#if defined(ESP32P4)
  // modeTx() has already disabled RX and its interrupt before this TX.
  // RX/TX have separate memory blocks on P4; no TX data needs clearing.
  // readTelemetry() uses the hardware write count, never stale trailing words.
  rmt_ll_rx_set_mem_owner(&RMT, channel, RMT_LL_MEM_OWNER_HW);
  rmt_ll_rx_reset_pointer(&RMT, channel);
  rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_RX_DONE(channel) | RMT_LL_EVENT_RX_ERROR(channel));
#else
  rmt_ll_rx_set_mem_owner(&RMT, channel, RMT_MEM_OWNER_RX);
  rmt_ll_rx_reset_pointer(&RMT, channel);
  rmt_ll_clear_rx_end_interrupt(&RMT, channel);
  rmt_ll_enable_rx_end_interrupt(&RMT, channel, true);
  _rmt_rx_zero_mem(channel, 2); // clear first item of rx buffer to avoid reading tx items
#endif
  rmt_ll_rx_enable(&RMT, channel, true);
  _telemetryStats[channel].rxArmed++;
}

void IRAM_ATTR EscDriverEsp32::disableRx(rmt_channel_t channel)
{
  //rmt_channel_t rx_ch = (rmt_channel_t)RMT_ENCODE_RX_CHANNEL(channel);

  //rmt_rx_stop((rmt_channel_t)channel);
  rmt_ll_enable_rx_end_interrupt(&RMT, channel, false);
  rmt_ll_rx_enable(&RMT, channel, false);
}

void IRAM_ATTR EscDriverEsp32::txDoneCallback(rmt_channel_t channel, void *arg)
{
  auto instance = instances[channel];
  if(!instance) return;
  instance->_telemetryStats[channel].txDone++;
  if(instance->_async)
  {
    instance->transmitOne(channel);
  }
  if(instance->_digital && instance->_dshot_tlm)
  {
    instance->modeRx(channel);
  }
}

void IRAM_ATTR EscDriverEsp32::transmitOne(uint32_t i)
{
  if (!_channel[i].attached()) return;
  if (_digital)
  {
    writeDshotCommand(i, _channel[i].pulse);
  }
  else
  {
    writeAnalogCommand(i, _channel[i].pulse);
  }
  transmitCommand(i);
}

void IRAM_ATTR EscDriverEsp32::transmitAll()
{
#if !defined(ESP32P4)
  readTelemetry();
#endif
  for (size_t i = 0; i < ESC_CHANNEL_COUNT; i++)
  {
    if (!_channel[i].attached()) continue;
    if (_digital)
    {
      writeDshotCommand(i, _channel[i].pulse);
    }
    else
    {
      writeAnalogCommand(i, _channel[i].pulse);
    }
  }
  for (size_t i = 0; i < ESC_CHANNEL_COUNT; i++)
  {
    if (!_channel[i].attached()) continue;
    transmitCommand(i);
  }
}

void IRAM_ATTR EscDriverEsp32::readTelemetry()
{
  for (size_t i = 0; i < ESC_CHANNEL_COUNT; i++)
  {
    if(!_digital || !_dshot_tlm) continue;
    if(!_channel[i].attached()) continue;

    rmt_channel_t rx_ch = (rmt_channel_t)RMT_ENCODE_RX_CHANNEL(i);

#if defined(ESP32P4)
    _responseAttempted[i] = _responsePending[i];
    if(!_responsePending[i]) continue;
    _responsePending[i] = false;
    // RX LL indices are 0..3; memory/GPIO indices are encoded as 4..7.
    _channel[i].telemetryValue = 0; // A missing response must not replay old RPM.
    const uint32_t status = rmt_ll_rx_get_interrupt_status_raw(&RMT, i);
    auto& diagnostic = _telemetryStats[i];
    diagnostic.rxStatus = status;
    diagnostic.rxWords = rmt_ll_rx_get_memory_writer_offset(&RMT, i);
    diagnostic.rxHardware = rmt_ll_rx_get_status_word(&RMT, i);
    diagnostic.lineLevel = gpio_ll_get_level(&GPIO, _channel[i].pin);
    if(status & RMT_LL_EVENT_RX_ERROR(i)) diagnostic.rxErrors++;
    if(!(status & RMT_LL_EVENT_RX_DONE(i)))
    {
      diagnostic.missing++;
      continue;
    }
    disableRx(static_cast<rmt_channel_t>(i));
    rmt_ll_rx_set_mem_owner(&RMT, i, RMT_LL_MEM_OWNER_SW);
    size_t count = rmt_ll_rx_get_memory_writer_offset(&RMT, i);
    if(count > SOC_RMT_MEM_WORDS_PER_CHANNEL) count = SOC_RMT_MEM_WORDS_PER_CHANNEL;
    // Read volatile peripheral memory into a bounded local buffer.
    uint32_t capture[SOC_RMT_MEM_WORDS_PER_CHANNEL];
    for(size_t n = 0; n < count; ++n) capture[n] = RMTMEM.chan[rx_ch].data32[n].val;
    rmt_ll_clear_interrupt_status(&RMT, RMT_LL_EVENT_RX_DONE(i) | RMT_LL_EVENT_RX_ERROR(i));
    if(!count)
    {
      diagnostic.missing++;
      continue;
    }
    const uint32_t value = extractTelemetryGcr(capture, count, _channel[i].dshot_tlm_bit_len);
    auto& stats = _telemetryStats[i];
    stats.packets++;
    stats.items = count;
    stats.firstItem = capture[0];
    stats.gcr = value;
    if(gcrToRawValue(value) == INVALID_TELEMETRY_VALUE)
    {
      stats.invalid++;
    }
    else stats.valid++;
    _channel[i].telemetryValue = value;
#else
    RingbufHandle_t rb = NULL;
    if(ESP_OK != rmt_get_ringbuf_handle((rmt_channel_t)rx_ch, &rb)) continue;

    size_t rmt_len = 0;
    rmt_item32_t* data = (rmt_item32_t*)xRingbufferReceive(rb, &rmt_len, 0);
    if (data)
    {
      uint32_t value = extractTelemetryGcr((uint32_t*)data, rmt_len >> 2, _channel[i].dshot_tlm_bit_len);
      auto& stats = _telemetryStats[i];
      stats.packets++;
      stats.items = rmt_len / sizeof(rmt_item32_t);
      stats.firstItem = rmt_len >= sizeof(rmt_item32_t) ? data[0].val : 0;
      stats.gcr = value;
      if(gcrToRawValue(value) == INVALID_TELEMETRY_VALUE) stats.invalid++;
      else stats.valid++;

      vRingbufferReturnItem(rb, (void *)data);
      
      _channel[i].telemetryValue = value;
    }
#endif
  }
}

void IRAM_ATTR EscDriverEsp32::writeAnalogCommand(uint32_t channel, int32_t pulse)
{
  Slot &slot = _channel[channel];
  int minPulse = 800;
  int maxPulse = 2200;
  if (_protocol == ESC_PROTOCOL_BRUSHED)
  {
    minPulse = 1000;
    maxPulse = 2000;
  }
  pulse = constrain(pulse, minPulse, maxPulse);
  int duration = _map(pulse, 1000, 2000, slot.pulse_min, slot.pulse_max);

  int count = 2;
  if (_async)
  {
    int space = slot.pulse_space - duration;
    slot.setDuration(0, duration, 1);
    slot.setDuration(1, space, 0);
    slot.setTerminate(2, 0);
    count = 3;
  }
  else
  {
    slot.setDuration(0, duration, 1);
    slot.setTerminate(1, 0);
  }

  _rmt_fill_tx_items((rmt_channel_t)channel, _channel[channel].items, count, 0);
}

void IRAM_ATTR EscDriverEsp32::writeDshotCommand(uint32_t channel, int32_t pulse)
{
  if(_digital && _dshot_tlm)
  {
    modeTx((rmt_channel_t)channel);
  }

  const bool command = pulse >= -5 && pulse <= -1;
  const int tone = command ? -pulse : 0;
  if(command) _channel[channel].pulse = 1000; // One-shot, including async TX callbacks.
  pulse = constrain(pulse, 0, 2000);
  // scale to dshot commands (0 or 48-2047)
  int value = command ? tone : dshotConvert(pulse);
  uint16_t frame = dshotEncode(value, _dshot_tlm, command);

  Slot& slot = _channel[channel];
  for (size_t i = 0; i < DSHOT_BIT_COUNT; i++)
  {
    int val = (frame >> (DSHOT_BIT_COUNT - 1 - i)) & 0x01;
    slot.setDshotBit(i, val, _dshot_tlm);
  }
  slot.setTerminate(DSHOT_BIT_COUNT, _dshot_tlm);

  _rmt_fill_tx_items((rmt_channel_t)channel, slot.items, Slot::ITEM_COUNT, 0);
}

void IRAM_ATTR EscDriverEsp32::transmitCommand(uint32_t channel)
{
#if defined(ESP32P4)
  if(_dshot_tlm) _responsePending[channel] = true;
  if(_async || _dshot_tlm) rmt_ll_enable_tx_end_interrupt(&RMT, channel, true);
#endif
  _rmt_tx_start((rmt_channel_t)channel, true);
}

uint32_t EscDriverEsp32::getClockDivider() const
{
  switch (_protocol)
  {
    case ESC_PROTOCOL_PWM:        return 24;
    case ESC_PROTOCOL_ONESHOT125: return 4;
    case ESC_PROTOCOL_ONESHOT42:  return 2;
    case ESC_PROTOCOL_MULTISHOT:  return 1;
    case ESC_PROTOCOL_BRUSHED:    return 8;
    default: return 1;
  }
}

uint32_t EscDriverEsp32::getPulseMin() const
{
  switch (_protocol)
  {
    case ESC_PROTOCOL_PWM:        return nsToTicks(1000 * 1000);
    case ESC_PROTOCOL_ONESHOT125: return nsToTicks(125 * 1000);
    case ESC_PROTOCOL_ONESHOT42:  return nsToTicks(42 * 1000);
    case ESC_PROTOCOL_MULTISHOT:  return nsToTicks(5 * 1000);
    case ESC_PROTOCOL_BRUSHED:    return 0;
    case ESC_PROTOCOL_DSHOT150:
    case ESC_PROTOCOL_DSHOT300:
    case ESC_PROTOCOL_DSHOT600:
    default:
      return 0;
  }
}

uint32_t EscDriverEsp32::getPulseMax() const
{
  switch (_protocol)
  {
  case ESC_PROTOCOL_PWM:        return nsToTicks(2000 * 1000);
  case ESC_PROTOCOL_ONESHOT125: return nsToTicks(250 * 1000);
  case ESC_PROTOCOL_ONESHOT42:  return nsToTicks(84 * 1000);
  case ESC_PROTOCOL_MULTISHOT:  return nsToTicks(25 * 1000);
  case ESC_PROTOCOL_BRUSHED:    return nsToTicks(_interval * 1000);
  case ESC_PROTOCOL_DSHOT150:
  case ESC_PROTOCOL_DSHOT300:
  case ESC_PROTOCOL_DSHOT600:
  default:
    return 2048;
  }
}

uint32_t EscDriverEsp32::nsToTicks(uint32_t ns) const
{
  int div = getClockDivider();
  // multiply by 2 as DURATION_CLOCK is aleredy doubled, and round to nearest integer
  return (ns * 2 + ((div * DURATION_CLOCK) >> 1)) / div / DURATION_CLOCK;
}

uint16_t EscDriverEsp32::getDshotPulse(uint32_t width) const
{
  switch (_protocol)
  {
    case ESC_PROTOCOL_DSHOT150: return nsToTicks(width * 4);
    case ESC_PROTOCOL_DSHOT300: return nsToTicks(width * 2);
    case ESC_PROTOCOL_DSHOT600: return nsToTicks(width * 1);
    case ESC_PROTOCOL_BRUSHED:
    case ESC_PROTOCOL_PWM:
    case ESC_PROTOCOL_ONESHOT125:
    case ESC_PROTOCOL_ONESHOT42:
    case ESC_PROTOCOL_MULTISHOT:
    default:
      return 0;
  }
}

bool EscDriverEsp32::isDigital(EscProtocol protocol) const
{
  switch (protocol)
  {
    case ESC_PROTOCOL_DSHOT150:
    case ESC_PROTOCOL_DSHOT300:
    case ESC_PROTOCOL_DSHOT600:
      return true;
    default:
      return false;
  }
}

#endif
