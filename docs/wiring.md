# Wiring and power guide

## Power plan

- **Arduino 5V rail (from USB):** flame sensors, MQ smoke sensor, servo, buzzer, relay VCC and IN, LEDs, and the pump (switched by the relay).
- **18650 battery:** the SIM800L only.
- The two sources share **GND only**. Never join the two + wires.

Breadboard: use the top rail pair for 5V and GND. On many full-size boards each rail is split in the middle, so bridge the two halves with a short jumper if needed.

## Connections

### Rails
- Arduino 5V to top + rail
- Arduino GND to top - rail

### Flame sensors
- Fixed sensor: VCC to + rail, GND to - rail, DO to D2
- Servo sensor (mounted on top of the servo): VCC to + rail, GND to - rail, DO to D3

### MQ smoke sensor
- VCC to + rail, GND to - rail, AO to A0

### Servo
- Red to + rail, brown or black to - rail, signal (orange or yellow) to D9

### Buzzer (active)
- + to D8, - to - rail

### Relay and pump
- Relay VCC to + rail, GND to - rail, IN to D7
- Relay COM to + rail
- Relay NO to pump red wire
- Pump black wire to - rail

### SIM800L (on its own 18650)
- Battery + to SIM800L VCC, battery - to SIM800L GND
- SIM800L GND also to the Arduino - rail (shared ground)
- SIM800L TXD to D10
- SIM800L RXD to D11 through a voltage divider:
  - 10 kohm from D11 to a junction
  - 20 kohm from the junction to GND
  - junction to SIM800L RXD
- Insert a SIM card with balance, and attach the antenna.
- Optional: a 1000 to 2200 uF capacitor across VCC and GND, close to the module, if it resets when sending.

### LEDs (optional)
- Standby LED: D12 to a 220 to 330 ohm resistor to the LED long leg (+), short leg (-) to GND
- Alert LED: D13 to a 220 to 330 ohm resistor to the LED long leg (+), short leg (-) to GND

## Mounting

Mount the servo flame sensor and the pump pipe together on the servo horn, pointing the same way. The sensor decides where the water goes.

## Voltages at a glance

| Part | Voltage |
|---|---|
| Flame sensors, MQ2, servo, relay, pump | 5 V (Arduino 5V rail) |
| SIM800L | 3.7 V (18650), never 5 V |
| SIM800L RXD pin | about 3.3 V, after the divider |
