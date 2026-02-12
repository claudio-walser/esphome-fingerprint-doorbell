import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, ICON_FINGERPRINT
from . import FingerprintDoorbell, CONF_FINGERPRINT_DOORBELL_ID, fingerprint_doorbell_ns

DEPENDENCIES = ["fingerprint_doorbell"]

CONF_MATCH_NAME = "match_name"
CONF_ENROLL_STATUS = "enroll_status"
CONF_LAST_ACTION = "last_action"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FINGERPRINT_DOORBELL_ID): cv.use_id(FingerprintDoorbell),
        cv.Optional(CONF_MATCH_NAME): text_sensor.text_sensor_schema(
            icon=ICON_FINGERPRINT,
        ),
        cv.Optional(CONF_ENROLL_STATUS): text_sensor.text_sensor_schema(
            icon="mdi:account-plus",
        ),
        cv.Optional(CONF_LAST_ACTION): text_sensor.text_sensor_schema(
            icon="mdi:history",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FINGERPRINT_DOORBELL_ID])

    if CONF_MATCH_NAME in config:
        sens = await text_sensor.new_text_sensor(config[CONF_MATCH_NAME])
        cg.add(parent.set_match_name_sensor(sens))

    if CONF_ENROLL_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_ENROLL_STATUS])
        cg.add(parent.set_enroll_status_sensor(sens))

    if CONF_LAST_ACTION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LAST_ACTION])
        cg.add(parent.set_last_action_sensor(sens))
