# TODO XSFC

## Final upload and publication, Nov 3

- Added Russian rate setup guide `docs/P4_LOOP_RATES_RU.md`: 2/4/8kHz,
  independent dividers, rollback, actual-rate checks, ESC/Blackbox limitations.
- Final P4 build and 19 Python checks passed. App-only upload at0x10000;
  separate verify-flash confirmed matching digest. USB watchdog reset still
  reports Windows OSError433 after programming/verification, board boots.
- Before upload board already had gyro_rate4000,pid_sync1,mixer_sync1.
  Preserved it rather than forcing prior2kHz baseline. All411 settings equal
  before/after,debugNONE,FLASH/PSRAM. Runtime30s:4982valid/0bad/0timeouts,
  actual gyro/PID/mixer4kHz,accel500Hz. First subsequent dump timed out,
  retry succeeded. No settings save, ARM, motor or erase commands.
- Configurator MSP suite60passed/1optional analysis skipped; production build
  passed. Local editor changes, reference sources, caches and unreferenced
  XSFC.ico deliberately excluded from publication.

## Accel ownership / P4 logging (local, hardware-unvalidated)

- [x] P4 CLI `blackbox_flash_mode DIRECT|PSRAM`, persisted in EEPROM v9,
  applied at boot; default PSRAM. v8 migration retains gyroRate and all prior
  fields, excludes old tail padding. DIRECT does not allocate PSRAM or use the
  deferred-save LOAD interlock, but restores synchronous in-flight flash stalls.
- [ ] Hardware verify both modes, v8 migration, dump/save/reboot and readback.
- [x] Successful PSRAM save emits DShot BEEP1/2/3, 500ms apart, only with
  disarmed/stopped motors. No success tone on failure, boot repair or DIRECT.
  Hardware verify ESC tone support/audibility; no new user parameter.

- [x] Remove the 0.75..1.25g / 100ms Mahony guard at user request.
- [x] Queue a copy of each raw accel sample; filter/fusion/Blackbox consume
  processed accel on CPU0. Fixes a concrete raw/filtered shared-vector race.
  Does not prove all measured 3..7g values came from that race.
- [x] Stage P4 Blackbox in PSRAM; commit on close only while disarmed and
  motors stopped, including EOF-close handling. Reads include staged data.
  Allocation failure disables flash logging rather than using blocking writes.
  Power loss before commit loses the current recording. Commit can take seconds.
- [x] Extend CLI registry: receiver thresholds/provider, masks, all PID slots,
  all filter components, telemetry; fix unsigned16 printing and Param copying.
- [ ] Hardware verify accel at rest/motor vibration and active-log timing;
  confirm save/download/reboot, full-storage behavior and dump/restore parity.
- [x] Pending flash save blocks arming via LOAD; failed commit keeps interlock.
  Hardware verify interlock/readback; do not unplug before commit finishes.

## 2026-11-02 flight-log findings

- [x] Parse both supplied logs without invalid frames; see FLIGHT_LOG_ANALYSIS_20261102.md.
- [x] Confirm invalid signed D-term Hz disable LPFs; add MSP/UI overflow guards.
  Existing bad settings are not automatically repaired; source not reproduced.
- [x] Add missing CLI serialrx_provider for future dump round-trips.
- [x] XSFC-configurator filenames use XSFC board identity, not BTFL wire variant.
- [ ] Reproduce ARM OR duplication (supplied dumps/decoder have only one ARM).
- [ ] Isolate measured PID timing shortfall (3059/3339 vs nominal4000 Hz),
  including synchronous flash writes; second flight has bidirectional DShot OFF.
- [ ] Prop-free restore/readback and valid-filter logging tests; no further flight yet.

## Selectable gyro rates and RPM defaults

- [x] Add persisted CLI gyro_rate2000/4000/8000, capped by bus/sensor;
  pid_sync remains independent divider. Latest P4 default SPI2k/PID2k/mixer2k.
  Append field with EEPROMv8 migration retaining v7 configuration prefix.
- [x] RPM filter disabled by default (harmonics0); saved settings unchanged.
  RMT hardware RX glitch filter is unrelated and retained.
- [ ] Hardware verify v7 migration, save/reboot/readback and actual2/4/8k
  gyro/PID combinations. PID8k telemetry limitation remains unresolved.

## Blackbox / PID-configurator regression investigation

- [x] Supplied XSFC.txt is a current/post-reset snapshot: blackbox_dev NONE,
  DShot telemetry off, gyro SPI pins29/30, PID divider2. It cannot identify
  the exact pre-reset regression or prove PID/rate corruption.
- [x] Blackbox rate index0 is full-rate, not disabled; device NONE disables.
  Add bounded legacy I/P-ratio conversion shared by init and MSP response,
  explicit sample-index response byte and optional write support for XSFC1.43.
- [x] Configurator audit: Angle edits incorrectly used PID row3 (ALT) instead
  of XSFC LEVEL row7; PID tab save lacked required XSFC reboot; D labels
  implied unsupported D-min/D-max. Fixes local in XSFC-configurator only.
- [x] P4 build, native Blackbox rate ASan/UBSan test,4 Python checks and
  13 configurator CPU/MSP tests passed. Actual flash logging/download and
  PID/Angle save-reboot-readback remain hardware checks, not proven fixed.
- [ ] Verify selected FLASH device, nonzero flash capacity/free space,
  start/stop log, download and parse. Flash erase/test commands not run.
- [ ] Compare before/after config on each PID/Angle/rates edit; no bad-run dump
  available. Do not assert these confirmed UI defects explain every symptom.

## multicore_tests — cleanup and SPI preparation

- [x] User reports PID8kHz/mixer4kHz did not improve errors; output frequency
  alone does not explain regression. No verified PID8kHz telemetry fix.
- [x] P4 defaults updated: SPI gyro2kHz, pid_sync1/mixer_sync1; DShot600
  bidirectional, latest flight PID/filter values and verified SPI pins.
  Gyro/PID2/4/8kHz remain selectable; saved settings are not overwritten.
  I2C first-use path retains its existing pid_sync1 behavior at gyro2kHz.
- [x] Final P4 build and ICM42688 ASan/UBSan test passed, plus4 Python
  checks. Hardware PID4kHz telemetry~1% is user-reported, not flight validation.

- [x] CPU counters now follow physical cores: CPU0 PID/mixer, CPU1 gyro
  on dual-core targets; MSP XC ordering updated together. Single-core update
  retains CPU0. No task migration or timing instrumentation added.
- [ ] Known limitation: user reports near100% bidirectional DShot errors
  on DShot300 AND600 despite some RPM. Check mixer_sync2/4 with gyro/PID8kHz
  to isolate output/receive window. Missing or bad samples remain real errors;
  retained last-good RPM does not prove every response arrived.
- [x] Hardware follow-up: setting PID4kHz reduces reported telemetry errors
  to~1%. This also reduces mixer rate, so CPU contention vs RX-window timing
  is not isolated. Next test: pid_sync1/mixer_sync2 (gyro/PID8kHz, mixer4kHz).

- [x] User confirms ICM42688 SPI detection after saved pin mapping
  MOSI29/MISO30/SCK28/CS32; stats reports4kHz, CPU1 work~18%, CPU0~32%.
  These wall-time percentages do not validate ESC timing or prior reset issue.
- [x] User-requested P4 SPI gyro cap raised to8000Hz for bench test;
  I2C cap remains2000Hz. Saved PID/mixer dividers unchanged, no profiling added.
