## ADDED Requirements

### Requirement: Exact model matching for MIOT property profiles
The system SHALL select model-specific MIOT property mappings only when the device model string returned by `miIO.info` exactly matches a supported model profile identifier.

#### Scenario: Exact model match enables the new profile
- **WHEN** the device reports model `chuangmi.plug.212a01`
- **THEN** the system applies the `chuangmi.plug.212a01` MIOT property profile for power reads

#### Scenario: Non-matching models do not use the new profile
- **WHEN** the device reports any model string other than `chuangmi.plug.212a01`
- **THEN** the system MUST NOT use the `chuangmi.plug.212a01` power property mapping

### Requirement: Power reading for chuangmi.plug.212a01
For devices whose exact model is `chuangmi.plug.212a01`, the system SHALL read power from MIOT property `siid=5, piid=6` and convert the raw value to watts using the documented raw-value scaling for that model.

#### Scenario: Read live power for chuangmi.plug.212a01
- **WHEN** the system reads power from a device whose model is exactly `chuangmi.plug.212a01`
- **THEN** it queries MIOT property `siid=5, piid=6`
- **AND** it converts the raw response to watts before returning the value to the plugin display layer

#### Scenario: Legacy model behavior remains unchanged
- **WHEN** the system reads power from a device that is supported by the existing legacy mapping
- **THEN** it continues using the existing legacy MIOT property path without applying `chuangmi.plug.212a01` scaling

### Requirement: Supported-model documentation
The system documentation SHALL identify `chuangmi.plug.212a01` as a supported exact-match model and describe the model-specific power property used for that support.

#### Scenario: Documentation lists the exact supported model
- **WHEN** a user reads the compatibility section for supported devices
- **THEN** the documentation includes `chuangmi.plug.212a01`
- **AND** it describes that this model uses a different MIOT power property than the legacy supported model
