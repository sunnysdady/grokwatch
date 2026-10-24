import glob,os
from PIL import Image,ImageDraw,ImageFont
fn=ImageFont.truetype(next(iter(glob.glob('/usr/share/fonts/**/*CJK*.tt*',recursive=True)+glob.glob('/usr/share/fonts/**/*.ttf',recursive=True))),16)
out=[]
for p in sorted(glob.glob('raw/*.ppm')):
    im=Image.open(p).convert('RGB'); m=Image.new('L',im.size,0); ImageDraw.Draw(m).ellipse((0,0,465,465),fill=255)
    o=Image.new('RGB',im.size,(0,0,0)); o.paste(im,(0,0),m); ImageDraw.Draw(o).ellipse((0,0,465,465),outline=(70,70,70),width=1)
    n=os.path.basename(p)[:-4]+'.png'; o.save(n); out.append(n)
# pair day/night per page
names=sorted({n.rsplit('_',1)[0] for n in out}); t=220; cols=6; import re
rows=(len(names)*2+cols-1)//cols; S=Image.new('RGB',(cols*(t+12)+12,rows*(t+40)+12),(20,20,20)); D=ImageDraw.Draw(S); i=0
for nm in names:
  for v in ('day','night'):
    x=12+(i%cols)*(t+12); y=12+(i//cols)*(t+40); S.paste(Image.open(f'{nm}_{v}.png').resize((t,t),Image.LANCZOS),(x,y)); D.text((x,y+t+4),(re.sub(r'^\d+[a-z]?_','',nm).replace('standby_glyphs_','glyph ')[:20]+(' 日' if v=='day' else ' 夜')),fill=(200,200,200),font=fn); i+=1
S.save('contact_sheet.png'); print(len(out),S.size)
