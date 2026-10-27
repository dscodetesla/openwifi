# PlutoSky-R2: Openwifi з нуля
27.10.2026, Europe/Kyiv.

Практичний шлях: **заводський PlutoSky-R2 → збірка Openwifi → пробний RAM boot → запис Linux/FPGA/rootfs у QSPI → автоматичний старт і перевірка**. Linux-хост x86_64; початковий еталон — X99MG. Інтеграція ШІ/MCP, WFB-NG та розробка нового FPGA не потрібні для цього шляху.

## 1. Що буде встановлено і що вважати успіхом

Openwifi тут — Linux + mac80211 драйвер + FPGA Wi-Fi MAC/PHY + RF конфігурація AD936x. Запуск окремого ELF або встановлення hostapd на заводську прошивку цього не замінює.

Підтверджений результат нашого екземпляра: звичайний QSPI boot, Ethernet/SSH, калібрований цифровий RX/TX інтерфейс та monitor RX; окремо зовнішня Wi-Fi карта прийняла beacon. **Повна AP-асоціація ще не пройшла кваліфікацію**: була спроба з timeout 12 s. Тому нижче є завершений шлях встановлення і старту monitor, а AP наведено як наступний тест, без обіцянки готової точки доступу.

Історичний перевірений FIT: 21 241 156 байтів, SHA-256
`b90269d4454e097fcf25ec89ce1e1f8fe75846b60c4e0aa5d6e1b77fe0091b98`.
Нова збірка може мати інший hash через toolchain/timestamps; її треба кваліфікувати окремо. Цей гайд не містить готового binary download і не заявляє нової чистої збірки на порожньому хості.

Позначення команд: **HOST** — Linux-комп’ютер; **BOARD** — shell SDR; **U-BOOT** — DEBUG-консоль завантажувача. Не вставляти блоки різних середовищ в один shell.

## 2. Обладнання і підключення

Потрібні PlutoSky-R2 (XC7Z020, фактичний AD9363 або AD9361), справне живлення, Ethernet, кабель USB-C з передачею даних для DEBUG, microSD з доступним FAT-розділом і кардридер. Наявну vendor SD спочатку скопіювати/зберегти; не форматувати її заради цього тесту.

| Роз’єм | Призначення в цьому встановленні |
|---|---|
| USB-C DEBUG | UART 115200 8N1 і JTAG через USB bridge; основна консоль/recovery |
| Звичайний USB-C | Native USB Zynq; gadget/DFU не використовуємо як гарантований шлях |
| RJ45 | LAN/SSH та передавання файлів; Ethernet залежить від PL bitstream |
| microSD | FAT-файл FIT для пробного завантаження через заводський U-Boot |

На нашій Openwifi прошивці native USB не пройшов enumeration; це не заважає Ethernet + DEBUG. Не блокувати встановлення очікуванням USB RNDIS.

Для QSPI режиму у manual MODE1=1/MODE2=0; SD=1/1, JTAG=0/0. Звірити маркування конкретної ревізії; змінювати перемикачі тільки без живлення. Основний шлях залишає заводський QSPI boot. SD тут є носієм файлу, а не зміною boot mode.

Використати штатний внутрішній 40 MHz reference, не переставляти clock jumpers під живленням. Перший RX bring-up виконувати без корисного TX-трафіку; RF-підключення, PA/bypass і навантаження визначати за фактичним SKU. Поточне встановлення не кваліфікує кожний PA/RFIC варіант автоматично.

## 3. Підготувати Linux-хост

Команди нижче — для Debian/Ubuntu. Це початковий набір залежностей; фактичний Buildroot dependency check є остаточним джерелом відсутніх пакетів. Збірку виконувати звичайним користувачем на локальному диску, не root і не в Google Drive mount. Резервувати десятки GiB вільного місця; фактичний обсяг залежить від кешу та паралелізму.

