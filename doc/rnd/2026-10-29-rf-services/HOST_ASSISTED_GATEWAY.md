# PlutoSky-R2: host-assisted RF packet gateway
2026-10-29 Europe/Kyiv. DESIGN / SOURCE FEASIBILITY, NOT BUILT OR DEPLOYED.

## Уточнена вимога користувача
На PlutoSky-R2 не виконувати video decode/transcode, WFB FEC/decrypt, MAVLink routing, batman routing чи ArduTracking application. RF traffic передавати по Ethernet/IP на виділений host port. Зворотний TX — через LAN/IP до radio injector.
FPGA Openwifi PHY decode/encode залишається необхідним для packet gateway. Якщо «нічого декодувати» включає OFDM PHY, потрібен RAW IQ gateway — інший design.

## RAW format і бюджет
- Complex IQ з I16+Q16,40MSPS: один RX=1.28Gbps; два RX=2.56Gbps тільки payload; RX+TX двох трактів=5.12Gbps. Це перевищує1GbE. Фактична Ethernet speed/goodput у цій ітерації не вимірювалася.
- 20MSPS два RX I16Q16=1.28Gbps. Reduced sample bits/rate змінюють RF/EVM/канальну смугу й потребують qualification; compression не має гарантованого коефіцієнта.
- Packet gateway пересилає decoded Wi-Fi/WFB packets, тому обсяг залежить від traffic, не постійного IQ clock. 720p без codec/FPS/bitrate/GOP не визначає bitrate.
- Приклад бюджету:6Mbps video ×12/8 FEC=9Mbps, без WFB/802.11/LAN overhead. Це розрахунок, не measured capacity.

## Підтверджений distributed WFB mechanism
Source pin89f50a361b5bc78f6740e832cdf1680a6ea9a819.
SDR RX wfb_rx forwarder(-f) -> host RX aggregator(-a).
Host TX wfb_tx distributor(-d) -> SDR TX injector(-I).
На host: packet FEC, auth/decrypt/encrypt, service endpoints, stats, video consumer, MAVLink router, tracking.
На SDR: RF init/calibration, FPGA Openwifi, mac80211/driver, capture/filter/metadata forwarding і raw injection.
Forwarder strips radiotap/802.11 header and transports encrypted WFB payload + wrxfwd metadata. Не є full raw802.11 tunnel або IQ.
Stock core C++ binaries залишаються linked з libsodium/FEC-related code; forwarder dedup використовує crypto_shorthash, обидва main мають sodium_init. Не видаляти deps без slim build і ELF dependency check.
Standalone Python WFB server на SDR не потрібний для direct core binaries; lifecycle/config/logging owner все одно потрібний.

## Service ownership
Video: FPGA PHY -> SDR forwarder -> host aggregator -> RTP/UDP LAN sink. Codec decode лише у споживача.
MAVLink: host-owned bidirectional WFB streams/endpoints, rate budget і один command writer.
ArduTracking: host consumer telemetry; точний project/protocol/model невідомий, не припускати готовий adapter. Власник actuator commands один.
Mesh: batman-adv потребує Ethernet-style L2 bearer. Не додавати monitor або WFB IPv4 TUN прямо у bat0.
Кандидати mesh: окремий data-interface/кваліфіковане radio; або L2-over-IP поверх WFB з додатковою інкапсуляцією, MTU і loop control; або custom raw-frame tunnel/TAP frontend. Нічого з цього не є вже працюючим mesh profile.
VIF/UDP ports не створюють другий PHY. Low MAC SIFS/ACK/CCA лишити FPGA — LAN host не замінює real-time ACK loop.
Два RX: current XSA sample1 -> side_ch, sole packetdecoder on sample0; host offload не створює другий RXdecoder. Для concurrent packet RX потрібен HDL variant.

## Linux/ARM optimization candidates
- Runtime profile без GUI, ffmpeg/gstreamer, video codecs, Python orchestrator, hostapd/wpa/DHCP-server, local batctl/batman routing ЯКЩО їхня функція остаточно перенесена на host; management DHCP client лишити за потреби.
- Зберегти required NIC/PHY, AD936x/SPI/GPIO/clock/FIR/calibration, DMA/Openwifi/mac80211/cfg80211, AF_PACKET, IP/UDP, management/recovery.
- Kernel minimization робити через actual merged config і boot/runtime dependency manifest, не тільки base kernel_config.
- Зменшити driver per-packet logging, bounded logs у RAM. Не вимикати fault/calibration diagnostics.
- Стартувати штатними forwarder/injector; далі perf/counters. recvmmsg/sendmmsg, ring buffers, IRQ/CPU affinity і buffer tuning — лише після вимірювання. Немає zero-copy/line-rate claim.
- MTU1500 початково; actual encapsulated UDP datagram length gate, без IP fragmentation. Не переносити WFB4KiB jumbo на поточний RXbuffer2048. Jumbo LAN не збільшує RF RXbuffer.
- Окремі RX-forward, TX-inject, control/status ports; per-stream selectors/linkIDs. Port allocation прикладна, перевірити collisions. TX input тільки від заданого LAN peer; private WFB keys зберігати на host позаGit.
- Пріоритет control/MAVLink/tracking, bounded video queue і measured shaper; DSCP alone не гарантує RF priority.

## QSPI та SD
TargetR2 W25Q25632MiB/Linux30MiB — не плутати з original7020 W25Q12816MiB/Linux14MiB.
Кандидат: QSPI містить self-contained thin radio runtime і recovery; SD — R&D kernels/FPGA candidates/tools/log archives. Boot policy/fallback явні, обидва носії qualification окремо.
Альтернатива QSPI rescue+SD main можлива, але це SD-dependent runtime, не full main QSPI firmware.
Не змінювати bootloader/env/NVM заради rootfs slimming. SD rootfs/overlay позначати persistent SD, RAM runtime не є QSPI persistence.
Робочий QSPI лишити до успішних build/RAM/SD qualification. Не оголошувати compact image готовим без size/hash/readback/normal boot.

## Next measurements
1. Cross-build pinned stock wfb_rx/wfb_tx for actual ARM/kernel/libc; inspect runtime deps/size.
2. Verify Ethernet link/goodput, packet MTU, UDP drops. RX-first від незалежного peer, host FEC/auth PASS.
3. Host distributor -> SDR injector -> peer payload hash; sendmsg alone insufficient.
4. Concurrent video+MAVLink/tracking load: loss/latency/jitter, CPU/RAM/IRQ/DMA. Mesh separate gate.
5. Slim package only after baseline; no kernel/HDL edits bundled with initial protocol test.

## Evidence and scope
Reviewed local source configs and prior XSA audit; no fresh board runtime, build, Flash, SD, RF TX or service changes performed.
Source: https://github.com/svpcom/wfb-ng/blob/89f50a361b5bc78f6740e832cdf1680a6ea9a819/src/rx.cpp
Source: https://github.com/svpcom/wfb-ng/blob/89f50a361b5bc78f6740e832cdf1680a6ea9a819/src/tx.cpp
Source: https://docs.kernel.org/networking/batman-adv.html
Related: REQUIREMENTS.md, DUAL_RF_FEASIBILITY.md.
