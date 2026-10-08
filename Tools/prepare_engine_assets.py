"""Recreate local-only Epic assets from the developer's licensed UE installation.

Run by build.ps1 before cooking when the ignored cubemap is missing. This does
not rebuild project materials or copy any reference-game or private assets.
"""
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[1]
weather = json.loads((root / "Graphics/weather.json").read_text())
destination = weather["ambient_cubemap"].split(".")[0]
if destination != "/Game/Art/WeatherV09/T_AmbientDaylight":
    raise RuntimeError("Unrecognized generated ambient cubemap; review the source recipe")
source = "/Engine/MapTemplates/Sky/DaylightAmbientCubemap"
library = unreal.EditorAssetLibrary
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(
    ["/Engine/MapTemplates/Sky"], True
)
if not library.does_asset_exist(destination):
    if not isinstance(library.load_asset(source), unreal.TextureCube):
        raise RuntimeError("The licensed engine installation is missing " + source)
    library.make_directory("/Game/Art/WeatherV09")
    if not library.duplicate_asset(source, destination):
        raise RuntimeError("Could not create local ambient cubemap")
asset = library.load_asset(destination)
if not isinstance(asset, unreal.TextureCube) or not library.save_loaded_asset(asset, False):
    raise RuntimeError("Could not validate/save local ambient cubemap")
print("SEIGE_LOCAL_ENGINE_ASSETS_READY")
