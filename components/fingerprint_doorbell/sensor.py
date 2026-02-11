import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    ICON_FINGERPRINT,
    STATE_CLASS_MEASUREMENT,
)
from . import FingerprintDoorbell, CONF_FINGERPRINT_DOORBELL_ID, fingerprint_doorbell_ns

DEPENDENCIES = ["fingerprint_doorbell"]

CONF_MATCH_ID = "match_id"
CONF_CONFIDENCE = "confidence"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FINGERPRINT_DOORBELL_ID): cv.use_id(FingerprintDoorbell),
        cv.Optional(CONF_MATCH_ID): sensor.sensor_schema(
            icon=ICON_FINGERPRINT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CONFIDENCE): sensor.sensor_schema(
            icon=ICON_FINGERPRINT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FINGERPRINT_DOORBELL_ID])

    if CONF_MATCH_ID in config:
        sens = await sensor.new_sensor(config[CONF_MATCH_ID])
        cg.add(parent.set_match_id_sensor(sens))

    if CONF_CONFIDENCE in config:
        sens = await sensor.new_sensor(config[CONF_CONFIDENCE])
        cg.add(parent.set_confidence_sensor(sens))