**HOST:**
```sh
sudo apt update
sudo apt install build-essential git python3 python3-venv   make gcc g++ bc bison flex cpio rsync unzip zip wget curl   patch file perl tar gzip bzip2 xz-utils sed gawk findutils   diffutils which ca-certificates libssl-dev libgnutls28-dev   libncurses-dev pkg-config device-tree-compiler   openssh-client picocom acl usbutils iproute2 network-manager
```

Vivado/Vitis не потрібні для звичайної збірки Linux/rootfs з готовим R2 XSA: Buildroot збирає потрібні host tools. Для JTAG recovery потрібні працездатні hardware-server/XSCT або інший кваліфікований JTAG шлях. На нашому стенді використані AMD 2025.2; повна їх інсталяція не є передумовою кожного повторного build.

**HOST:**
```sh
mkdir -p "$HOME/src/plutosky-r2-clean"
cd "$HOME/src/plutosky-r2-clean"
df -h .
git --version
python3 --version
make --version
gcc --version
```

Обрати шлях без пробілів; у PATH/LD_LIBRARY_PATH не повинно бути порожніх елементів чи ".". Якщо shell успадкував AMD environment, зібрати у чистому звичайному shell; не додавати глобальні ABI library symlinks.

## 4. Встановити початковий доступ до заводської плати

Спочатку DEBUG. На Linux перевірити `lsusb` і `ls -l /dev/serial/by-id/`. Наш Digilent bridge давав UART на interface01, JTAG на interface00; точний by-id/serial кожної плати власний. Закрити інші програми, що тримають UART.

**HOST, підставити фактичний шлях:**
```sh
picocom -b 115200 /dev/serial/by-id/ACTUAL_UART_DEVICE
```

За потреби надати користувачу доступ до конкретного UART штатною групою або ACL. Доступ до UART не надає доступ до JTAG libusb: там окремий /dev/bus/usb/BBB/DDD, який змінюється після reconnect. Не копіювати старий USB node з чужого стенда.

Через консоль з’ясувати `ip addr`, `ip route`, `uname -a`, `cat /proc/mtd`. Заводська адреса за manual часто 192.168.2.1; перевірити її фактично. У нашого Openwifi профілю після boot буде **192.168.10.122/24**, це інша адреса.

Для прямого Ethernet-хоста можна створити окремий NetworkManager profile з двома локальними адресами. Спочатку вибрати фізично виділений порт — не WAN.

**HOST, приклад з явною заміною імені порту:**
```sh
SDR_ETH=enp6s0
ip route show default
nmcli device status
# Переконатися, що SDR_ETH не є інтерфейсом потрібного default route.
nmcli connection add type ethernet ifname "$SDR_ETH"   con-name plutosky-direct   ipv4.method manual   ipv4.addresses "192.168.2.10/24,192.168.10.2/24"   ipv4.never-default yes ipv6.method disabled
nmcli connection up plutosky-direct
```

Якщо така connection вже існує, перевірити її замість повторного add. Якщо 192.168.2.0/24 вже зайнята іншим SDR/мережею, усунути конфлікт; не обирати плату лише за IP. WAN/Wi-Fi залишити активними.

SSH fingerprint заводської плати звірити через DEBUG/довірений канал, використати окремий known_hosts. Пароль вводити інтерактивно; не записувати його в скрипти. Заводські credentials взяти з комплекту конкретної плати. Перший Openwifi login задається відкритою Buildroot конфігурацією як root/openwifi; перед підключенням до спільної мережі змінити credentials у source profile або налаштувати ключі з перевіреною персистентністю.

У наступних блоках **HOST**:
```sh
BOARD_FACTORY=192.168.2.1
BOARD_OPENWIFI=192.168.10.122
# Після першої перевірки fingerprint створити/використовувати:
KNOWN_HOSTS="$HOME/src/plutosky-r2-clean/known_hosts"
```
Для команд SSH нижче додавати `-o UserKnownHostsFile="$KNOWN_HOSTS" -o StrictHostKeyChecking=yes`. При першому Openwifi RAM boot нова OS identity може бути іншою: спочатку звірити новий fingerprint через DEBUG, а не вимикати перевірку. Ключ Openwifi має зберігатися на доступному /boot; RAM/Flash повторні boot перевіряють його сталість.

