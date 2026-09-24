"""RFLink RX compatibility bridge. GitHub external_component; Arduino only."""
from pathlib import Path
import logging

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import remote_base, switch, text_sensor
from esphome.const import CONF_ID, CONF_NAME, CONF_TRIGGER_ID
from esphome.core import CORE

from .stage_sources import select_plugins, stage

DEPENDENCIES = ["remote_receiver"]
AUTO_LOAD = ["remote_base", "switch", "text_sensor"]
MULTI_CONF = False
CODEOWNERS = []

CONF_RX_PLUGINS = "rx_plugins"
CONF_PLUGIN_PROFILE = "plugin_profile"
CONF_ON_MESSAGE = "on_message"
CONF_LOG_MESSAGES = "log_messages"
CONF_PLUGIN_SWITCHES = "plugin_switches"
CONF_RESTORE = "restore"
CONF_PLUGINS = "plugins"
CONF_PLUGIN_ID = "plugin_id"
CONF_ACTIVE_PLUGINS = "active_plugins"
_LOGGER = logging.getLogger(__name__)
REPO = Path(__file__).resolve().parents[2]

PLUGIN_NAMES = {
    1: "Packet preprocessor", 2: "LaCrosse v2 2300/3600", 3: "Kaku / ARC",
    4: "NewKAKU / Intertechno", 5: "Eurodomest", 6: "Blyss", 7: "Conrad RSL2",
    8: "Kambrook", 9: "X10 RF", 10: "TRC02 RGB", 11: "Home Confort",
    12: "Flamingo FA500R", 13: "Powerfix / Quigg", 14: "Ikea Koppla", 15: "Home Easy EU",
    16: "Silvercrest socket remote", 18: "Louvolite R1492-6CH-WH",
    29: "Alecto V2", 30: "Alecto V1", 31: "Alecto V3", 32: "Alecto V4",
    33: "Conrad Pool Thermometer", 34: "Cresta", 35: "Imagintronix", 36: "F007_TH",
    37: "AcuRite 986", 40: "Mebus", 41: "LaCrosse v3 WS7000", 42: "UPM / Esic",
    43: "LaCrosse v1", 44: "Auriol v3", 45: "Auriol", 46: "Auriol v2 / Xiron",
    47: "Auriol v4", 48: "Oregon V2/V3", 49: "LaCrosse TX141/TX145",
    50: "Fine Offset WH2", 60: "Ajax / Chubb / Varel", 61: "EV1527 / Chinese sensors",
    62: "Chuango", 63: "Oregon PIR / Alarm / Light", 64: "Atlantic / Visonic",
    70: "Select Plus / Quhwa", 71: "Plieger York", 72: "Byron SX", 73: "Deltronic",
    74: "RL02", 75: "Silvercrest doorbell", 76: "CAME TOP-432", 77: "Avantek",
    80: "Flamingo FA20 / KD101", 81: "Mertik Maxitrol / Dru", 82: "Mertik Maxitrol / Dru",
    83: "Brel / Dooya", 254: "Unsupported packet debug",
}

ns = cg.esphome_ns.namespace("rflink")
RFLinkComponent = ns.class_("RFLinkComponent", cg.Component, remote_base.RemoteReceiverListener)
RFLinkPluginSwitch = ns.class_("RFLinkPluginSwitch", switch.Switch, cg.Component)
RFLinkMessageTrigger = ns.class_("RFLinkMessageTrigger", automation.Trigger.template(cg.std_string))


def validate_plugins(value):
    if isinstance(value, str):
        selection = cv.one_of("configured", "all", lower=True)(value)
    else:
        selection = cv.ensure_list(cv.int_range(min=1, max=255))(value)
    return selection


PLUGIN_SWITCH_SCHEMA = switch.switch_schema(
    RFLinkPluginSwitch,
    default_restore_mode="RESTORE_DEFAULT_ON",
    entity_category="config",
    icon="mdi:puzzle",
).extend({
    cv.Required(CONF_PLUGIN_ID): cv.int_range(min=1, max=255),
}).extend(cv.COMPONENT_SCHEMA)


