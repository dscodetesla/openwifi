# Гайд ШІ-агента: PlutoSky-R2 / Openwifi на трьох хостах

27.10.2026, Europe/Kyiv. Спочатку X99MG, потім dell-T3500, потім desk-tyan.

## 0. Доручення, готовність і контракт

Прочитай README.md, CURRENT_STATE.json, manifest.json, FINDINGS.md, HOSTS.json та локальні AGENTS.md. Не вважай минулий чат або старий PASS доказом поточного стану. Кваліфіковано: один образ R2 із фіксованими 40 MHz, Flash boot, цифровий PN, monitor RX та OTA beacon. AP association і WFB link НЕ кваліфіковано.

Ціль обирається явно: monitor | ap | wfb. COMPLETE означає виконання всіх перевірок обраної цілі на всіх запитаних хостах, а не закінчення доступних команд. Гайд задає детерміновану послідовність для агента; цей каталог не надає апаратних адаптерів, облікових даних, кабелів чи універсального runner для роботи без нагляду. Для підготовленого стенда агент виконує дозволені кроки без повторних запитів; відсутність фізичного підключення або потрібного дозволу означає BLOCKED із точним способом усунення.

Зберігай приватний run-dir поза Git з режимом 0700. Для кожної команди зафіксувати timeout, exit code, час UTC із позначенням часового поясу, обмежені stdout/stderr і хеші вихідних артефактів. Приватні ключі й паролі зберігати лише в secret store або локальних файлах із правами 0600; не передавати через argv, чат чи Git. Для SSH суворо перевіряти known hosts: спочатку звірений fingerprint через довірений канал, далі StrictHostKeyChecking=yes; зміну ключа не приймати автоматично.

## 1. План і захист від loop/дрейфу

1. Задай run_id, board_id (локально звірений silicon/board identity, не просто IP), owner_host, goal, desired manifest hash.
2. Один координатор володіє board lock; локальний flock захищає лише один хост, тому для кількох потрібний спільний авторитетний lock/lease й явний handoff. Після завершення lease перевірити попередній процес; не забирати автоматично керування живою платою.
3. Перегляд sources/docs/build можна делегувати незалежним subagents. Вони повертають знахідки й diff, не змінюють SDR. Root виконує зміни послідовно. CLI agent не запускає ще один orchestrator.
4. Один пакет інвентаризації на хост; не більше 2 незалежних викликів читання одночасно та лише одна операція зміни. Опитувати job з інтервалами приблизно 15 → 30 → 60 s із jitter; враховувати фактичний прогрес. На 429 — Retry-After; після 3 невдалих transport retries зупинка/BLOCKED. За невизначеного результату запису спочатку виконати reconcile/readback, НЕ повторювати запис.
5. Для reboot/Flash/calibration не робити сліпих повторних спроб. Новий тест лише з гіпотезою, критерієм успіху та cleanup. У кінці кожної фази атомарний checkpoint (tempfile, fsync, rename).

Контракт стану (приклад, не поточна job):

```json
{
  "schema_version": 1,
  "run_id": "generated-uuid",
  "board_id": "verified-locally",
  "owner_host": "X99MG",
  "goal": "monitor",
  "phase": "DISCOVER",
  "status": "PENDING",
  "manifest_sha256": "compute-from-manifest-bytes",
  "fit_sha256": null,
  "attempt": 0,
  "evidence": [],
  "updated_at": null,
  "block_reason": null,
  "next_action": "read-only inventory"
}
```

Статуси PENDING/RUNNING/PASS/FAILED/BLOCKED/NOT_RUN/SKIPPED. Записи evidence містять: relative path, sha256, created_at, board_id, source_commit, command exit. При resume перевірити фактичні процеси, апаратний стан і хеші, а не лише status. Відновлення контексту не запускає старий PID як нову job.

## 2. X99MG: discovery та окрема робоча копія

