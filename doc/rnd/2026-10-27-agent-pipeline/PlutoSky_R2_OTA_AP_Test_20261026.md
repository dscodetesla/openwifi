# PlutoSky-R2: ефірний TX та AP — фактичний тест
26.10.2026, Europe/Kyiv.

**Результат: OTA beacon TX підтверджений незалежним Wi-Fi приймачем. Повна AP-асоціація та IP-обмін не підтверджені.** Після тестів відновлено monitor-профіль і початковий Wi-Fi uplink хоста. Прошивка/Flash не змінювалися.

## Стенд і метод

PlutoSky-R2: Zynq7020/AD936x, Linux6.12.0-openwifi, встановлений образ із калібратором RX03/TX50. hostapd2.11, nl80211, канал7/2442МГц, HT20; advertised basic OFDM rates6/12/24Mbps, WMM. Тестова AP була відкритою, із MAC allowlist лише для виділених тестових клієнтів, без bridge/NAT/DHCP. Для можливого IP-тесту використані окремі10.77.0.1/24 та10.77.0.2/24.

Незалежний ефірний приймач: Wi-Fi MT7921U на X99MG. Додаткові сканування виконані на dell-T3500(RTL8822BU) та desk-tyan(RTL8812AU). Вони не знайшли тестову AP у своїх scan results; це не встановлює їхню фізичну відстань від SDR.

Створено тимчасові унікальні SSID, BSSID звірений з інтерфейсом SDR. Під час AP-тесту XPU11=0 дозволяв потрібні AP ACK. Після тесту повернуто XPU11=0x10.

## Що фактично отримано

| Рівень | Спостереження | Оцінка |
|---|---|---|
| Запуск AP | hostapd повідомив AP-ENABLED; sdr0 type AP/channel7 | PASS |
| Апаратний TX | TX IRQ зростав; сумарний Linux TX counter зріс0→35 за всі тести | PASS на рівні передачі драйвером/апаратурою |
| Незалежне ефірне приймання | X99MG побачив у свіжому scan точний тестовий SSID/BSSID, канал7,2442МГц | OTA beacon PASS |
| TX attenuation30dB | SSID не знайдений | Нижче надійного рівня спостереження в цьому тесті |
| TX attenuation10dB | SSID знайдений, NetworkManager signal54/100 | PASS; це не вимір dBm/EIRP |
| Клієнтська асоціація | Перша спроба NetworkManager завершилася timeout12s | Не підтверджено |
| Уточнювальна спроба | Попередній свіжий scan нового SSID не знайшов; тест зупинено до асоціації | NOT_RUN за precondition |
| ICMP/IP | Не запускався: немає підтвердженої асоціації | NOT_RUN |
| WPA2/WPA3, throughput, latency, EVM, RF power | Не вимірювалися | NOT_TESTED |

Перший scan читав кеш одразу після запиту rescan; його негативний результат не використано як доказ відсутності RF. Позитивний висновок отримано наступним примусовим свіжим scan з точним SSID/BSSID.

Для клієнтської спроби X99MG коротко перемикав Wi-Fi на тестову AP. Заздалегідь створено незалежний одноразовий rollback45s. Основний finally успішно відновив початковий connectionUUID і видалив тестовий NM profile; резервний процес завершився ROLLBACK_SKIPPED. Уточнювальна спроба мала60s rollback, але не дійшла до перемикання, тому він не запускався.

## Межа висновку

RF TX не є повністю «мертвим»: незалежна стандартна Wi-Fi карта декодувала наш beacon. Це сильніший доказ, ніж PN loopback або лише TX IRQ.

Водночас повний AP ще не має PASS: немає підтвердженої станції та двостороннього ping. Причину клієнтського тайм-ауту не встановлено. Це не доведений дефект конкретного драйвера/ACK/PHY. Наступний точковий тест — свіжий peer scan, синхронний запис authentication/association frames і ACK/TX status на обох сторонах, потім IP. Нові прошивки для цього зараз не потрібні.

hostapd показував нефатальний warning «Cannot open RFKILL control device». У debug-лозі був також очікуваний перехід mode busy→успішна зміна при interface down. Обидва не завадили AP-ENABLED; не оголошуються причиною timeout.

10dB — встановлене attenuation AD936x, не заявлена вихідна RF-потужність. Значення iw txpower не замінює вимір приладом. Фактичний beacon MCS/rate, EIRP та спектральна якість окремо не виміряні.

## Повернення до початкового стану

- hostapd тесту зупинений; sdr0 monitor, канал7/2442МГц HT20.
- Auto-ACK вимкнений: XPU11=0x10.
- Attenuation повернені: TX0 gain0dB, TX1 gain−89.75dB.
- SPI delays залишилися RX03/TX50.
- Flash payload MD5 a2229cc4f92f88e4ad704ae4c6895db0 збігається.
- Початковий Wi-Fi uplink X99MG активний; тестові профілі/адреса прибрані.
- Після тестів:100 RX frames,61hardware-good/39bad,100/100 узгоджених metadata,0 kernel drops. Це не контрольований тест зміни RF-якості.

## Докази

X99MG: /home/user/src/plutosky-r2-openwifi/evidence/ota-ap-20261026T170215Z/.
Позитивний ефірний результат: phase-b/x99-independent-scan.json.
Клієнт: phase-c/client-association.log, client-outcome.json, client-uplink-restore.log.
Precondition уточнення: phase-d/client-preassociation-scan.json.
Повернення/RX: post-test-health.log, post-test-capture.log, post-test-rx.json.
Сирі логи/PCAP з адресами збережені локально; у публічний репозиторій не переносилися.
