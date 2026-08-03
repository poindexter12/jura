import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from .. import jura_ns, Jura

DEPENDENCIES = ["jura"]

CONF_JURA_ID = "jura_id"

JuraPowerSwitch = jura_ns.class_("JuraPowerSwitch", switch.Switch)

CONFIG_SCHEMA = switch.switch_schema(
    JuraPowerSwitch,
    icon="mdi:power",
    default_restore_mode="DISABLED",
).extend(
    {
        cv.GenerateID(CONF_JURA_ID): cv.use_id(Jura),
    }
)


async def to_code(config):
    var = await switch.new_switch(config)
    parent = await cg.get_variable(config[CONF_JURA_ID])
    cg.add(var.set_parent(parent))
