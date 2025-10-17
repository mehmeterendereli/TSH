import pytest

from memory_patcher.types import ValueType, decode_value, encode_value


def test_encode_decode_int32():
    encoded = encode_value(ValueType.INT32, "-42")
    assert encoded.raw == (-42).to_bytes(4, "little", signed=True)
    assert decode_value(ValueType.INT32, encoded.raw) == "-42"


def test_encode_decode_uint32():
    encoded = encode_value(ValueType.UINT32, "4294967295")
    assert encoded.raw == (2**32 - 1).to_bytes(4, "little")
    assert decode_value(ValueType.UINT32, encoded.raw) == "4294967295"


def test_encode_decode_float():
    encoded = encode_value(ValueType.FLOAT, "3.5")
    decoded = float(decode_value(ValueType.FLOAT, encoded.raw))
    assert decoded == pytest.approx(3.5)


def test_encode_decode_ascii_utf16_hex():
    ascii_encoded = encode_value(ValueType.ASCII, "TEST")
    assert decode_value(ValueType.ASCII, ascii_encoded.raw) == "TEST"

    utf16_encoded = encode_value(ValueType.UTF16, "\u00e7\u011f")
    assert decode_value(ValueType.UTF16, utf16_encoded.raw) == "\u00e7\u011f"

    hex_encoded = encode_value(ValueType.HEX, "0A FF")
    assert hex_encoded.raw == b"\x0a\xff"
    assert decode_value(ValueType.HEX, hex_encoded.raw) == "0aff"


def test_encode_invalid_range():
    with pytest.raises(ValueError):
        encode_value(ValueType.INT32, str(2**31))
    with pytest.raises(ValueError):
        encode_value(ValueType.UINT32, "-1")
    with pytest.raises(ValueError):
        encode_value(ValueType.FLOAT, "nan")


def test_encode_invalid_hex():
    with pytest.raises(ValueError):
        encode_value(ValueType.HEX, "zz")
