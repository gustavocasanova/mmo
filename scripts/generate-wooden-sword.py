"""Generate our original low-poly training sword. No external assets or packages.
Run from any directory with Python 3. Coordinates are meters, +Y along the blade;
the origin is the middle of the grip (the attachment pivot).
"""
from pathlib import Path
import json
import math
import struct

OUT = Path(__file__).resolve().parents[1] / 'content/items/wooden_sword'
PALETTE = [
    ('Oak face', (0.65, 0.38, 0.16, 1)),
    ('Fresh bevel', (0.88, 0.61, 0.31, 1)),
    ('End grain', (0.47, 0.25, 0.09, 1)),
    ('Walnut hilt', (0.29, 0.13, 0.045, 1)),
    ('Carved grip', (0.42, 0.22, 0.085, 1)),
    ('Wood grain', (0.49, 0.265, 0.10, 1)),
]
triangles = [[] for _ in PALETTE]

def triangle(a, b, c, material):
    u = [b[i]-a[i] for i in range(3)]
    v = [c[i]-a[i] for i in range(3)]
    n = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
    length = math.sqrt(sum(x*x for x in n))
    if length < 1e-10:
        return
    n = tuple(x/length for x in n)
    triangles[material].extend([(tuple(p), n) for p in (a,b,c)])

def quad(a,b,c,d,material):
    triangle(a,b,c,material); triangle(a,c,d,material)

# Octagonal section with broad, flat faces and bevelled corners.
def section(y, width, depth):
    return [(x,y,z) for x,z in [(width*.72,depth),(-width*.72,depth),
        (-width,depth*.5),(-width,-depth*.5),(-width*.72,-depth),
        (width*.72,-depth),(width,-depth*.5),(width,depth*.5)]]

def solid(rings, materials):
    for first,second in zip(rings,rings[1:]):
        for i in range(8):
            j=(i+1)%8
            quad(first[i],second[i],second[j],first[j],materials[i])
    for i in range(1,7):
        triangle(rings[0][0],rings[0][i],rings[0][i+1],materials[0])
        triangle(rings[-1][0],rings[-1][i+1],rings[-1][i],2)

solid([section(.125,.054,.022),section(.69,.052,.022),section(.835,.012,.012)],
      [0,1,2,1,0,1,2,1])
solid([section(-.125,.026,.026),section(.09,.026,.026)],[4]*8)
solid([section(.09,.145,.036),section(.13,.145,.036)],[3]*8)
solid([section(-.16,.034,.032),section(-.125,.038,.032)],[3]*8)
# Raised carved rings, deliberately wooden rather than metal fittings.
for y in [-.095,-.055,-.015,.025,.065]:
    solid([section(y,.0275,.0275),section(y+.009,.0275,.0275)],[3]*8)
# Three tapered grain lines on both broad blade faces; slight offset avoids z fighting.
for z,reverse in [(.0223,False),(-.0223,True)]:
    for x in [-.023,0,.021]:
        a=(x-.001,.20,z);b=(x+.001,.20,z);c=(x+.004,.64,z);d=(x+.001,.66,z)
        if reverse: quad(d,c,b,a,5)
        else: quad(a,b,c,d,5)

blob=bytearray(); views=[]; accessors=[]; primitives=[]
def accessor(values):
    offset=len(blob)
    for value in values: blob.extend(struct.pack('<3f',*value))
    views.append({'buffer':0,'byteOffset':offset,'byteLength':len(blob)-offset,'target':34962})
    accessors.append({'bufferView':len(views)-1,'componentType':5126,'count':len(values),
        'type':'VEC3','min':[min(v[i] for v in values) for i in range(3)],
        'max':[max(v[i] for v in values) for i in range(3)]})
    return len(accessors)-1
for material,vertices in enumerate(triangles):
    if not vertices: continue
    primitives.append({'attributes':{'POSITION':accessor([v[0] for v in vertices]),
        'NORMAL':accessor([v[1] for v in vertices])},'material':material,'mode':4})
document={'asset':{'version':'2.0','generator':'MMO wooden training sword generator'},
    'scene':0,'scenes':[{'nodes':[0]}],
    'nodes':[{'name':'WoodenSword_GripPivot','mesh':0}],
    'meshes':[{'name':'Wooden training sword','primitives':primitives}],
    'materials':[{'name':name,'pbrMetallicRoughness':{'baseColorFactor':color,
        'metallicFactor':0,'roughnessFactor':.95}} for name,color in PALETTE],
    'buffers':[{'uri':'wooden_sword.bin','byteLength':len(blob)}],
    'bufferViews':views,'accessors':accessors}
OUT.mkdir(parents=True,exist_ok=True)
(OUT/'wooden_sword.bin').write_bytes(blob)
(OUT/'wooden_sword.gltf').write_text(json.dumps(document,indent=2)+'\n')
print(f'Generated {sum(len(t) for t in triangles)//3} triangles at {OUT}')