## 5. Отримати точно потрібні sources і XSA

Основний варіант використовує наш source pin з виправленнями R2, а не довільний upstream master.

**HOST, нові порожні каталоги:**
```sh
cd "$HOME/src/plutosky-r2-clean"
git clone --no-checkout https://github.com/dscodetesla/openwifi.git openwifi
git -C openwifi checkout --detach d8381c449692443d7e2c0b26a497a6fc6c51635a
git -C openwifi submodule update --init buildroot

git clone --no-checkout https://github.com/open-sdr/openwifi-hw-img.git openwifi-hw-img
git -C openwifi-hw-img checkout --detach 64f27c5f897a807cf58fc383dd4b08c728abc07c

git -C openwifi rev-parse HEAD
git -C openwifi/buildroot rev-parse HEAD
git -C openwifi-hw-img rev-parse HEAD
test -s openwifi-hw-img/boards/plutosky_r2/sdk/system_top.xsa
sha256sum openwifi-hw-img/boards/plutosky_r2/sdk/system_top.xsa
```

Очікуваний Buildroot: `ff3b00e7d01894963ce05f8cc9077ac0d61d4ad9`.
Kernel ADI pin: `40201abd70d8f7848547a09fc7e366fba064483c`; R2 fragment завантажує його tarball, тому вручну clone всього adi-linux не потрібний.
XSA обов’язково від `boards/plutosky_r2`; XSA іншого Pluto/ANTSDR не підходить. Vendor Vivado project не потрібний для цього prebuilt-FPGA шляху.

## 6. Зібрати firmware

**HOST:**
```sh
cd "$HOME/src/plutosky-r2-clean/openwifi"
python3 tests/rnd/test_r2_boot_services.py
set -o pipefail
OPENWIFI_XSA="$HOME/src/plutosky-r2-clean/openwifi-hw-img/boards/plutosky_r2/sdk/system_top.xsa"   ./buildroot-build.sh plutosky_r2 build 2>&1 | tee build-r2.log
```

Використовувати bash для `set -o pipefail`. Помилка build не повинна маскуватися успішним tee. Для source/package/helper змін у вже зібраному checkout використати `rebuild-system`, а не сподіватися на старі Buildroot stamps; ця команда очищує частину build output.

Ключовий результат:
`openwifi/output/plutosky_r2/images/openwifi-plutosky_r2-qspi.dfu`.

Це **FIT із 16-байтовим DFU suffix**, не SD disk image. `BOOT.BIN`, `u-boot.img`, `rootfs.ext4` та інші проміжні результати не треба автоматично прошивати поверх заводського bootchain. Для основного шляху зберігаємо заводський mtd0/U-Boot.

Helper містяться в окремій гілці документації. Отримати її в сусідній каталог; checkout sources для збірки залишається на pin із п.5. Каталог призначення має бути відсутнім.

**HOST:**
```sh
git clone --single-branch --branch docs/plutosky-r2-openwifi-from-zero-2026-10-27 \
  https://github.com/dscodetesla/openwifi.git \
  "$HOME/src/plutosky-r2-clean/openwifi-from-zero-guide"
GUIDE_DIR="$HOME/src/plutosky-r2-clean/openwifi-from-zero-guide/doc/rnd/2026-10-27-from-zero"
test -s "$GUIDE_DIR/extract_r2_fit.py"
test -s "$GUIDE_DIR/prepare_r2_bootdelay.py"
```

