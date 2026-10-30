# Стенд і наступні досліди

## A. Доступно зараз: SDR + хости

1. Визначити physical SDR identities, antenna/fixture connections і точну peer waveform; не вважати AWUS1900 чи original7020 кваліфікованим WFB peer лише через enumeration. Не пряме TX→RX з'єднання без перевіреного link budget/attenuation і board input limit.
2. Один owner плати; lock/handoff для RF/gain/sample-rate/interface changes. Перед зміною зберегти config і rollback. За відсутності контрольованого джерела зробити read-only snapshot, не називати його RF PASS.
3. RX-first: packet sequence IDs від незалежного відомого peer, host UTC плюс DUT uptime/boot ID. Якщо timestamps між hosts не синхронізовані — latency оцінювати RTT або вказати synchronization uncertainty.
4. Записати unique RX/duplicates/gaps, goodput, host socket/kernel drops, CPU/IRQ/DMA counters і reset events; переповнення до point of measurement тримати окремо. 0 RX не має діагнозу без known stimulus.
5. Gain test: initial profile -> один дозволений manual/AGC variation -> restored profile. TX/gain/threshold конфігурації змінювати окремо, не піднімати CCA як заміну RF-захисту. Fast AGC already present не означає optimized under blockers.
6. Відносне порівняння двох антен/орієнтації/положення або кабелю лише за незмінного peer traffic; відокремити fading/time variation повторенням baseline. Без калібрування результат у relative loss/goodput, не absolute dBm sensitivity/NF.
7. Якщо відоме альтернативне штатно сумісне PSU доступне: A→B→A з однаковими RF/cable/load conditions; перепідключення фізично виконує оператор. Це кореляція supply variant, НЕ вимірювання ripple. Спочатку встановити дозволений voltage/input/polarity/reverse-current path двох USB-C.

## B. Потрібне оснащення: абсолютні RF і живлення

- RF: calibrated source/measurement reference, wanted/unwanted sources або validated equivalent, combiner/isolation, calibrated attenuators/cables і verified safe input. Записати reference plane на DUT connector, losses vs frequency, mismatch, source noise/harmonics, analyzer overload.
- Wanted-only PER criterion визначити до sweep; потім blocker offset/level/waveform/duty cycle одним фактором. Desense — зміна required wanted power при тому самому PER, не IIP3. Окремо in-band/OOB, LMT/ADC overload.
- Power: осцилограф/щуп з відомою BW/noise floor, ground spring або належний differential probe. DC-coupled startup/loadstep та окремий ripple capture з визначеною BW. Не подавати rail на50Ω безпосередньо; не замикати невідому землю через PE.
- Для LDO: actual part, load/headroom/temp, PSRR на частотах ripple, Cout/ESR stability. Для LC: impedance/damping/startup/loadstep/DCdrop/saturation. Номінали до вимірювання/схеми не обирати.

## C. Етапи розробки

1. Вимоги + identity + метрики: завершення = methods/owners/gaps записані.
2. Архітектура: розділити packet gateway/IQ mode/mesh L2; timing/bandwidth budgets checked.
3. Моделі: synthetic quantization тепер; RFIC nonlinear/power models тільки після перевірки applicability. Simulation не закриває physical RF gate.
4. Hardware options: reversible RF preselector/attenuator/LNA bypass та supply A/B після frequency/input/power constraints. Functional BOM поки не закупівельний: BPF passband/IL/rejection/power; attenuator loss/power; rated PSU/cable; fixtures — exact parts OPEN.
5. Firmware candidate: isolated branch/build hashes, no unrelated kernel/HDL edits; RAM first. Dual decoder synthesis після utilization/timing budget.
6. Acceptance: video/MAVLink/tracking concurrent, mesh separately then coexistence; defined loss/deadline/temperature/blocker/supply envelope. PASS тільки для tested configuration/envelope.

## D. Evidence record

Для кожної спроби: test ID, revision/hash, UTC/uptime, instrument/calibration, fixture, stimulus, expected criterion, actual raw files, uncertainty/packet statistics, result PASS/FAIL/BLOCKED/NOT_RUN, rollback і next hypothesis. Після двох однакових невдач змінити гіпотезу або спосіб вимірювання.

## Поточні ризики

- Analog compression до ADC навіть без digital clipping; mitigation: preselection/gain study.
- I8 втрачає wanted detail при великому blocker; mitigation: baseline precision плюс окремий filtered/scaled IQ mode.
- Невідомий power path/regulators, USB ground/backfeed; mitigation: schematic/measurement before modification.
- Неточний RTC; mitigation: host UTC + monotonic/boot IDs.
- Shared LO/single PHY; mitigation: versioned capability contract, no fake dual-radio flags.
- No calibrated instruments; block лише абсолютні claims, продовжувати digital/relative tests.
