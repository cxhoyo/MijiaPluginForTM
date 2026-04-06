## Context

The plugin currently treats power reads as a single fixed MIOT query in `MiioDevice::GetPower`, using property `siid=11, piid=2` for all devices. That works for the currently supported model path but fails for `chuangmi.plug.212a01`, even though the device is reachable and authenticates successfully over local miIO.

Exploration established two important facts:
- `chuangmi.plug.212a01` exposes live power at `siid=5, piid=6`, not at `siid=11, piid=2`
- the device model can be queried from `miIO.info`, so profile selection does not need to rely on guesswork

The user explicitly wants exact model matching so the new property mapping cannot be applied to the wrong Xiaomi plug.

## Goals / Non-Goals

**Goals:**
- Add support for `chuangmi.plug.212a01` without regressing the current supported device path
- Select the new property mapping only for the exact model string `chuangmi.plug.212a01`
- Keep model-specific protocol knowledge inside the miIO device layer
- Document the new supported model and its model-specific power property

**Non-Goals:**
- Introduce generic heuristics for “similar” Xiaomi plug models
- Add user-configurable `siid/piid` overrides in the UI or INI file
- Broaden the plugin into a generic Xiaomi device browser
- Change the plugin’s public behavior beyond enabling power reads for the exact new model

## Decisions

### Use `miIO.info` for model detection
The device layer will query `miIO.info` and cache the `model` string on the `MiioDevice` instance.

Why:
- the device already supports `miIO.info`
- the result provides a precise model identifier
- it avoids applying property mappings based on guesswork

Alternative considered:
- Blindly trying multiple power properties and accepting the first numeric result
- Rejected because it could read the wrong field on unrelated models and violates the exact-match requirement

### Introduce a small internal model-profile mapping
The device layer will use an internal profile mapping keyed by exact model string. The initial mapping set will include:
- existing legacy path for current support
- `chuangmi.plug.212a01` -> power at `siid=5, piid=6`

Why:
- the codebase already centralizes protocol behavior in `MiioDevice`
- a profile table is small, explicit, and easier to extend than scattered `if` branches
- it keeps `MijiaPowerPlugin` unchanged

Alternative considered:
- Putting model logic into `MijiaPowerPlugin`
- Rejected because it leaks transport/protocol details outside the miIO abstraction

### Enforce exact model matching for `chuangmi.plug.212a01`
The `chuangmi.plug.212a01` profile will only activate when the model returned by `miIO.info` is exactly `chuangmi.plug.212a01`.

Why:
- this is the user’s explicit safety requirement
- Xiaomi plug families often expose different MIOT service/property layouts
- exact matching avoids accidental reads from wrong properties on unknown models

Alternative considered:
- Prefix matching such as `chuangmi.plug.*`
- Rejected because it is too broad for the known variability in plug models

### Apply model-specific scaling in the profile
The `chuangmi.plug.212a01` profile will define both the power property and the conversion needed to return watts to the rest of the plugin.

Why:
- the rest of the plugin already expects `GetPower(double&)` to return display-ready watts
- keeping scaling beside the property definition reduces hidden model-specific behavior

Alternative considered:
- Returning raw values and scaling at the plugin/UI layer
- Rejected because it spreads protocol semantics outside `MiioDevice`

## Risks / Trade-offs

- [Model detection adds one more miIO query] → Cache the model once per `MiioDevice` instance and reuse it for subsequent reads
- [Loose JSON parsing in the current device layer can make `miIO.info` parsing brittle] → Parse only the exact `model` field needed and keep the extraction logic narrow
- [Legacy behavior may still be too narrow for other unsupported models] → Preserve current behavior, but do not broaden support beyond exact known profiles in this change
- [Incorrect scaling would produce believable but wrong watt values] → Keep the scaling factor defined as part of the exact model profile and document it in README

## Migration Plan

1. Add the new OpenSpec capability and design artifacts
2. Update `MiioDevice` to detect and cache the model string from `miIO.info`
3. Add exact-match profile selection for `chuangmi.plug.212a01`
4. Keep the existing legacy power path for current supported devices
5. Update README compatibility and power-property documentation
6. Verify behavior against a real `chuangmi.plug.212a01` device and ensure legacy model behavior still works

Rollback strategy:
- Remove the `chuangmi.plug.212a01` profile and model-detection path, leaving the original fixed legacy power query in place

## Open Questions

- Whether the existing legacy path should also be expressed as a named exact-model profile now, or left as the current default behavior for compatibility