**HOST**, витягти FIT:
```sh
GUIDE_DIR="$HOME/src/plutosky-r2-clean/openwifi-from-zero-guide/doc/rnd/2026-10-27-from-zero"
IMAGES="$HOME/src/plutosky-r2-clean/openwifi/output/plutosky_r2/images"
python3 "$GUIDE_DIR/extract_r2_fit.py"   "$IMAGES/openwifi-plutosky_r2-qspi.dfu"   "$IMAGES/openwifi-plutosky_r2-qspi.itb" | tee "$IMAGES/fit-manifest.json"

"$HOME/src/plutosky-r2-clean/openwifi/output/common/host/bin/dumpimage"   -l "$IMAGES/openwifi-plutosky_r2-qspi.itb"
```

Helper відмовляється перезаписувати наявний ITB. Якщо він уже є, спочатку перевірити його hash/походження; не видаляти невідомий артефакт автоматично. Helper перевіряє саме pinned DFU suffix/CRC, FIT magic/total_size та межу 30 MiB; він не є повним семантичним FIT validator.

У FIT мають бути R2 DTB, raw `system_top.bit`, zImage, rootfs.cpio.gz і `config@0`. У цьому packager не додані FIT hash/signature nodes; `dumpimage -l` не замінює SHA-256 усього файлу та перевірку джерела. Зберегти fit-manifest.json, source pins, build log і XSA hash.

## 7. Резервна копія і важлива різниця карт Flash

На нашому R2 фізично W25Q256, 32 MiB. Заводський Linux спочатку показував лише нижні 16 MiB, з mtd3=14 MiB. Openwifi RAM kernel/DT розпізнав 32 MiB і відкрив mtd3=30 MiB. **Це зміна програмного опису діапазону, не фізичне пересування boot/NVM даних.**

| Ділянка після RAM boot | Offset | Розмір | Дія |
|---|---:|---:|---|
| mtd0, bootchain | 0x000000 | 0x100000 | Зберегти |
| mtd1, environment | 0x100000 | 0x020000 | Окремі контрольовані env-правки |
| mtd2, NVM | 0x120000 | 0x0e0000 | Зберегти |
| mtd3, Linux FIT | 0x200000 | 0x1e00000 | Запис нового FIT після RAM gates |

Назви mtdN можуть бути іншими на іншій ревізії. Звірити /proc/mtd, sysfs offset/size і фізичний chip capacity, а не лише номер. Якщо factory показує 14 MiB, **не записувати 21 MiB через цей mtd3** і не збільшувати його навмання.

Перед першим env або firmware write зняти доступні заводські ділянки й перевірити їх незалежним читанням. Перед окремою правкою bootdelay обов’язково зберегти перевірений оригінал усієї 128 KiB env ділянки. При старій 16 MiB карті це ще **не повний 32 MiB backup**. Повну копію зробити з Openwifi RAM kernel до запису нового payload.

**BOARD, read-only inventory:**
```sh
cat /proc/mtd
for d in /sys/class/mtd/mtd[0-9]*; do
  echo "$d"
  cat "$d/name" "$d/size" "$d/erasesize"
  if test -r "$d/offset"; then cat "$d/offset"; fi
done
```

**HOST, приклад копіювання одного ПЕРЕВІРЕНОГО розділу:**
```sh
umask 077
mkdir -p "$HOME/src/plutosky-r2-clean/private-backup"
ssh -o UserKnownHostsFile="$KNOWN_HOSTS" -o StrictHostKeyChecking=yes   root@"$BOARD_FACTORY" 'cat /dev/mtd0'   > "$HOME/src/plutosky-r2-clean/private-backup/factory-mtd0.bin"
```

Повторити для всіх реально наявних розділів; після кожного — перевірка exit code, довжини за sysfs та hash проти другого незалежного читання. Текст UART/SSH не повинен потрапити у binary stream. Backup і environment не публікувати: вони можуть містити унікальні дані й credentials.

Після RAM boot повторити для mtd0..3 з повною 32 MiB картою; розміри з таблиці мають скластися у 33 554 432 байти. Перевірити безперервність offsets; зберегти копію на другому диску. Лише тоді firmware write gate відкритий.

