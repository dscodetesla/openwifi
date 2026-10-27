# PlutoSky-R2 / Openwifi та WFB-NG: оцінка OFDM-сумісності
26.10.2026, Europe/Kyiv.

**Висновок: технічно обґрунтований кандидат існує; фактична WFB-NG interoperability на цій платі ще не перевірена.** Поточний образ не містить готового WFB runtime. Новий FPGA OFDM modem для першого експерименту не потрібний: спочатку слід кваліфікувати наявний Openwifi як raw802.11 NIC.

## Що означає «OFDM стек WFB-NG»

WFB-NG використовує Wi-Fi PHY через monitor/raw802.11: додає packet FEC, шифрування, stream/channel identifiers і транспорт UDP. Сам WFB-NG не є окремим FPGA OFDM-модемом. У запропонованій схемі MAC/PHY та RF забезпечують Openwifi/AD936x, а WFB runtime працює на Linux.

| Шар | Функція |
|---|---|
| Openwifi FPGA/RF | Стандартний Wi-Fi PHY/OFDM та цифровий RF інтерфейс |
| Linux mac80211/драйвер | Monitor interface, radiotap, інжекція/приймання raw frames |
| WFB core | Розбиття UDP, packet FEC, crypto, broadcast framing |
| WFB server/host | Конфігурація radio/profile, статистика та UDP endpoints |

AP association WFB не потрібна. Тому сьогоднішній AP timeout не забороняє broadcast WFB експеримент; так само beacon PASS не доводить WFB data-frame сумісність.

## Перевірені джерела

Основний pin дослідження WFB-NG:
89f50a361b5bc78f6740e832cdf1680a6ea9a819.
Openwifi source baseline:
9d4d1b0030df7756a2cf99458f970af7cb66f53f плюс наші виправлення metadata/calibration.
Tag wfb-ng-25.01.2 — окрема release-основа; параметри та wire compatibility між версіями не змішувати без перевірки.

| Вимога WFB | Що знайдено | Оцінка |
|---|---|---|
| TX framing | HT radiotap13bytes, NOACK, MCS; raw data frame header24bytes, broadcast receiver | Підходить до monitor injection моделі |
| API TX | PF_PACKET/SOCK_RAW/sendmsg | Немає залежності від конкретного libpcap TX backend |
| Openwifi TX flags | Driver honors mac80211 NO_ACK та MCS/GI fields | Source-level match, runtime ще потрібний |
| PHY | Плата рекламує HT20/MCS0–7, один spatial stream, без STBC/LDPC/VHT | Звужений кандидат |
| WFB defaults | У base STBC1 та LDPC1 | Обов’язково перевизначити0 |
| Packet FEC | WFB Reed–Solomon у userspace | Незалежний від PHY LDPC |
| FCS | wfb_rx знімає4байти при FCS-present і відхиляє BADFCS; повний CRC32 не перераховує | Змінений FPGA останній FCS byte не повинен змінювати WFB payload, якщо radiotap правильний |
| Metadata | Наш per-frame reset виправляє перенесення BADFCS/MCS flags | Важлива передумова, а не WFB PASS |

Чотири IQ-канали PN та два RF chains AD936x не означають підтримку2 spatial streams у поточному Openwifi PHY. Поточні advertised MCS0–7 — один spatial stream.

## Початкові параметри кандидата

Це контракт першого тесту, не готовий deployment config:

| Параметр | Значення |
|---|---|
| Channel/width | Поточні2442МГц,20МГц |
| PHY | HT; force_vht=False |
| MCS | Почати0, далі1 після перевірки |
| GI | Long; short_gi=False |
| STBC / LDPC | Обидва0 на активних stream/radio profiles |
| Frame type | data |
| ACK | NOACK, monitor auto-ACK вимкнений |
| Packet FEC | Наприклад k8/n12; окремо від LDPC |
| UDP payload | Початково≤1445bytes |
| Card initialization | Зберегти S95/calibrator ownership; common.primary=False або керовані core binaries |

Ліміт1445 — обережна початкова межа для кваліфікації, не виміряна максимальна пропускна здатність. Поточний Openwifi RX buffer2048bytes не дозволяє автоматично переносити upstream jumbo frames близько4KiB. Треба перевіряти фактичний MPDU з усіма WFB/crypto/802.11 overheads.

## Фактичні GAP поточного образу

На X99MG перевірено Buildroot config: C++ toolchain вимкнений, target g++ відсутній, libsodium не вибраний, Python3 не вибраний; libpcap увімкнений. Отже це не випадок «просто запустити встановлений wfb_tx». Потрібні відтворювана cross-build C++ runtime/core dependencies і окремий пакет. Повний Python server на ARM — окреме рішення.

Для RX-first можна дослідити upstream forwarder/aggregator: radio capture на SDR, FEC/decryption на хості через Ethernet. Аналогічно є distributor/injector для TX. Наявність цих режимів у source не означає, що вони вже зібрані або перевірені на R2.

На desk-tyan виявлено RTL8812AU із driver rtw88_8812au. Він не кваліфікований тут як WFB injector; не слід ототожнювати його з рекомендованим patched88XXau stack. Openwifi також не має встановленої цим тестом upstream WFB hardware qualification.

## Послідовність наступного експерименту

1. Зафіксувати одну WFB revision на обох peers; зібрати ARM core/libsodium без зміни поточного FPGA. Приватні ключі зберігати поза Git.
2. Зробити RX-first від кваліфікованого WFB peer; перевірити channel/linkID/keys, data-frame filter, FCS flags, session/FEC/auth.
3. Перевірити bounded NOACK injection в обидва боки: фактичне приймання peer, sequence/CRC/hash payload. Успіх sendmsg сам не є ефірним PASS.
4. Перевірити MCS0/1, довжини payload, burst loss, FEC recovery, latency та CPU. Потім тривалий тест для історичного RX-stall.
5. Лише після цього — video/RTP/MAVLink чи інші корисні потоки. Довільний custom OFDM поза Wi-Fi PHY був би іншим FPGA/peer проєктом, не звичайним WFB NIC port.

Результат сьогодні: source/protocol feasibility PASS із переліченими GAP; build/runtime/E2E WFB NOT_RUN. Поточний monitor і Flash не змінені дослідженням.

## Первинні джерела

- [WFB configuration](https://github.com/svpcom/wfb-ng/blob/89f50a361b5bc78f6740e832cdf1680a6ea9a819/wfb_ng/conf/master.cfg)
- [Radiotap/frame definitions](https://github.com/svpcom/wfb-ng/blob/89f50a361b5bc78f6740e832cdf1680a6ea9a819/src/wifibroadcast.hpp)
- [TX core](https://github.com/svpcom/wfb-ng/blob/89f50a361b5bc78f6740e832cdf1680a6ea9a819/src/tx.cpp)
- [RX core/FCS handling](https://github.com/svpcom/wfb-ng/blob/89f50a361b5bc78f6740e832cdf1680a6ea9a819/src/rx.cpp)
- [Card initialization](https://github.com/svpcom/wfb-ng/blob/89f50a361b5bc78f6740e832cdf1680a6ea9a819/wfb_ng/server.py)
- [Openwifi driver baseline](https://github.com/open-sdr/openwifi/blob/9d4d1b0030df7756a2cf99458f970af7cb66f53f/driver/sdr.c)
