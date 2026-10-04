"""Download selected CC0 nature source models and PBR maps from Poly Haven.

Development-time only. Large unmodified source blends stay in ignored .tools;
the project retains optimized editable blends, required maps, and provenance.
"""
from pathlib import Path
import concurrent.futures, hashlib, json, re, urllib.request

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/"Art/Nature"
CACHE=ROOT/".tools/nature-downloads"
ART.mkdir(parents=True,exist_ok=True)
ASSETS=("fir_tree_01","tree_small_02","fern_02","rock_moss_set_01")
HEADERS={"User-Agent":"SEIGE local asset preparation (CC0; single developer download)"}

def request_json(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers=HEADERS),timeout=90) as response:
        return json.load(response)

def download(job):
    path,record=job
    path.parent.mkdir(parents=True,exist_ok=True)
    if not path.exists() or path.stat().st_size!=record["size"]:
        temp=path.with_suffix(path.suffix+".download")
        with urllib.request.urlopen(urllib.request.Request(record["url"],headers=HEADERS),timeout=120) as response, temp.open("wb") as output:
            while block:=response.read(1024*1024):output.write(block)
        temp.replace(path)
    digest=hashlib.sha256();md5=hashlib.md5()
    with path.open("rb") as source:
        while block:=source.read(1024*1024):digest.update(block);md5.update(block)
    if path.stat().st_size!=record["size"] or md5.hexdigest()!=record["md5"]:
        raise RuntimeError("Official checksum mismatch: "+str(path))
    return {"local_path":path.relative_to(ROOT).as_posix(),"url":record["url"],"bytes":path.stat().st_size,
        "sha256":digest.hexdigest(),"official_md5":record["md5"]}

manifest={"provider":"Poly Haven","license":"CC0-1.0","license_url":"https://polyhaven.com/license",
    "legal_code":"https://creativecommons.org/publicdomain/zero/1.0/","assets":{}}
jobs=[]
for name in ASSETS:
    files=request_json("https://api.polyhaven.com/files/"+name)
    info=request_json("https://api.polyhaven.com/info/"+name)
    data={"name":info["name"],"source_page":"https://polyhaven.com/a/"+name,"authors":info.get("authors",{}),"maps":{}}
    manifest["assets"][name]=data
    model=files["blend"]["1k"]["blend"]
    path=CACHE/name/(name+"_1k.blend")
    jobs.append((name,"source",path,model))
    for key,variants in files.items():
        normalized=key.lower()
        channel=None
        for suffix,role in (("diffuse","color"),("diff","color"),("nor_gl","normal"),("rough","roughness"),("alpha","alpha")):
            if normalized==suffix or normalized.endswith("_"+suffix):
                channel=role;group=normalized[:-len(suffix)].rstrip("_") or "surface";break
        if not channel:continue
        resolution="2k" if channel in ("color","normal","alpha") else "1k"
        formats=variants.get(resolution) or variants.get("1k")
        if not formats:raise RuntimeError("Missing map sizes "+name+"/"+key)
        extension="png" if channel=="alpha" else "jpg"
        record=formats.get(extension)
        if not record:
            extension=next(iter(formats));record=formats[extension]
        local=ART/"Textures"/name/Path(record["url"]).name
        jobs.append((name,(group,channel),local,record))

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    pending={pool.submit(download,(path,record)):(name,slot) for name,slot,path,record in jobs}
    for future in concurrent.futures.as_completed(pending):
        name,slot=pending[future];result=future.result()
        if slot=="source":manifest["assets"][name]["original_source"]=result
        else:
            group,channel=slot;manifest["assets"][name]["maps"].setdefault(group,{})[channel]=result
        print("DOWNLOADED "+result["local_path"]+" "+str(result["bytes"]),flush=True)

(ART/"sources.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
print("NATURE_DOWNLOAD_COMPLETE "+str(len(jobs))+" verified files",flush=True)
