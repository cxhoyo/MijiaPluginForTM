## Why

The plugin currently assumes a single MIOT power property mapping and therefore fails against Xiaomi plug models that expose power under a different service/property pair. We have verified that `chuangmi.plug.212a01` is reachable and authenticated successfully, but its power data does not live at the hard-coded property the plugin reads today.

## What Changes

- Add exact-model support for `chuangmi.plug.212a01` in the local miIO/MIOT device layer.
- Detect the device model before applying model-specific power property mappings.
- Require an exact model string match before using the `chuangmi.plug.212a01` property mapping, so the plugin does not read the wrong property on unknown or partially similar devices.
- Preserve existing behavior for the currently supported model path.
- Update user-facing documentation to list `chuangmi.plug.212a01` as a supported model and document its power property source.

## Capabilities

### New Capabilities
- `miot-model-profiles`: Support exact-model-specific MIOT property mappings for local Xiaomi device reads, starting with `chuangmi.plug.212a01`.

### Modified Capabilities
- None.

## Impact

- Affected code:
  - `MiioDevice.h`
  - `MiioDevice.cpp`
  - `README.md`
- No new external runtime dependencies are required.
- No UI or configuration changes are expected for the initial model-support addition.
