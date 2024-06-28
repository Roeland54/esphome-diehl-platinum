import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart, time
from esphome.const import (
    CONF_TIME_ID,
    CONF_ID,
    CONF_POWER,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_WATT,
    UNIT_WATT_HOURS
)

CODEOWNERS = ["@roeland54"]
DEPENDENCIES = ["uart", "time"]

CONF_DAY_ENERGY = 'day_energy'

diehl_ns = cg.esphome_ns.namespace("diehl")
DiehlComponent = diehl_ns.class_(
    "DiehlComponent", cg.PollingComponent, uart.UARTDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DiehlComponent),
            cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_DAY_ENERGY): sensor.sensor_schema(

                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                unit_of_measurement=UNIT_WATT_HOURS,
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(

                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
                unit_of_measurement=UNIT_WATT,
            )
        }
    )
    .extend(cv.polling_component_schema("32s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "diehl", baud_rate=19200, require_rx=True, require_tx=True
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    time_ = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_))

    # Standard sensors
    for key in [
        CONF_DAY_ENERGY,
        CONF_POWER
    ]:
        if key not in config:
            continue
        conf = config[key]
        sens = await sensor.new_sensor(conf)
        cg.add(getattr(var, f"set_{key}_sensor")(sens))
