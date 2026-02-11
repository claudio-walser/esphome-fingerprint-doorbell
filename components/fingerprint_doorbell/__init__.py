import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome import pins

AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor"]

CONF_FINGERPRINT_DOORBELL_ID = "fingerprint_doorbell_id"
CONF_TOUCH_PIN = "touch_pin"
CONF_DOORBELL_PIN = "doorbell_pin"
CONF_IGNORE_TOUCH_RING = "ignore_touch_ring"
CONF_SENSOR_RX_PIN = "sensor_rx_pin"
CONF_SENSOR_TX_PIN = "sensor_tx_pin"

fingerprint_doorbell_ns = cg.esphome_ns.namespace("fingerprint_doorbell")
FingerprintDoorbell = fingerprint_doorbell_ns.class_(
    "FingerprintDoorbell", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(FingerprintDoorbell),
        cv.Optional(CONF_TOUCH_PIN): pins.gpio_input_pin_schema,
        cv.Optional(CONF_DOORBELL_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_IGNORE_TOUCH_RING, default=False): cv.boolean,
        cv.Optional(CONF_SENSOR_RX_PIN, default=16): cv.int_range(min=0, max=39),
        cv.Optional(CONF_SENSOR_TX_PIN, default=17): cv.int_range(min=0, max=39),
    }
).extend(cv.COMPONENT_SCHEMA)


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
    cg.add(var.set_sensor_pins(config[CONF_SENSOR_RX_PIN], config[CONF_SENSOR_TX_PIN]))
