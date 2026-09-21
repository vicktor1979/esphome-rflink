"""RFLink RX compatibility bridge. GitHub external_component; Arduino only."""
from pathlib import Path
import logging

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import remote_base
from esphome.const import CONF_ID, CONF_TRIGGER_ID
from esphome.core import CORE

from .stage_sources import select_plugins, stage

DEPENDENCIES = ["remote_receiver"]
AUTO_LOAD = ["remote_base"]
MULTI_CONF = False
CODEOWNERS = []

CONF_RX_PLUGINS = "rx_plugins"
CONF_ON_MESSAGE = "on_message"
CONF_LOG_MESSAGES = "log_messages"
_LOGGER = logging.getLogger(__name__)
REPO = Path(__file__).resolve().parents[2]

ns = cg.esphome_ns.namespace("rflink")
RFLinkComponent = ns.class_("RFLinkComponent", cg.Component, remote_base.RemoteReceiverListener)
RFLinkMessageTrigger = ns.class_("RFLinkMessageTrigger", automation.Trigger.template(cg.std_string))


def validate_plugins(value):
    if isinstance(value, str):
        selection = cv.one_of("configured", "all", lower=True)(value)
    else:
        selection = cv.ensure_list(cv.int_range(min=1, max=255))(value)
    try:
        select_plugins(REPO, selection)
    except (OSError, ValueError) as error:
        raise cv.Invalid(str(error)) from error
    return selection


CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(RFLinkComponent),
        cv.Optional(CONF_RX_PLUGINS, default="configured"): validate_plugins,
        cv.Optional(CONF_LOG_MESSAGES, default=True): cv.boolean,
        cv.Optional(CONF_ON_MESSAGE): automation.validate_automation({
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RFLinkMessageTrigger),
        }),
    }).extend(remote_base.REMOTE_LISTENER_SCHEMA).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
)


async def to_code(config):
    # Same build-source staging stage as ESPHome's includes handling. Placed
    # outside src/esphome/, which is maintained/deleted by the component writer.
    ids = stage(REPO, Path(CORE.relative_src_path("rflink_vendor")), config[CONF_RX_PLUGINS])
    _LOGGER.info("RFLink RX plugins: %s; source files unchanged; TX disabled",
                 ", ".join(f"{n:03d}" for n in ids))
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await remote_base.register_listener(var, config)
    cg.add(var.set_log_messages(config[CONF_LOG_MESSAGES]))
    for conf in config.get(CONF_ON_MESSAGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)
