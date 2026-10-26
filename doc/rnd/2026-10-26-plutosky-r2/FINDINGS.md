# Findings and bounded conclusions

| Finding | Evidence / resolution |
|---|---|
| Factory .2.1 over LAN timed out | Factory USB used192.168.2.1; actual Ethernet192.168.1.10. ARP flux/asymmetric replies explained misleading reachability. Openwifi Ethernet default192.168.10.122; dedicated host link192.168.10.2 without gateway. |
| Native USB unreliable | DEBUG FTDI works; native UDC bound/powered with unknown speed and no carrier. Factory briefly enumerated then failed with USB descriptor error-71. Do not blame RNDIS/IP before enumeration. Physical data cable/port/PHY path still needs testing. |
| AD9361 calibration timeout | R2 DTS used reset67 while supplied FPGA uses100(EMIO46); EN_AGC corrected66→98. RAM reset overlay restored probing, then DTS fixed permanently. Reference clock40MHz was not changed. |
| DTC GCC15 build error | Old DTC1.7.2 hit C23 const-preserving glibc functions under Werror. Scoped gnu17 restored the previous dialect; actual R2 DTS compilation passed. |
| Kernel fetch overhead | Exact ADI commit archive replaced a full Linux Git fetch. Transport change, not a source-revision change; config replay passed. |
| SPL padding host dd incompatibility | R2-only patch replaces padding with exact Python NUL alignment. Prefix unchanged,137230→137232bytes. Built SPL remains unqualified for replacing factory bootchain. |
| Executable permissions | Preserve100755 for S42,S43,packager,newS95. Text-only uploads that lose chmod reintroduce startup/packaging failure. |
| DFU suffix rejected | Reflected running CRC state needs final XOR inversion relative to binascii.crc32. Independent dfu-suffix passed after correction. |
| DDS diagnostic garbage | Stale copy of kernel-private DDS struct caused invalid field/pointer reads. Opaque pointer/exported helper retained; log now uses Linux device name. |
| Inconsistent BADFCS metadata | rx_status was initialized once per IRQ and reused across buffers. Per-skb reset prevents FCS/short-GI/A-MPDU carry-over. Pre-fix300frames had2 mismatches; fixed RAM351 and Flash300 hadnone. |
| FCS interpretation | Last octet is FPGA status/sequence, not pristine CRC32. Compare first3 FCS octets and status flags; do not repair the PCAP or report a full32-bit CRC pass. |
| RX stall after5h | No new frames/IRQs. Retune/interface/module reload failed to recover; full boot restored RX. DDS/RX-metadata patches are not claimed to fix this separate volatile-state fault. |
| TX all-fail | Current-rate internal loopback had working RX timing window, both PLL locks, all32 TX delay positions failed. Pinout/framing comparison found no decisive mismatch. Need per-channel PN_ERR/PN_OOS; RX success does not qualify TX/AP. |
| SSH first-boot identity | Dropbear -R can generate keys lazily after the S51 save step already made an empty directory. Session saved the verified public-identity key after first connection and verified next-boot persistence. Generic source script not repaired in this checkpoint; empty directory alone is not a success check. |
| Unsynchronized boot time | Logs started at1970; archive original logs and host collection time before RAM clock correction. No persistent NTP configuration was installed. |
| DHCP/FAT/unused peripherals | AP DHCP config starts before sdr0 and is unused for staticLAN+monitor. FAT dirty/ad7291/mwipcore warnings retained; no speculative SD repair or peripheral deletion. |
| Mixed old manifest fields | Raw local manifest retained historical false/stale top-level values beside final state. Publish curated validation/provenance; historical PID/build fields are not live job state. |

## Next experiments

1. Long RX with known periodic beacon, collecting RF/IRQ/DMA state before resetting on a stall; separate RF rebind from PL reload.
2. Per-channel TX PN status and test-source readback before another delay sweep or FPGA changes.
3. Replace only the native USB data cable, then independently test another host port.
4. Fix first-boot key persistence and monitor/AP service profiling in a separately tested change; do not silently fold untested service behavior into the recorded firmware.

The original experiment completed a useful RX bring-up. Open TX/stability/USB failures remain failures, not hidden gates or production readiness claims.
