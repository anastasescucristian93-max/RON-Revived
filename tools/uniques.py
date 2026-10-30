import re, sys, xml.etree.ElementTree as ET
p = sys.argv[1]
txt = open(p, encoding='latin-1').read()
units = {}
for m in re.finditer(r'<UNIT>(.*?)</UNIT>', txt, re.S):
    d = dict(re.findall(r'<([A-Z_0-9]+)>([^<]*)</\1>', m.group(1)))
    units[d.get('NAME','?').strip()] = d
TRIBES = ["Aztecs","Maya","Inca","Bantu","Nubians","Greeks","Romans","Egyptians","Turks","Spanish","French","British","Germans","Russians","Chinese","Japanese","Koreans","Mongols","Iroquois","Lakota","Americans","Indians","Dutch","Persians"]
def num(s, d=0):
    m = re.search(r'-?\d+', s or ''); return int(m.group()) if m else d
def cost(s):
    return sum(int(x) for x in re.findall(r'(\d+)[a-z]', s or ''))
def val(u):
    a, h, ar, rc = num(u.get('ATTACK')), num(u.get('HITS')), num(u.get('ARMOR')), max(num(u.get('RECHARGE'),1),1)
    return a, h, ar, rc
rows=[]
for n,u in units.items():
    mask = (u.get('TRIBE_MASK') or '').strip()
    g = (u.get('GRAFT') or '').strip()
    if not mask or '1' not in mask or mask.count('1')>6 or g in ('','none') or g not in units: continue
    tribes=[TRIBES[len(mask)-1-i] for i,c in enumerate(mask) if c=='1' and len(mask)-1-i < len(TRIBES)]
    b=units[g]
    a1,h1,ar1,r1=val(u); a0,h0,ar0,r0=val(b)
    c1,c0=cost(u.get('COST')),cost(b.get('COST'))
    t1,t0=num(u.get('JOB_TIME')),num(b.get('JOB_TIME'))
    # power ~ dps * hp; per cost
    p1=(a1/r1)*h1; p0=(a0/r0)*h0 if a0 and h0 else 0
    ratio = p1/p0 if p0 else float('nan')
    pc = (p1/c1)/(p0/c0) if p0 and c1 and c0 else float('nan')
    rows.append((','.join(tribes), n, g, a1,a0,h1,h0,ar1,ar0,r1,r0,u.get('RANGE','').strip(),b.get('RANGE','').strip(),u.get('MOVES'),b.get('MOVES'),c1,c0,t1,t0,ratio,pc))
rows.sort()
print("tribe|unique|base|atk|hp|arm|rech|range|moves|cost|time|power x|power/cost x")
for r in rows:
    print(f"{r[0]}|{r[1]}|{r[2]}|{r[3]}/{r[4]}|{r[5]}/{r[6]}|{r[7]}/{r[8]}|{r[9]}/{r[10]}|{r[11]}/{r[12]}|{r[13]}/{r[14]}|{r[15]}/{r[16]}|{r[17]}/{r[18]}|{r[19]:.2f}|{r[20]:.2f}")
