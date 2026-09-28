# Troubleshooting

## The pump runs all the time, or never runs
The relay type is probably opposite to the code. In the sketch, flip:

```cpp
const bool RELAY_ACTIVE_LOW = true;   // try false
```

Most blue relay boards are active-LOW (`true`).

## The flame sensor range is too short
Range is set on the module, not in code. Turn the small blue screw (potentiometer) slowly while watching the module LED, which lights when it sees flame. Set it so the LED is off in normal light but turns on from as far away as possible. Keep sensors away from direct sunlight and bulbs, since both give off infrared.

## The sensors always show fire, or never show fire
Your module may output the opposite level. In the sketch, flip:

```cpp
#define FLAME_ACTIVE_STATE LOW    // try HIGH
```

## The servo twitches or the Arduino resets
The USB power source cannot supply enough current. Use a stronger USB adapter or power bank.

## The SMS does not send
- The SIM needs balance, no PIN lock, and 2G coverage (SIM800L works on 2G only).
- Power the SIM800L from a battery, not from the Arduino 5V pin.
- Check the voltage divider on D11 to RXD, and that GND is shared.
- Add a 1000 to 2200 uF capacitor across the module's VCC and GND.
- Confirm the antenna is attached and `ALERT_PHONE_NUMBER` is correct.

## The smoke value looks wrong
The MQ2 needs a few minutes to warm up. Watch the Smoke number in the Serial Monitor in clean air, then set `SMOKE_THRESHOLD` above that level.

## The dashboard cannot connect
- Use Chrome or Edge on a computer.
- Close the Arduino Serial Monitor first. Only one program can use the port.
- Baud rate must be 9600.
- Use Try demo to check that the page itself works.

## The phone alert does not arrive
- The topic name must match exactly in the ntfy app and on the dashboard.
- The laptop needs internet, and the dashboard page must stay open.
- Allow notifications for ntfy on the phone, and turn off battery optimization for it on Android.