- [ ] Validate8kHz acquisition/PID/output timing and bidirectional DShot
  telemetry on hardware. If unstable restore SPI cap4000Hz; PID/mixer can
  be reduced independently with pid_sync2. No flight validation claimed.

- [x] P4 4kHz I2C experiment cancelled by user: gyro cap restored to2000Hz.
  I2C ceiling800kHz retained (default i2cSpeed already800); saved settings,
  combined reads and other targets unchanged. No profiling reintroduced.
- [ ] Validate 800kHz/2kHz timing, delayed NACK and ESC stability on bench.
  Not flight validated; restore I2C ceiling400kHz if bus experiment is reverted.

- [x] Restore ICM42688 combined accel+gyro reads by default on accel-due
  cycles; retain 6-byte gyro-only reads and single-use cache/short-read safety.
- [x] Remove analysis-only FlightTiming, cpustats, task snapshots, event
  timestamps, I2C profiling, section maxima and added atomic accumulators.
  Keep baseline stats, CPU status extension, ibatinfo and checked IMU reads.
- [x] Restore main PID polling, queue and mixer scheduling exactly; remove
  experimental consumer notifications/wait path, not just disable a flag.
- [ ] SAFETY: intermittent ESC sounds/reset reported; actual reset vs DShot
  dropout/beacon unknown. Rollback is mitigation, NOT verified repair.
  Validate disarmed on bench first; no flight validation claimed.
- [ ] Delayed I2C unexpected NACK / ESP_ERR_INVALID_STATE remains unresolved;
  no automatic retry/reset or bus speed change added.
- [x] Retain 360MHz; 400MHz experiment cancelled.
- [ ] SPI wiring/module mode confirmation and hardware validation required.
  Default P4 pins: SCK28, MOSI30, MISO29, CS32. Motors33/49/50/52 unchanged.
- [x] Cleanup P4 production build passed; native ICM42688 ASan/UBSan
  tests passed (6/12-byte reads, cache invalidation, short reads and mocked
  explicit-CS dispatch), plus 3 Python checks. Hardware still unvalidated.

- [x] Follow-up hardware report: user confirms stable operation after restart.
  Supplied escinfo shows configured/effective telemetry=1, all four outputs
  init_error=0, DSHOT300. This confirms initialization, not telemetry under load.
  Constant-zero current explained by saved ibat_source NONE (scale 100,
  offset 0); select ADC explicitly, do not silently enable disabled sources.
- [x] Regression checks: exhaustive 4096-payload DShot decoder/capture test
  with ASan/UBSan; P4 and non-P4 mode-ID/mask tests; current enable/conversion
  native tests; 3 Python layout/pin checks. P4 production build passed.

- [ ] User hardware report: ESC CUR connected to P4 GPIO21 reads ~0.8 A at
  maximum motor output. ESC identified as Skystars KO55II 55Ax4 25V 3-6S,
  running Bluejay; exact revision CUR sensitivity remains unconfirmed.
  Verify ESC CUR sensitivity, ibat_scale/offset and
  calibrated ADC millivolts before claiming correct amperage. No actual
  reference current supplied. Fixed source/pin guard; UI calibration adapted
  to XSFC multiplicative scale, hardware calibration still pending.

- [x] ESP32-P4: AUX GPIO mode excluded from advertised/accepted modes;
  GPIO5 startup writes and runtime AUX output disabled only for P4. Stored
  mode IDs and ESP32/S3 implementation preserved.
- [x] Dual-core MSP_STATUS_EX extension: versioned XC tail exposes physical
  core 0/1 measured XSFC work time in 0.1%; legacy aggregate retained.
  P4 production build passed; hardware values/mode migration not yet validated.
- [ ] Validate dual-core timing on hardware; these are instrumented work-cycle
  timings, not complete RTOS/ISR CPU utilization. Stats cross-core sampling
  is inherited and not a synchronized system-wide profiler.

- [ ] XSFC-configurator: отдельная копия в соседней папке; по уточнению
  пользователя сохранены исходные формы/вкладки Betaflight, только перекраска
  и логотип. Замена вкладок урезанными формами отменена. Совместимость каждого
  поля с API 1.43 и аппаратный save/reboot/read-back ещё требуют проверки;
  сборка интерфейса не подтверждает поддержку записи настроек.

- [x] Удалена постоянная DShot runtime diagnostics по запросу пользователя:
  TX/RX/packet/missing/invalid counters драйвера, чтение RX hw/words/GPIO
  только для escinfo, сохранение последнего GCR/items и повторная CRC decode.
  escinfo оставляет только pins/config/effective/init_error; status пишется
  при инициализации, не на каждом кадре. Штатные errors/RPM в Mixer и их
  CRC, RX_DONE polling и необходимое чтение RMT memory сохранены.
- [ ] Hardware: проверить RPM и штатную статистику ошибок без runtime
  diagnostics; RX sequence/IRQ core/priority в этом изменении не трогались.

- [x] По явному запросу пользователя восстановлено состояние ESC-кода
  непосредственно ДО сообщения о нулевой ошибке DShot300: static GPIO routes,
  IRQ allocation на core 0, TX_DONE batch fast path, RX arm trim, idle 30 мкс,
  только исходные missing counters (без empty/partial), без recorder.
  Предыдущий recovery к более раннему callback-пути отменён этим запросом.
  Сообщение о сбое/перезапусках остаётся нерасследованным; восстановление
  состояния не является подтверждением устранения причины сбоя.

- [ ] BLOCKER hardware: после последних диагностических сборок пользователь
  сообщает ошибку FC и самопроизвольные перезапуски ESC. Нет panic/reset log,
  точная причина и реально установленный образ пока не установлены.
- [x] Recovery: отменены последующие TX_DONE batch fast path, RX arm trim,
  missing_empty/partial diagnostics; восстановлены callback→TX_STOP→полная
  прежняя подготовка RX и idle 30 мкс. Сохранены static GPIO routes и IRQ
  allocation на output/core 0, после которых hardware показал улучшение.
  Это возврат к ранее проверенному пути, не доказанное исправление reboot.
- [ ] Сначала подтвердить стабильность recovery без моторного теста,
  получить текст ошибки FC/reset reason. Не продолжать tuning до проверки.

- [x] Уточнено hardware: Q-H-40, Bluejay 0.21.0, PWM 24 кГц. DShot600
  имеет remaining missing; DShot300 после первых секунд выхода из нуля
  показывает 0% error. Не объявлять DShot600 неподдерживаемым/не переключать
  протокол автоматически. Проверены исходники tag v0.21.0: wait100ms при
  старте и Wait_For_Timer3 packet preparation присутствуют в этой версии.
- [x] escinfo: missing_empty/missing_partial разделяют missing по hardware
  RX word count (0 / >0). Только task-side counter при уже missing, без
  кадров/окон/timestamps/ISR overhead. Empty не доказывает отсутствие
  электрического ответа, partial не доказывает полный кадр; missing total
  и CRC/error accounting сохранены.
- [ ] Hardware DShot600: escinfo с новыми counters после устойчивой тяги,
  чтобы проверить empty vs partial и не менять decoder/тайминги вслепую.

- [x] Свежий hardware escinfo после устойчивого вращения: TX_DONE=RX_ARMED
  97513 на каждом выходе; packets=valid 87248/87091/86825/86841,
  invalid=0 и rx_errors=0 на всех. Missing=10264/10421/10687/10671.
  Текущий остаток — отсутствие завершённого RX, не GCR/CRC errors.
  Накопительные missing включают startup/переходы; это не steady rate.
  Равенство tx_done/rx_armed не доказывает своевременность RX.
