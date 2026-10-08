# Upright shuttle and hover workers

Original project assets authored in Blender for the v0.9 test baseline. No reference-game models or private photograph pixels are included.

`Tools/create_orbital_assets_v09.py` produces the editable `Source/Seige_Orbital_Workforce.blend` and FBX exports. `Tools/import_orbital_assets_v09.py` imports three meshes with three LODs each. `import_report.json` records the actual imported dimensions and triangle counts.

| Mesh | Purpose | LOD0 triangles |
| --- | --- | ---: |
| SM_Shuttle | Upright command shuttle, four outriggers, cargo hatch and two weapon banks | 13,388 |
| SM_Core | Upgrade wings surrounding the retained central shuttle | 16,548 |
| SM_Robot | Legless hover worker with visor, tool arms and four lift pods | 4,792 |

The shuttle is normalized to the level-one plot; its central hull retains that scale at subsequent upgrades. The separate expansion campus grows around it. The authored hatch aligns with the external worker-rule hatch position. Rendered hardpoint modules use the equipped loadout rather than baking weapons into the mesh: the provisional default is one large, two medium and eight small lasers across two large banks.

Each visible worker now corresponds to one persistent simulation body ID. Operators, builders and couriers share that finite workforce; changing work does not create another body or move it instantly. Initial workers stay inside the shuttle during descent, leave through its authored hatch, and walk to individual workstations. The body hovers without leg animation. Render interpolation follows the same identity between fixed simulation steps.

Cargo remains at its source until the assigned worker arrives and loads it. A haul obeys both the configured 40 kg and 60 L limits. An intact 80 kg stored worker self-relocates under its own ID rather than becoming an oversized package carried by a second worker. Storage, export, recycling, destruction and evacuation retain identity records. The 160 L packed berth and 90% material recovery are editable prototype rules. Destroying a pickup source cancels uncollected reservations without moving surviving workers; already carried cargo stays with its carrier.

The six dedicated worker tests passed in `Saved/Automation/v09-integration-12`: hatch departure/motion, physical pickup/capacity/atomic save, manufacture/recycling/self-relocation, completed hauls joining paid road construction, spare core operators reaching construction, and preserving the final on-site core operator. That integration run records 21 clean passes and two developed-AI failures. These bounded results do not establish Shipping verification or final visual acceptance; later source changes still require reruns.

`Tools/render_orbital_ui_v09.py` renders transparent portraits from actual building meshes and a larger shuttle portrait for the menu. The wall portrait recreates the runtime wall geometry using its rule dimensions. `UI/portraits.json` identifies each source. These are prerendered mesh portraits, not live scene-capture widgets.

This is an original prototype art pass. Panel geometry and LODs are implemented; fine surface wear, engine-plume effects, detailed tool animation, and final art-direction polish remain further work. Import success alone is not a visual acceptance test; rendered game checks and performance measurements belong in the release verification record.
