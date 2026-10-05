# Provisional first-playable combat balance

Runtime authority is [weapons.json](../../Rules/weapons.json), together with [combat.json](../../Rules/combat.json) and [chassis.json](../../Rules/chassis.json). This is an authored balance candidate, not a confirmed design constraint or a completed gameplay acceptance result.

On 6 October 2026, all twelve player weapons received twice their previous damage, electricity per shot and ammunition per shot. Alien `bug_plasma` is unchanged. Reload time, accuracy, range, splash radius, protection multipliers, hardpoint area, module mass, chassis characteristics and building durability are unchanged. Resource cost per point of damage is preserved; discrete overkill can change the actual cost of a kill.

| Family | Small DPS | Medium DPS | Large DPS |
|---|---:|---:|---:|
| Laser | 6 | 14.4 | 30 |
| Kinetic | 8 | 19.2 | 40 |
| Missile | 7.2 | 17.28 | 36 |
| Plasma | 7 | 16.8 | 35 |

These are nominal values before misses, protection, power shortages or ammunition exhaustion. Four default large core lasers total 120 nominal DPS. The small laser needs four hits against a 45-HP unprotected bug, taking six seconds after the first shot; the preceding candidate needed eight hits over fourteen seconds. Electricity for those hits remains 0.024 kWh.

The adjustment follows continued colony attrition after friendly-fire screening, defensive flanking, sensor coverage, guard servicing and AI planning fixes. The older single turret delivered 25 DPS; the preceding modular small laser delivered 3 DPS. Its newer shield and armor already give roughly comparable initial durability, so this candidate does not also raise health. Tactical viability and final release acceptance remain pending.

Static combat, rules and full-configuration validation pass, including 79 malformed-rule cases. Conservative bounds covering every legal mixed weapon loadout fit all twelve vehicle cargo limits and all fifteen platform stores: vehicle ammunition buffers require at most 16/64/256/1024 kg and litres by size; tower buffers require at most 16/64/256 litres against 200/500/1600-litre stores. Fabrication/refit bills and other demand buffers also pass the existing storage validator. The audit is recorded locally in `Saved/Diagnostics/combat-ammo-buffer-v08.json`.

The changed rules fingerprint intentionally rejects saves made against the earlier candidate; start a new scenario for comparison.
