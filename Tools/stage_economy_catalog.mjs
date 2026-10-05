// Reproducible economy catalogue proposal. Stages files; never overwrites live Rules.
import fs from 'node:fs';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..'), out=path.join(root,'Art/EconomyV08/staged');
fs.mkdirSync(out,{recursive:true});
const read=n=>JSON.parse(fs.readFileSync(path.join(root,'Rules',n+'.json'),'utf8').replace(/^\uFEFF/,''));
const write=(n,v)=>fs.writeFileSync(path.join(out,n+'.json'),JSON.stringify(v,null,2)+'\n');
const version='prototype-8.0';
const raw=[
 ['water','Water','standard','L',1,1,[.24,.62,.9]],
 ['iron_ore','Metal ore','standard','kg',1,.4,[.57,.36,.25]],
 ['silica','Silica','standard','kg',1,.65,[.76,.8,.77]],
 ['carbon','Biomass','standard','kg',1,2,[.32,.49,.25]],
 ['copper_ore','Rare metals','rare','kg',1,.3,[.7,.46,.27]],
 ['radioactive_ore','Radioactive ore','rare','kg',1,.35,[.57,.64,.27]],
 ['crystalline','Crystalline material','rare','kg',1,.5,[.54,.38,.7]],
 ['hydrocarbons','Hydrocarbons','rare','L',.85,1,[.28,.25,.22]],
];
const products=[
 ['alloy','Construction alloys',1,'kg',1,.2,'ingots',[.7,.8,.87]],
 ['conductors','Conductors',1,'kg',1,.2,'crates',[.9,.58,.25]],
 ['substrates','Industrial glass',1,'kg',1,.5,'crates',[.48,.76,.73]],
 ['fuel','Fuel',1,'L',.8,1,'crates',[.79,.59,.27]],
 ['plastic','Plastic pellets',1,'kg',1,1.5,'bulk',[.75,.76,.7]],
 ['organic_food','Organic food',1,'kg',1,1.5,'crates',[.67,.61,.32]],
 ['circuits','Control circuits',2,'kg',1,.8,'crates',[.36,.75,.45]],
 ['components','Robotic parts',2,'kg',1,.6,'crates',[.69,.58,.8]],
 ['batteries','Battery modules',2,'kg',1,.6,'crates',[.37,.63,.69]],
 ['ai_chips','AI chips',3,'kg',1,1,'crates',[.48,.57,.91]],
 ['fusion_reactors','Fusion reactor assemblies',3,'kg',1,.4,'crates',[.75,.81,.92]],
];
write('resources',{version,quantity_note:'Raw family names abstract mixed industrial feedstock. Units, bulk storage volumes and recipe yields are provisional balancing data. Credits and grid energy are not cargo resources.',resources:[...raw.map(([id,name,cls,unit,mass,volume,color])=>({id,name,class:cls,unit,unit_mass_kg:mass,litres_per_unit:volume,tier:0,color,stockpile_visual:'bulk'})),...products.map(([id,name,tier,unit,mass,volume,stockpile_visual,color])=>({id,name,class:'manufactured',unit,unit_mass_kg:mass,litres_per_unit:volume,tier,color,stockpile_visual}))]});
const recipes=[
 ['smelt_alloy',4,{iron_ore:5},{alloy:4,conductors:1},.2],
 ['draw_conductors',4,{iron_ore:5},{conductors:4,alloy:1},.2],
 ['make_substrates',4,{silica:2},{substrates:2},.12],
 ['make_circuits',5,{conductors:1,substrates:1,plastic:1},{circuits:3},.3],
 ['make_components',7,{alloy:2,conductors:1,circuits:1},{components:4},.2],
 ['refine_biofuel',8,{carbon:8,water:2},{fuel:5,plastic:6},.3],
 ['refine_hydrocarbons',8,{hydrocarbons:10},{fuel:5,plastic:4.5},.2],
 ['make_organic_food',10,{carbon:4,water:1},{organic_food:5},.15],
 ['make_batteries',12,{alloy:4,conductors:2,plastic:1,substrates:1},{batteries:8},.4],
 ['make_ai_chips',20,{circuits:2,copper_ore:.5,crystalline:.5},{ai_chips:3},1],
 ['make_fusion_reactors',30,{alloy:10,conductors:3,substrates:2,components:3,ai_chips:1,crystalline:1},{fusion_reactors:20},2],
 ['assemble_robot',60,{components:.5,circuits:.5},{},.1],
];
write('recipes',{version,balance_status:'Provisional gameplay recipes, not industrial chemistry or final balance. Physical-output recipes conserve input mass using resource unit_mass_kg; worker assembly converts its material into population. Fusion assemblies are manufactured equipment, not a claim that radioactive ore is fusion fuel.',recipes:recipes.map(([id,seconds,inputs,outputs,energy_kwh])=>({id,seconds,inputs,outputs,energy_kwh}))});
const b=read('buildings'), by=new Map(b.buildings.map(v=>[v.id,v]));
const baseExtractor=structuredClone(by.get('extract_iron_ore')), baseFactory=structuredClone(by.get('alloy_refinery')), baseDepot=structuredClone(by.get('depot'));
for(const [id,name,cls,unit,mass,volume,color] of raw){const eid='extract_'+id;const v=structuredClone(by.get(eid)??baseExtractor);Object.assign(v,{id:eid,name:name+' extractor',extract_resource:id,color,storage_capacity:160,description:`Extracts ${name.toLowerCase()} at a fixed rate from one matching deposit; requires road-grid power and local cargo storage.`});by.set(eid,v);}
for(const v of by.values()){v.next_upgrade='';v.upgrade_cost={};v.power_usage_kw=0;v.power_generation_kw=0;}
by.get('substrate_works').name='Industrial glass works';
by.get('component_works').name='Robotic parts works';
by.get('robot_service_bay').description='Charging berths and maintenance for 16 workers, supplied through the connected road power grid and courier-delivered robotic parts.';
const add=(id,name,role,visual,cost,seconds,jobs,extra={})=>{const v=structuredClone(role==='processor'?baseFactory:baseDepot);Object.assign(v,{id,name,role,visual,cost,construction_seconds:seconds,jobs,recipe:'',extract_resource:'',extract_rate:0,next_upgrade:'',upgrade_cost:{},power_usage_kw:0,power_generation_kw:0,description:name+'; quantities and timings are provisional.',...extra});by.set(id,v);};
add('solar_array','Solar array','generator','depot',{alloy:12,circuits:3},600,0,{category:'logistics',storage_capacity:100,worker_activity:'inspection',description:'Passive solar generation supplies buildings on the same connected road grid.'});
add('fuel_generator','Fuel generator','generator','factory',{alloy:16,circuits:4,components:2},900,1,{category:'logistics',storage_capacity:100,description:'Consumes delivered fuel to generate electricity on the connected road grid.'});
add('battery_bank','Grid battery bank','battery','depot',{alloy:12,circuits:3,batteries:8},900,0,{category:'logistics',storage_capacity:100,description:'Stores electricity for its connected road grid; installed battery capacity is separate from cargo storage.'});
add('trading_port','Trading port · Level 1','trade','depot',{alloy:18,circuits:5,components:2},900,1,{category:'logistics',storage_capacity:450,description:'External imports and exports settle in Galactic credits. Starts with no free credits; sell local goods to fund imports.',next_upgrade:'trading_port_2',upgrade_cost:{alloy:24,circuits:8,components:4}});
add('trading_port_2','Trading port · Level 2','trade','depot',{alloy:18,circuits:5,components:2},1800,2,{category:'logistics',storage_capacity:900,description:'Upgraded external trade capacity and cargo handling.',next_upgrade:'trading_port_3',upgrade_cost:{alloy:36,circuits:12,components:8}});
add('trading_port_3','Trading port · Level 3','trade','depot',{alloy:18,circuits:5,components:2},2700,3,{category:'logistics',storage_capacity:1500,description:'Highest current trading-port level; external credits remain separate from physical cargo.'});
for(const [id,name,recipe,seconds,cost] of [
 ['biofuel_refinery','Biomass refinery','refine_biofuel',1800,{alloy:18,circuits:5}],
 ['fuel_refinery','Hydrocarbon refinery','refine_hydrocarbons',2700,{alloy:18,circuits:5}],
 ['food_producer','Organic food producer','make_organic_food',1800,{alloy:16,circuits:4}],
 ['battery_works','Battery works','make_batteries',3600,{alloy:22,circuits:6}],
 ['ai_chip_works','AI chip works','make_ai_chips',7200,{alloy:30,circuits:10,components:4}],
 ['fusion_reactor_works','Fusion assembly works','make_fusion_reactors',10800,{alloy:40,circuits:12,components:8}],
])add(id,name,'processor','factory',cost,seconds,2,{category:'resource',recipe,storage_capacity:160,description:id==='food_producer'?'Produces organic food for Rex, the morale companion, and external export. Robotic workers do not eat food.':'Receives physical inputs and consumes grid energy per production transaction.'});
const menu=[...raw.map(r=>'extract_'+r[0]),'alloy_refinery','conductor_works','substrate_works','biofuel_refinery','fuel_refinery','food_producer','circuit_works','component_works','battery_works','ai_chip_works','fusion_reactor_works','solar_array','fuel_generator','battery_bank','trading_port','sensor','turret','depot','robot_service_bay'];
write('buildings',{...b,buildings:[...by.values()],build_menu:menu});
const scenario=read('scenario');delete scenario.scenario.deposits;
scenario.scenario.starting_inventory.organic_food=10;
Object.assign(scenario.scenario,{starting_credits:0,resource_generation:{standard_count:3,rare_count:2,inner_area_fraction:.75,minimum_separation_half_size_fraction:.12},bootstrap:{building_definitions:['solar_array','trading_port','robot_service_bay'],road_length_meters:500,extractor_count:1},quantity_note:'Starting stock is a finite carried landing kit. It covers a road-connected solar array, level-1 trading port and a local extractor before any export income; it does not grant credits.'});
write('scenario',scenario);
const ai=JSON.parse(fs.readFileSync(path.join(root,'AIFILES/colony_ai.json'),'utf8'));
ai.build_targets=[['solar_array',1],['robot_service_bay',1],...raw.map(r=>['extract_'+r[0],1]),['trading_port',1],['turret',4],['sensor',2],['robot_service_bay',2],['alloy_refinery',1],['conductor_works',1],['substrate_works',1],['biofuel_refinery',1],['circuit_works',1],['component_works',1],['depot',1],['robot_service_bay',3]].map(([definition,count])=>({definition,count}));
ai.economy={solar_definition:'solar_array',trade_definition:'trading_port',export_batch:20,import_batch:10,reserve_targets:{alloy:80,circuits:16,components:12},recipe_input_buffer_cycles:4,credit_buffer_batches:3};
write('colony_ai',ai);
const ui=JSON.parse(fs.readFileSync(path.join(root,'Interface/ui.json'),'utf8'));
const keys=[['W','M','S','K','R','U','C','H'],['A','C','G','F','H','O','E','R','T','I','V'],['R','U','S','D','C','P','E','T','G'],['T']];
const groups=[['extraction','Extraction','R','Each region contains three standard and two rare deposits. Extract only where the matching deposit exists.',raw.map(r=>'extract_'+r[0])],['production','Production','I','Physical ingredients and electricity are consumed per transaction; some recipes produce multiple goods.',menu.filter(id=>by.get(id).role==='processor')],['logistics','Logistics','L','Roads share grid power. Store electricity and physical cargo, support workers, and trade externally.',['road','upgrade_road','sensor','depot','robot_service_bay','solar_array','battery_bank','trading_port','fuel_generator']],['defense','Defense','D','Automatic defenses require staffing and operating energy.',['turret']]];
ui.build_groups=groups.map(([id,name,shortcut,description,entries],index)=>({id,name,shortcut,description,entries:entries.map((definition,i)=>({definition,shortcut:keys[index][i]}))}));
write('ui',ui);
console.log(`Staged ${by.size} building definitions, ${menu.length} buildable blueprints, 19 cargo resources and ${recipes.length} recipes in ${out}`);