- [ ] Уточнить установленную версию/target Bluejay и проверить её путь
  telemetry packet preparation при вращении; upstream main не считать
  автоматически совпадающим с установленной ESC firmware. Не ослаблять
  CRC/не подавлять missing/не менять decoder при invalid=0.

- [x] Hardware feedback RX idle 80 мкс: заметного улучшения нет. Возвращён
  исходный 30 мкс, чтобы не добавлять задержку завершения захвата без пользы.
- [ ] Получить актуальный escinfo после устойчивого вращения с новым IRQ
  core/fast path: отделить remaining missing от invalid. Старые bad captures
  сделаны до устранения baseline errors и не доказывают текущую причину.
  Upstream Bluejay готовит telemetry частями в Wait_For_Timer3; ISR отвечает
  только при Flag_Telemetry_Pending. Возможность отсутствия готового пакета
  при commutation не считать доказанной причиной без свежих missing counters
  и сверки установленной версии ESC. Не добавлять recorder/не скрывать errors.

- [x] Hardware feedback RX arm trim: без явного изменения, возможно немного
  меньше ошибок. Не считать остаточную проблему решённой.
- [x] P4 RX idle threshold увеличен 30→80 мкс: тот же порог ограничивает
  ожидание первого фронта и конечный idle. Проверка гипотезы раннего RX_DONE
  при задержанном ответе ESC; CRC/decoder/request rate/IRQ не менялись.
- [ ] Hardware: сравнить steady и transient error с RX idle 80 мкс;
  проверить отсутствие missing/RPM regression. Гипотеза пока не доказана.

- [x] P4 RX turnaround: убраны повторный rx_disable (уже выполнен modeTx),
  повторное отключение RX IRQ и очистка двух RX words от TX (на P4 память
  раздельная, чтение ограничено hardware writer offset). RX_DONE/ERROR
  очищаются одним mask write. Owner/reset/enable остаются после TX_DONE;
  приоритет/core IRQ, CRC и legacy ESP32/S3 путь не менялись.
- [ ] Hardware: проверить следующую оптимизацию RX turnaround: 0% в покое,
  steady error ~1–1.5% и переходные всплески до ~19% — предыдущий baseline,
  не обещать их устранение без замера. Регистратор не возвращать.

- [x] Hardware feedback TX_DONE fast path: после стабилизации тяги error
  иногда падает до ~1–1.5%; при изменении тяги остаются всплески до ~19%.
  Не считать переходные ошибки устранёнными и не подавлять счётчик.
- [x] Проверен default dynamic notch P4: FEATURE_DYNAMIC_FILTER уже входит
  в XSFC_FEATURE_MASK, ModelConfig использует её по умолчанию. Добавлен
  regression guard и инструкция для сохранённой конфигурации; коэффициенты
  не менялись, принудительное включение при каждом boot не добавлялось.
- [ ] Hardware: включить DYNAMIC_FILTER в существующей конфигурации,
  сохранить и проверить состояние после reboot; PID loop должен быть >=1 кГц.

- [x] Hardware feedback: после переноса RMT IRQ на mixer/core 0 пользователь
  наблюдает 0% error при нулевой тяге и примерно двукратное снижение под
  тягой. Поддерживает гипотезу задержек обслуживания, но не доказывает
  причину всех оставшихся ошибок. Не возвращать IRQ на gyro/core 1.
- [x] P4 synchronous bidirectional TX_DONE: отдельный ISR fast path,
  batch acknowledge/mask завершённых каналов, enableRx без повторного
  TX_STOP/config-update и общего callback; txDone counters после RX arming.
  Нулевой end marker уже завершил non-looping TX. RX preparation sequence,
  IRQ priority и legacy/async callback сохранены.
- [ ] Hardware: сравнить остаточный error rate на устойчивой тяге после
  TX_DONE fast path; проверить RPM всех выходов, отсутствие регрессии.

- [x] Hardware feedback: static GPIO routing не дал заметного изменения
  error rate; не считать оптимизацию исправлением причины.
- [x] P4 bidirectional RMT ISR registration перенесена из initChannel()
  (gyroTask/core 1, вместе с I2C/GPTimer) в первый apply() перед TX
  (PID/mixer/core 0). ESP-IDF закрепляет IRQ за allocating core.
  Регистрация однократная, без смены priority/RX sequence; при ошибке
  не запускать bidirectional TX без turnaround, init_error доступен в escinfo.
- [ ] Hardware: проверить перенос IRQ на output core: RPM всех четырёх,
  baseline error и зависимость от устойчивой тяги. Причина роста error
  пока не доказана; host GCR tests не проверяют interrupt latency.

- [x] P4: GPIO matrix TX/RX маршруты для bidirectional DShot настроены один
  раз после rmt_config RX/TX; pad input остаётся включённым, RX channel
  отключён во время TX. В turnaround ISR теперь только release output
  и прежняя подготовка RX, без повторного ROM matrix routing. Убрана
  лишняя работа из последовательного обслуживания четырёх каналов.
  IRQ priority, порядок RX reset/owner/enable и ESP32/S3 путь сохранены.
- [ ] Hardware: проверить RPM и error в покое/при устойчивой тяге после
  static GPIO routing. Поздний turnaround остаётся гипотезой; устранение
  роста до ~18% не подтверждено. Регистратор не возвращать.

- [x] Auto-trigger hardware capture: worst window ~6.43–6.53 s,
  commands 1035–1077, 200 missing / 201 attempts on all four ESC, invalid=0.
  Этот эпизод — отсутствие завершённых RX, не ошибки GCR.
  Upstream Bluejay wait_for_start_nonzero вызывает wait100ms без подготовки
  новых telemetry packets; ISR отправляет ответ лишь при Telemetry_Pending.
  Это объясняет ~100-мс паузу при старте из остановки (нужна сверка версии ESC),
  но не доказывает причину скачков при уже вращающихся моторах или baseline
  ~2.45% invalid. Не подавлять эти missing в статистике автоматически.

- [x] По запросу пользователя удалена временная запись худших окон,
  frozen/last-bad captures, дополнительные micros() в TX/ISR и команда
  сброса записи. Удалены её helper, host test и инструкция; результаты
  предыдущего аппаратного замера выше оставлены как история диагностики.
  Базовые счётчики escinfo (missing отдельно от invalid) сохранены.
- [ ] Hardware: проверить сборку без регистратора на отсутствие регрессии.
- [ ] Устойчивое увеличение тяги повышает error rate примерно до 18%
  даже вне запуска/резких переходов. Причина не установлена; стартовая
  wait100ms Bluejay и BEEPER этого не объясняют. Не маскировать ошибки
  и не ослаблять CRC; RMT sequence/IRQ priority пока не менять.

- [ ] Hardware: после восстановления RX ~2.4–2.5% invalid в покое,
  при резком увеличении RPM error временно 40–60%; BEEPER около 62.5%.
  В upstream Bluejay Modules/DShot.asm BEEP1–5 выполняются с IE_EA=0,
  затем wait200ms до включения interrupts: провал телеметрии во время beep
  ожидаем, точный процент зависит от версии/длительности/интервала.
  Не смешивать его с причинами invalid при обычном вращении.

- [x] Отменена неудачная оптимизация turnaround (RX preparation before TX
  и IRQ level 3): пользователь получил 100% errors и зависания. Возвращён
  предыдущий путь с рабочими RPM/~2.45% invalid; bidirectional не отключён.
