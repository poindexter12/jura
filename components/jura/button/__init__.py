import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from .. import jura_ns, Jura

DEPENDENCIES = ["jura"]

CONF_JURA_ID = "jura_id"
CONF_DRINK = "drink"
CONF_COMMAND = "command"

JuraButton = jura_ns.class_("JuraButton", button.Button)

# Common FA commands. These simulate front-panel button presses and are
# model-specific on the machine side -- see COMMANDS.md.
DRINKS = {
    "espresso": "FA:04",
    "ristretto": "FA:05",
    "hot_water": "FA:06",
    "cappuccino": "FA:07",
    "coffee": "FA:09",
}


def _exactly_one_of_drink_or_command(config):
    if (CONF_DRINK in config) == (CONF_COMMAND in config):
        raise cv.Invalid("Provide exactly one of 'drink' or 'command'")
    return config


CONFIG_SCHEMA = cv.All(
    button.button_schema(JuraButton, icon="mdi:coffee").extend(
        {
            cv.GenerateID(CONF_JURA_ID): cv.use_id(Jura),
            cv.Optional(CONF_DRINK): cv.one_of(*DRINKS, lower=True),
            cv.Optional(CONF_COMMAND): cv.string_strict,
        }
    ),
    _exactly_one_of_drink_or_command,
)


async def to_code(config):
    var = await button.new_button(config)
    parent = await cg.get_variable(config[CONF_JURA_ID])
    cmd = DRINKS[config[CONF_DRINK]] if CONF_DRINK in config else config[CONF_COMMAND]
    cg.add(var.set_parent(parent))
    cg.add(var.set_command(cmd))