## 8. Підготувати доступний U-Boot і RAM boot

### 8.1 Якщо UART дозволяє зупинити старт

Після появи `Hit any key to stop autoboot:` натиснути клавішу. Перевірити `version`, `printenv bootcmd bootargs fit_size bootdelay`, `mmc list`. Зберегти текст у приватному run log. Не друкувати повний env у публічний звіт.

### 8.2 Якщо bootdelay=0 і перехоплення не працює

Це був реальний блокер нашого екземпляра. Не циклувати reboot/reset code. Якщо працездатний заводський Linux має правильно налаштований штатний env tool, використати його тільки після backup і звірки offset/format.

Перевірений на нашій платі альтернативний шлях: підготувати **офлайн** копію nonredundant 128 KiB environment, змінити лише bootdelay 0→3 та CRC, записати назад лише цю підтверджену env ділянку і прочитати її повністю назад. Це окремий env write; нова прошивка ще не записується.

**HOST:**
```sh
python3 "$GUIDE_DIR/prepare_r2_bootdelay.py"   "$HOME/src/plutosky-r2-clean/private-backup/factory-mtd1.bin"   "$HOME/src/plutosky-r2-clean/private-backup/env-bootdelay3.bin"
```

Helper не звертається до SDR. Він приймає лише 128 KiB, little-endian CRC32 над рештою env, рівно одну повну змінну bootdelay=0 і не більше 5 змінених байтів. Redundant env, інший розмір або формат — STOP, цей helper не застосовувати.

Для фізичного env write потрібний ARM writer із п.9, перевірена mtd1 offset 0x100000/size 0x20000, backup, JTAG recovery та стабільне живлення. Команда writer — така сама, як payload, але **env-bootdelay3.bin → підтверджена env ділянка**, не mtd3. Writer не перевіряє призначення ділянки: цей gate виконує оператор/агент перед командою. Після readback побайтово звірити з підготовленим env і перевірити CRC. Старий env зберегти. Якщо ці умови не забезпечені, спочатку відновити консоль через кваліфікований JTAG шлях; не виконувати raw env write навмання.

### 8.3 Файл на SD і пробне завантаження

Скопіювати ITB на існуючий FAT-розділ окремої/збереженої microSD як `R2TEST.ITB`; не писати DFU через dd на всю карту. Виконати sync, безпечно відмонтувати карту і вставити її в знеструмлений R2. Залишити QSPI boot mode.

На HOST відкрити UART 115200; увімкнути плату, зупинити U-Boot. Встановити фактичний mmc device/partition через `mmc list`, `mmc dev N`, `mmc rescan`, `fatls mmc N:P`.

**U-BOOT**, приклад лише якщо підтверджено 0:1:
```text
fatload mmc 0:1 0x2080000 R2TEST.ITB
printenv filesize
crc32 0x2080000 ACTUAL_FIT_SIZE_HEX
setenv bootargs console=ttyPS0,115200 root=/dev/ram0 rw
printenv bootargs
bootm 0x2080000#config@0
```

ACTUAL_FIT_SIZE_HEX замінити значенням fit_size_hex з вашого fit-manifest.json; crc32 має збігтися з manifest. `filesize` в U-Boot є hex. Для історичного образу size 0x1441d44/CRC 978300f1, але не переносити ці значення на нову збірку.

Не виконувати saveenv для RAM-test bootargs. Прибрати успадкований `initrd=0x00800000,16M`: він спричинив panic у нашій першій спробі. RAM boot не перезаписує Flash payload. Якщо boot не завершився, зберегти UART log і повернутися до заводського QSPI boot; нового payload не записувати.

## 9. Перевірити Openwifi в RAM і записати payload

Після RAM boot через DEBUG перевірити адреси та нову Openwifi identity. Хост має дістатися 192.168.10.122 через виділений Ethernet.

