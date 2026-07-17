# Migrate legacy configuration to stable device identities

On first load, the plugin imports every repeated legacy `[Device]` block, backs up the original INI, and writes the identified multi-device format. The first imported device retains `MijiaPowerW`, while later devices receive persistent alphanumeric item IDs, so existing TrafficMonitor presentation settings survive migration and future device renames.
