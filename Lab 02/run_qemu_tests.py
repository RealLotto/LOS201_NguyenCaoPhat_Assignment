#!/usr/bin/env python3
"""Boot the LAB-02 initramfs and capture reproducible console tests."""

from __future__ import annotations

import os
import re
import select
import signal
import subprocess
import sys
import time
from pathlib import Path


HOME = Path.home()
LAB1 = HOME / "embedded_lab1"
LAB2 = HOME / "embedded_lab2"
PROJECT = Path(__file__).resolve().parent
LOG_DIR = LAB2 / "logs"
PROMPT = b"\r\n/ # "
ANSI_ESCAPE = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")


def clean(data: bytes) -> str:
    return ANSI_ESCAPE.sub(b"", data).replace(b"\r", b"").decode(
        "utf-8", errors="replace"
    )


def main() -> int:
    required = [
        LAB1 / "output/zImage",
        LAB1 / "output/vexpress-v2p-ca9.dtb",
        LAB2 / "output/initramfs_lab2.cpio.gz",
        PROJECT / "driver/lab2_driver.ko",
    ]
    for path in required:
        if not path.is_file():
            print(f"Required LAB-01/LAB-02 artifact is missing: {path}", file=sys.stderr)
            return 2
    LOG_DIR.mkdir(parents=True, exist_ok=True)

    qemu = [
        "qemu-system-arm",
        "-M", "vexpress-a9",
        "-cpu", "cortex-a9",
        "-m", "512M",
        "-smp", "2",
        "-nographic",
        "-kernel", str(LAB1 / "output/zImage"),
        "-dtb", str(LAB1 / "output/vexpress-v2p-ca9.dtb"),
        "-initrd", str(LAB2 / "output/initramfs_lab2.cpio.gz"),
        "-append", "console=ttyAMA0,115200 rdinit=/sbin/init mem=512M",
    ]
    master, slave = os.openpty()
    process = subprocess.Popen(
        qemu,
        stdin=slave,
        stdout=slave,
        stderr=slave,
        close_fds=True,
        start_new_session=True,
    )
    os.close(slave)
    transcript = bytearray()

    def wait_for(pattern: bytes, timeout: float, start: int = 0) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline and process.poll() is None:
            if pattern in transcript[start:]:
                return True
            ready, _, _ = select.select([master], [], [], 0.2)
            if ready:
                try:
                    chunk = os.read(master, 65536)
                except OSError:
                    return False
                if not chunk:
                    return False
                transcript.extend(chunk)
        return pattern in transcript[start:]

    def run_command(command: str, timeout: float = 25.0) -> bytes:
        start = len(transcript)
        os.write(master, command.encode() + b"\n")
        if not wait_for(PROMPT, timeout, start):
            raise RuntimeError(f"Timed out waiting for the shell after: {command}")
        return bytes(transcript[start:])

    try:
        if not wait_for(b"Please press Enter to activate this console", 45):
            raise RuntimeError("QEMU did not reach the BusyBox console.")
        boot = bytes(transcript)
        os.write(master, b"\n")
        if not wait_for(PROMPT, 10):
            raise RuntimeError("BusyBox init did not present a shell prompt.")
        (LOG_DIR / "boot_log.txt").write_text(clean(bytes(transcript)))

        if b"[ LAB-02 ] lab2_driver loaded OK" not in boot:
            raise RuntimeError("The LAB-02 driver did not auto-load during rcS.")
        if b"[ LAB-02 ] /dev/lab2 ready" not in boot:
            raise RuntimeError("rcS did not create /dev/lab2.")

        driver_result = run_command("test_driver.sh")
        (LOG_DIR / "driver_test.txt").write_text(clean(driver_result))
        if b"=== Test PASSED ===" not in driver_result:
            raise RuntimeError("The character-device test did not pass.")

        proc_result = run_command("test_procfs.sh")
        (LOG_DIR / "procfs_test.txt").write_text(clean(proc_result))
        if b"=== procfs/sysfs Test PASSED ===" not in proc_result:
            raise RuntimeError("The procfs/sysfs test did not pass.")

        direct_result = run_command(
            "echo '=== Direct /proc and sysfs verification ==='; "
            "cat /proc/lab2_info; "
            "ls -l /sys/class/lab2_class/lab2; "
            "cat /sys/class/lab2_class/lab2/buffer_len; "
            "cat /sys/class/lab2_class/lab2/open_count; "
            "cat /sys/class/lab2_class/lab2/reverse_read; "
            "cat /sys/class/lab2_class/lab2/overwrite_count; "
            "cat /sys/class/lab2_class/lab2/last_data; "
            "dmesg | grep 'lab2_driver:'"
        )
        (LOG_DIR / "proc_sysfs_direct.txt").write_text(clean(direct_result))
        for expected in (
            b"Major number  : 240",
            b"buffer_len",
            b"open_count",
            b"reverse_read",
            b"overwrite_count",
            b"Overwrites    :",
            b"last_data",
            b"lab2_driver: loaded, major=240",
        ):
            if expected not in direct_result:
                raise RuntimeError(
                    f"Direct proc/sysfs verification missed {expected.decode()}."
                )

        failure_result = run_command(
            "rmmod lab2_driver && "
            "mv /lib/modules/5.15.0/lab2_driver.ko "
            "/tmp/lab2_driver.ko.testbak && "
            "/etc/init.d/rcS"
        )
        (LOG_DIR / "module_failure_test.txt").write_text(clean(failure_result))
        if (
            b"lab2_driver.ko was not found; continuing boot without the driver"
            not in failure_result
            or b"System initialization complete!" not in failure_result
        ):
            raise RuntimeError("The optional module-failure rcS path did not continue.")
        restored_result = run_command(
            "mv /tmp/lab2_driver.ko.testbak "
            "/lib/modules/5.15.0/lab2_driver.ko && "
            "insmod /lib/modules/5.15.0/lab2_driver.ko && "
            "grep '^lab2_driver ' /proc/modules"
        )
        if b"lab2_driver " not in restored_result:
            raise RuntimeError("Could not restore the driver after the rcS failure test.")

        lifecycle_result = run_command("test_module_lifecycle.sh")
        (LOG_DIR / "module_lifecycle.txt").write_text(clean(lifecycle_result))
        if b"=== Lifecycle Test PASSED ===" not in lifecycle_result:
            raise RuntimeError("The unload/reload lifecycle test did not pass.")

        final_result = run_command(
            "echo '=== Final module state ==='; "
            "grep '^lab2_driver ' /proc/modules; "
            "ls -l /dev/lab2; "
            "cat /proc/lab2_info"
        )
        (LOG_DIR / "module_final_state.txt").write_text(clean(final_result))

        required_start = len(transcript)
        required_result = run_command(
            "echo LAB2_MODULE_REQUIRED=1 > /etc/lab2.conf && "
            "rmmod lab2_driver && "
            "mv /lib/modules/5.15.0/lab2_driver.ko "
            "/tmp/lab2_driver.ko.testbak && "
            "/etc/init.d/rcS"
        )
        (LOG_DIR / "module_required_test.txt").write_text(clean(required_result))
        if (
            b"[ LAB-02 ] FATAL: lab2_driver.ko was not found" not in required_result
            or b"Boot is paused in a rescue shell" not in required_result
            or b"System initialization complete!" in required_result
        ):
            raise RuntimeError("The required-module rcS path did not pause in rescue.")

        rescue_exit_start = len(transcript)
        os.write(master, b"\x04")
        if not wait_for(
            b"Rescue shell exited; init remains paused",
            10,
            rescue_exit_start,
        ) or not wait_for(PROMPT, 10, rescue_exit_start):
            raise RuntimeError("The required-module rescue loop did not remain paused.")
        required_log = clean(bytes(transcript[required_start:]))
        (LOG_DIR / "module_required_test.txt").write_text(required_log)
        if "System initialization complete!" in required_log:
            raise RuntimeError("Required-module failure incorrectly completed boot.")

        print("PASS: LAB-01 kernel + DTB booted the LAB-02 initramfs.")
        print("PASS: rcS auto-loaded lab2_driver and created /dev/lab2.")
        print(
            "PASS: character-device read/write/seek, reverse-read, overwrite "
            "counter, procfs, sysfs, unload/reload."
        )
        print("PASS: rcS warns and completes when the optional driver file is missing.")
        print("PASS: rcS pauses in rescue when the required driver is missing.")
        print(f"Evidence saved under {LOG_DIR}")
        return 0
    except Exception as error:
        (LOG_DIR / "boot_log.txt").write_text(clean(bytes(transcript)))
        print(f"FAIL: {error}", file=sys.stderr)
        print(f"Partial QEMU transcript saved to {LOG_DIR / 'boot_log.txt'}")
        return 1
    finally:
        if process.poll() is None:
            try:
                os.write(master, b"\x01x")
                process.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired):
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
        os.close(master)


if __name__ == "__main__":
    raise SystemExit(main())
