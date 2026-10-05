"""Incremental mixed-height sward import plus measured terrain contrast update."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
script=ROOT/'Tools/import_environment_v04.py'
exec(compile(script.read_text(),str(script),'exec'),{'__file__':str(script),'__name__':'__main__','MEADOW_ONLY':True})
for name in ('import_terrain_v04.py','verify_environment_v04.py'):
    script=ROOT/'Tools'/name
    exec(compile(script.read_text(),str(script),'exec'),{'__file__':str(script),'__name__':'__main__'})