**BOARD:**
```sh
uname -r
cat /etc/openwifi-board
ip addr show eth0
cat /proc/mtd
cat /var/log/openwifi-monitor.log
iw dev sdr0 info
cat /sys/class/net/sdr0/type
cd /root/openwifi
./sdrctl dev sdr0 get reg xpu 11
```

Очікування: Linux 6.12.0-openwifi, board plutosky_r2, sdr0 monitor/type 803, channel 7/2442 MHz/HT20, XPU11=0x10. У поточному boot log свіжий CALIBRATION_PASS **після** кінцевого RF/FIR init. Helper має підтвердити RX і TX на всіх 4 IQ каналах по 500 ms; sampling frequency 40 MHz/FIR 1 для обох трактів. IIO device знайти за name=ad9361-phy, не вважати device0 універсальним.

RX03/TX50 — наш історичний виміряний результат. Не примушувати його вручну замість helper scan. Ранній Tuning TX FAILED може передувати успішній post-FIR калібровці; відсутній пізніший PASS закриває Flash gate.

**BOARD, bounded RX capture:**
```sh
tcpdump -i sdr0 -s 0 -c 300 -w /tmp/r2-ram-rx.pcap
```

Якщо немає кадрів, зупинити capture після заздалегідь визначеного timeout; не чекати безмежно. Перенести PCAP приватно на хост, перевірити radiotap/hardware status і kernel drops. FPGA змінює останній байт FCS: не вимагати невідповідного повного CRC32; для нашого профілю застосовується preserved CRC24 + hardware status. Виправлення metadata працює на кожний SKB, не лише IRQ.

Повторити повний 32 MiB backup з п.7 **до** payload write; звірити U-Boot `sf probe` повідомлення physical 32 MiB. Верхні 16 MiB не припускати порожніми на іншому екземплярі.

### 9.1 Зібрати статичний ARM writer

**HOST:**
```sh
cd "$HOME/src/plutosky-r2-clean/openwifi"
output/common/host/bin/arm-buildroot-linux-musleabihf-gcc   -static -O2 -Wall -Wextra tools/mtd_flash_write.c   -o "$HOME/src/plutosky-r2-clean/mtd_flash_write"
file "$HOME/src/plutosky-r2-clean/mtd_flash_write"
```

Очікується ARM32 hard-float EABI executable, статично скомпонований. Для env-кроку 8.2 його можна зібрати відразу після build, ще до RAM boot. Не запускати x86 binary на платі.

Перенести writer і ITB у /tmp плати через `scp -O` (legacy SCP, якщо Dropbear не має SFTP) або інший перевірений transport. Звірити повні SHA256 після копіювання; зробити writer executable.

**HOST, для Openwifi RAM boot після перевірки нового SSH fingerprint:**
```sh
scp -O -o UserKnownHostsFile="$KNOWN_HOSTS" -o StrictHostKeyChecking=yes \
  "$HOME/src/plutosky-r2-clean/mtd_flash_write" \
  "$IMAGES/openwifi-plutosky_r2-qspi.itb" root@"$BOARD_OPENWIFI":/tmp/
```

Для окремої env-правки п.8.2, поки працює заводський Linux, потрібні writer та підготовлений env-файл на фактичній заводській адресі; цей приклад передавання ITB не замінює env gate.

### 9.2 Останній gate і власне запис

Тільки якщо RAM boot/PN/RX пройшли, backup 32 MiB перевірений, фактичний mtd3 має offset 0x200000/size 0x1e00000, hash скопійованого FIT збігається і консоль/recovery доступні.

**BOARD:**
```sh
/etc/init.d/S95openwifi-monitor stop
chmod 0755 /tmp/mtd_flash_write
/tmp/mtd_flash_write /tmp/openwifi-plutosky_r2-qspi.itb /dev/mtd3
```

