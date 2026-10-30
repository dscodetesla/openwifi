# PlutoSky-R2: завади, точність IQ та живлення

2026-10-30, Europe/Kyiv. Статус: R&D, фізична завадостійкість НЕ кваліфікована.

## Рішення

Зберегти наявну точність RF/OFDM-тракту та його калібрування. Основний runtime — Openwifi packet gateway з WFB FEC/crypto і прикладними сервісами на LAN-хості. I8 RAW IQ — окремий експериментальний transport profile, без заміни робочого PHY.

За сильних позасмугових сигналів пріоритет має RF preselector перед першим перевантажуваним активним каскадом. Якщо є зовнішній LNA, перевірити доцільність його bypass і розташування фільтра. Конкретний фільтр не вибрано: частоти/рівні завади та board-level safe input невідомі. Для внутрішньосмугової завади bandpass, що пропускає корисний канал, не дає вибіркового придушення; потрібні контроль частоти/смуги з peer, антени/рознесення та бюджет SINR. Не підміняти analog protection підвищенням CCA/receiver thresholds.

AGC fast_attack вже активний. Його налаштування порівнювати з manual gain за відомими переданими пакетами; почати без модифікації регістрів. Відсутність ADC clipping не виключає перевантаження LNA/mixer/TIA (ADI UG-570 Rev A, gain-control section).

Живлення: причина не доведена. Не додавати довільні LC/LDO/конденсатори та не підключати друге незалежне USB джерело до перевірки схеми power path. Кандидат виправлення після причинного A/B/A — перевірене джерело/кабель і, за необхідності, узгоджені фільтр та регулятор із перевіркою stability/headroom/startup/thermal.

## Фактично виконано

- Read-only SSH R2 з перевіркою наявного trusted key. Два знімки: [runtime_snapshot.json](runtime_snapshot.json).
- `plutosky_r2`, Linux6.12.0-openwifi, monitor2442MHz/HT20, sample rates40MSPS; Ethernet1000/full.
- RX analog bandwidth25215513Hz, TX25215414Hz; gain modesfast_attack, gains71/67dB у двох знімках. Ці readback значення не є вимірюванням noise figure, RF input power або доказом оптимальності.
- RX28280/TX53 не змінилися за приблизно5.3s. Контрольний RF stimulus не подавався. Нуль delta не доводить ні healthy RX, ні RX stall, ні blocker.
- Boot ID однаковий між двома знімками, uptime близько28.8h. Плата показує1970; chronology вести за host UTC/uptime/boot ID. Не змінювали board clock.
- `/boot` SD vfat mounted RO, rootfsRAM. Це не встановлює джерело поточного boot і не є новою перевіркою QSPI persistence.
- `iw txpower20dBm` — driver-advertised setting; actual TX RF output не виміряно. TX attenuation0dB не є output0dBm.
- Користувач підтвердив: доступні тільки SDR та хости. Осцилограф/RF generator/analyzer/calibrated fixture не підтверджені; абсолютні RF і ripple gates BLOCKED.

## Синтетичний дослід I12/I8

[quantization_study.py](quantization_study.py), NumPy2.3.5, seed20261030; [results](quantization_results.json). 4096 ідеально синхронізованих OFDM symbols,64-point FFT,QPSK52bins; tonebin30 поза корисними піднесучими, але у sampled band. Модель не прив'язана до actual RF sample rate, не моделює analog LPF/compression, real AGC, clocks, PSU або packet decoder/FEC.

| Умова, ideal peak rescale | I12 EVM,dB | I8 EVM,dB |
|---|---:|---:|
| Без blocker | -35.88 | -33.77 |
| Blocker +20dB до wanted RMS | -35.76 | -26.17 |
| Blocker +40dB | -31.01 | -8.58 |

Більше від'ємне EVM означає меншу помилку. Це model result, НЕ виміряний EVM R2 і НЕ RF sensitivity/PER. Ideal peak rescale знає peak усього capture: це сприятливий математичний випадок, не доказ реалізованого AGC. При fixed gain сильний tone спричиняє clipping в обох точностях. I12 тут математична модель12-bit quantizer; нативні12bits RFIC у16bit container не дорівнюють16bit RF resolution або12bit ENOB.

Практичний висновок: спочатку придушувати blocker і встановлювати належний analog gain; цифрова фільтрація/scale перед I8 packetization може зменшити transport error, але не відновить дані після analog compression.

## Пакет і критерії

- [requirements.csv](requirements.csv): вимоги, джерела, методи й відкриті числові параметри.
- [BENCH_PROTOCOL.md](BENCH_PROTOCOL.md): relative bench зараз; calibrated RF/power пізніше.
- [interfaces.json](interfaces.json): інтерфейси/owners та compatibility gates.
- [STATE.json](STATE.json): статуси/блокери/наступна дія.

Не проводили: build нового firmware, RF TX, зміни gain/filter/threshold, IQ capture або I8 deployment, power cycling, QSPI/SD write, physical blocker/ripple tests. Dirty driver changes залишилися без змін.

## Першоджерела

- ADI UG-570 Rev A: https://www.analog.com/media/en/technical-documentation/user-guides/AD9361_Reference_Manual_UG-570.pdf
- ADI AGC methodology (ADRV9361-Z7035 reference experiment, not R2 validation): https://wiki.analog.com/resources/eval/user-guides/ad9361_agc_tuning
- ADI RFIC architecture: https://wiki.analog.com/resources/eval/user-guides/ad9361
- TI input-filter stability: https://www.ti.com/lit/an/snva801/snva801.pdf
- WFB source pin: https://github.com/svpcom/wfb-ng/tree/89f50a361b5bc78f6740e832cdf1680a6ea9a819

Наступна конкретна дія: відтворюваний WFB RX→LAN baseline з незалежним peer; перед TX встановити antennas/fixture/power envelope. Абсолютні blocking/ripple характеристики потребують окремого метрологічного оснащення.

## Межа відтворюваності source
[source_identity.json](source_identity.json) містить HEAD, hash tracked diff і relevant file hashes. Це identity record, не повна копія dirty sources і не доказ provenance поточного running image. Відтворювана в цьому пакеті саме синтетична модель; перед firmware build потрібен повний source freeze.