- [ ] Не считать причину обрезанных capture доказанной: после изменения
  общей тяги ошибки временно растут, в покое ~2.4–2.5%. Нужны измерения
  задержки ISR и полные ошибочные capture под нагрузкой, без одновременной
  смены IRQ priority и последовательности конфигурации периферии.

- [x] По двум hardware-замерам: missing M1–M3 не растёт, M4 +1 за
  ~19710 TX; invalid +483 (~2.45%) синхронно на всех выходах. Bad captures
  убывают от 5 до 2 items M1→M4: вероятно поздний RX turnaround, не доказано.
  Попытка подготовки RX перед TX и IRQ level 3 отменена после hardware-регрессии.
- [ ] Проверить изменение invalid rate и длины bad captures после сокращения
  TX→RX latency; CRC-проверка сохранена, EDT пока не добавлен.

- [x] P4 telemetry processing: сбор RX перенесён перед разбором Mixer,
  учитываются только отправленные запросы; missing отделён от invalid GCR,
  временное сохранение ошибочного capture впоследствии удалено.
- [x] GCR extraction ограничен 21 битом, terminal HIGH idle не добавляется
  как лишние биты; overlong LOW/interior runs отклоняются. Все 4096 payload
  DSHOT300/600 с синтетическим idle-tail проверены ASan/UBSan — PASS.
- [ ] Измерить изменение реального error rate после исправления порядка
  чтения и границ GCR: исходные ~2.4% invalid пока не объяснены capture-данными.

- [x] По hardware escinfo: TX done/RX armed растут, RX packets=0 на всех
  четырёх выходах. P4 direct backend дополнен сбросом RX/TX clock dividers
  (ранее выполнялся при driver_install) и явным GPIO-контролем output enable
  после ROM matrix routing для half-duplex release. Добавлены rx_raw/rx_hw,
  rx_words/rx_errors/line в escinfo. Нужна повторная аппаратная проверка;
  причина отсутствия RX этими данными ещё не доказана.

- [x] P4 bidirectional DShot: убран временный запрет, legacy RMT ISR заменён
  собственным TX-done ISR без FreeRTOS API; RX читается один раз за кадр из
  RMT memory без ring buffer, auto-rearm и ping-pong. Телеметрия использует
  synchronous output. ESP32/S3 сохраняют прежний backend.
- [x] Host regression GCR: все 4096 payload, обратное differential/GCR
  кодирование и синтетические RMT runs для DSHOT300/600, ASan/UBSan — PASS.
- [ ] Проверить новый P4 RMT backend на плате: отсутствие interrupt WDT,
  packets/valid в escinfo, RPM и ошибки на четырёх Bluejay ESC. Предыдущая
  диагностическая сборка d94a285d2 зависала в legacy ISR → xQueueGiveFromISR
  → spinlock; точный механизм блокировки не доказан.

- [x] Добавлена CLI-диагностика `escinfo`: init errors, TX done, RX armed,
  RX packets, valid/invalid GCR и последний захваченный RMT item.
- [ ] Подтвердить исправление bidirectional DShot на P4: на пользовательской плате моторы
  вращаются при включённой телеметрии, но Configurator показывает 100% ошибок
  на всех четырёх выходах. Сначала локализовать потерю ответа через `escinfo`;
  наличие RX-кода и успешная сборка не подтверждают работу телеметрии.

- [x] Общий технический README: архитектура, подсистемы, драйверы, CLI,
  платформы, сборка, диагностика и тесты;
  GPL-3.0-only для оригинальных изменений XScythelin и third-party notices.
- [ ] Завершить аудит происхождения/лицензий bundled code, в частности
  GPLv2 Kalman.h и совместимость с GPLv3; комплектовать распространяемые
  копии полными текстами применимых лицензий.

- [x] ESC BEEPER: стандартная DShot BEEP1-команда через Mixer при disarm
  и нулевом газе, раз в 500 мс, telemetry-request bit, one-shot async TX.
  Документация: docs/ESC-BEEPER.md; host tests расписания и DShot encoding.
- [ ] Проверить BEEPER через ESC на P4 без пропеллеров: ON/OFF, отсутствие
  вращения, тест моторов и bidirectional DShot. Startup Doom после reset
  контроллера остаётся отдельной аппаратно-непроверенной проблемой.

Общий список отложенных задач. Выполненные пункты отмечаем, новые добавляем сюда.

## IMU и I2C

- [x] I2C: перед первым register transfer к адресу выполняется address-only
  probe; отсутствующие адреса не получают register write/read. Успешные
  адреса кэшируются, ошибки рабочего обмена не подавляются.
- [x] Явный выбор IMU ограничивает поиск указанной моделью на SPI/I2C.
- [ ] Проверить на P4 исчезновение стартовых transaction NACK при AUTO
  с ICM42688 на 0x69; реальные ошибки проводки/рабочего обмена остаются видны.

- [x] ICM42688: I2C-обнаружение 0x68/0x69, добавление в поиск Hardware,
  исключение несовместимого MPU slave-bus, проверка полного чтения образцов
  и ошибок reset; C++ fake-bus regression tests.
- [ ] ICM42688: подтвердить на плате I2C 0x68/0x69 при CS=3.3V,
  стабильность 2 кГц опроса на P4 и отсутствие регрессии SPI 4 кГц.

- [x] AUTO для gyro_dlpf: ICM20948 включает DLPF, остальные IMU сохраняют
  прежний режим; существующие сохранённые значения не переписываются.
  Команды, частоты и ограничения программной фильтрации: docs/ICM20948.md.

- [x] ICM20948: проверять каждый обмен при запуске, окончание reset с тайм-аутом,
  ключевые регистры чтением обратно, bank 0 и контрольные gyro/accel-образцы.
- [x] ICM20948: реализовать собственные getRate()/setRate() вместо MPU6050.
- [x] ICM20948: исключить MPU-совместимый gyroSlaveBus.
- [ ] ICM20948: внутренний магнитометр
  подключать только через реализацию его собственной карты банков/регистров.
- [x] При явно выбранной модели IMU ограничить обнаружение этой моделью;
  проверить отсутствие записей в неподходящий датчик при остальных пробах.
- [ ] Все IMU: проверить контракты readGyro/readAccel во всех драйверах — успех
  только при полном образце, выходной вектор не изменять при ошибке.
  Затем учитывать статус в GyroSensor/AccelSensor, не передавать неудачные
  образцы в калибровку/фильтры/fusion; добавить время последнего успешного
  чтения, счётчики ошибок и безопасную реакцию на потерю IMU.
- [ ] Проверить read-modify-write помощники BusDevice: отрицательный результат
  чтения не должен трактоваться как успех или приводить к записи мусора.
- [ ] Добавить подробную диагностику этапов и попыток запуска IMU и проверить
  запрет арминга при отсутствии/потере корректных данных.
- [x] ICM20948: SPI-обнаружение, конфигурация 1 МГц / запрос 7 МГц при чтении,
  без общего readFast 16 МГц. Режимы 256/EX: gyro DLPF bypass 9 кГц;
  остальные: DLPF 1125 Гц / делитель, с отдельным режимом accel.
- [x] Регрессионные C++ тесты ICM20948 выполнены в WSL с ASan/UBSan:
  запуск I2C/SPI, тёплый reset, тайм-аут reset, ошибочный ID/запись,
  короткие/отрицательные чтения, неизменность вектора при ошибке,
  signed-декодирование, режимы DLPF/делитель и запрос SPI 7 МГц.
