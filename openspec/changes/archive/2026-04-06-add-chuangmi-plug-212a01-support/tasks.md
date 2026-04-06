## 1. Miio Device Profile Support

- [x] 1.1 Add a way for `MiioDevice` to query and cache the exact device model from `miIO.info`
- [x] 1.2 Introduce an internal exact-model profile mapping in `MiioDevice` for power-property selection
- [x] 1.3 Add the `chuangmi.plug.212a01` power profile using `siid=5, piid=6`
- [x] 1.4 Apply the required raw-value scaling for `chuangmi.plug.212a01` so `GetPower(double&)` continues returning watts
- [x] 1.5 Ensure the `chuangmi.plug.212a01` profile activates only when the reported model string exactly matches `chuangmi.plug.212a01`

## 2. Compatibility Preservation

- [x] 2.1 Preserve the existing legacy power-read behavior for the currently supported model path
- [x] 2.2 Keep model-specific protocol handling inside `MiioDevice` without changing plugin UI or configuration flow
- [x] 2.3 Confirm that unsupported or non-matching models do not use the `chuangmi.plug.212a01` property mapping

## 3. Documentation And Verification

- [x] 3.1 Update `README.md` to list `chuangmi.plug.212a01` as a supported exact-match model
- [x] 3.2 Document the model-specific power property and scaling used for `chuangmi.plug.212a01`
- [x] 3.3 Verify the implementation against a real `chuangmi.plug.212a01` device and confirm the returned watt value is plausible
- [x] 3.4 Verify that the legacy supported model path still works after the change
