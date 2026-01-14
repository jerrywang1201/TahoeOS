#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse
import sys
import time

try:
    import serial as _serial
except Exception as exc:
    _serial = None

TESTS = [
    "LCD",
    "BACKLIGHT",
    "TOUCH",
    "KEY",
    "IMU",
    "STEP",
    "ENV",
    "COMPASS",
    "BARO",
    "HR",
    "RTC",
    "BAT",
    "BLE",
    "BUZZER",
    "VIB",
]

TERMCOLOR = {
    "OK": "\033[92m",
    "WARN": "\033[93m",
    "FAIL": "\033[91m",
    "INFO": "\033[94m",
    "END": "\033[0m",
    "BOLD": "\033[1m",
}


def print_msg(msg, level="INFO"):
    color = TERMCOLOR.get(level, "")
    end = TERMCOLOR["END"]
    print(f"{color}{msg}{end}")


def open_serial(port, baud, timeout):
    if _serial is None or not hasattr(_serial, "Serial"):
        module_path = getattr(_serial, "__file__", "unknown") if _serial else "not found"
        print_msg("Error: pyserial not available or 'serial' module conflict.", "FAIL")
        print_msg(f"Detected module: {module_path}", "INFO")
        print_msg("Install: python3 -m pip install pyserial", "INFO")
        sys.exit(2)
    try:
        return _serial.Serial(port=port, baudrate=baud, timeout=timeout)
    except _serial.SerialException as exc:
        print_msg(f"Error: Cannot open serial port {port}: {exc}", "FAIL")
        sys.exit(2)


def send_cmd(ser, cmd):
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode("utf-8"))


def wait_for_result(ser, test_name, timeout):
    deadline = time.time() + timeout
    prefix = f"TEST:{test_name}:"
    while time.time() < deadline:
        line = ser.readline().decode("utf-8", errors="ignore").strip()
        if not line:
            continue
        print(line)
        if line.startswith(prefix):
            parts = line.split(":", 3)
            status = parts[2] if len(parts) > 2 else "UNKNOWN"
            detail = parts[3] if len(parts) > 3 else ""
            return status, detail
    return "TIMEOUT", ""


def run_single(ser, test_name, timeout):
    if test_name in ("TOUCH", "KEY"):
        print_msg(f"Action required: {test_name} within 5 seconds", "WARN")
    send_cmd(ser, f"OV+TEST={test_name}")
    status, detail = wait_for_result(ser, test_name, timeout)
    if status == "PASS":
        print_msg(f"{test_name}: PASS {detail}".strip(), "OK")
        return True
    if status == "UNSUPPORTED":
        print_msg(f"{test_name}: UNSUPPORTED", "WARN")
        return True
    if status == "UNKNOWN":
        print_msg(f"{test_name}: UNKNOWN", "WARN")
        return False
    if status == "TIMEOUT":
        print_msg(f"{test_name}: TIMEOUT", "FAIL")
        return False
    print_msg(f"{test_name}: FAIL {detail}".strip(), "FAIL")
    return False


def run_all(ser, timeout):
    results = {name: None for name in TESTS}
    send_cmd(ser, "OV+TEST=ALL")
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = ser.readline().decode("utf-8", errors="ignore").strip()
        if not line:
            continue
        print(line)
        if line.startswith("TEST:"):
            parts = line.split(":", 3)
            if len(parts) < 3:
                continue
            name = parts[1]
            status = parts[2]
            detail = parts[3] if len(parts) > 3 else ""
            if name in results and results[name] is None:
                results[name] = (status, detail)
        if all(v is not None for v in results.values()):
            break

    ok = True
    for name in TESTS:
        status, detail = results.get(name, ("TIMEOUT", ""))
        if status == "PASS":
            print_msg(f"{name}: PASS {detail}".strip(), "OK")
        elif status == "UNSUPPORTED":
            print_msg(f"{name}: UNSUPPORTED", "WARN")
        elif status == "TIMEOUT":
            print_msg(f"{name}: TIMEOUT", "FAIL")
            ok = False
        else:
            print_msg(f"{name}: {status} {detail}".strip(), "FAIL")
            ok = False
    return ok


def main():
    parser = argparse.ArgumentParser(description="OV Watch factory test tool")
    parser.add_argument("--port", required=True, help="UART port (e.g. /dev/tty.usbserial-XXXX)")
    parser.add_argument("--baud", type=int, default=115200, help="UART baud rate")
    parser.add_argument("--timeout", type=float, default=8.0, help="Per-test timeout (seconds)")
    parser.add_argument("--do", default="list", help="Test name, 'all', or 'list'")
    args = parser.parse_args()

    ser = open_serial(args.port, args.baud, timeout=0.2)

    if args.do.lower() in ("list", "help"):
        send_cmd(ser, "OV+TEST=LIST")
        time.sleep(0.2)
        while True:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if not line:
                break
            print(line)
        ser.close()
        return 0

    if args.do.lower() == "all":
        ok = run_all(ser, timeout=60.0)
        ser.close()
        return 0 if ok else 1

    test_name = args.do.strip().upper()
    if test_name not in TESTS:
        print_msg(f"Unknown test: {test_name}", "FAIL")
        print_msg(f"Valid: {', '.join(TESTS)}", "INFO")
        ser.close()
        return 2

    ok = run_single(ser, test_name, args.timeout)
    ser.close()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
