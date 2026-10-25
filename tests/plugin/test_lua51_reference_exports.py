"""Check the shipped Windows signature reference without loading its DLL."""

import struct
import sys
from pathlib import Path


def exported_names(path: Path) -> set[str]:
    data = path.read_bytes()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("invalid PE reference")
    optional = pe + 24
    if struct.unpack_from("<H", data, pe + 4)[0] != 0x8664 or struct.unpack_from("<H", data, optional)[0] != 0x20B:
        raise ValueError("the signature reference must be Windows x64")
    first = optional + struct.unpack_from("<H", data, pe + 20)[0]
    sections = [struct.unpack_from("<IIII", data, first + i * 40 + 8)
                for i in range(struct.unpack_from("<H", data, pe + 6)[0])]

    def offset(rva: int) -> int:
        for _, start, size, raw in sections:
            if start <= rva < start + size:
                result = raw + rva - start
                if result < len(data):
                    return result
        raise ValueError(f"reference RVA outside file sections: {rva:#x}")

    directory = offset(struct.unpack_from("<I", data, optional + 112)[0])
    count = struct.unpack_from("<I", data, directory + 24)[0]
    names = offset(struct.unpack_from("<I", data, directory + 32)[0])
    result = set()
    for i in range(count):
        start = offset(struct.unpack_from("<I", data, names + i * 4)[0])
        end = data.index(b"\0", start)
        result.add(data[start:end].decode("ascii"))
    return result


if __name__ == "__main__":
    required = {"lua_getfuncchunkname", "lua_setfuncchunkname", "lua_isunsafedataallowed"}
    missing = required - exported_names(Path(sys.argv[1]))
    if missing:
        sys.exit("signature reference lacks required Klei APIs: " + ", ".join(sorted(missing)))
    print("Windows signature reference exports the Klei provenance and sandbox APIs.")
