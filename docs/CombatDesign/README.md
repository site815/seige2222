# Combat design reference data

These files preserve the confirmed design constraints and earlier open questions used to develop combat. They are **reference documents, not runtime configuration**. Current runtime data is loaded from [combat.json](../../Rules/combat.json), [weapons.json](../../Rules/weapons.json) and [chassis.json](../../Rules/chassis.json), with building, energy, resource and transport definitions in the same `Rules` directory.

The current v0.8 source implements fleets, configurable vehicles, weapons, defenses, paid assembly/refitting and a privateering world bridge. Fresh native and Shipping acceptance for the expanded revision is pending; see the [development report](../DEVELOPMENT_REPORT.md). These reference JSON files do not establish runtime defaults or verification.

The design records specify a **50-point** starting fleet cap; **1/2/4/8** points for small/medium/large/behemoth chassis; and **1/2/3 total shared fleet slots** at command-center levels 1/2/3. Slots serve defense, escort or privateering, and only physically embarked vehicles leave aboard the shuttle. Chassis records cover twelve classes; weapon records cover four families and shot/defense requirements.

Null fields in the older reference files mean unresolved design choices at the time they were written. They are not zero-valued rules. Live catalogs now contain explicit provisional numeric balance and validation; consult those for the current supported behavior. See the [detailed specification](../game-design/WEAPONS_DEFENSES_AND_VEHICLE_OUTFITTING.md) for confirmed requirements and remaining design scope.

The [first-playable balance note](BALANCE_FIRST_PLAYABLE.md) records the current provisional damage and shot-cost adjustment, its rationale, and validation limits.