def validate_plugin_switch(value):
    if isinstance(value, int) or (isinstance(value, str) and value.strip().isdigit()):
        value = {CONF_PLUGIN_ID: int(value)}
    elif not isinstance(value, dict):
        raise cv.Invalid("plugin_switches.plugins entries must be a plugin ID or mapping")
    value = dict(value)
    plugin_id = cv.int_range(min=1, max=255)(value[CONF_PLUGIN_ID])
    if plugin_id == 1:
        raise cv.Invalid("Plugin 001 is the RFLink packet preprocessor and must stay enabled; do not expose it as a runtime switch.")
    value[CONF_PLUGIN_ID] = plugin_id
    value.setdefault(CONF_NAME, f"RFLink {plugin_id:03d} · {PLUGIN_NAMES.get(plugin_id, 'Plugin')}")
    return PLUGIN_SWITCH_SCHEMA(value)


PLUGIN_SWITCHES_SCHEMA = cv.Schema({
    cv.Optional(CONF_RESTORE, default=True): cv.boolean,
    cv.Required(CONF_PLUGINS): cv.ensure_list(validate_plugin_switch),
    cv.Optional(
        CONF_ACTIVE_PLUGINS,
        default={CONF_NAME: "RFLink aktív pluginok"},
    ): text_sensor.text_sensor_schema(
        entity_category="diagnostic",
        icon="mdi:format-list-numbered",
    ),
})


def validate_plugin_config(config):
    try:
        compiled = select_plugins(REPO, config[CONF_RX_PLUGINS], config[CONF_PLUGIN_PROFILE])
    except (OSError, ValueError) as error:
        raise cv.Invalid(str(error)) from error
    switches = config.get(CONF_PLUGIN_SWITCHES)
    if switches:
        ids = [entry[CONF_PLUGIN_ID] for entry in switches[CONF_PLUGINS]]
        if len(ids) != len(set(ids)):
            raise cv.Invalid("plugin_switches.plugins contains duplicate plugin IDs")
        missing = sorted(set(ids) - set(compiled))
        if missing:
            raise cv.Invalid("Runtime switch requested for plugin(s) not compiled: " + ", ".join(f"{n:03d}" for n in missing))
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(RFLinkComponent),
        cv.Optional(CONF_RX_PLUGINS, default="configured"): validate_plugins,
        cv.Optional(CONF_PLUGIN_PROFILE, default="legacy"): cv.one_of("legacy", "extended", lower=True),
        cv.Optional(CONF_LOG_MESSAGES, default=True): cv.boolean,
        cv.Optional(CONF_PLUGIN_SWITCHES): PLUGIN_SWITCHES_SCHEMA,
        cv.Optional(CONF_ON_MESSAGE): automation.validate_automation({
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RFLinkMessageTrigger),
        }),
    }).extend(remote_base.REMOTE_LISTENER_SCHEMA).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
    validate_plugin_config,
)


async def to_code(config):
    ids = stage(REPO, Path(CORE.relative_src_path("rflink_vendor")), config[CONF_RX_PLUGINS], config[CONF_PLUGIN_PROFILE])
    _LOGGER.info("RFLink profile: %s", config[CONF_PLUGIN_PROFILE])
    _LOGGER.info("RFLink RX plugins: %s; source files unchanged; TX disabled",
                 ", ".join(f"{n:03d}" for n in ids))
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await remote_base.register_listener(var, config)
    cg.add(var.set_log_messages(config[CONF_LOG_MESSAGES]))

    plugin_switches = config.get(CONF_PLUGIN_SWITCHES)
    cg.add(var.set_plugin_switch_mode(plugin_switches is not None))
    if plugin_switches:
        restore = plugin_switches[CONF_RESTORE]
        for conf in plugin_switches[CONF_PLUGINS]:
            sw = cg.new_Pvariable(conf[CONF_ID], var, conf[CONF_PLUGIN_ID])
            await cg.register_component(sw, conf)
            await switch.register_switch(sw, conf)
            if not restore:
                cg.add(sw.set_restore_mode(switch.RESTORE_MODES["ALWAYS_ON"]))
            elif conf[CONF_PLUGIN_ID] == 254:
                # Unsupported-packet debug is intentionally OFF on its first
                # boot. Once changed by the user, restore:true keeps that state.
                cg.add(sw.set_restore_mode(switch.RESTORE_MODES["RESTORE_DEFAULT_OFF"]))
        active = await text_sensor.new_text_sensor(plugin_switches[CONF_ACTIVE_PLUGINS])
        cg.add(var.set_active_plugins_text_sensor(active))

    for conf in config.get(CONF_ON_MESSAGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)
