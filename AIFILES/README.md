# Scenario colony AI

These editable definitions control local AI colonies and the center colony in observer mode. `Rules` remains the authority for live costs, individual workers, production, transport, power, repairs and threats.

## Starting and established scenarios

A **Starting AI** uses the player's finite landing manifest: one deploying shuttle, six carried workers, zero credits and the authored 9.57-tonne material kit. It constructs, staffs and supplies new facilities through ordinary paid actions. The player landing is unchanged.

A **Developed AI** now starts from the explicit established state in [developed_start.json](developed_start.json). It does not simulate hours of colony growth while loading. The current provisional manifest contains a command core plus 23 completed facilities, 38 operators and six idle logistics workers, four stored worker bodies, 0.5 Galactic credits, 145 kWh in local batteries, and bounded inventories in named buildings. Inventories total 7,936 kg and 5,515.8 litres, including the four packed 80 kg workers; active bodies, installed structures and the finite guard fleet are separate assets. These are authored scenario starting assets, not recurring income or grants during play.

The manifest's 60-second colony age marks an already deployed settlement. It is not claimed as simulated construction history and does not advance the shared world calendar. Produced/delivered statistics begin at zero. Scenario integration binds the common calendar after all region candidates succeed. Normal threat timers begin after initialization, honoring the scenario's background-bug and periodic-attack switches.

The manifest declares every building's definition, relative offset in metres, operator count, selected recipe, cargo and stored battery energy. The mine binds to one actual generated standard deposit. Equipment uses the ordinary definition loadouts and the finite `Rules/combat.json` scenario guard manifest. Deterministic candidate rotations must satisfy the same dry-land, reserved-plot, road-corridor and access geometry as live construction. All plots are present before routes are generated, and every facility must join a powered core grid. Worker initialization creates unique physical identities at legal workstations and exact stored-body locations. Capacities, inventory units, assignments, body overlap, support, recipes and building references are validated before committing the candidate.

`BeginInitialize` may finish an established region immediately; the scenario still commits all regions atomically and permits cancellation between regions. The layout search is bounded, but initialization is not promised to fit a six-millisecond frame. Failure preserves the current colony. Fresh compile, native initialization, save/load and bounded live-continuation verification are pending.

The selector `colony_ai.json.developed_initialization` defaults to `established_manifest`. `simulated_history` retains the former paid-growth path for explicit diagnostics; its provisional 129,600-second ceiling and action budget are not the default scenario's acceptance criteria. The historical growth failures remain diagnostic evidence, not successful established-scenario tests. Starting AI still has known weaknesses in sustaining full industry under prolonged attacks; choosing an established start does not claim to fix that planner/combat limitation.

## Live controller

Both starts then run the same ordinary controller. The ordered plan targets the same 23 facilities, rebuilding missing installations when affordable. It opens with power, support, sensors, a tower, extraction, trade, fuel generation and worker assembly, then completes the perimeter before industry. The large battery bank follows the material-producing chain. Buildings and roads remain real paid, routed construction orders after initialization.

`decision_scheduling_policy: independent_tactics_trade_construction` updates the existing guard fleet, attempts one paid trade transaction and permits up to `max_actions_per_decision` construction actions at each cadence. `shared_action_budget` is available for comparisons. Neither policy creates units, energy or resources.

- `core_replication_policy: funded_shortage_first` selects useful outputs from the external recipe list, preferring a batch whose local unreserved inputs, energy and space can commit. Committed work finishes normally; an operating specialist is preferred to duplicating its recipe in the slower command replicator.
- `fuel_import_buffer_cycles` and `fuel_import_refill_fraction` keep four operating fuel buffers and refill below half that target, avoiding tiny repeated imports. Consumption and physical delivery remain unchanged.
- `bulk_input_policy: remaining_output_bill` derives useful raw feedstock from outstanding manufactured-output needs, counts coproducts once, and caps purchases by that bill, credits, port mass and free volume. Ordinary raw refills use the greater of the AI cycle buffer and the simulation's two-haul `ProductionInputBuffer`; manufactured inputs retain their cycle buffers.
- `export_policy: surplus_shipment_value` compares actual legal shipment quotes. Manufactured exports protect remaining planned building bills and replacements, unfinished construction/roads, the next connector, current operating/AI reserves and configured worker-reserve inputs. Discrete workers are excluded. This protection covers current recipes and direct bills, not an entire hypothetical future recipe tree. `local_raw_only` remains available for comparison. Physical loading, local claims, flight time and real credit settlement apply.
- `support_recovery_policy: restore_capacity_before_expansion` restores missing configured capacity ahead of ordinary expansion without buying duplicate pending bays. A temporary power outage in sufficient intact support instead leaves grid recovery available.
- Placement previews the proposed reserved plot in the future road planner. Legal access must remain dry and wide enough for the corridor; purchasing still checks the full bill. `prefer_covered_approaches` favors sampled approach coverage from existing fixed guns while respecting structure occlusion. It cannot guarantee all-angle protection; `first_legal` remains a comparison.

Guards are the actual finite carried fleet. Visible danger takes precedence over a new worksite. The fleet returns below 30% battery and normally resumes at 90%; visible danger can interrupt charging only when all guards exceed the return threshold. Travel, clear firing positions, ammunition and shot energy remain ordinary simulation behavior. No hidden enemy knowledge or defensive reinforcements are granted.

The 23-facility plan provides approximately 105.92 kW cycle-average generation at full staffing and adequate fuel, with 145 kWh storage. The six industrial recipes would require approximately 921.86 kW at continuous full cadence, before defense and charging. This is a partial-throughput prototype economy, not a promise that every factory runs continuously. Core fusion is independent of daylight but depends on actual operators.

## Validation and persistence

`node Tools/validate_configuration.mjs` validates the live definition set. `node Tools/test_ai_configuration.mjs` includes malformed established-manifest cases. Native tests must additionally exercise terrain-dependent layouts, distinct body accounting, save/load, deterministic state and continued live operation. A bounded continuation test is not an indefinite survival guarantee.

Decision timing derives from saved simulation time and state. Save/load initializes definitions with `bDeveloped=false`, then restores the existing snapshot; it does not apply the manifest again. Changing either AI JSON file changes the configuration fingerprint. Runtime validation rejects invalid references, quantities, layout, stock capacity, worker assignments and unsupported modes atomically.

## Historical evidence

The paid-growth experiments through run21 exposed real transport, reservation and combat defects, several of which now have focused regressions. They did not validate indefinite AI survival or the replacement established initializer. Those logs and snapshots remain under `Saved/Automation` and `Saved/Diagnostics`.

The released [v0.8.1 record](../docs/verification/v0.8.1.json) verifies the older single-mine controller and its 117-stage Shipping route. The [v0.8 record](../docs/verification/v0.8.0.json) includes the earlier 20-installation paid preparation. Their rules, worker model and acceptance scope differ from the current v0.9 implementation; they are historical evidence only.
