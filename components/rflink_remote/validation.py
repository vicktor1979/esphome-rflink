"""Pure validation helpers; also exercised without ESPHome in host tests."""
import re

GESTURES = (
    "press", "release", "single", "double", "triple", "click_4", "click_5",
    "click_6", "click_7", "click_8", "click_9", "click_10", "hold",
    "hold_repeat", "hold_release", "cancel", "multi_overflow",
)
EVENTS = GESTURES + ("received",)
TIMING_DEFAULTS = {
    "release_timeout": 180, "hold_release_timeout": 450, "repeat_fresh_timeout": 180,
    "multi_click_timeout": 350, "hold_time": 700, "repeat_interval": 250,
    "max_press_time": 30000,
}
TIMING_ORDER = tuple(TIMING_DEFAULTS)

def validate_timing(values):
    t = {**TIMING_DEFAULTS, **values}
    r, hr, fresh, multi, hold, repeat, maximum = (t[k] for k in TIMING_ORDER)
    if not (20 <= r <= 2000 and r <= hr <= 2000 and 20 <= fresh <= hr
            and 50 <= multi <= 3000 and r < hold <= 10000
            and 100 <= repeat <= 5000 and hold < maximum <= 300000):
        raise ValueError("Invalid timing: release 20..2000; hold_release >= release; "
                         "fresh 20..hold_release; multi 50..3000; "
                         "hold > release and <=10000; repeat 100..5000; "
                         "max_press > hold and <=300000 (all milliseconds).")
    return t

def checked_text(value, field, maximum, allow_empty=False):
    if not isinstance(value, str):
        raise ValueError(f"{field} must be quoted text; retain leading zeros, e.g. '085372'.")
    if not allow_empty and not value:
        raise ValueError(f"{field} must not be empty.")
    if len(value.encode('utf-8')) > maximum or any(ord(c) < 32 or ord(c) == 127 for c in value):
        raise ValueError(f"{field} exceeds {maximum} UTF-8 bytes or contains a control character.")
    return value

def validate_pattern(config):
    c = dict(config)
    for key, n in (("protocol", 32), ("rf_id", 32), ("button", 16), ("command", 32)):
        c[key] = checked_text(c[key], key, n, key in ("button", "command"))
    mode = c.get("mode", "auto")
    if mode not in ("auto", "gestures", "message"):
        raise ValueError("mode must be auto, gestures, or message.")
    if mode == "auto":
        mode = "gestures" if c["protocol"] == "EV1527" else "message"
    c["mode"] = mode
    if mode == "gestures":
        if c["protocol"] != "EV1527":
            raise ValueError("Full repeat-frame gestures currently support EV1527 only; "
                             "use mode: message for other decoded protocols.")
        if not re.fullmatch(r"[0-9a-fA-F]{1,6}", c["rf_id"]) or int(c["rf_id"], 16) > 0xFFFFF:
            raise ValueError("EV1527 rf_id must be hexadecimal in range 000000..0fffff.")
        if not re.fullmatch(r"[0-9a-fA-F]{1,2}", c["button"]) or int(c["button"], 16) > 15:
            raise ValueError("EV1527 button must be hexadecimal 00..0f.")
        if c["command"] != "ON":
            raise ValueError("The existing EV1527 frame observer exposes command ON only.")
        c["rf_id"] = f'{int(c["rf_id"], 16):06x}'
        c["button"] = f'{int(c["button"], 16):02x}'
    allowed = GESTURES if mode == "gestures" else ("received",)
    types = c.get("event_types", list(allowed))
    if not isinstance(types, list) or not types or any(t not in allowed for t in types):
        raise ValueError(f"event_types for mode {mode} must be a nonempty selection from {allowed}.")
    if len(types) != len(set(types)):
        raise ValueError("event_types must not contain duplicates.")
    if mode == "message" and ("pressed" in c or "timing" in c):
        raise ValueError("pressed and gesture timing require mode: gestures.")
    c["event_types"] = list(types)
    return c

def event_mask(names):
    return sum(1 << EVENTS.index(name) for name in names)
