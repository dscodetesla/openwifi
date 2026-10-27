#!/usr/bin/env python3
"""Read-only local host/artifact probe. No SSH, RF, package install or Flash."""
import argparse
import hashlib
import json
import pathlib
import platform
import shutil
import subprocess
import sys

def verify_fit(path, manifest):
    p = pathlib.Path(path)
    h = hashlib.sha256()
    size = 0
    with p.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            size += len(chunk)
            h.update(chunk)
    expected = manifest["fit"]
    actual = {"bytes": size, "sha256": h.hexdigest()}
    actual["match"] = size == expected["bytes"] and actual["sha256"] == expected["sha256"]
    return actual

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--manifest", type=pathlib.Path, default=pathlib.Path(__file__).with_name("manifest.json"))
    ap.add_argument("--fit", type=pathlib.Path)
    args = ap.parse_args(argv)
    result = {"scope": "READ_ONLY_LOCAL_PREFLIGHT_NOT_HARDWARE_QUALIFICATION",
              "platform": platform.system(), "architecture": platform.machine(),
              "commands": {x: bool(shutil.which(x)) for x in ("python3", "git", "ssh", "nmcli", "make", "gcc", "iw", "tunnel-client")}}
    try:
        raw = args.manifest.read_bytes()
        manifest = json.loads(raw)
        expected = manifest["fit"]
        if (manifest.get("schema_version") != 1 or not isinstance(expected["bytes"], int)
                or isinstance(expected["bytes"], bool) or expected["bytes"] <= 0
                or not isinstance(expected["sha256"], str)
                or len(expected["sha256"]) != 64
                or any(c not in "0123456789abcdef" for c in expected["sha256"])
                or int(expected["fit_size_hex"], 16) != expected["bytes"]):
            raise ValueError("invalid manifest")
        result["manifest_sha256"] = hashlib.sha256(raw).hexdigest()
        result["default_route_media"] = []
        if shutil.which("ip"):
            try:
                routes = subprocess.run(["ip", "-j", "route", "show", "default"],
                                        capture_output=True, text=True, timeout=5, check=True)
                for route in json.loads(routes.stdout):
                    dev = route.get("dev", "")
                    wireless = (pathlib.Path("/sys/class/net") / dev / "wireless").exists()
                    result["default_route_media"].append("wifi" if wireless else "other")
                result["route_probe"] = "OBSERVED"
            except (OSError, ValueError, subprocess.SubprocessError):
                result["route_probe"] = "UNAVAILABLE_NOT_A_NETWORK_PASS"
        else:
            result["route_probe"] = "UNAVAILABLE_NOT_A_NETWORK_PASS"
        if args.fit:
            result["fit"] = verify_fit(args.fit, manifest)
            if not result["fit"]["match"]:
                result["status"] = "FAILED_FIT_MISMATCH"
                print(json.dumps(result, indent=2))
                return 2
        else:
            result["fit"] = {"status": "NOT_CHECKED"}
        result["status"] = "PASS_LOCAL_CHECKS" if all(result["commands"][x] for x in ("python3", "git", "ssh")) else "BLOCKED_MISSING_BASE_COMMAND"
        print(json.dumps(result, indent=2))
        return 0 if result["status"] == "PASS_LOCAL_CHECKS" else 3
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as exc:
        # Do not print exception contents, which can include local paths or upstream data.
        result["status"] = "FAILED_PREFLIGHT"
        result["error_type"] = type(exc).__name__
        print(json.dumps(result, indent=2))
        return 2

if __name__ == "__main__":
    sys.exit(main())
