"""Validate empty PT_TLS handling through ELF conversion and PE execution."""

import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture


def main():
    relinker = Path(sys.argv[1]).resolve()
    cases = [
        ("empty", 0, 0, False, None),
        ("nonempty", 8, 16, False, None),
        ("invalid-sizes", 1, 0, False, "Invalid or unsupported ELF TLS layout"),
        ("empty-access", 0, 0, True, "Guest TLS access with empty PT_TLS"),
    ]
    with tempfile.TemporaryDirectory(prefix="anyps5-tls-") as directory:
        for name, file_size, memory_size, access, error in cases:
            source = Path(directory) / (name + ".elf")
            output = source.with_suffix(".exe")
            data = fixture()
            struct.pack_into("<H", data, 0x38, 3)
            struct.pack_into("<IIQQQQQQ", data, 176,
                             7, 4, 0x800, 0x800, 0x800, file_size, memory_size, 1)
            if access:
                data[0x200:0x20a] = bytes.fromhex("64 48 8b 04 25 00 00 00 00 c3")
            source.write_bytes(data)
            result = subprocess.run([str(relinker), "--windows", str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            if error:
                assert result.returncode == 2 and error in result.stderr and not output.exists(), result
                continue
            assert result.returncode == 0, (result.stdout, result.stderr)
            pe = output.read_bytes()
            pe_offset = struct.unpack_from("<I", pe, 0x3c)[0]
            tls_rva, tls_size = struct.unpack_from("<II", pe, pe_offset + 24 + 112 + 9 * 8)
            assert bool(tls_rva) == bool(memory_size), (name, tls_rva)
            assert bool(tls_size) == bool(memory_size), (name, tls_size)
            if os.name == "nt":
                executed = subprocess.run([str(output)], capture_output=True, timeout=20)
                assert executed.returncode == 42, (name, executed.returncode)
    print("Empty TLS integration tests passed")


if __name__ == "__main__":
    main()
