"""Small signal-processing helpers shared by controller and metrics code."""

from __future__ import annotations

import numpy as np
from numpy.typing import ArrayLike, NDArray


def moving_average(values: ArrayLike, depth: int) -> NDArray[np.float64]:
    """Centred moving average with edge padding.

    Args:
        values: One-dimensional input signal.
        depth: Window length in samples. ``1`` returns an unmodified copy.

    Returns:
        A new array of the same length as ``values``.
    """
    array = np.asarray(values, dtype=float)
    if array.ndim != 1:
        raise ValueError("moving_average expects a 1-D signal")
    if depth <= 1 or array.size == 0:
        return array.copy()

    depth = min(depth, array.size)
    pad_left = depth // 2
    pad_right = depth - 1 - pad_left
    padded = np.pad(array, (pad_left, pad_right), mode="edge")
    kernel = np.ones(depth) / depth
    return np.convolve(padded, kernel, mode="valid")
