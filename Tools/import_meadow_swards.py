"""Import only the new meadow swards and original wildflowers; keep trees intact."""
from pathlib import Path
script=Path(__file__).resolve().parent/'import_environment_v04.py'
exec(compile(script.read_text(),str(script),'exec'),{'__file__':str(script),'__name__':'__main__','MEADOW_ONLY':True})
verify=script.parent/'verify_environment_v04.py'
exec(compile(verify.read_text(),str(verify),'exec'),{'__file__':str(verify),'__name__':'__main__'})
