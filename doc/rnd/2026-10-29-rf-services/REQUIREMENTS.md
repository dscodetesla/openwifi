# PlutoSky-R2: цільовий контракт RF і мережевих сервісів
Дата: 2026-10-29, Europe/Kyiv. Статус: вимоги та архітектурний кандидат; не runtime PASS.

## Уточнення після первинних вимог
Актуальний розподіл обробки визначає [HOST_ASSISTED_GATEWAY.md](HOST_ASSISTED_GATEWAY.md): на R2 — RF/PHY і forwarding/injection; WFB FEC/crypto та прикладні сервіси — на LAN-хості. Подальші розділи фіксують первинний контракт; локальне розміщення batman/WFB-залежностей є умовним для standalone-профілю.

## Вимоги користувача
- PlutoSky-R2 + Openwifi; кваліфікувати TX1/RX1 і TX2/RX2.
- WFB-NG поверх Wi-Fi OFDM: RF RX відео 1280x720 з передаванням у LAN.
- Двобічний MAVLink.
- Двобічний mesh B.A.T.M.A.N.; робоча інтерпретація batman-adv (L2), уточнити за потреби.
- Одночасність сервісів є ціллю; її підтримка поточним FPGA/драйвером не доведена.

## Межі апаратури й протоколів
Два аналогові RF-тракти не доводять два незалежні Wi-Fi PHY, два частотні канали, 2x2 spatial MIMO чи ефірний full duplex. У поточному baseline advertised MCS0–7: один spatial stream. Upstream описує вибір RX/TX антени, CDD і coherent IQ capture; не переносити ці можливості на наш bitstream без тесту.
WFB-NG використовує raw 802.11 monitor/injection, packet FEC і crypto; OFDM забезпечує Openwifi.
batman-adv потребує Ethernet-style L2 hard interface. Raw monitor не є таким інтерфейсом; WFB IPv4 TUN сам по собі не переносить Ethernet batman-adv.

## Архітектурний кандидат
1. WFB: RF -> Openwifi monitor -> WFB RX/FEC/auth -> RTP/UDP -> заданий LAN endpoint. Відео вже стиснене джерелом; SDR не повинен транскодувати його для простого forwarding.
2. MAVLink: окремі WFB stream IDs/UDP endpoints для uplink/downlink; budget/rate control, один command writer, без routing loops і дублювань.
3. Mesh: окремий профіль Openwifi зі звичайним Ethernet-style data interface (наприклад кваліфікований IBSS) -> batman-adv -> bat0. B.A.T.M.A.N. не означає автоматично 802.11s.
4. Перша кваліфікація WFB і mesh окремо. Для одночасності дослідити один PHY/multi-interface/ACK/airtime, або використати окремий кваліфікований радіоінтерфейс; AWUS1900 ще не кваліфікований як WFB injector.
5. Host-assisted WFB forwarder/distributor/aggregator можливий кандидат; не видавати його за standalone SDR runtime.

## Не вилучати під час компактного firmware build
- Узгоджені FPGA/kernel/modules, AD936x, DMA, calibration helper/S95.
- mac80211/cfg80211, monitor RX і injection/radiotap.
- LAN, management/recovery, потрібні USB gadget функції.
- batman-adv і batctl, iproute2 та потрібні bridge/VLAN/TUN/TAP/netfilter/qdisc функції відповідно до обраної архітектури.
- Для standalone WFB: ARM C++ runtime, libsodium, libpcap та точні залежності обраного pinned release; Python3 лише якщо потрібен повний Python server.
- Не обіцяти однаковий комплект пакетів для standalone і host-assisted профілів.

## Board/storage guard
PlutoSky-R2: раніше перевірений W25Q256 32 MiB, Linux 30 MiB.
Оригінальний PlutoSky 7020 AD936X: W25Q128 16 MiB, Linux 14 MiB; це інша board target.
Компактний 14 MiB образ original не є вимогою R2 за замовчуванням.
Persistent PASS потребує exact-board backup, size gate, readback/hash і нормального boot з QSPI; RAM boot не доводить постійне прошивання.

## Приймальні тести
- Окремо RX і TX кожного RF-тракту: незалежний peer, channel/MCS, payload hash, PN/calibration; counters/sendmsg недостатньо.
- WFB RX-first, далі двобічний data/MAVLink. Початковий кандидат HT20/MCS0–1/long GI/STBC0/LDPC0/NOACK, UDP <=1445 B; параметри звірити з pinned peer і actual MPDU limit.
- Відео: фіксувати codec, FPS, encoder bitrate, GOP, RTP packetization, LAN destination, FEC k/n; 720p не визначає bitrate. Виміряти decoded frames, loss/FEC, bitrate, latency, CPU/RAM.
- MAVLink: SYSID/component, endpoints, measured rate, RTT, reconnect, no duplicates/loops; video load не повинен витісняти control traffic.
- Mesh: >=3 вузли для multi-hop; originators/neighbours, unicast/broadcast, route recovery, MTU і absence L2 loops.
- Coexistence: video off/on при однаковому telemetry/mesh load; CPU, RF airtime, latency/jitter, loss. PASS кожного сервісу окремо не є simultaneous PASS.

## Невизначене
Codec/FPS/bitrate/latency target; topology і кількість hops; peer hardware; чи потрібна автономність без X99MG; одночасність обох RF-трактів. Поточне доручення не визначає цих параметрів.

## Джерела
- [Попередній пакет досліджень і pipeline](https://github.com/dscodetesla/openwifi/tree/8b4b9867e5dfe62ced44198e2eea54d362f8197a) — historical baseline.
- https://github.com/open-sdr/openwifi/blob/master/doc/README.md
- https://github.com/open-sdr/openwifi/blob/master/doc/app_notes/iq_2ant.md
- https://github.com/svpcom/wfb-ng
- https://docs.kernel.org/networking/batman-adv.html

Зміни цієї ітерації: документація вимог. Build, Flash write, reboot, RF TX і runtime configuration не виконувалися.
