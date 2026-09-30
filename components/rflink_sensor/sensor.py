"""Select one decoded RFLink numeric field by protocol and exact RF ID."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import rflink, sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["rflink"]
AUTO_LOAD = ["json"]
ns = cg.esphome_ns.namespace("rflink_sensor")
RFLinkSensor = ns.class_("RFLinkSensor", sensor.Sensor, cg.Component)
fields_ns = cg.esphome_ns.namespace("rflink_data")
FIELDS = "SET_LEVEL TEMP HUM BARO HSTATUS BFORECAST UV LUX RAIN RAINRATE WINSP AWINSP WINGS WINDIR WINCHL WINTMP CHIME CO2 SOUND KWATT WATT CURRENT DIST METER VOLT CHAN WINDIR_DEG".split()


def match_text(value):
    value = cv.string_strict(value)
    if not value or len(value) > 32 or value != value.strip() or any(ord(c) < 32 or ord(c) == 127 for c in value):
        raise cv.Invalid("Use the complete, nonempty RFLink name/ID (max. 32 characters, no surrounding whitespace).")
    return value


def defaults(config):
    metadata = {
        "TEMP": ("°C", "temperature", 1),
        "WINCHL": ("°C", "temperature", 1),
        "WINTMP": ("°C", "temperature", 1),
        "HUM": ("%", "humidity", 0),
        "BARO": ("hPa", "atmospheric_pressure", 0),
    }
    if config["field"] in metadata:
        unit, device_class, decimals = metadata[config["field"]]
        config.setdefault("unit_of_measurement", unit)
        config.setdefault("device_class", device_class)
        config.setdefault("accuracy_decimals", decimals)
        # This hook runs AFTER sensor_schema: defaults added here must already
        # be validated, otherwise codegen emits a C++ string instead of an enum.
        config.setdefault("state_class", sensor.validate_state_class("measurement"))
    return config


CONFIG_SCHEMA = cv.All(sensor.sensor_schema(RFLinkSensor).extend({
    cv.GenerateID("rflink_id"): cv.use_id(rflink.RFLinkComponent),
    cv.Required("protocol"): match_text,
    cv.Required("rf_id"): match_text,
    cv.Optional("field", default="TEMP"): cv.one_of(*FIELDS, upper=True),
}).extend(cv.COMPONENT_SCHEMA), defaults)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    parent = await cg.get_variable(config["rflink_id"])
    cg.add(var.set_parent(parent))
    cg.add(var.set_match(config["protocol"], config["rf_id"]))
    cg.add(var.set_field(getattr(fields_ns, config["field"])))
