import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import i2c, time
from esphome.const import CONF_ID, CONF_SLEEP_DURATION

DEPENDENCIES = ['i2c']

CONF_I2C_ADDR = 0x51

bm8563 = cg.esphome_ns.namespace('bm8563')
BM8563 = bm8563.class_('BM8563', time.RealTimeClock, i2c.I2CDevice)
WriteAction = bm8563.class_("WriteAction", automation.Action)
ReadAction = bm8563.class_("ReadAction", automation.Action)
SleepAction = bm8563.class_("SleepAction", automation.Action)

CONFIG_SCHEMA = time.TIME_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(BM8563),
    cv.Optional(CONF_SLEEP_DURATION): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(CONF_I2C_ADDR))

@automation.register_action(
    "bm8563.write_time",
    WriteAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(BM8563),
        }
    ),
    synchronous=True
)
async def bm8563_write_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

@automation.register_action(
    "bm8563.apply_sleep_duration",
    SleepAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(BM8563),
        }
    ),
    synchronous=True
)
async def bm8563_apply_sleep_duration_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

@automation.register_action(
    "bm8563.read_time",
    ReadAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(BM8563),
        }
    ),
    synchronous=True
)
async def bm8563_read_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await time.register_time(var, config)
    if CONF_SLEEP_DURATION in config:
        cg.add(var.set_sleep_duration(config[CONF_SLEEP_DURATION]))
