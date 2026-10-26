# PlutoSky-R2 TX calibration investigation — 2026-10-26

Status: tested calibration prototype; hardened source/image built; normal-boot qualification and Flash deployment BLOCKED by JTAG USB permission. The physical board currently has a kernel panic from the RAM boot procedure described below. Do not report it operational.

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
- Hardened helper normal-boot execution is NOT yet qualified: RAM boot failed before init.

The branch also retains the preceding SSH key-store/DHCP boot fixes. Those were deployed and boot-verified in the prior services image.

## RAM boot failure and recovery gate

The new FIT was copied to the existing SD FAT partition as TXTEST.ITB and verified by SHA256 plus U-Boot RAM CRC32. No saveenv or Flash programming occurred. Manual bootm inherited the generic vendor argument initrd=0x00800000,16M. Linux disabled that overlapping initrd and panicked mounting root. This is a test-launch fault, not a helper result.

Required next steps:
1. Restore JTAG libusb access on the host, then enumerate and verify the exact board before a system reset. Existing UART permission does not grant USB-bus/JTAG access.
2. Return to U-Boot and print bootargs. For the next RAM test explicitly set temporary bootargs to console=ttyPS0,115200 root=/dev/ram0 rw (omit the stale initrd= argument); do not save the temporary environment.
3. Load TXTEST.ITB from SD, verify byte count/CRC32, boot its config@0. Require CALIBRATION_PASS, expected helper hash, monitor channel7/2442MHz, XPU11=0x10, SSH key continuity and RX capture.
4. Only after those gates, program mtd3 with readback verification, save the exact new fit_size and repeat normal Flash boot tests. Preserve bootchain/NVMFS. Use the existing complete factory backup for recovery.
5. Remove the temporary SD FIT after successful final verification, restoring the SD mount read-only.

Current USB-device ACL must be checked against the live device path; its bus/device number changes after reconnection. A privileged setfacl command was rejected by the host-control plugin. Final deployment remains blocked until host access or a physical board reset is provided.

## Remaining limits

No OTA TX/AP, receiver power/EVM test, packet injection or RF output measurement was performed. Auto-ACK remains disabled. Long-duration RX stability, native USB enumeration and persistent clock synchronization remain open. Raw packet captures, Flash dumps, host keys and machine-specific access details are not published.