**УВАГА до семантики writer:** він стирає **весь переданий MTD partition**, потім пише image і порівнює кожний payload byte. Він не має вбудованого allowlist mtd3 і не обмежує запис лише R2; неправильний device може знищити bootchain. Перший аргумент IMAGE, другий DEVICE. Перевірку target/offset/backup виконати до запуску. За невизначеного transport result спочатку readback/status, не повторний erase.

Зробити незалежний readback exact FIT length:
```sh
# BOARD: замінити число точною десятковою довжиною вашого FIT.
head -c ACTUAL_FIT_BYTES /dev/mtd3 | sha256sum
md5sum /dev/mtd0 /dev/mtd2
```

Порівняти SHA payload з fit-manifest.json, protected partitions — з backup baseline. У shell із pipe без pipefail hash короткого read stream недостатній: перенести readback на HOST і окремо перевірити byte count. Не перезавантажуватися до завершення запису та перевірки.

## 10. Завершити налаштування QSPI boot

Після успішного readback зробити контрольований reboot з відкритою DEBUG консоллю, перехопити countdown. Старий fit_size може не вмістити новий FIT, тому **не залишати перший reboot без консольного супроводу**.

Після reboot U-Boot заново читає збережений env; перевірити bootcmd/bootargs, переконатися, що тимчасові RAM args не були збережені.

**U-BOOT:**
```text
printenv bootcmd bootargs fit_size bootdelay
setenv fit_size ACTUAL_FIT_SIZE_HEX
setenv bootdelay 3
printenv fit_size bootdelay
saveenv
reset
```

Не змінювати заводський bootcmd, offsets, Ethernet MAC/NVM або інші env змінні без окремої потреби. На нашому U-Boot saveenv успішно повідомляв `Writing to SPI flash...done`; не шукати тільки слово OK. Перевірити CRC і значення env незалежним читанням після старту.

Штатний QSPI boot має сам прочитати новий FIT, запрограмувати FPGA і завантажити Kernel/rootfs. Перевірити не просто наявність Ethernet, а новий boot log та той самий FIT/profile. Якщо bootcmd іншої factory ревізії не використовує fit_size/очікуваний offset, це окрема адаптація; не вважати блок команд універсальним.

## 11. Автозапуск і перевірка після вимкнення живлення

У source pin уже є `/etc/init.d/S95openwifi-monitor`: завантаження модулів/RF-FIR → interface down → post-FIR calibration → monitor up → ACK off → channel 7 HT20 → AGC. Окремий host daemon для цього не потрібний.

Після звичайного Flash boot повторити всі runtime-перевірки з п.9 та bounded RX capture; перевірити /boot read-only і сталість SSH keys. Після першого успіху виконати один контрольований cold boot із консольним логом і перевірити повторюваність. Довготривалий RX soak — окрема acceptance перевірка: історично був stall понад 5 h, його не закриває 300 кадрів.

Перевірити:

- board profile, kernel/module ABI, sampling 40 MHz/FIR 1/1 та свіжий CALIBRATION_PASS;
- monitor/2442 HT20/auto ACK off, відсутність test PRBS/loopback;
- Ethernet/SSH після reboot, сталість host key та доступність /boot для його збереження;
- повний payload hash/readback, env CRC/exact fit_size;
- DHCP не запускається на monitor профілі;
- backup 32 MiB існує поза SDR; тимчасовий R2TEST.ITB прибрати лише зі своєї карти після успіху.

Ручний перезапуск профілю:
```sh
# BOARD: це повторно виконує RF init і калібровку, тому не застосовувати як polling.
/etc/init.d/S95openwifi-monitor restart
```

Rootfs цього FIT — RAM filesystem. Зміна /etc або новий hostapd config під час роботи не стають постійними від sync. Для сталих змін редагувати rootfs-overlay/Buildroot recipe і перебудовувати образ; наявний SSH key store — окремий передбачений persistent path. Постійний NTP ще не налаштований.

## 12. Що далі для Wi-Fi AP