- [ ] Проверить фактический SCK, режимы DLPF и стабильность ICM20948 на железе.
- [ ] DRDY-синхронизация / FIFO: текущий таймерный опрос не гарантирует
  получение каждого образца 9 кГц. Не поднимать частоту PID автоматически.
- [ ] Проверка на железе без пропеллеров: холодное включение, десятки reboot,
  save/reboot, USB с полным снятием питания и USB при питании от аккумулятора.
  Проверить освобождение SDA после прерванной транзакции и отказ при SCL LOW.
- [ ] Измерить загрузку I2C при потолке 400 кГц и высоких частотах опроса;
  согласовать частоту опроса с пропускной способностью без изменения PID.

Ограничения: восстановление I2C выполняется только при запуске до обнаружения
датчиков. Не добавлять автоматический reset/reinit IMU в полёте без отдельного
проектирования безопасной реакции. PID, полётные фильтры и bypass не менять
в рамках исправлений надёжности запуска.

## ESP32-P4

- [x] Разметить полные 16 МиБ flash: приложение 4 МиБ, данные 11.875 МиБ,
  coredump 64 КиБ, прежние адреса NVS/otadata. PSRAM включена профилем платы
  и SDK (malloc); реальные объёмы flash/heap/PSRAM доступны через devinfo.
- [ ] Переход PWM/DShot с legacy RMT на новый API с проверкой сигналов на железе.
- [ ] Подтвердить на плате отсутствие watchdog-ошибок и ответы MSP после
  последнего исправления запуска; проверить подключение Configurator.
## CLI / reconnect

- [x] По запросу пользователя default ESC GPIO для P4: M1–M4 = 33, 49, 50, 52.
  CS SPI-барометра отключён (-1), чтобы не конфликтовать с M1 на GPIO33.
  Таблица default UART/I2C/SPI/ESC: docs/ESP32-P4-PINS.md.
  Сохранённые настройки не мигрируют автоматически.

- [x] `save` возвращён к прежнему варианту без `Rebooting`: сохранение,
  flush, restart, ручной Disconnect/Connect. Автоматический reconnect
  в BFC Desktop 10.10.0 на P4 зависал (проверка пользователем).
- [x] По запросу пользователя отменён весь эксперимент CLI/reconnect:
  удалены resume, Rebooting handshake, gyro-only калибровка с уведомлением,
  связанные экспериментальные документация и source-level тесты.
  Исходная команда cal gyro и прежнее поведение CLI восстановлены.
  Изменения ICM20948, DLPF и памяти сохранены.
## Current measurement follow-up (2026-11-01)

- [x] Add read-only `ibatinfo` CLI diagnostics: saved source/pin/scale/offset,
  last raw sample, estimated ADC millivolts, unfiltered and filtered amps.
  No additional ADC read or automatic recalibration. ESP32-P4 build passed.
- [ ] Current accuracy remains unvalidated. User measured ~0.07 V at GPIO21/GND
  while UI showed approximately 0.9 A; scale100 predicts 0.7 A at that voltage.
  Readings were approximate, not synchronized. At rest ibatinfo raw/mV/A were zero.
  No reference current supplied; do not infer required amps from motor heating.
  Raw-to-volts scaling is approximate and ESC CUR sensitivity remains unknown.
# P4 startup recovery (2026-11-02)

## Follow-up at 20:02

### Historical 2 kHz baseline follow-up

### Hardware application and bench verification

### USB diagnosis continued (user reconnected board)

