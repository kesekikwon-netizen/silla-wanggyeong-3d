import PyOpenColorIO as O, numpy as np, itertools, sys
cfg = O.Config.CreateFromFile("config.ocio")
print("ocio config loaded; scene_linear role ->", cfg.getRoleColorSpace("scene_linear"), file=sys.stderr)
dvt = O.DisplayViewTransform(src="Linear Rec.709", display="sRGB", view="AgX")
proc = cfg.getProcessor(dvt).getDefaultCPUProcessor()
vals = [0.0, 0.001, 0.01, 0.05, 0.18, 0.5, 1.0, 2.0, 5.0, 16.0]
rows=[]
for r,g,b in itertools.product(vals, repeat=3):
    rows.append((r,g,b))
# 대표 색: 하늘, 흙, 풀, 식생 어두운 곳 등 선형 값
rows += [(0.05,0.12,0.30),(0.35,0.28,0.18),(0.08,0.2,0.05),(0.9,0.85,0.7),(0.02,0.03,0.025),(3.0,2.4,1.5)]
img = np.array(rows, dtype=np.float32).reshape(-1,1,3).copy()
proc.applyRGB(img.reshape(-1,3))
out = img.reshape(-1,3)
with open("agx_reference.csv","w") as f:
    f.write("# Blender 4.2.3 config.ocio 'AgX' view (sRGB display) evaluated with OpenColorIO %s: scene-linear Rec.709 -> display sRGB (0..1)\n" % O.__version__)
    f.write("r,g,b,R,G,B\n")
    for (a,b_,c),(x,y,z) in zip(rows,out):
        f.write("%.7g,%.7g,%.7g,%.7f,%.7f,%.7f\n"%(a,b_,c,x,y,z))
print(len(rows),"rows", file=sys.stderr)
print("grey 0.18 ->", out[rows.index((0.18,0.18,0.18))], file=sys.stderr)
print("white 1.0 ->", out[rows.index((1.0,1.0,1.0))], file=sys.stderr)
print("black ->", out[0], file=sys.stderr)
