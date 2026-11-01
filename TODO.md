# TODO XSFC

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
