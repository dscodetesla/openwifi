# PlutoSky-R2: source checkpoint, 26.10.2026

This branch preserves the source used during the 25–26 October R&D session, its fixes, reproducible procedure and sanitized evidence. It is not a release or a new hardware measurement.

- [Build/deployment pipeline](PIPELINE.md)
- [Findings, failed attempts and next experiments](FINDINGS.md)
- [Source/artifact provenance](provenance.json)
- [Historical validation and offline regression](validation.json)
- [Board, USB and recovery companion](https://github.com/dscodetesla/PlutoSky-R2/tree/chore/plutosky-r2-rnd-checkpoint-2026-10-26/docs/rnd/2026-10-26-openwifi-r2)

The branch starts from tested Openwifi commit `9d4d1b0030df7756a2cf99458f970af7cb66f53f`, one README-only commit after this fork's observed master. Main/master are unchanged.

The effective source included 12 locally changed paths: R2 reset GPIO100/EN_AGC98; scoped DTC C17 host build; exact Linux archive pin; R2 SPL byte padding; DFU suffix CRC and executable modes; reset-code recovery; channel7 HT20 monitor startup with auto-ACK disabled; opaque DDS state; per-frame RX status reset. These are preserved as source, not only a patch attachment.

Final recorded Flash test: 300 RX frames, 248 FPGA-good, 52 bad, 248 preserved-CRC24 matches, 300 metadata agreements, zero kernel drops. TX/AP and long RX stability remain unqualified. Native USB enumeration remains unreliable; Ethernet and DEBUG UART were usable.

Firmware, factory Flash, keys, raw PCAP and account/host dumps are not in Git. Their public summaries and input/output hashes are recorded. Local shallow checkouts are not presented as complete offline Git-history backups. GitHub branch ancestry preserves the repository history; external build caches/toolchains and private recovery backups remain separate.

The recorded module contains the pre-commit Git revision plus dirty source changes. Rebuilding this committed checkpoint changes generated Git-revision metadata and timestamps; output hashes may differ. Historical hashes identify the actual tested artifacts, not a claim of bit-for-bit reproducible future builds.
