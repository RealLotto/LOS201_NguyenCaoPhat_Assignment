#!/usr/bin/env python3
"""Boot the layered LAB-03 initramfs and verify LAB-02 plus LAB-03 behavior."""

from __future__ import annotations

import os
import re
import select
import subprocess
import sys
import time
from pathlib import Path


PROJECT = Path(__file__).resolve().parent
HOME = Path.home()
LAB1 = Path(os.environ.get("LAB1", HOME / "embedded_lab1"))
LAB3 = Path(os.environ.get("LAB3", PROJECT))
LOG_DIR = LAB3 / "logs"
GDB_HOST_PORT = os.environ.get("GDB_HOST_PORT", "1235")
PROMPT = b"\r\n/ # "
ANSI_ESCAPE = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")


def clean(data: bytes) -> str:
    return ANSI_ESCAPE.sub(b"", data).replace(b"\r", b"").decode(
        "utf-8", errors="replace"
    )


def main() -> int:
    kernel = LAB1 / "output/zImage"
    dtb = LAB1 / "output/vexpress-v2p-ca9.dtb"
    initramfs = LAB3 / "output/initramfs_lab3.cpio.gz"
    for path in (kernel, dtb, initramfs):
        if not path.is_file():
            print(f"Required LAB-01/LAB-03 artifact is missing: {path}", file=sys.stderr)
            return 2
    if not shutil_which("qemu-system-arm"):
        print("qemu-system-arm is required for this compatibility test.", file=sys.stderr)
        return 2

    LOG_DIR.mkdir(parents=True, exist_ok=True)
    command = [
        "qemu-system-arm",
        "-M", "vexpress-a9",
        "-cpu", "cortex-a9",
        "-m", "512M",
        "-smp", "2",
        "-nographic",
        "-kernel", str(kernel),
        "-dtb", str(dtb),
        "-initrd", str(initramfs),
        "-append", "console=ttyAMA0,115200 rdinit=/sbin/init mem=512M",
        "-net", "nic,model=lan9118",
        "-net", "user,hostfwd=tcp::" + GDB_HOST_PORT + "-:1234",
    ]
    master, slave = os.openpty()
    process = subprocess.Popen(
        command,
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

    def run_command(command_line: str, timeout: float = 45.0) -> bytes:
        start = len(transcript)
        os.write(master, command_line.encode() + b"\n")
        if not wait_for(PROMPT, timeout, start):
            raise RuntimeError(f"Timed out waiting for shell after: {command_line}")
        return bytes(transcript[start:])

    def command_output(result: bytes) -> bytes:
        lines = clean(result).splitlines()
        if lines and lines[0].lstrip().endswith("#"):
            lines = lines[1:]
        elif lines:
            lines = lines[1:]
        return "\n".join(lines).encode()

    try:
        if not wait_for(b"Please press Enter to activate this console", 60):
            raise RuntimeError("QEMU did not reach the BusyBox console.")
        os.write(master, b"\n")
        if not wait_for(PROMPT, 15):
            raise RuntimeError("BusyBox init did not present a shell prompt.")
        (LOG_DIR / "qemu_lab3_boot.txt").write_text(clean(bytes(transcript)))

        checks = [
            (
                "lab2_driver_compatibility.txt",
                "test_driver.sh",
                b"=== Test PASSED ===",
            ),
            (
                "lab2_procfs_compatibility.txt",
                "test_procfs.sh",
                b"=== procfs/sysfs Test PASSED ===",
            ),
            (
                "lab3_leaky_app_arm.txt",
                "leaky_app_arm --fixed",
                b"Leak mode: fixed",
            ),
            (
                "lab3_memory_status.txt",
                "leaky_app_arm --leak --hold & pid=$!; sleep 1; "
                "cat /proc/$pid/status; "
                "wait $pid; leaky_app_arm --fixed --hold & pid=$!; "
                "sleep 1; cat /proc/$pid/status; wait $pid",
                b"VmRSS:",
            ),
            (
                "lab3_perf_target_arm.txt",
                "perf_target_arm",
                b"Done",
            ),
            (
                "lab3_inventory.txt",
                "ls -l /usr/bin/buggy_app /usr/bin/leaky_app_arm "
                "/usr/bin/perf_target_arm /lib/modules/5.15.0/lab2_driver.ko",
                b"buggy_app",
            ),
            (
                "lab3_gdbserver.txt",
                "gdbserver --version",
                b"GNU gdbserver",
            ),
            (
                "lab3_gdbserver_listening.txt",
                "ifconfig eth0 10.0.2.15 netmask 255.255.255.0 up; "
                "gdbserver 0.0.0.0:1234 /usr/bin/buggy_app --case oob "
                ">/tmp/lab3-gdbserver.log 2>&1 & "
                "echo $! >/tmp/lab3-gdbserver.pid; sleep 1; "
                "cat /tmp/lab3-gdbserver.log",
                b"Listening on port 1234",
            ),
        ]
        for log_name, command_line, expected in checks:
            result = run_command(command_line, timeout=90)
            (LOG_DIR / log_name).write_text(clean(result))
            if expected not in command_output(result):
                raise RuntimeError(
                    f"{command_line!r} did not produce expected output "
                    f"{expected.decode(errors='replace')!r}."
                )
            if log_name == "lab3_memory_status.txt":
                output = command_output(result).decode(errors="replace")
                if output.count("VmRSS:") < 2:
                    raise RuntimeError("Could not read VmRSS for both ARM app modes.")

        gdb = shutil_which("gdb-multiarch")
        if gdb:
            gdb_result = subprocess.run(
                [
                    gdb,
                    "-q",
                    str(LAB3 / "apps/buggy_app"),
                    "-batch",
                    "-ex",
                    "set architecture arm",
                    "-ex",
                    f"target remote localhost:{GDB_HOST_PORT}",
                    "-ex",
                    "break apps/buggy_app.c:10 if i == len",
                    "-ex",
                    "continue",
                    "-ex",
                    "print i",
                    "-ex",
                    "backtrace",
                    "-ex",
                    "info registers pc sp lr",
                    "-ex",
                    "detach",
                ],
                check=False,
                capture_output=True,
                text=True,
                timeout=45,
            )
            gdb_log = gdb_result.stdout + gdb_result.stderr
            (LOG_DIR / "gdb_system_mode_session.txt").write_text(gdb_log)
            if (
                gdb_result.returncode != 0
                or "process_data" not in gdb_log
                or "$1 = 5" not in gdb_log
            ):
                raise RuntimeError(
                    "Host gdb-multiarch did not complete the system-mode "
                    "remote debugging check; see gdb_system_mode_session.txt."
                )
            (LOG_DIR / "gdb_system_mode_status.txt").write_text(
                "PASS: host gdb-multiarch connected through QEMU port "
                f"{GDB_HOST_PORT} to guest gdbserver :1234 and stopped at i=5.\n"
            )
        else:
            (LOG_DIR / "gdb_system_mode_status.txt").write_text(
                "NOT RUN: host gdb-multiarch is unavailable; guest gdbserver "
                "started and listened successfully. Use the README GDB steps.\n"
            )
        cleanup = run_command(
            "kill \"$(cat /tmp/lab3-gdbserver.pid)\" 2>/dev/null || true; "
            "wait \"$(cat /tmp/lab3-gdbserver.pid)\" 2>/dev/null || true"
        )
        (LOG_DIR / "gdb_system_mode_cleanup.txt").write_text(clean(cleanup))

        gdb_status = (LOG_DIR / "gdb_system_mode_status.txt").read_text().strip()
        (LOG_DIR / "qemu_lab3_compatibility.txt").write_text(
            "PASS: LAB-01 kernel/DTB booted the LAB-03 initramfs.\n"
            "PASS: LAB-02 character-device test passed.\n"
            "PASS: LAB-02 procfs/sysfs test passed.\n"
            "PASS: LAB-03 ARM fixed-memory and profiling applications ran.\n"
            "PASS: ARM gdbserver started and listened on guest TCP port 1234.\n"
            f"{gdb_status}\n"
        )
        print((LOG_DIR / "qemu_lab3_compatibility.txt").read_text(), end="")
        (LOG_DIR / "qemu_lab3_failure.txt").unlink(missing_ok=True)
        return 0
    except (OSError, RuntimeError) as error:
        (LOG_DIR / "qemu_lab3_failure.txt").write_text(
            f"{error}\n\n--- QEMU transcript ---\n{clean(bytes(transcript))}"
        )
        print(f"QEMU compatibility test failed: {error}", file=sys.stderr)
        return 1
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        os.close(master)


def shutil_which(command: str) -> str | None:
    """Keep the test runner dependency-free while checking PATH."""
    for directory in os.environ.get("PATH", "").split(os.pathsep):
        candidate = Path(directory) / command
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return str(candidate)
    return None


if __name__ == "__main__":
    raise SystemExit(main())
