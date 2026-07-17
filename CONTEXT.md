# Context

## Glossary

### Device display item

A TrafficMonitor display item that represents the real-time power of exactly one configured smart plug. Each device display item can be shown, hidden, ordered, and styled independently by TrafficMonitor.

### Device collection change

An addition, removal, or reordering of configured smart plugs that changes the set or order of device display items exposed to TrafficMonitor.

### Device label

The configured device name shown before a device display item's power value when labels are enabled. When labels are disabled, the item shows only its power value and unit.

### Double-line device flow

The taskbar presentation in which configured device display items fill the upper and lower rows of a column before continuing in the next column to the right.

### Stable item identity

The persistent alphanumeric identity of a device display item. It remains unchanged when the device is renamed or its connection details are edited, preserving TrafficMonitor's item-specific settings.

### Legacy single-device configuration

An existing `MijiaPower.ini` that stores device details in one or more repeated `[Device]` sections rather than as an explicitly identified device collection.

### Device settings

The name, local IP address, authentication token, connection state, and power history belonging to one configured smart plug.

### Shared display settings

The sampling interval, history recording switch, label visibility, unit visibility, and decimal precision applied consistently to every device display item.
