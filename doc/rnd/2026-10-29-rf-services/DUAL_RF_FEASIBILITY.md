# PlutoSky-R2: розблокування другого RF-тракту
2026-10-29, Europe/Kyiv. SOURCE/XSA AUDIT; RF/build NOT_RUN.

## Фактична основа
X99MG: /home/user/src/plutosky-r2-openwifi/openwifi.
Driver HEAD 9d4d1b0030df7756a2cf99458f970af7cb66f53f плюс збережені dirty changes (не перезаписані).
XSA: openwifi-hw-img/boards/plutosky_r2/sdk/system_top.xsa
SHA256 e684d2908da3d159d1c7a797a3994aadd80449f4d907cd7334b989ce8c988c1e.
git_info: openwifi-hw ca50ac833184f37307a96994e46333c60615de86, Vivado2022.2, ADI7ea497b6c24172ee293d6e0a9a387a16973da5ce; OpenOFDM b2a4833e133642cd4e0669349b837748d2c0d8ca.
Це аудит наявного hardware input; відповідність поточного завантаженого bitstream цій XSA у цій ітерації не перевірялася.

## Встановлена причина
- driver/sdr.c:1515–1522: TXmask1/2/3 дозволені, RXmask3 відхилена; target_rx_ant=((rx_ant&1)?0:1).
- Тому дозвіл RXmask3 сам по собі вибере лише RX0, не додасть одночасний decoder.
- driver/sdr.h:334: num_different_channels=1.
- driver/sdr.c:2517/2536: HT MCS rx_mask[0]=0xff, один spatial stream.
- XSA system.hwh: axi_ad9361 MODE_1R1T=0; присутні всі adc_i0/q0/i1/q1 і dac_i0/q0/i1/q1.
- По одному openofdm_rx_0, openofdm_tx_0, rx_intf_0, tx_intf_0, xpu_0.
- openofdm_rx_0.sample_in -> rx_intf_0.sample0.
- side_ch_0.sample0_in/sample1_in -> rx_intf_0.sample0/sample1.
Висновок: RF2 доходить до PL, але нема другого пакетного PHY/MAC path. Два DMA у XSA не означають два модеми (packet/sidechannel roles потребують явного tracing).
Upstream rx_intf RTL на ca50... підтверджує перестановку двох IQ половин за ant_flag, не два RX decoders.

## Шляхи розвитку
A. Без HDL: вибір RX1/RX2 і TX1/TX2; TXmask3 — CDD/один payload; coherent dual IQ capture. Потрібна окрема RF-кваліфікація R2.
B. RX diversity: вибір антени до/між пакетами або coherent combining. Combining потребує компенсації фази/затримки/підсилення та HDL; це один modem, не два.
C. Dual RX: sample1 -> другий openofdm_rx; окремий packet/FCS/metadata path, packet arbitration/queues/DMA/IRQ, driver demux/counters. Почати monitor RX-only, TX/ACK ще спільні/вимкнені в контрольованому тесті. Це менший проміжний кандидат, не готовий dual modem.
D. Dual independent bidirectional modems: два RX/TX PHY/MAC contexts, xpu/CCA/ACK/TSF/queues, RF routing, address/IRQ/DMA map і Linux instances; централізоване керування спільними clock/rate/FIR/LO. Потрібні synthesis/utilization/route/timing/RF tests; місткість XC7Z020 НЕ доведена.
E. Два незалежні RF frequency bands: shared RX LO та shared TX LO не дають окремого LO кожному тракту. Близькі цифрові subchannels через DDC/DUC — окремий bandwidth/channelizer проєкт. 2.4 і5.8GHz одночасно на двох RX одного AD936x так не реалізувати; потрібні інший transceiver/зовнішня конверсія/другий SDR.

## Рішення для video+MAVLink+mesh
Не робити cosmetic mask/channel-capability patch.
Найменший дослід на одній R2: підтвердити обидва RF paths, зв'язок sample1 і side_ch, після цього dual RX-only synthesis кандидат. Full dual modem — наступний окремий етап після resource gate.
Для спільного каналу WFB і mesh спершу дослідити multi-interface/ACK/airtime ownership; додаткові VIF не дають окремого PHY або окремої частоти.
Для незалежних одночасних WFB і mesh найменший system-risk кандидат — два кваліфіковані радіо. Це альтернатива, не заміна вимоги дослідити R2.

## Наступні gates
1. Відновити exact editable ca50...+OpenOFDM sources; не використовувати лише XSA/ELF.
2. Отримати baseline hierarchical utilization і timing для XC7Z020 без припущення 2x LUT=2x повний design.
3. RX-only branch: тест двох перекривних у часі незалежних frame sequences на двох входах, незалежні FCS/counters/antenna metadata; synthesis і route.
4. RAM qualification до будь-якого QSPI; persistent gate окремий.
Поточна ітерація: тільки читання sources/XSA та цей документ; build, RF injection, Flash, reboot, register changes NOT_RUN.

## Джерела
https://github.com/open-sdr/openwifi/blob/9d4d1b0030df7756a2cf99458f970af7cb66f53f/driver/sdr.c
https://github.com/open-sdr/openwifi/blob/master/doc/README.md
https://github.com/open-sdr/openwifi/blob/master/doc/app_notes/iq_2ant.md
https://github.com/open-sdr/openwifi-hw/blob/ca50ac833184f37307a96994e46333c60615de86/ip/rx_intf/src/rx_intf.v
https://wiki.analog.com/resources/eval/user-guides/ad9361
