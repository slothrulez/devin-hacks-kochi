import glob, os, colorsys, shutil, sys
from PIL import Image
SRC='data/ryu'
def shade(rgb, s_mul=1.0, v_mul=1.0, hue=None, s_set=None):
    r,g,b=[c/255 for c in rgb]; h,s,v=colorsys.rgb_to_hsv(r,g,b)
    if hue is not None: h=hue
    if s_set is not None: s=s_set
    s=min(1,s*s_mul); v=min(1,v*v_mul)
    return tuple(int(round(c*255)) for c in colorsys.hsv_to_rgb(h,s,v))
def classify(rgb):
    r,g,b=rgb
    if (r,g,b)==(0,0,0): return 'bg'
    h,s,v=colorsys.rgb_to_hsv(r/255,g/255,b/255); hd=h*360
    if v>0.45 and s<0.33 and r>120: return 'gi'
    if 15<=hd<=40 and s>=0.33 and v>0.35: return 'skin'
    if (hd<12 or hd>340) and s>0.5 and v>0.4: return 'red'
    return 'other'
# style: dict class -> fn(rgb)
STYLES={
 'p1': {'gi': lambda c: shade(c, hue=42/360, s_set=0.14, v_mul=1.0),          # kasavu cream
        'skin': lambda c: shade(c, s_mul=1.15, v_mul=0.72),                    # deeper brown
        'red':  lambda c: shade(c, hue=45/360, s_mul=1.0, v_mul=1.05)},        # gold headband/gloves
 'p2': {'gi': lambda c: shade(c, hue=145/360, s_set=0.72, v_mul=0.50),         # deep green
        'skin': lambda c: shade(c, s_mul=1.2, v_mul=0.62),                     # darker brown
        'red':  lambda c: shade(c, hue=0/360, s_mul=1.0, v_mul=1.0)},          # red stays
}
def recolor(im, style):
    im=im.convert('RGB'); px=im.load(); cache={}
    for y in range(im.height):
        for x in range(im.width):
            c=px[x,y]
            if c not in cache:
                k=classify(c); cache[c]=STYLES[style][k](c) if k in STYLES[style] else c
            px[x,y]=cache[c]
    return im
if __name__=='__main__':
    mode=sys.argv[1]
    frames=sorted(glob.glob(f'{SRC}/*.png'))
    if mode=='preview':
        names=['idle00.png','1punch00.png','kick00.png','fireball00.png']
        names=[n for n in names if os.path.exists(f'{SRC}/{n}')]
        out=Image.new('RGB',(128*len(names),128*3))
        for i,n in enumerate(names):
            im=Image.open(f'{SRC}/{n}')
            out.paste(im.convert('RGB'),(128*i,0)); out.paste(recolor(im,'p1'),(128*i,128)); out.paste(recolor(im,'p2'),(128*i,256))
        out=out.resize((out.width*3,out.height*3),Image.NEAREST); out.save('/tmp/fighters_preview.png'); print('preview', names)
    else:
        for style in ('p1','p2'):
            d=f'data/{style}'; os.makedirs(d,exist_ok=True)
            for f in frames: recolor(Image.open(f),style).save(f'{d}/{os.path.basename(f)}')
            y=open(f'{SRC}/ryu.yml').read().replace('data/ryu/',f'data/{style}/').replace('name: ryu',f'name: {style}')
            open(f'{d}/{style}.yml','w').write(y)
        print('wrote', len(frames), 'frames x2')