Встановлення Openwifi не дорівнює кваліфікації AP. Для AP потрібні interface AP, hostapd nl80211, ACK on, узгоджений канал/basic OFDM rates/WMM, IP/DHCP або static IPs та незалежний клієнт. Не запускати upstream fosdem script наосліп поверх S95: він може повторно змінити RF init/мережу.

Наш останній тест: beacon PASS на 2442 MHz, attenuation 10 dB; association timeout 12 s, ping NOT_RUN. Повний AP PASS потребує station dump/auth-association trace і двостороннього трафіку. Для цієї мети виконати окремий тест; не перепрошивати образ через один timeout. Нативний USB і WFB-NG також не є передумовами базового встановлення.

## 13. Типові блокери і відновлення

| Симптом | Наступна дія |
|---|---|
| Є тільки USB-C DEBUG, немає RNDIS | Використати Ethernet; це не доказ провалу Openwifi |
| Немає LAN після FPGA load | Перевірити UART boot log, R2 XSA/DT, PL Ethernet, кабель/host route |
| Kernel panic з initrd | Прибрати успадкований аргумент initrd у тимчасових RAM bootargs |
| Не зупиняється boot | Звірити bootdelay/console; один перевірений env або JTAG recovery, не цикл reset |
| mtd3=14 MiB | Не писати 21 MiB; спочатку Openwifi RAM boot і 32 MiB map |
| Tuning TX FAILED | Шукати пізніший свіжий post-FIR PN PASS; якщо його немає — зупинитися |
| Помилка key persistence | Перевірити /boot FAT mount/RO restore/S49/S51; не вимикати StrictHostKeyChecking |
| FAT unclean warning | Зберегти носій; окрема offline fsck за потреби, не доведена corruption |
| XSCT libncurses.so.5 | Справжні ABI5 libncurses/libtinfo у child env; ABI6 symlink не підходить |
| Flash write timeout | Readback і стан процесу до будь-якого повтору |

Recovery після невдалого нового payload: зберегти заводський mtd0, підняти перевірений RAM image через існуючий U-Boot/SD або кваліфікований JTAG, тоді повернути **свій** попередній payload і env/fit_size з backup з повною перевіркою. Відновлення цілого chip 32 MiB через JTAG — окремий останній шлях, не штатний крок цього гайда. Не виконувати `flash_plutosky_r2_qspi.tcl` як звичайне оновлення Linux: він пише offset 0/bootchain.

Якщо заводський bootchain уже зруйнований або конкретний board SKU не підтверджений, цього payload-only гайда недостатньо: спочатку кваліфікувати FSBL/U-Boot/JTAG recovery саме для цієї ревізії. Це не причина автоматично стерти Flash.

## Джерела та межа перевірки

- [Виробник: R2 manual](https://blog.opensourcesdrlab.com/archives/plutosky-R2): роз’єми, варіанти RFIC/PA, режими завантаження. У manual є суперечність 16/32 MB; для нашого екземпляра 32 MiB підтверджені chip probe/full readback.
- [Source pin](https://github.com/dscodetesla/openwifi/tree/d8381c449692443d7e2c0b26a497a6fc6c51635a): buildroot-build.sh, R2 packager, calibrator, S95, writer.
- [R2 XSA pin](https://github.com/open-sdr/openwifi-hw-img/tree/64f27c5f897a807cf58fc383dd4b08c728abc07c/boards/plutosky_r2).
- [Попередні результати й hashes](https://github.com/dscodetesla/openwifi/tree/docs/plutosky-r2-agent-pipeline-2026-10-27/doc/rnd/2026-10-27-agent-pipeline).

Код helper перевіряється окремо, але цей документ не означає повторного factory Flash cycle. Наш попередній екземпляр пройшов RAM/Flash/PN/monitor; наведений набір apt-пакетів на чистому хості та кожна заводська ревізія не пройшли окремого нового E2E тесту. AP, native USB і довгий RX залишаються явно позначеними як відкриті.
