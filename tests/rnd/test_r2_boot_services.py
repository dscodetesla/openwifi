#!/usr/bin/env python3
"""Isolated service logic tests; fake keys and mount/DHCP executables only."""
from pathlib import Path
import json, os, shutil, subprocess, tempfile

BASE = Path(__file__).resolve().parents[2]
COMMON = BASE / "buildroot-external/board/common"
INIT = COMMON / "rootfs-overlay/etc/init.d"
RESULTS = []

def check(condition, message):
    if not condition:
        raise AssertionError(message)

def run(script, env, *args, ok=True):
    result = subprocess.run(["/bin/sh", str(script), *args], env=env,
                            capture_output=True, text=True)
    if ok:
        check(result.returncode == 0,
              f"{script.name} failed: {result.stderr}")
    else:
        check(result.returncode != 0, f"{script.name} unexpectedly succeeded")
    return result

class Fixture:
    def __init__(self, parent, mount_mode="ro"):
        self.root = parent
        self.boot = parent / "boot"
        self.store = self.boot / ".openwifi/dropbear"
        self.runtime = parent / "etc/dropbear"
        self.bin = parent / "test-bin"
        self.mounts = parent / "mounts"
        self.log = parent / "calls"
        self.boot.mkdir(parents=True)
        self.runtime.mkdir(parents=True)
        self.bin.mkdir()
        self.mounts.write_text(f"/dev/test {self.boot} vfat {mount_mode},relatime 0 0\n")
        self.env = os.environ.copy()
        self.env.update(OPENWIFI_SERVICE_ROOT=str(parent),
                        OPENWIFI_SERVICE_MOUNTS=str(self.mounts),
                        TEST_CALLS=str(self.log),
                        PATH=str(self.bin) + ":" + self.env["PATH"])
        stubs = {
            "dropbearkey": """#!/usr/bin/env python3
import os,sys
from pathlib import Path
with open(os.environ['TEST_CALLS'],'a') as f:f.write('keygen\\n')
if os.environ.get('FAIL_KEYGEN')=='1':sys.exit(1)
assert sys.argv[1:3]==['-t','ed25519']
p=Path(sys.argv[sys.argv.index('-f')+1])
if p.exists():sys.exit(1)
p.write_bytes(b'isolated-fake-host-key')
""",
            "mount": """#!/usr/bin/env python3
import os,sys
from pathlib import Path
mode=sys.argv[2]
with open(os.environ['TEST_CALLS'],'a') as f:f.write(mode+'\\n')
if mode=='remount,rw' and os.environ.get('FAIL_RW')=='1':sys.exit(1)
if mode=='remount,ro' and os.environ.get('FAIL_RO')=='1':sys.exit(1)
p=Path(os.environ['OPENWIFI_SERVICE_MOUNTS'])
s=p.read_text()
s=s.replace('ro,relatime','rw,relatime') if mode=='remount,rw' else s.replace('rw,relatime','ro,relatime')
p.write_text(s)
""",
            "sync": "#!/bin/sh\nexit 0\n",
            "cp": """#!/usr/bin/env python3
import os,sys
if os.environ.get('FAIL_COPY')=='1':sys.exit(1)
os.execv('/usr/bin/cp',['cp']+sys.argv[1:])
"""
        }
        for name, text in stubs.items():
            f = self.bin / name
            f.write_text(text)
            f.chmod(0o755)

    def calls(self):
        return self.log.read_text().splitlines() if self.log.exists() else []

    def key(self, where, value=b"isolated-test-key"):
        where.mkdir(parents=True, exist_ok=True)
        (where / "dropbear_ed25519_host_key").write_bytes(value)

def case(name, action):
    with tempfile.TemporaryDirectory(prefix="service-case-", dir=BASE) as directory:
        action(Fixture(Path(directory)))
    RESULTS.append({"case": name, "result": "PASS"})

def fresh(f):
    run(INIT / "S49dropbear-host-key", f.env, "start")
    key = f.runtime / "dropbear_ed25519_host_key"
    check(key.stat().st_mode & 0o777 == 0o600, "runtime key mode")
    run(INIT / "S51dropbear-host-key-save", f.env, "start")
    check((f.store / key.name).read_bytes() == key.read_bytes(), "not saved")
    check("ro,relatime" in f.mounts.read_text(), "mount not restored")
    before = f.calls()
    run(INIT / "S49dropbear-host-key", f.env, "start")
    run(INIT / "S51dropbear-host-key-save", f.env, "start")
    check(f.calls() == before, "second run rotated or remounted")

def empty_store(f):
    f.store.mkdir(parents=True)
    run(INIT / "S49dropbear-host-key", f.env, "start")
    run(INIT / "S51dropbear-host-key-save", f.env, "start")
    check((f.store / "dropbear_ed25519_host_key").stat().st_size > 0, "empty store persisted")

def preserve_existing(f):
    f.key(f.store, b"isolated-persisted-A")
    f.key(f.runtime, b"isolated-runtime-B")
    run(INIT / "S51dropbear-host-key-save", f.env, "start")
    check((f.store / "dropbear_ed25519_host_key").read_bytes() == b"isolated-persisted-A", "identity overwritten")
    check(not f.calls(), "unnecessary remount")

def writable_mount(f):
    f.mounts.write_text(f"/dev/test {f.boot} vfat rw,relatime 0 0\n")
    f.key(f.runtime)
    run(INIT / "S51dropbear-host-key-save", f.env, "start")
    check("rw,relatime" in f.mounts.read_text(), "rw mount changed")
    check(not f.calls(), "unnecessary mount call")

