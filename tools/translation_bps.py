"""Apply BPS1 patches with source, target and patch checksum validation."""
import struct
import zlib


def apply(source, patch):
    if len(patch) < 19 or patch[:4] != b"BPS1":
        raise ValueError("Invalid BPS header")
    source_crc, target_crc, patch_crc = struct.unpack("<III", patch[-12:])
    if zlib.crc32(patch[:-4]) != patch_crc:
        raise ValueError("Patch CRC mismatch")
    if zlib.crc32(source) != source_crc:
        raise ValueError("Source CRC mismatch")
    position = 4
    payload_end = len(patch) - 12

    def read_number():
        nonlocal position
        value = 0
        shift = 1
        for _ in range(10):
            if position >= payload_end:
                raise ValueError("Truncated BPS integer")
            byte = patch[position]
            position += 1
            value += (byte & 127) * shift
            if byte & 128:
                return value
            shift <<= 7
            value += shift
        raise ValueError("Invalid BPS integer")

    source_size = read_number()
    target_size = read_number()
    metadata_size = read_number()
    position += metadata_size
    if source_size != len(source) or target_size > 32 * 1024 * 1024:
        raise ValueError("Invalid BPS image size")
    if position > payload_end:
        raise ValueError("Truncated BPS metadata")
    target = bytearray()
    source_relative = 0
    target_relative = 0
    while position < payload_end:
        command = read_number()
        operation = command & 3
        count = (command >> 2) + 1
        if len(target) + count > target_size:
            raise ValueError("BPS write exceeds target size")
        if operation == 0:
            start = len(target)
            if start + count > len(source):
                raise ValueError("BPS source read exceeds source size")
            target.extend(source[start:start + count])
        elif operation == 1:
            if position + count > payload_end:
                raise ValueError("Truncated BPS literal")
            target.extend(patch[position:position + count])
            position += count
        else:
            distance = read_number()
            delta = distance >> 1
            if distance & 1:
                delta = -delta
            if operation == 2:
                source_relative += delta
                if source_relative < 0 or source_relative + count > len(source):
                    raise ValueError("BPS source copy exceeds source size")
                target.extend(source[source_relative:source_relative + count])
                source_relative += count
            else:
                target_relative += delta
                if not 0 <= target_relative < len(target):
                    raise ValueError("Invalid BPS target copy")
                for _ in range(count):
                    target.append(target[target_relative])
                    target_relative += 1
    if position != payload_end or len(target) != target_size:
        raise ValueError("Incomplete BPS target")
    if zlib.crc32(target) != target_crc:
        raise ValueError("Target CRC mismatch")
    return bytes(target)