1. Прочитати /home/user/AGENTS.md і AGENTS.md проєкту, перевірити hostname, ОС, disk free, git/python/make/compiler/SSH/NMCLI, версії встановлених tools, routes, active NM UUID, USB/serial by-id, повноваження. Не встановлювати pip packages у загальне середовище; використовувати venv/uv проєкту.
2. Виконати preflight.py з manifest; опційно --fit FILE. Він виконує лише локальні перевірки без змін і не кваліфікує апаратну частину.
3. Зберегти наявний dirty checkout та перелік змін; не робити reset/clean. Створити новий checkout у сусідньому каталозі від точного commit; не використовувати output іншого профілю плати. При partial clone читати точні refs, не весь Git archive.
4. Поточний фізичний SDR керується через виділений Ethernet; старий default 192.168.2.1 не є доказом адреси. Останній R2 LAN 192.168.10.122, але його треба звірити з board identity/SSH key. На desk-tyan 192.168.2.x належить іншій ADALM PLUTO.
5. X99 Wi-Fi MT7921U — єдиний WAN. Зберегти через нього керування; для AP client test обрати окремий peer. Не перепризначати primary WAN без незалежного rollback і перевіреного alternate control path.

Приклад нового source checkout (папка має бути відсутня):

```sh
git clone --no-checkout https://github.com/dscodetesla/openwifi.git openwifi-agent
cd openwifi-agent
git checkout --detach d8381c449692443d7e2c0b26a497a6fc6c51635a
git submodule update --init buildroot
git -C buildroot rev-parse HEAD
# Перевірити manifest.source.buildroot_commit перед build.
```

Документацію цього checkpoint читати з її власного точного commit; source pin вище навмисно окремий від doc revision. Отримати openwifi-hw-img 64f27c5f897a807cf58fc383dd4b08c728abc07c в sibling dir; перевірити наявність boards/plutosky_r2/sdk/system_top.xsa. Не підміняти XSA іншою моделлю. Vendor reference a8a7... лише еталон board, не наш deployed rootfs.

## 3. Build і статичні gates

Переважно використовувати вже перевірений FIT з manifest і skip build, якщо потрібне точне відтворення вже встановленого образу. Новий build не зобов’язаний бути byte-identical (timestamps/tool versions); інші байти отримують новий manifest і нові RAM/Flash gates, а не переписаний trusted hash.

Для повторної збірки змінених sources у новому checkout:

```sh
OPENWIFI_XSA=/absolute/path/openwifi-hw-img/boards/plutosky_r2/sdk/system_top.xsa \
  ./buildroot-build.sh plutosky_r2 rebuild-system
```

Для першого build також підтриманий ./buildroot-build.sh plutosky_r2 build. Важливо: cached build може пропускати змінені local packages; при helper/package/kernel змінах використовувати rebuild-system у виділеному output. Ця команда очищує каталоги збірки Linux/packages, тому не запускати на чужій робочій збірці. Shell script і Buildroot recipe — джерело залежностей; відсутні залежності хоста встановлювати за їхніми diagnostics через дозволену процедуру роботи з packages, не вигадувати універсальний apt набір. Поточне виправлення не вимагає нового Vivado bitstream; FPGA rebuild є окремою кваліфікацією.

Запустити tests/rnd/test_r2_boot_services.py згідно його CLI; перевірити попередження компіляції helper, режим і порядок S95, правильність DTS GPIO/refclock, ELF ARM32 musl ABI, helper SHA і kernel/module ABI. Перевірити buildroot/linux pins проти manifest. Генеровані DTS/config і їх sha зберегти приватно.

Вихідні артефакти — output/plutosky_r2/images; packager може залишити DFU, а ITB потрібно витягти окремо. Перевірити DFU suffix CRC, FIT/FDT total_size, список компонентів і хеші через dumpimage, відсутність перекриття діапазонів завантаження, правильні .bit та config@0. Не відкидати 16 байт у довільного файлу.

Порівняти байти й права helper/S95, вбудованих у cpio. Заархівувати точний FIT+manifest на приватному artifact store; у Git лише sources/docs/sanitized summary.

## 4. RAM gate до будь-якого нового Flash

