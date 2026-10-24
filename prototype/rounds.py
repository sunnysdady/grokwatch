import glob,colorsys
from PIL import Image,ImageDraw,ImageFont
fn=ImageFont.truetype('/usr/share/fonts/opentype/noto/NotoSerifCJK-Bold.ttc',18)
def mask(p):
    im=Image.open(p).convert('RGB'); m=Image.new('L',im.size,0); ImageDraw.Draw(m).ellipse((0,0,465,465),fill=255)
    o=Image.new('RGB',im.size,(0,0,0)); o.paste(im,(0,0),m); ImageDraw.Draw(o).ellipse((0,0,465,465),outline=(90,90,90),width=1); return o
pages=['ride','nav','nav_turn_now','control_center']; t=240
rows=[('r0','day','v2'),('r1','day','R1'),('r2','day','R2'),('r3','day','R3'),('r4','day','R4'),('r5','day','R5 白天'),('r5','night','R5 夜间')]
ref=Image.open('/workspace/homeview-study/crop_modular_green.png').convert('RGB'); ref.thumbnail((t,t))
S=Image.new('RGB',(90+5*(t+12),len(rows)*(t+12)+12),(22,22,22)); D=ImageDraw.Draw(S)
for j,(r,v,lab) in enumerate(rows):
  y=12+j*(t+12); D.text((10,y+t//2),lab,fill=(220,220,220),font=fn)
  for i,p in enumerate(pages):
    im=mask(f'raw/{r}_{p}_{v}.ppm'); im.save(f'{r}_{p}_{v}.png'); S.paste(im.resize((t,t),Image.LANCZOS),(90+i*(t+12),y))
  S.paste(ref,(90+4*(t+12),y))
S.save('rounds.png')
# metrics
def bg(p):
    im=Image.open(p).convert('RGB'); px=[im.getpixel((233,40)),im.getpixel((233,233+0)),im.getpixel((233,420))]
    return [ (lambda h,l,s:'S%.2f L%.2f'%(s,l))(*colorsys.rgb_to_hls(*[x/255 for x in q])) for q in px]
for r,v,_ in rows: print(r,v,'ride bg top/mid/bot',bg(f'{r}_ride_{v}.png'))
