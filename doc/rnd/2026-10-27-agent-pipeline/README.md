# PlutoSky-R2: передача знань і pipeline для ШІ-агента
27.10.2026, Europe/Kyiv. Цей каталог є актуальною точкою входу; старі snapshots залишаються історичними.

- [Гайд агента](AI_AGENT_GUIDE.md): підготовка, build, RAM/Flash deployment, monitor, AP, WFB та перенесення між хостами.
- [Поточний підтверджений стан](CURRENT_STATE.json): докази 26.10; це не live telemetry 27.10.
- [Знахідки, помилки й рішення](FINDINGS.md).
- [Профілі хостів](HOSTS.json): інвентаризація 27.10 без зміни мережі.
- [Web ChatGPT та інші ШІ-клієнти](INTEGRATION.md).
- [Ефірний TX/AP](PlutoSky_R2_OTA_AP_Test_20261026.md).
- [WFB-NG / OFDM](PlutoSky_R2_WFB_NG_OFDM_Assessment_20261026.md).
- [Read-only preflight](preflight.py), [manifest](manifest.json), [перевірки](test_preflight.py).

Це інструкція для автономного агента з явними gates, а не твердження, що універсальний unattended runner вже розгорнутий. У цій операції збережено документацію/інструмент preflight; нових Flash/RF операцій не виконано. Автоматизація не може програмно переставити кабелі, надати відсутні права або створити авторизацію облікового запису.

Canonical implementation: [openwifi calibration branch](https://github.com/dscodetesla/openwifi/tree/fix/plutosky-r2-timing-calibration-2026-10-26).
База цього доповнення: openwifi d8381c449692443d7e2c0b26a497a6fc6c51635a; PlutoSky-R2 1a256fe544db3e83140ef50a2ff4cc72482e3c2d.
Новий документаційний checkpoint не змінює кваліфікований FIT і не є merge/release.