Поточний стабільний Flash не чіпати для documentation/status. Якщо новий FIT або невідомий installed hash:

1. Підтвердити власника, board identity, поточну partition map та спосіб recovery. Звірити UART device by-id/115200 та JTAG device ACL. Bus 003/100 — старий приклад, не команда для поточного node.
2. Якщо потрібний JTAG reset — лише для перевіреної цілі xc7z020/CortexA9#0. Не скидати інший SDR і не відтворювати panic повторенням старої команди. Прив’язати hw_server до loopback; XSCT ABI5 задавати лише в дочірньому середовищі, без глобальних library symlinks.
3. За countdown prompt UART зупинити boot (останнє bootdelay 3). Linux reset code 2 не гарантує stop; якщо немає контролю, BLOCKED, не нескінченні reboot.
4. RAM FIT через перевірений доступний SD/TFTP adapter. Для історично успішного SD path: fatload FIT до 0x2080000, точний size й CRC32 звірити, тимчасові bootargs 'console=ttyPS0,115200 root=/dev/ram0 rw', bootm 0x2080000#config@0. Конкретні mmc device/partition визначити за інвентаризацією; не припускати 0:1. Не зберігати RAM test args в env.
5. Перевірити новий boot id, RAM boot mode, kernel 6.12.0-openwifi/DT/AD936x, strict SSH, helper hash; 40 MHz для обох трактів, FIR 1/1, свіжий CALIBRATION_PASS після останнього RF/FIR init, 500 ms RX/TX, all 4 PN zero, FDD/no PRBS/no loopback. RX03/TX50 — історичний результат, приймати виміряний helper центр, не задавати примусово.
6. Monitor 2442 HT20, ACK off, XPU11=0x10; bounded RX capture з analyzer, metadata узгоджені, 0 kernel drops у контрольованому capture. Зовнішні bad FCS не рахувати автоматично software failure, але зафіксувати. Старий status не підходить.
7. На failure повернутися до перевіреного Flash boot, зберегти UART log/actual state, FAILED, без Flash write.

## 5. Flash deployment gate

Пропустити перезапис, якщо actual installed payload readback SHA/length == qualified manifest і boot gates вже підтверджені. Невідомий MD5 не замінює trusted SHA для нового артефакту.

Перед write: повний 32 MiB backup усіх реальних MTD ділянок, size/hash і читабельність, окрема копія поза платою, env CRC/payload size, mtd0/mtd2 hash, відсутність overlap. Backup непублічний. Карта поточного R2 у FINDINGS; /proc/mtd і sysfs offsets мають збігтися. Writer разом із partition map має відмовлятися від короткого або завеликого образу чи неправильної цілі; не використовувати generic dd як звичайну прошивку.

Новий payload лише mtd3 offset 2 MiB/size 30 MiB після exact same FIT RAM gate. Native writer CLI image→device. Після write повний байтовий readback exact length + SHA, перевірити, що mtd0/mtd2 не змінені. Зберегти fit_size відповідно до нового точного розміру лише за перевіреною процедурою зміни env; поточний 0x1441d44. Ознакою успіху U-Boot saveenv може бути 'Writing to SPI flash...done'. Env CRC перевірити.

Звичайний Flash boot повторює runtime gates із п. 4; SSH host key continuity/DHCP skip, /boot RO. При failure відновлення лише за явним verified plan через UART/JTAG, не автоповтор erase.

Видалити лише власний SD test FIT після успіху, зупинити тимчасовий hw_server, переконатися, що немає активного MTD writer.

## 6. Monitor, AP та WFB — незалежні acceptance гілки

Monitor: calibrator startup → sdr0 monitor → 2442 HT20 → ACK off → RX capture. Цифровий PN не є RF EVM.

