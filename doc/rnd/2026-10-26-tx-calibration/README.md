# PlutoSky-R2 TX calibration investigation — 2026-10-26

Status: deployed to mtd3; complete readback, normal Flash boot, digital RX/TX PN and monitor RX qualification PASS. SSH works. OTA TX/AP remains unqualified.

## Reproduced fault and boundary

The previous 40-MHz kernel calibration sometimes selected RX delay 0x90 and then reported every TX point failing. An independent 20-ms PN test of RX0x90 and RX0xF6 failed on all four IQ channels. RX0x03/04 passed. With RX0x04 held fixed, a full 16x16 TX grid found77 points with all four statuses zero for500ms; the bad points showed PN_OOS/PN_ERR. Source pin/IOSTANDARD comparison found all16 TX signals identical to vendor constraints; pinned TX/RX custom PN polynomials also matched. This rules out an invariant all-TX-dead interpretation; it does not prove implemented routing or RF output.

A fresh kernel current-rate test could pass RX03/TX50 or60, then another test chose the invalid RX clock-delay branch and failed TX. Replaying rf_init_11n.sh reproduced TX FAILED during RF/FIR setup, while an immediately following current-rate test passed. The demonstrated failure mechanism is acceptance/use of an RX timing choice that is not valid under the subsequent independent measurement; it prevents the TX PN test from synchronizing. The underlying clock/settling reason for the unstable scan branch is not established. No global AD9361 kernel change or guessed pin swap is applied.

TX packet count0 and disabled auto-ACK are expected for the receive-monitor profile; they are not independent evidence that the digital transmitter is broken.

## Scoped correction

tools/plutosky_r2_calibrate.c runs after final RF/FIR initialization, with sdr0 down, only on the exact R2/40-MHz/2R2T/core/FIR profile. It mutes both RF paths, tests the16 actual RX data-delay points with clock tap0, selects the center of the longest passing window, then tests16 actual TX clock-delay points with data tap0. Each point has20ms settling plus20ms PN observation. Both selected points require500ms all-four-channel confirmation. The hardened version verifies PN selectors, DAC sources and channel controls before interpreting statuses. It restores saved configuration and gains, retaining only confirmed delays; failures/signals restore original delays. It never writes Flash, changes rates, skips TX checks, or treats the kernel debugfs write return code as a PASS.

S95openwifi-monitor invokes the helper after wgd/FIR and before interface-up; a nonzero result prevents that startup path from proceeding. Buildroot compiles/installs it only for the R2 DTS profile. This is a fixed-profile startup correction, not a general multi-rate calibration replacement or an AP qualification.

## Evidence completed

- Native full TX grid:77 confirmed points,500ms each, RX04.
- Prototype automatic calibration:3 repeated successes plus successful recovery from deliberately bad RX90; all selected RX03/TX50.
- Target interface-up guard rejected execution; SIGTERM returned2 and restored RX03/TX50, FDD, no-loopback/no-PRBS.
- Exact window-selection function:65,536 patterns passed offline.
- Existing14 boot-services cases passed with their standalone runner.
- Canonical hardened C: warning-clean cross-build; complete image assembly passed; embedded helper and S95 bytes/modes verified.
- Kernel, FPGA bitstream and sdr.ko unchanged from the prior verified image.
- Hardened helper passed both corrected RAM boot and normal Flash boot; exact helper hash and both500ms PN confirmations were verified.

The branch also retains the preceding SSH key-store/DHCP boot fixes. Those were deployed and boot-verified in the prior services image.

## Recovery and completed deployment

The first RAM launch failed before init because manual bootm inherited initrd=0x00800000,16M. JTAG access was restored with the USB-device ACL (UART access alone was insufficient); the identified Zynq7020 was reset to the previous Flash image. XSCT used existing genuine ABI5 libraries through child LD_LIBRARY_PATH only.

Factory bootdelay0 prevented reliable UART interception. A private complete environment backup was made; only bootdelay0->3 and its CRC changed (at most5bytes), with full128KiB readback. The abort key sent after the countdown prompt then reliably entered U-Boot. Linux reset-code2 is not assumed to guarantee console entry in this vendor bootloader.

Corrected RAM procedure used fatload of TXTEST.ITB to0x2080000, CRC32 verification, temporary bootargs console=ttyPS0,115200 root=/dev/ram0 rw, then bootm config@0. No stale initrd= argument and no saveenv of RAM-test bootargs.

The hardened helper passed RAM boot: RX03/TX50,300records294hardware-good/6bad,300consistent metadata,0kernel drops. Only after this gate was mtd3 programmed; every payload byte read back and MD5 matched. Bootchain mtd0 and NVMFS mtd2 hashes were preserved. Exact fit_size 0x1441d44 was saved, environment CRC checked, and normal Flash boot repeated all gates.

Final Flash result: RX03/TX50, both500ms confirmations all4statuses zero; 300frames,292good/8bad,300consistent metadata,0kernel drops. Strict SSH continuity/key persistence, DHCP skipped for monitor, /boot read-only, channel7/2442MHz HT20 and XPU11=0x10 passed. Temporary SD FIT was removed.

FIT:21241156bytes, SHA256 b90269d4454e097fcf25ec89ce1e1f8fe75846b60c4e0aa5d6e1b77fe0091b98, MD5 a2229cc4f92f88e4ad704ae4c6895db0; helper SHA256 e395597f28c1a4ede9e31294d1b5320894ecbaface3082d58a9b3c47488ab0b6. Kernel, FPGA and sdr.ko unchanged. Early upstream Tuning TX FAILED can precede successful post-FIR recovery; require the later fresh CALIBRATION_PASS/confirmations rather than interpreting the old warning as current failure.

An additional maximum-attenuation packet-injection experiment was deliberately deferred: this driver maps89.75dB attenuation to radio-off/rfkill, making that setup unsuitable. No injection was run and no OTA qualification is claimed. Capture quality differences across runs are not a controlled RF-performance comparison.

## Remaining limits

No OTA TX/AP, receiver power/EVM test, packet injection or RF output measurement was performed. Auto-ACK remains disabled. Long-duration RX stability, native USB enumeration and persistent clock synchronization remain open. Raw packet captures, Flash dumps, host keys and machine-specific access details are not published.
