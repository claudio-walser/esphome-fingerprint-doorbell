import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome import pins, automation

AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor", "web_server_base"]

CONF_FINGERPRINT_DOORBELL_ID = "fingerprint_doorbell_id"
CONF_TOUCH_PIN = "touch_pin"
CONF_DOORBELL_PIN = "doorbell_pin"
CONF_IGNORE_TOUCH_RING = "ignore_touch_ring"

fingerprint_doorbell_ns = cg.esphome_ns.namespace("fingerprint_doorbell")
FingerprintDoorbell = fingerprint_doorbell_ns.class_(
    "FingerprintDoorbell", cg.Component
)

# Actions for automations
EnrollAction = fingerprint_doorbell_ns.class_("EnrollAction", automation.Action)
CancelEnrollAction = fingerprint_doorbell_ns.class_("CancelEnrollAction", automation.Action)
DeleteAction = fingerprint_doorbell_ns.class_("DeleteAction", automation.Action)
DeleteAllAction = fingerprint_doorbell_ns.class_("DeleteAllAction", automation.Action)
RenameAction = fingerprint_doorbell_ns.class_("RenameAction", automation.Action)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(FingerprintDoorbell),
        cv.Optional(CONF_TOUCH_PIN): pins.gpio_input_pin_schema,
        cv.Optional(CONF_DOORBELL_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_IGNORE_TOUCH_RING, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


# Action schemas for automations
ENROLL_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(FingerprintDoorbell),
        cv.Required("finger_id"): cv.templatable(cv.int_range(min=1, max=200)),
        cv.Required("name"): cv.templatable(cv.string),
    }
)

CANCEL_ENROLL_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(FingerprintDoorbell),
    }
)

DELETE_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(FingerprintDoorbell),
        cv.Required("finger_id"): cv.templatable(cv.int_range(min=1, max=200)),
    }
)

DELETE_ALL_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(FingerprintDoorbell),
    }
)

RENAME_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(FingerprintDoorbell),
        cv.Required("finger_id"): cv.templatable(cv.int_range(min=1, max=200)),
        cv.Required("name"): cv.templatable(cv.string),
    }
)


@automation.register_action("fingerprint_doorbell.enroll", EnrollAction, ENROLL_ACTION_SCHEMA)
async def enroll_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    
    template_ = await cg.templatable(config["finger_id"], args, cg.uint16)
    cg.add(var.set_finger_id(template_))
    
    template_ = await cg.templatable(config["name"], args, cg.std_string)
    cg.add(var.set_name(template_))
    
    return var


@automation.register_action("fingerprint_doorbell.cancel_enroll", CancelEnrollAction, CANCEL_ENROLL_ACTION_SCHEMA)
async def cancel_enroll_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action("fingerprint_doorbell.delete", DeleteAction, DELETE_ACTION_SCHEMA)
async def delete_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    
    template_ = await cg.templatable(config["finger_id"], args, cg.uint16)
    cg.add(var.set_finger_id(template_))
    
    return var


@automation.register_action("fingerprint_doorbell.delete_all", DeleteAllAction, DELETE_ALL_ACTION_SCHEMA)
async def delete_all_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action("fingerprint_doorbell.rename", RenameAction, RENAME_ACTION_SCHEMA)
async def rename_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    
    template_ = await cg.templatable(config["finger_id"], args, cg.uint16)
    cg.add(var.set_finger_id(template_))
    
    template_ = await cg.templatable(config["name"], args, cg.std_string)
    cg.add(var.set_name(template_))
    
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_TOUCH_PIN in config:
        touch_pin = await cg.gpio_pin_expression(config[CONF_TOUCH_PIN])
        cg.add(var.set_touch_pin(touch_pin))

    if CONF_DOORBELL_PIN in config:
        doorbell_pin = await cg.gpio_pin_expression(config[CONF_DOORBELL_PIN])
        cg.add(var.set_doorbell_pin(doorbell_pin))

    cg.add(var.set_ignore_touch_ring(config[CONF_IGNORE_TOUCH_RING]))