- Added `test/imu/soak_board.py`: read-only, disarmed precondition, preserves complete USB byte stream so panic text is not discarded by MSP parsing.
- Pre-fix 300-second soak: 49,632 valid IMU replies, zero checksum errors, 3 timeouts; no Guru/panic/ROM/assert markers. Does not resolve the previously observed reboot.
- Found independent multi-core memory-safety defect: `BuzzerState` used unchecked shared `idx++`/`--idx` across producers on both cores and consumer. Replaced storage with bounded non-waiting `Utils::TryStack`; overlapping notifications may be dropped instead of corrupting shared stack state. Retains LIFO behavior. NOT established as cause of observed panic.
- Added native two-producer stress test (400,000 push attempts); normal and ASan/UBSan runs passed in WSL. TSan could not run due to runtime unexpected-memory-mapping error; do not count as a pass.
- P4 build and 17 Python contract tests passed. Flashed application at 0x10000, verified hash; USB re-enumeration error after watchdog reset again, then soak successfully started. No settings/log erase or motor/ARM commands.
- Post-fix 300-second soak: 50,341 valid IMU replies, zero checksum errors, 4 timeouts; full panic/reboot captured around 164 seconds. Buzzer fix did NOT resolve the rare crash.
- Matching coredump (ELF SHA prefix `3262a18ad`) places the crash in `Input::filterInputs`, on gyroTask/core1, outside interrupt context. Loop index reached 3484 instead of 16; `_input._model` was corrupted. Task stack had 7824 bytes free; no evidence of stack exhaustion. Originating write remains unknown.
- Temporary P4 diagnostic firmware installs STORE watchpoints on `input.channelCount` and the first word of `_input` on both cores. A further 300-second read-only soak was launched. Matching ELF/bin preserved as `xsfc-watchpoints-firmware.{elf,bin}` in the approved temporary directory. Remove diagnostic watchpoints before a normal flight build; do not recommend flight while corruption remains unresolved.
- Flight/motor-load and long-log persistence/tone checks remain unperformed.
- First watchpoint soak completed: 300 seconds, 49,640 valid replies, zero checksum errors, 3 timeouts; zero Guru/panic/ESP-ROM/assert markers. Reply RTT median/p99/max 0.737/1.208/6.037 ms (not end-to-end sensor latency). Corruption was not reproduced; this is NOT a fix or flight clearance. Started a further 1800-second read-only soak on the same diagnostic build, output prefix `xsfc-soak-watchpoints-30min` in the approved temporary directory; result pending.
- Continued RC audit: `InputCRSF::parse` accepts 64-byte wire frames but `CrsfMessage` was only 62 bytes (`58+1` payload/CRC bytes plus 3 header bytes). Fixed RX storage to 64 bytes without changing the TX fragmentation budget; added a static assertion and guarded-storage regression for every accepted frame size. Overflow would hit the following channel storage; no evidence yet that it caused the observed `input.channelCount` corruption.
- Added CRSF/MSP length validation before header reads and subtraction, rejected oversized declared MSP messages, validated continuation state, reset the MSP read cursor on new starts, and added a last-line capacity check to `MspMessage::append`. Reject truncated packed RC channel frames instead of decoding stale payload bytes. Tests `test_crsf_frame.cpp` and `test_crsf_msp_bounds.cpp` passed under ASan/UBSan; P4 build and 17 Python tests passed. These changes are local and NOT flashed while the 1800-second watchpoint soak is active. Matching on-board diagnostic ELF/bin remain preserved separately.
- ICM42688 review confirms only ODR/range/power registers are explicitly configured; AAF/UI filters remain at reset settings and `setDLPFMode`/`setRate` are no-ops. INAV reference explicitly programs banked AAF coefficients and UI bandwidth. Do not guess equivalent legacy ICM20948 settings or change them during corruption diagnosis; register verification and measured noise/latency comparison still needed.
- Removed the remaining direct cross-core `DEBUG_GYRO_RAW` read in the gyro filtering path: `Xsfc::updateOther` now passes both processed-input and raw snapshots from the same queued event through SensorManager to GyroSensor. Single-core/no-argument paths retain existing behavior. Extended event-copy regression to cover both gyro vectors; ASan/UBSan test and P4 build passed. Not flashed while the existing diagnostic soak owns COM26. This addresses sample consistency, not the unproven origin of RC memory corruption.
- The 1800-second watchpoint soak completed: 297,707 valid replies, zero checksum errors, 13 timeouts, no panic/ROM/assert markers reported. Initial/final status identical. No corruption reproduced; NOT proof of root-cause resolution.
- Subsequently flashed prepared CRSF/MSP bounds and raw gyro snapshot changes at application offset 0x10000, hash verified. Watchdog reset again produced the known USB re-enumeration exception after verification; application boot confirmed by a 60-second probe: 9,967 valid replies, zero checksum errors/timeouts, no panic markers. No ARM/motor/settings/log-erase commands. Temporary watchpoints still installed; remains a diagnostic, not flight-cleared build. Matching ELF/bin saved as `xsfc-crsf-snapshot-firmware.{elf,bin}` in the approved temporary directory. No further long soak launched automatically.
- Added `TimingProbe` (single-writer atomic CLI-readable lifetime maxima), gyro/PID interval probes, completed-read-to-dequeue age and coalesced timer notification count. New `stats` fields explicitly exclude SPI acquisition and sensor/UI latency. Added wraparound regression; ASan/UBSan passed. P4 build passed (initial private-access compile error corrected via public recording method); application flashed/hash verified, matching artifacts saved as `xsfc-timing-firmware.{elf,bin}`. Watchpoints retained.
- Timing build first 60-second probe: 9,918 valid replies, zero errors/timeouts. Separate CLI stats read had only ~15,654 intervals since startup; do NOT attribute its maxima to that entire preceding soak. Added `soak_board.py --stats` to capture stats within the same USB connection before CLI exit/reboot.
- Continuous 60-second probe with end-of-run stats: 9,898 valid replies, zero checksum errors, one timeout, no panic/ROM/assert markers. Lifetime counts ~206,430 gyro/PID intervals; gyro max 513 us, PID max 521 us, completed-read-to-dequeue max 22 us, coalesced ticks 0; CPU360MHz, gyro/PID/mixer2kHz, accel500Hz. These are observed disarmed USB maxima, NOT flight/flash-save worst-case bounds. CLI exit reboots without saving. No new long soak running.
- Added cached startup register diagnostics to ICM42688 (bank switching ONLY before sampling; bank0 restored), and verify power/ODR/range readback before marking ready. Board registers: bank0 4E/4F/50/51/52 = `0F 03 03 16 11`; bank1 0B..0E = `A0 0D AA 80`; bank2 03 = `30`. Gyro AAF coefficients 13/170/8 correspond to ~585Hz in the datasheet table; hardware filtering is NOT disabled. No filter/ODR retuning performed. Readback failure regression and existing driver tests pass ASan/UBSan.
- Incremental post-disarm PSRAM commit now verifies each 512-byte write by raw partition readback/memcmp before advancing; journal still follows data, verification failure leaves save pending/failed and suppresses success. P4 build and 17 Python contract tests pass (snapshot contract updated for timestamped event and paired raw snapshot). Physical long-save/EOF/persistence/tone still UNTESTED; USB probe cannot exercise this path without recording.
- Latest application flashed/hash verified, artifacts `xsfc-verified-save-firmware.{elf,bin}` preserved. 15-second USB probe: 2,490 valid replies, zero errors/timeouts/panic markers; observed gyro/PID interval maxima 513/524us, dequeue age23us, coalesced ticks0. Watchpoints remain; rare corruption root cause unresolved. No ARM/motor/log erase/settings save. No background soak running.
- Added native `test_flashfs_persistence.cpp` compiling the ACTUAL production `BlackboxFlashfs.cpp` with isolated host-only partition/PSRAM stubs under `test/imu/flashfs_stubs`. NOR model uses bitwise-AND programming (cannot restore 0 to 1 without erase). ASan/UBSan passed: >2MiB patterned log and bytewise persistence verification, <=512-byte/one-write-per-service bound, no flash writes while simulated flight active, data-before-journal ordering, append second log without modifying first, full data-partition EOF and restart, all32 journal slots and extra-write rejection, write failure and readback mismatch retain pending/failed state and suppress completion. Erases in this test affect ONLY the host vector, never hardware. Does not cover physical cache-off timing/watchdogs, actual BBL framing/encoder, power loss, or audible ESC completion. No firmware changes/flash needed for this host-only test; board remains on verified-save diagnostic build.
- User requested a stopping point and removal of temporary runtime instrumentation. Removed both-core watchpoints, TimingProbe/atomic timing counters and recording calls, gyro event timestamp, register diagnostic virtual API/cache/bank reads/CLI output. Removed unused timing-only header/test. Kept minimal 3-byte IMU startup readback, immutable sample queues, memory fixes and post-disarm flash verification as production safeguards. Host tests remain outside firmware.
- Added production RC bounds: clamp device channel count at begin; invalid runtime count marks input invalid and enters stage2 failsafe; bound channel loops, setInput/getFailsafeValue, CRSF copy length, RSSI access; initialize temporary channel array and reject invalid remaps. These prevent the observed OOB cascade but do NOT establish the originating corruption writer. No claim of root cause solved.
- Clean-runtime P4 build and 19 Python tests passed; ICM42688 native ASan/UBSan test passed. Application flashed at0x10000/hash verified, `xsfc-clean-firmware.{elf,bin}` saved in approved temp directory. 60-second USB probe: 9,970 valid replies, zero errors/timeouts/panic markers; gyro/PID2000Hz, mixer~1997..2000Hz, accel~500Hz. Reported average CPU load12.1% versus ~15% instrumented (different runs, not a controlled benchmark). No background test running.
- Fresh read-only backup `xsfc-clean-settings.txt` has411 settings. Compared to old `xsfc-final-baseline-20261102.txt`, observed accel offsets -48/-439/-56 -> -44/-496/-53, DShot telemetry1->0, Blackbox NONE->FLASH. Not modified or reverted by this cleanup; preserve as potentially user-origin changes. Do not assume bidirectional telemetry currently enabled. No settings save/defaults/ARM/motor/log erase performed.
- Stopping point: runtime instrumentation removed, clean-runtime USB smoke test passed. NOT flight clearance: prior rare memory-corruption source remains unproven, clean build only short-tested, real receiver/failsafe, motor-load, long flash save/cache/watchdog/ESC tone and new BBL flight behavior remain unverified. Recommend prop-free validation before any airborne trial.
- Latest flight `XSFC_BLACKBOX_LOG_20261102_223921.BBL` analyzed; see final section of `FLIGHT_LOG_ANALYSIS_20261102.md`. Exactly2kHz overall/27,031 valid frames/zero invalid, but t2..26s processed accel norm median3.265g/p95 6.170g/max11.844g. Sub50Hz power dominates, stationary ends ~1g. Queue fix did not eliminate loaded accel issue. Header acc_lpf_hz1500 is existing Hz*100 encoding, NOT active1500Hz; fresh read-only411-setting dump confirms BIQUAD15 and unchanged calibration/trims. USB100-sample read-only MSP probe gives ~1g/nearzero gyro, RC centers1500; physical level/transmitter validity unknown. No tuning, firmware flash, save/calibration/ARM/motors/log erase. Driver samples accel snapshots at500Hz from8kHz ODR before software LPF and leaves AAF/UI reset defaults; aliasing/loaded data integrity are hypotheses, not proven root cause. Rare corruption remains unresolved.
- Following user approval, ICM42688 accel hardware filtering now explicitly configured at startup: AAF213Hz (Bank2 03/04/05=0A/19/A0), UI ODR/40=200Hz and3rd order (Bank0 52 high nibble7,53 bits4:3=2). TDK DS-000347 rev1.9 downloaded/extracted in approved temp directory; verified coefficients §5.3 and UI fields §14.40/14.41. Gyro nibble/DEC2/reserved bits preserved, gyro filters unchanged. All modified values read back; failed Bank2 path attempts Bank0 restore. No added runtime sampling/probes or magnitude guards; ODR8kHz/combined reads/software LPF15/PID/fusion/calibration unchanged. This addresses inadequate pre-host-decimation filtering, NOT proof of flight root cause solved.
- Updated bank-aware fake-bus driver tests pass ASan/UBSan: coefficients/UI fields, fail/dropped writes for each AAF register, Bank0 restoration, gyro/DEC2 preservation, existing combined-read/error/address tests. P4 build and19 Python tests passed. App only flashed0x10000/hash verified, new `xsfc-accel-aaf-firmware.{elf,bin}` saved alongside prior clean firmware. New BIN SHA256=15728a5c992f7354cb9387c95e61a1237dbdc134c7f1f25d4be95492f48a777a. Incremental build retains old CLI compile date22:31:57; distinguish via hash, NOT date.
- New-board USB60s:9905 valid/0bad/1timeout, average CPU12.2%, gyro/PID2kHz/accel500Hz. Separate400/400 valid IMU reads, RTT median1.74ms/max2.77ms; stationary accel median[5,20,2048],gyro[0,0,0]. Initial immediately reopened backup got empty MSP reply; retry succeeded. Before/after411 settings exactly equal. No settings save, ARM, motor commands, calibration or log erase. No loaded verification or new flight yet; rare memory corruption remains unproven. See docs/ICM42688.md for filter details/limits.
- New user flight23:05:39 after accel AAF/UI change: user reports best flight, initial vertical/level takeoff. Parsed39,700 valid/0invalid frames,39.699s/exact2kHz/log-end valid. t2..38s norm median1.036g/p95 1.670g/max2.366g vs prior t2..26s3.265/6.170/11.844g; horizontalRMS2.779->0.367g(7.57x lower), meanXYZ near[0,0,1]g; absolute pitchRCmean128.59->21.86. New t2..6s pitch nearly centered(absmean0.0385); later corrections grow but multi-g issue does not return. Final landing-section peak6.62g, not steady-flight maximum. Strong observed improvement, uncontrolled trajectories; residual accel fluctuations/horizon and rare corruption not solved. Detailed comparison in FLIGHT_LOG_ANALYSIS_20261102.md. No tuning/settings/flash/board operations for this analysis. Preserve working firmware; future attitude-debug recording can discriminate residual horizon behavior, do not add always-on instrumentation.
- User approved attitude-debug preparation. On COM26 checked disarmed, backed up411 settings to `xsfc-before-attitude-debug-settings.txt`, explicitly set ONLY `debug_mode AC_CORRECTION`, verified debug logging1/PSRAM, saved (CLI confirmed Saved), rebooted and obtained411-setting `xsfc-after-attitude-debug-settings.txt`. Diff: debug NONE->AC_CORRECTION and gyro_offset_z7->8; latter is automatic Model::preSave() copying current runtime gyro bias (not an explicit calibration/offset command). No other differences. Firmware unchanged, no ARM/motors/log erase. Existing Fusion debug[0..2] = estimated roll/pitch/yaw in0.1deg; debugNONE can be restored after diagnosis. This selectable debug adds3 conversions per fusion update and log payload, NOT zero overhead; no watchpoints/probes reinstated. Next BBL needed to verify actual debug fields and residual horizon; physical attitude remains unknown without user observation.
- Flight23:15:01 decoded32,025 valid/0invalid frames/32.024s/exact2kHz; AC_CORRECTION Euler present. t2..30s accel norm median1.090g/p95 1.699g/max2.589g,horizontalRMS0.450g. CenteredRP subset median estimated angles[-0.2,-0.2]deg, abs95[2.7,4.0] including transients. Feedback sign opposes estimated angle in>99.7% zeroRC/nonzeroangle samples; median gain4.444/s. Multiple centered ~1s segments return estimated pitch/roll nearzero (pitch-9.2->-0.9deg t8.085..9.119s). This verifies control against estimated attitude, NOT physical horizon accuracy/position hold. Detailed analysis in flight document. No board/settings changes; awaiting pilot observation of remaining behavior before tuning.
- Following intermittent forward-horizon report: audited Mahony timestep500Hz/rad-s input/accel event fusion ownership, no simple rate/unit mismatch found. Added offline analyze_attitude.py reconstructing approximate Pcorrection and Euler-rate gyro integral; t10..12s pitch delta-minus-gyro~4.40deg, mean reconstructed Pcorrection~2.04deg/s, consistent with substantial accel correction (not proof of wrong physical attitude). No magnitude guards/gain/LPF/PID/trim changes.
- Found independent helper_3dmath invSqrt strict-aliasing/long-width bug: nativeLP64 ASan catches8byte read from4byte float. Fixed via uint32_t+memcpy with unchanged positive-input32bit results. ESP32 long32bit: NOT established cause of P4 rare corruption/drift. Actual Mahony host test added (inverse sqrt accuracy/equivalence,30deg integration,+/-20deg convergence at500/1000/2000Hz),-O2 ASan/UBSan passes. P4 build+19 Python tests pass. NOT FLASHED; board remains best-flight accel AAF firmware with AC_CORRECTION. No hardware/settings operations during this audit.
- Completed actual-code Mahony offline replay of23:15 log,16,013 rows, ASan/UBSan pass. Approximate startup/PT1/gyro-decimation/log-phase; baseline estimatedEuler match RMSroll0.207/pitch0.512deg,peak0.441/1.026deg. LowerP/gyro-only alter estimate but no groundtruth means not demonstrably better; do NOT tune based on closeness/deviation. Production synthetic test confirms0.1g sustained horizontal accel can cause~6deg apparent pitch with zero real rotation, returns after clean accel. This is inherent ambiguity, not implementation defect. Tools: prepare_mahony_replay.py/replay_mahony.cpp. Full results in flight document. No firmware/settings/board operations. Independent known-level prop-free orientation measurements now needed to distinguish static frame/math mismatch from loaded acceleration effects; another same-channel flight alone cannot settle it.
- User-assisted prop-free static checks completed via read-only MSP/capture_attitude.py (disarmed each sample): initialRP0.0/-0.1deg; held nose-down-0.1/+25.3, return+0.1/+0.1; nose-up+0.2/-16.6,return0.0/-0.1; rightroll+15.2/-0.5,return+0.3/0.0; leftroll-8.3/-0.5,finalreturn-0.2/-0.1. Held accel agrees, gyro~zero. One emptyMSP reply after nose-up, retry passed. No gross static axis/sign error or multi-degree settled offset reproduced. Angles not independently measured; disconnected sequential captures do NOT validate dynamic recovery/yaw coupling or loaded behavior. No flash/settings/calibration/ARM/motors/log erase. Evidence and limitations in flight document; board remains accelAAF firmware+AC_CORRECTION.
- Continuous prop-free capture v2 completed6645samples/180s/0MSPgaps/maxdt39.1ms, same USB connection. User tilted almost90deg rather than15..25; Euler coupling nearvertical is not by itself failure. All4returns nearbaseline, final10s RPmedian[0.1,-0.2]deg; stationary-level runs initially up to~1.1deg transient then settled. No persistent horizon shift reproduced unloaded. See flight document. No board settings/flash/motors. Initial capture aborted/lost samples on missingMSP; script now streams/flushed JSONL, finally saves JSON, same-connection framing recovery and5consecutive-failure stop, disarmed each sample. No independent motor-loaded attitude reference; cannot infer flight fixed.
- Nov3 user requested flight-runtime cleanup, explicitly keep Blackbox. Set debug_modeNONE (disable Euler conversions/debug payload), retained FLASH+PSRAM. Verified disarmed and backed up411settings before/after. Removed unconditional micros() call in Blackbox update (now only DEBUG_PIDLOOP). Previously removed watchpoints/timing probes remain absent. Kept selectable debug APIs/host tests (inactive or not linked), production stats, RC bounds, IMU startup readback/AAF and verified Blackbox save/interlock. Included previously host-tested invSqrt uint32/memcpy UB fix with same intended32bit math; no fusion/PID/filter retuning.
- P4 build+19 Python tests passed; app flashed0x10000/hash verified; artifacts xsfc-flight-cleanup-firmware.{elf,bin} saved alongside best-flight fallback. USB30s4975valid/0bad/0timeouts/no panic,gyro/PID2kHz/accel500Hz. Postreboot dump411settings: debugAC_CORRECTION->NONE; gyro_offsetsXYZ(-4,-7,8)->(-6,6,-3) automatically captured by Model::preSave current runtime bias, no explicit offset/calibration command. All other settings equal, including FLASH/PSRAM,PID,accel calibration/filters. No ARM/motors/log erase. Initial script equality check rejected CRLF normalization mismatch before save; fixed parsing and retried. No background capture running. Flight horizon issue/rare corruption remain unproven; cleanup is not safety clearance.