AP: окремий виділений peer з незалежним керуванням і відомим місцем/антенами. Унікальний SSID; hostapd nl80211/HT20/WMM/basic OFDM 6/12/24; ACK увімкнути лише для AP; тимчасова окрема subnet без NAT/bridge. Синхронні UART/kernel/hostapd й peer 802.11 traces, свіжий scan для точного SSID/BSSID перед association. Gate: AP-ENABLED + незалежний beacon + authentication/association frames і station dump на SDR + peer connected + статичний IP + двосторонній ping + bounded UDP payload hash. Тайм-аут без кадрів не доводить причину. Лише одна повторна спроба після нового доказу/корекції; не reflash від timeout. У finally видалити тестові IP/NM profile/hostapd PID, повернути monitor/ACK off/частоту/gains/первинний WAN. Аттенюація не dBm. RF режим/допустимий канал/антени — частина конкретного стенда.

WFB: незалежний goal, AP не є передумовою. Прочитати WFB assessment; зафіксувати один upstream commit на обох peers. Buildroot сьогодні C++/libsodium/Python3 off: додати окремий reproducible package + toolchain C++ config і новий profile, не підкладати чужий glibc бінарник. У варіанті core-only Python може працювати поза ARM; RX forwarder/host aggregator та TX distributor/board injector — можливі варіанти після перевірки source CLI. Ключі генерувати локально, не зберігати в Git. Новий rootfs → новий manifest/RAM gates.

Кандидат HT20, MCS0 потім 1, long GI, STBC0, LDPC0, force_vht False, data frames, NOACK, packet FEC 8/12, початковий UDP ≤1445; MPDU разом з overhead має вміщатися в RX buffer: <2048. common.primary False або core binaries, щоб WFB не перезапускав RF init поверх S95.

RX first від кваліфікованого peer → radiotap FCS flags/filters → session/auth/FEC → bounded TX в обидва боки → hash/sequence/payload → MCS/length/burst loss/latency/CPU → soak довше історичного 5 h stall. Критерії packet loss/latency/throughput задаються до експерименту. sendmsg return не є OTA pass. PHY LDPC і packet RS FEC незалежні. Сьогодні ця гілка NOT_RUN.

## 7. Перенесення X99MG → dell-T3500 → desk-tyan

Спочатку завершити X99 manifest/build/runtime checkpoint. На другому хості повторити host discovery/preflight, отримати точні sources/FIT/checksums без переносу output toolchain наосліп; перевірити залежності. dell Wi-Fi є sole WAN, не використовувати як AP client замість керування. Прямого підключення R2 до dell ще НЕ підтверджено: потрібні власні кабель, маршрут, ACL та перевірений SSH fingerprint. Не обов’язково перепрошивати плату при переносі; RAM boot потрібний лише для нового артефакту або за потреби відтворення.

Handoff: старий власник завершує writers/capture/UART/JTAG/RF test, зберігає final state і lease-release; новий звіряє той самий board id, fingerprint/FIT hash; потім бере lock. Можливість фізичного перепідключення або керованого USB перемикання не підтверджена — стан агента BLOCKED, доки вона не з’явиться.

Третій desk-tyan: wired WAN зберегти, spare RTL8812AU може бути OTA peer після mode/driver qualification. Наявний USB ADALM PLUTO — інша плата, не R2. Повторити перевірки того самого manifest/runtime; не переносити eth names/NM UUID/default IP. Історична інвентаризація HOSTS допомагає планувати, але не є беззастережним allowlist.

## 8. Інтеграція та завершення

Виконати INTEGRATION.md. Тести MCP не є Flash/RF qualification. Для кожного host зберегти capability matrix і actual direct call receipt. Відсутній tunnel-client у PATH не означає відсутність installed service.

Звітувати окремо про: source/docs commit, build artifact hash, board runtime, OTA/AP, WFB, host handoff, integrations. Не підвищувати готовність за одним green check.

Під час final cleanup перевірити режим плати, gains/ACK, власні процеси, тимчасові файли, /boot RO та WAN route. Звільняти lock лише після завершення власних процесів. Public commit тільки sanitized sources/docs/manifest; raw captures, full Flash/env, host keys/private profiles залишити приватними.

Для recovery створити Git bundle для точного ref, виконати git bundle verify і sha256sum, указати prerequisites. Відновлювати в новий checkout, не робити reset dirty workspace.
