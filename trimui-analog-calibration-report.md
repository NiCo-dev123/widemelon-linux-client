# Analog-stick calibration maps the upper Y range incorrectly

## Device

- Trimui Smart Pro S
- Input device: `/dev/input/event4`

## Problem

After analog-stick calibration, the left stick reaches the full range left, right and down, but only about 76% of its range upwards.

## `joypad.config`

```ini
x_min=326
x_max=3675
y_min=4
y_max=4095
x_zero=1942
y_zero=1758
deadzone=0.08
```

## Values read from `/dev/input/event4`

```yaml
x_min: -32760
x_max: +31739
y_min: -25022
y_max: +32760
```

Expected Y range:

```text
-32760 to +32760
```

Actual Y range:

```text
-25022 to +32760
```

The physical/calibrated ranges are asymmetric around the centre:

```text
upper range: y_zero - y_min = 1754
lower range: y_max - y_zero = 2337
```

This suggests that `trimui_inputd` scales both Y directions using one side of the axis range, instead of applying independent scaling above and below `y_zero`.

Expected mapping:

```text
if value < y_zero:
    normalized = (value - y_zero) / (y_zero - y_min)
else:
    normalized = (value - y_zero) / (y_max - y_zero)
```

Could you please check whether `trimui_inputd` correctly applies asymmetric axis calibration around `x_zero` / `y_zero`?
