# Scenario colony AI

These editable definitions control the simple local AI colonies and the center colony in observer mode. `Rules` remains the authority for construction costs, staffing, production, deliveries, repairs, and alien threats.

- [colony_ai.json](colony_ai.json) sets the decision interval, ordered building targets, placement search, sensor extension, and setup action limit. Repeated targets may raise an earlier count, such as one early turret followed by four total turrets.
- [developed_start.json](developed_start.json) supplies finite core inventory and population for the developed scenario option. Setup pays the ordinary construction costs from this stock to establish the target layout. It does not award manufactured output or advance the simulation clock.

A starting AI uses the ordinary scenario start. Both types then use the same controller: choose nearby matching deposits, extend live coverage when required, establish production and defenses, and rebuild missing target buildings when affordable. Orders use the normal simulation commands. Robots, inputs, maintenance, delivery time, and attacks retain their normal effects. The controller waits for sensor workers rather than repeatedly buying unstaffed masts.

This is a deterministic construction controller, not a strategic opponent with diplomacy, raids, fleets, or trade. Colonies have independent local inventories and threats; neighboring cells do not transfer goods or attack one another. The developed seed is a scenario choice, not recurring AI income.

Decision timing derives from saved simulation time and current state. Initialize the controller and then load the colony snapshot to resume. The scenario save must also preserve and validate `GetConfigFingerprint()`; changing either AI JSON file changes that fingerprint. No independent hidden AI timer or random state needs serialization.

Reload definitions by starting a new scenario. Runtime validation rejects bad versions, missing files, invalid numerical ranges, unknown building/resource references, overflowing seed inventory, and a developed layout that cannot be established. AI plans refer to existing building definitions; new simulation capabilities still require engine work.

Native tests are under `Seige.AI`: config rejection, starting/developed operation through normal rules, deterministic save continuation, and initial-core/threat-origin validation. All four passed in the initial native 0.2 test run; the starting controller manufactured five components by 300 simulation seconds. This verifies local AI operation, not victory, indefinite survival, the rendered neighborhood, or the latest package. Record later build and playtest results separately.