- User authorized flashing/applying/checking. Backed up live `dump all` and raw NVS 0x9000/0x5000 to `C:/Users/Scythelin/AppData/Local/Temp/opencode/xsfc-before-baseline-20261102.txt` and `xsfc-nvs-before-baseline-20261102.bin`.
- Flashed current `.pio/build/esp32p4/firmware.bin` at 0x10000 only; esptool verified hash. Final USB reset reported re-enumeration error, but application subsequently responded and ran normally. No log erase, ARM, motor commands or calibration command sent.
- Applied baseline CLI, verified every requested field in live dump, saved, rebooted and re-read. Actual configuration changes: gyro LPF1 0->100, LPF2 0->213, dynamic LPF 0/0->170/425. Existing rates/PIDs/other baseline fields already matched. Gyro offsets x/y changed by 1 stored unit during automatic startup calibration; accel calibration/pins/PIDs preserved.
- First 400 RAW_IMU replies: all valid 24-byte payloads, median/p99/max RTT 1.738/2.066/2.779 ms. Stationary accel norm min/mean/max 1.00197/1.00313/1.00491 g; gyro +/-0.1 deg/s. Repeat 400/400 valid. Not an end-to-end motion/UI latency test.
- Live stats: gyro read/filter and PID 2000 Hz; mixer output about 1997..2000 Hz; accel/fusion about 500 Hz; IMU read errors 0. These are window averages, not worst-case loop jitter measurements.
- IMPORTANT: one 30-second stress run observed panic handler re-entry and a reboot around 12 seconds. Captured tail in `xsfc-baseline-sustained-20261102.json`. Not explained/resolved. Subsequent 90-second run had 7500 valid replies/no failures. Later 120-second run had 11061 valid replies and one empty-header timeout, no observed reboot (uptime 206.9 s). Do NOT call this flight-ready.
- Read coredump partition to `xsfc-p4-coredump-20261102.bin`; decoder rejects application SHA (dump fc482171b vs current ELF 160dfd47b), so this is not evidence for the latest panic's cause. No partition erased.
- Final verified settings dump `xsfc-final-baseline-20261102.txt`. Logs report used=0; assistant did not erase them. Long-save/tone and loaded motor/flight behavior remain untested.

