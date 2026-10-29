# PlutoSky-R2: RF-тракти та LAN gateway

2026-10-29, Europe/Kyiv. Документація досліджень; не firmware release.

- [Вимоги](REQUIREMENTS.md): video720p, MAVLink, batman-adv та два RF-тракти.
- [Другий RF-тракт](DUAL_RF_FEASIBILITY.md): source/XSA evidence, single decoder, шляхи dual-RX/dual-modem і shared LO limits.
- [RF gateway з LAN-хостом](HOST_ASSISTED_GATEWAY.md): актуальне уточнення користувача; WFB forwarder/injector на SDR, FEC/crypto/application на хості, RAW IQ budget, QSPI/SD candidates та tests.

## Статус

Виконано source/config/XSA audit. Не виконано нові build, synthesis, route, RF E2E, SD/QSPI programming чи reboot. Наведені оптимізації — кандидати для вимірювання.

PlutoSky-R2 має інший QSPI target, ніж оригінальний PlutoSky7020: 32MiB/30MiB Linux проти16MiB/14MiB Linux. RAM boot не доводить постійне прошивання.

## Порядок наступних робіт

1. Cross-build pinned WFB core і перевірка ARM runtime dependencies.
2. RX-first forwarding по LAN з незалежним RF peer; далі bounded TX/injection.
3. Video/MAVLink/tracking load tests; mesh L2 transport окремо.
4. Hierarchical FPGA utilization/timing перед dual RX decoder.
5. Slim firmware після базової кваліфікації; persistent boot test окремий.

Пакет опубліковано окремою гілкою на основі попередньої from-zero documentation branch. Незавершені source changes X99MG до цього commit не включені.
