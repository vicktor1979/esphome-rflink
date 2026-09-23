"""RFLink configurable event hub and bounded, opt-in learning diagnostics (v0.1.6)."""
from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import rflink, text_sensor
from esphome.const import CONF_ID
from .validation import TIMING_DEFAULTS, TIMING_ORDER, validate_timing

DEPENDENCIES = ["rflink"]
AUTO_LOAD = ["event", "binary_sensor", "text_sensor", "json"]
MULTI_CONF = False
CODEOWNERS = []

ns = cg.esphome_ns.namespace("rflink_remote")
RFRemoteHub = ns.class_("RFRemoteHub", cg.Component)
CONF_REMOTE_ID = "remote_id"


def as_ms(value):
    return int(cv.positive_time_period_milliseconds(value).total_milliseconds)


def checked_timing(value):
    try:
        return validate_timing(value)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err


TIMING_SCHEMA = cv.All(cv.Schema({
    cv.Optional(k, default=f"{v}ms"): as_ms for k, v in TIMING_DEFAULTS.items()
}), checked_timing)

LEARNING_SCHEMA = cv.Schema({
    cv.Optional("enabled", default=False): cv.boolean,
    cv.Optional("duration", default="60s"): cv.All(as_ms, cv.int_range(min=1000, max=300000)),
    cv.Optional("max_signals", default=4): cv.int_range(min=1, max=8),
    cv.Optional("min_frames", default=3): cv.int_range(min=1, max=20),
    cv.Optional("log_events", default=True): cv.boolean,
    cv.Optional("signal"): text_sensor.text_sensor_schema(icon="mdi:radio-tower", entity_category="diagnostic"),
    cv.Optional("gesture"): text_sensor.text_sensor_schema(icon="mdi:gesture-tap", entity_category="diagnostic"),
})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(RFRemoteHub),
    cv.GenerateID("rflink_id"): cv.use_id(rflink.RFLinkComponent),
    cv.Optional("timing", default={}): TIMING_SCHEMA,
    cv.Optional("learning"): LEARNING_SCHEMA,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    parent = await cg.get_variable(config["rflink_id"])
    cg.add(var.set_parent(parent))
    cg.add(var.set_timing_values(*[config["timing"][k] for k in TIMING_ORDER]))
    if "learning" in config:
        c = config["learning"]
        cg.add(var.configure_learning(c["enabled"], c["duration"], c["max_signals"], c["min_frames"], c["log_events"]))
        for key in ("signal", "gesture"):
            if key in c:
                sensor = await text_sensor.new_text_sensor(c[key])
                cg.add(getattr(var, f"set_learning_{key}_sensor")(sensor))