- Reference is commit `1924668`, OLD ICM20948 driver, not the rewritten driver. Old driver kept hardware DLPF enabled; a 2 kHz control/polling setting is not proof of 2 kHz unique IMU samples.
- Removed P4-specific overrides disabling gyro LPF1/LPF2/dynamic LPF. Added `config/p4_2khz_baseline.cli` containing only explicit `set` commands, historical software filters and supplied current PID gains. Does not reset calibration/pins or save automatically; NOT applied to hardware.
- Baseline CLI registry/non-destructive contract and historical gyro-default checks added. All 17 Python source-contract tests pass. Latest P4 build passes (firmware.bin and merged image rebuilt).
- Latest hardware probe: COM26 absent; `python -m serial.tools.list_ports` reports no ports. No live timing/configuration/IMU measurements possible in this state. No flash or configuration write performed.
- Still inspect/measure consumer queue backlog, timer-notification coalescing, shared loop/mixer timer state and ICM42688 hardware anti-alias filtering. None is established as the cause of the reported flight regression yet.

- Read XSFC_cli_20261102_200203.txt. Referenced XSFC_BLACKBOX_LOG_20261102_200011.BBL is absent from Downloads at analysis time; no new-flight measurements available.
- Changed post-disarm commit to one 512-byte write per service call, preserving data-before-journal ordering and arming interlock. Long-recording persistence and completion tone remain hardware-unverified.
- Queued gyro sampled-vector snapshots now feed consumer filtering; consumer no longer reads the concurrently rewritten sampled vector. Raw gyro debug still reads shared producer state and needs separate cleanup.
- Existing `flash erase` CLI command now refuses armed/motor-test use and describes log-only erase. It was NOT executed.
- P4 build passed using C:/.platformio/penv/Scripts/python.exe; Python contract tests 15 passed. System Python PlatformIO failed with `No platform named win32`; use the PlatformIO virtualenv.
- These follow-up changes are not flashed. Lateral drift, accelerometer vibration/scaling, combined-read hardware coherence and motor noise remain unresolved; do not label flight-fixed.

- Confirmed regression: eager 0xBE0000-byte flash-to-PSRAM startup copy caused repeated HP watchdog resets before the gyro timer/MSP processing started. Internal bounce buffers and mapped bulk copying did not resolve it; exact low-level cache/panic cause remains unconfirmed.
- Removed the eager copy. Existing flash data is mapped read-only, with dirty data/journal ranges overlaid from PSRAM. Only new writes touch staging memory.
- PID consumer waits for initialization completion. Blackbox setup precedes motor-output interrupt installation.
- Flashed application only at 0x10000, hash verified; no NVS/log partition erase requested. Removed temporary ROM diagnostics.
- Hardware verified final image: API 1.43, BTFL variant, XSFC/ESP32P4 identity, status, raw IMU and dataflash summary respond; 60/60 API replies over 15 seconds. Python source-contract suite: 13 passed.
- Still pending: actual PSRAM flight recording/commit/readback, completion tone, DIRECT mode, dump/restore parity, timing/accel flight validation. Startup recovery does not establish flight safety or validate PSRAM writes under sustained load.
