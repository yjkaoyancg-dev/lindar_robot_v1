from __future__ import annotations

import math
import struct
from datetime import datetime

from .models import PointCloudFrame


FLOAT32 = 7


def decode_pointcloud2(msg, topic: str, max_points: int = 6000) -> PointCloudFrame:
    """Decode a lightweight sample from sensor_msgs/PointCloud2.

    This intentionally samples instead of decoding every point so PR4 remains
    responsive on IPC hardware.
    """

    fields = {field.name: field for field in msg.fields}
    if not all(name in fields for name in ("x", "y", "z")):
        return PointCloudFrame(
            topic=topic,
            frame_id=getattr(msg.header, "frame_id", ""),
            point_count=0,
            updated_at=datetime.now(),
            note="missing x/y/z fields",
        )

    if fields["x"].datatype != FLOAT32 or fields["y"].datatype != FLOAT32 or fields["z"].datatype != FLOAT32:
        return PointCloudFrame(
            topic=topic,
            frame_id=getattr(msg.header, "frame_id", ""),
            point_count=0,
            updated_at=datetime.now(),
            note="only float32 x/y/z point clouds are decoded in PR4",
        )

    width = int(getattr(msg, "width", 0) or 0)
    height = int(getattr(msg, "height", 0) or 0)
    point_step = int(getattr(msg, "point_step", 0) or 0)
    if point_step <= 0:
        return PointCloudFrame(topic=topic, frame_id=getattr(msg.header, "frame_id", ""), updated_at=datetime.now(), note="invalid point_step")

    total = width * max(1, height)
    step = max(1, math.ceil(total / max_points)) if total > max_points else 1
    data = bytes(msg.data)
    x_offset = fields["x"].offset
    y_offset = fields["y"].offset
    z_offset = fields["z"].offset

    points: list[tuple[float, float, float]] = []
    endian = ">" if getattr(msg, "is_bigendian", False) else "<"
    fmt = endian + "f"
    for index in range(0, total, step):
        base = index * point_step
        if base + point_step > len(data):
            break
        try:
            x = struct.unpack_from(fmt, data, base + x_offset)[0]
            y = struct.unpack_from(fmt, data, base + y_offset)[0]
            z = struct.unpack_from(fmt, data, base + z_offset)[0]
        except struct.error:
            break
        if math.isfinite(x) and math.isfinite(y) and math.isfinite(z):
            points.append((x, y, z))

    return PointCloudFrame(
        topic=topic,
        frame_id=getattr(msg.header, "frame_id", ""),
        point_count=total,
        sampled_points=points,
        updated_at=datetime.now(),
        note=f"sampled {len(points)} / {total}",
    )
