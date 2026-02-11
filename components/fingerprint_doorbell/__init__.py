import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID
from esphome import pins

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor"]

CONF_FINGERPRINT_DOORBELL_ID = "fingerprint_doorbell_id"
CONF_TOUCH_PIN = "touch_pin"
CONF_DOORBELL_PIN = "doorbell_pin"
CONF_IGNORE_TOUCH_RING = "ignore_touch_ring"

fingerprint_doorbell_ns = cg.esphome_ns.namespace("fingerprint_doorbell")
FingerprintDoorbell = fingerprint_doorbell_ns.class_(
    "FingerprintDoorbell", cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(FingerprintDoorbell),
        cv.Optional(CONF_TOUCH_PIN, default=5): pins.gpio_input_pin_schema,
        cv.Optional(CONF_DOORBELL_PIN, default=19): pins.gpio_output_pin_schema,
        cv.Optional(CONF_IGNORE_TOUCH_RING, default=False): cv.boolean,
    }
).extend(uart.UART_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    touch_pin = await cg.gpio_pin_expression(config[CONF_TOUCH_PIN])
    cg.add(var.set_touch_pin(touch_pin))

    doorbell_pin = await cg.gpio_pin_expression(config[CONF_DOORBELL_PIN])
    cg.add(var.set_doorbell_pin(doorbell_pin))

    cg.add(var.set_ignore_touch_ring(config[CONF_IGNORE_TOUCH_RING]))
