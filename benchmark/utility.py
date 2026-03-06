"""Utility functions for benchmark."""


def mask_str(width: int) -> str:
    """Generate a hexadecimal bitmask string for CPU affinity.

    Args:
        width: Number of bits in the mask (must be > 0)

    Returns:
        A hexadecimal string representation of a bitmask with all bits set,
        formatted as '0x...'.

    Raises:
        ValueError: If width is less than or equal to 0
    """
    if width <= 0:
        msg = "Invalid width"
        raise ValueError(msg)
    hex_digits = (1 << width) - 1
    return f"0x{hex_digits:X}"