def remount_denied(f):
    f.key(f.runtime)
    f.env["FAIL_RW"] = "1"
    run(INIT / "S51dropbear-host-key-save", f.env, "start", ok=False)
    check("ro,relatime" in f.mounts.read_text(), "failed mount changed mode")
    check(not f.store.exists(), "wrote after mount failure")

def copy_failure(f):
    f.key(f.runtime)
    f.env["FAIL_COPY"] = "1"
    run(INIT / "S51dropbear-host-key-save", f.env, "start", ok=False)
    check(f.calls() == ["remount,rw", "remount,ro"], "cleanup missing")
    check("ro,relatime" in f.mounts.read_text(), "failure left BOOT writable")

def restore_failure(f):
    f.key(f.runtime)
    f.env["FAIL_RO"] = "1"
    run(INIT / "S51dropbear-host-key-save", f.env, "start", ok=False)
    check(f.calls() == ["remount,rw", "remount,ro"], "restore not attempted")

def missing_keys(f):
    run(INIT / "S51dropbear-host-key-save", f.env, "start", ok=False)
    check(not f.calls(), "mounted without keys")

def keygen_failure(f):
    f.env["FAIL_KEYGEN"] = "1"
    run(INIT / "S49dropbear-host-key", f.env, "start", ok=False)
    check(not (f.runtime / "dropbear_ed25519_host_key").exists(), "false success")

def dhcp(f):
    init = f.root / "etc/init.d"
    init.mkdir(parents=True)
    (f.root / "etc/openwifi-board").write_text("plutosky_r2\n")
    monitor = init / "S95openwifi-monitor"
    monitor.write_text("#!/bin/sh\n")
    monitor.chmod(0o755)
    shutil.copy2(COMMON / "S80openwifi-dhcp-server", init / "S80dhcp-server")
    backend = f.root / "usr/libexec/openwifi/dhcp-server"
    backend.parent.mkdir(parents=True)
    backend.write_text('#!/bin/sh\nprintf "dhcp:%s\\n" "$*" >>"$TEST_CALLS"\n')
    backend.chmod(0o755)
    result = run(init / "S80dhcp-server", f.env, "start")
    check("Skipping DHCP" in result.stdout and not f.calls(), "monitor boot started DHCP")
    run(INIT / "isc-dhcp-server", f.env, "start")
    check(f.calls() == ["dhcp:start"], "manual AP start blocked")
    (f.root / "etc/openwifi-board").write_text("another_board\n")
    run(init / "S80dhcp-server", f.env, "start")
    check(f.calls() == ["dhcp:start", "dhcp:start"], "non-R2 changed")

def no_dhcp_package(f):
    init = f.root / "etc/init.d"
    init.mkdir(parents=True)
    for name in ["S42plutosky-r2-usb-gadget", "S43plutosky-r2-usb-console"]:
        (init / name).write_text("#!/bin/sh\\n")
    run(COMMON / "post-build.sh", f.env, str(f.root))
    check(not (init / "S80dhcp-server").exists(), "invented disabled DHCP package")

def invalid_empty_key(f):
    f.key(f.runtime)
    f.key(f.store, b"")
    run(INIT / "S51dropbear-host-key-save", f.env, "start", ok=False)
    check((f.store / "dropbear_ed25519_host_key").stat().st_size == 0,
          "overwrote existing invalid file instead of reporting it")
    check("ro,relatime" in f.mounts.read_text(), "mount not restored")

def missing_mount(f):
    f.key(f.runtime)
    f.mounts.write_text("")
    run(INIT / "S51dropbear-host-key-save", f.env, "start", ok=False)
    check(not f.calls() and not f.store.exists(), "wrote without mounted BOOT")

def postbuild(f):
    init = f.root / "etc/init.d"
    init.mkdir(parents=True)
    for name in ["S42plutosky-r2-usb-gadget", "S43plutosky-r2-usb-console"]:
        (init / name).write_text("#!/bin/sh\n")
    original = b"#!/bin/sh\n# original-isolated-dhcp-backend\nexit 0\n"
    (init / "S80dhcp-server").write_bytes(original)
    run(COMMON / "post-build.sh", f.env, str(f.root))
    run(COMMON / "post-build.sh", f.env, str(f.root))
    backend = f.root / "usr/libexec/openwifi/dhcp-server"
    check(backend.read_bytes() == original, "post-build saved wrapper as backend")
    check("openwifi-dhcp-wrapper:" in (init / "S80dhcp-server").read_text(), "wrapper missing")
    check(backend.stat().st_mode & 0o777 == 0o755, "backend not executable")

for name, action in [
    ("fresh-store-and-idempotent-restart", fresh),
    ("existing-empty-store", empty_store),
    ("never-overwrite-persisted-key", preserve_existing),
    ("preserve-original-writable-mount", writable_mount),
    ("read-only-remount-denied", remount_denied),
    ("copy-failure-restores-read-only", copy_failure),
    ("restore-failure-is-not-success", restore_failure),
    ("missing-runtime-key-is-not-success", missing_keys),
    ("keygen-failure-is-not-success", keygen_failure),
    ("monitor-skip-manual-ap-and-other-board", dhcp),
    ("post-build-rerun-preserves-original-backend", postbuild),
    ("post-build-without-dhcp-package", no_dhcp_package),
    ("invalid-empty-persisted-key-fails-closed", invalid_empty_key),
    ("missing-boot-mount-fails-closed", missing_mount),
]:
    case(name, action)

print(json.dumps({"scope": "isolated fake keys/stub mounts; no physical board",
                  "cases": RESULTS}, indent=2))
