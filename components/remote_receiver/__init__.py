"""RFLink RX gate diagnostic, ESP8266 only, based on ESPHome 2026.9.0.

Python: MIT. Copyright (c) 2019 ESPHome. Modified 2026-09-23.
The new capture_enabled option acts before setup, not just after Wi-Fi starts.
This external override intentionally rejects ESP32 and other targets.
"""
from typing import Any
from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import remote_base
from esphome.const import (
    CONF_BUFFER_SIZE, CONF_DUMP, CONF_FILTER, CONF_ID, CONF_IDLE,
    CONF_PIN, CONF_TOLERANCE, CONF_TYPE, CONF_VALUE,
)
from esphome.core import TimePeriod

AUTO_LOAD = ["remote_base"]
MULTI_CONF = True
CONF_CAPTURE_ENABLED = "capture_enabled"
CONF_HIGH_FREQUENCY = "high_frequency"
remote_receiver_ns = cg.esphome_ns.namespace("remote_receiver")
ToleranceMode = cg.esphome_ns.namespace("remote_base").enum("ToleranceMode")
TOLERANCE_MODE = {
    "percentage": ToleranceMode.TOLERANCE_MODE_PERCENTAGE,
    "time": ToleranceMode.TOLERANCE_MODE_TIME,
}
TOLERANCE_SCHEMA = cv.typed_schema(
    {
        "percentage": cv.Schema({cv.Required(CONF_VALUE): cv.All(cv.percentage_int, cv.uint32_t)}),
        "time": cv.Schema({cv.Required(CONF_VALUE): cv.All(
            cv.positive_time_period_microseconds,
            cv.Range(max=TimePeriod(microseconds=4294967295)),
        )}),
    }, lower=True, enum=TOLERANCE_MODE,
)
RemoteReceiverComponent = remote_receiver_ns.class_(
    "RemoteReceiverComponent", remote_base.RemoteReceiverBase, cg.Component
)

def validate_tolerance(value: Any):
    if isinstance(value, dict):
        return TOLERANCE_SCHEMA(value)
    if "%" in str(value):
        type_ = "percentage"
    else:
        try:
            cv.positive_time_period_microseconds(value)
        except cv.Invalid as exc:
            raise cv.Invalid("Tolerance must be a percentage or a time") from exc
        type_ = "time"
    return TOLERANCE_SCHEMA({CONF_VALUE: value, CONF_TYPE: type_})

CONFIG_SCHEMA = cv.All(
    remote_base.validate_triggers(
        cv.Schema({
            cv.GenerateID(): cv.declare_id(RemoteReceiverComponent),
            cv.Required(CONF_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_DUMP, default=[]): remote_base.validate_dumpers,
            cv.Optional(CONF_TOLERANCE, default="25%"): validate_tolerance,
            cv.Optional(CONF_BUFFER_SIZE, default="1000b"): cv.All(cv.validate_bytes, cv.int_range(min=3)),
            cv.Optional(CONF_FILTER, default="50us"): cv.All(
                cv.positive_time_period_microseconds, cv.Range(max=TimePeriod(microseconds=4294967295))),
            cv.Optional(CONF_IDLE, default="10ms"): cv.All(
                cv.positive_time_period_microseconds, cv.Range(max=TimePeriod(microseconds=4294967295))),
            cv.Optional(CONF_CAPTURE_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_HIGH_FREQUENCY, default=True): cv.boolean,
        }).extend(cv.COMPONENT_SCHEMA)
    ), cv.only_on_esp8266, cv.only_with_arduino,
)

async def to_code(config):
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], pin)
    for dumper in await remote_base.build_dumpers(config[CONF_DUMP]):
        cg.add(var.register_dumper(dumper))
    for trigger in await remote_base.build_triggers(config):
        cg.add(var.register_listener(trigger))
    await cg.register_component(var, config)
    cg.add(var.set_tolerance(config[CONF_TOLERANCE][CONF_VALUE], config[CONF_TOLERANCE][CONF_TYPE]))
    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))
    cg.add(var.set_filter_us(config[CONF_FILTER]))
    cg.add(var.set_idle_us(config[CONF_IDLE]))
    cg.add(var.set_high_frequency(config[CONF_HIGH_FREQUENCY]))
    cg.add(var.set_capture_enabled(config[CONF_CAPTURE_ENABLED]))
