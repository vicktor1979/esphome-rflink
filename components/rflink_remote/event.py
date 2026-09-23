"""A configured RFLink remote/button is a native ESPHome event entity."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import event, binary_sensor
from esphome.const import CONF_ID
from . import ns, RFRemoteHub, CONF_REMOTE_ID, TIMING_SCHEMA, as_ms
from .validation import TIMING_ORDER, validate_pattern, event_mask

DEPENDENCIES = ["rflink_remote"]
RFRemoteEvent = ns.class_("RFRemoteEvent", event.Event)


def checked_pattern(config):
    try:
        return validate_pattern(config)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err


CONFIG_SCHEMA = cv.All(event.event_schema(RFRemoteEvent, device_class="button").extend({
    cv.GenerateID(CONF_REMOTE_ID): cv.use_id(RFRemoteHub),
    cv.Required("protocol"): cv.string_strict,
    cv.Required("rf_id"): cv.string_strict,
    cv.Required("button"): cv.string_strict,
    cv.Required("command"): cv.string_strict,
    cv.Optional("mode", default="auto"): cv.one_of("auto", "gestures", "message", lower=True),
    cv.Optional("event_types"): cv.ensure_list(cv.string_strict),
    cv.Optional("timing"): TIMING_SCHEMA,
    cv.Optional("message_cooldown", default="0ms"): cv.All(as_ms, cv.int_range(min=0, max=60000)),
    cv.Optional("log_events", default=True): cv.boolean,
    cv.Optional("pressed"): binary_sensor.binary_sensor_schema(icon="mdi:gesture-tap-hold"),
}), checked_pattern)

async def to_code(config):
    var = await event.new_event(config, event_types=config["event_types"])
    hub = await cg.get_variable(config[CONF_REMOTE_ID])
    cg.add(var.set_pattern(config["protocol"], config["rf_id"], config["button"], config["command"], config["mode"] == "gestures"))
    cg.add(var.set_event_mask(event_mask(config["event_types"])))
    cg.add(var.set_message_cooldown(config["message_cooldown"]))
    cg.add(var.set_log_events(config["log_events"]))
    if "timing" in config:
        cg.add(var.set_timing_values(*[config["timing"][k] for k in TIMING_ORDER]))
    if "pressed" in config:
        sensor = await binary_sensor.new_binary_sensor(config["pressed"])
        cg.add(var.set_pressed_sensor(sensor))
    cg.add(hub.add_remote(var))
