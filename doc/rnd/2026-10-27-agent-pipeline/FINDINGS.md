# Знахідки, помилки та прийняті рішення

Стан доказів: 26.10.2026; документація 27.10, Europe/Kyiv.

## Цифровий RF інтерфейс

Штатний scan іноді приймав RX clock-delay 0x90, після чого TX PN не синхронізувався. Незалежні RX 20 ms тести 0x90/F6 провалювались на всіх 4 IQ, RX03/04 проходили. При фіксованому RX04 повна сітка 256 TX точок дала 77 чистих точок із 500 ms підтвердженням. Повторне kernel tune могло обрати RX03/TX50/60 і пройти або знову обрати невдалу RX гілку. RF/FIR init відтворював warning. Фізична причина clock/settling нестабільності НЕ встановлена; pinout/PN polynomial mismatch не доведений.

Рішення: R2-only helper tools/plutosky_r2_calibrate.c після фінального RF/FIR, до interface-up; 40 MHz, FIR 1/1, точний board/core/FDD guard, lock, mute двох RF трактів. 16 RX data taps при clock 0, потім 16 TX clock taps при data 0; 20 ms settle + 20 ms measure, центр найдовшого passing window; вибрані RX/TX — кожен по 500 ms/all 4 PN zero. Перевірка PN selector/DAC source/channel control readback. Відмова/сигнал повертає початкові delays; успіх зберігає лише підтверджені delays та відновлює решту. S95 не продовжує startup після помилки. Не підміняти це force RX03/TX50 без вимірювання.

Пройшли перевірки прототипу: 3 повтори + recovery RX90, interface-up guard, SIGTERM cleanup, 65536 window patterns і 14 boot service cases. Hardened helper пройшов RAM і нормальний Flash boot. Ранній Tuning TX FAILED може лишитися перед пізнішим CALIBRATION_PASS; оцінювати порядок/свіжість, не стирати warning і не приймати будь-який старий PASS.

## Драйвер, RX та сервіси

Прибрано небезпечне копіювання приватної структури DDS ABI. RX status reset перенесено на кожний SKB: BADFCS/HT/AMPDU flags не мають витікати між кадрами одного IRQ. FPGA замінює останній байт FCS status/sequence: збережені 24 біти CRC + hardware status є нашим доказом; повний CRC32 кадру не оголошувати збереженим.

SSH host keys генеруються завчасно та зберігаються з відновленням RO mount; monitor DHCP skip не прибирає ручний AP path. GPIO reset 100/ENAGC 98, ref 40 MHz; DTC/GCC 15 gnu17; виправлення DFU suffix CRC, SPL byte padding і QSPI layout збережені в попередньому checkpoint.

## Flash і recovery

Повний приватний factory backup 32 MiB перевірений. Для поточного layout mtd0 boot 1 MiB, mtd1 env 128 KiB, mtd2 NVM 896 KiB, mtd3 Linux 30 MiB offset 2 MiB. Старий 14 MiB layout для FIT 21 MiB непридатний. Не переносити цю карту на іншу плату без /proc/mtd/sysfs звірки.

Записано лише mtd3 payload після RAM gates, повний readback збігся. mtd0 MD5 b2fa54c2f273f066d4593ce5d259c7e8; mtd2 MD5 2e812e6a5cc43ad00d7e2fe44db01cae збережені. Точний fit_size 0x1441d44; bootdelay 3. Окрема мінімальна правка environment 0→3 + CRC змінила ≤5 байтів; виконано повний 128 KiB readback.

Перший ручний RAM boot успадкував initrd=0x00800000,16M і panic. Виправлено тимчасовими bootargs console=ttyPS0,115200 root=/dev/ram0 rw; RAM FIT load 0x2080000, bootm configuration config@0. Не зберігати тестові RAM args через saveenv. UART abort надсилати після countdown prompt: ранній banner input може бути очищений.

Reset-code 2 не гарантує входу в консоль vendor U-Boot. Не циклувати reset codes. USB-device ACL потрібний libusb/JTAG окремо від UART ACL; bus/node змінюється після reconnect (поточний inventory вже має інший node). Ідентифікувати Digilent/xc7z020/ARM target перед reset.

XSCT 2025.2 потребував справжніх ABI5 libncurses/libtinfo лише в child LD_LIBRARY_PATH; ABI6 symlink не є виправленням. Тимчасовий hw_server зупинено.

Native writer CLI: image FIRST, device SECOND; не плутати. Packager дає DFU; FIT extract = DFU без 16 байт suffix лише після DFU CRC + FDT total size validation. FIT містить raw .bit, не припускати .bit.bin.

## RF/AP/WFB межі

OTA beacon підтверджений незалежною MT7921U: 2442 MHz, attenuation 10 dB, NM signal 54/100. Це не 10 dBm, не 54 dBm, не EVM/throughput. Асоціація: timeout 12 s; повторна спроба зупинилась на scan precondition до асоціації. Ping не запускався. AP-ENABLED/IRQ/sendmsg самі по собі не є OTA/E2E.

Scan cache одразу після rescan міг бути застарілий; потрібний завершений fresh scan + точний SSID/BSSID. rtw88_8812au не дорівнює patched WFB 88XXau. Desk-tyan має іншу ADALM PLUTO — не вибирати її за адресою 192.168.2.1 автоматично.

WFB не вимагає AP association; кандидат описаний окремо. Build/runtime NOT_RUN. Injection при 89.75 dB не перевірявся: драйвер трактує це як radio-off/rfkill. Внутрішній loopback не гарантує нуль RF.

## Відкриті питання та evidence

Native USB powered/UNKNOWN, довгий RX stall (>5 h історично), постійний NTP і AP association залишаються відкритими. FAT warning про unclean unmount не доводить corruption; /boot RO, fsck не виконувався.

RAM capture 300: 294 good/6 bad; Flash 300: 292/8; metadata 300/300, 0 drops. Post OTA 100: 61/39, metadata 100/100, 0 drops. Різниця не є контрольованим RF benchmark.

Число TX 0 з історичного monitor не є актуальним після AP: сумарно 35 у фінальному capture стані. Поточну telemetry потрібно перечитувати.

Попередні джерела:

- openwifi checkpoint 73eb4376f749b700b6e4704715f6649c0fec7ef7, calibration-source a916bd9df6f3279d7ab5f428c7bd07f2f383041d, deployment docs d8381c449692443d7e2c0b26a497a6fc6c51635a.

- PlutoSky-R2 checkpoint afb1beb90f61801b8a00b102b020ecf696ca95d8, deployment docs 1a256fe544db3e83140ef50a2ff4cc72482e3c2d.

- Canonical старі findings: openwifi/doc/rnd/2026-10-26-plutosky-r2; calibration: doc/rnd/2026-10-26-tx-calibration. Старий NOT_TESTED OTA стосується того етапу; новий beacon PASS не змінює AP/WFB status.

## Контроль агента

Один board-owner, незалежні source-review/build-subagents. Не делегувати паралельні writes на один SDR. Одна гіпотеза → один bounded test → доказ → checkpoint. Timeout connector не означає board offline. Не обходити заборонений sudo іншим інструментом. Не запускати CLI agent рекурсивно. Не архівувати весь partial clone: раніше promisor fetch 100k objects зайняв 103 s; використовувати точні refs/paths.

Неправильна JSON→Python вставка true/false виправляється json.loads, не ручною заміною bool. Shell quote окремий від JSON encode. Не публікувати raw PCAP, SSID/MAC списки, host keys, Flash/env dumps, credentials або account logs.
