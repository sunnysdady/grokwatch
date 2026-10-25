import glob,os,sys
from PIL import Image
O=sys.argv[1]
shots=sorted(glob.glob(f'{O}/[0-9]*.ppm'))
for p in shots: Image.open(p).save(p[:-4]+'.png'); os.remove(p)
pngs=sorted(glob.glob(f'{O}/[0-9]*.png')); cols=4; t=233
rows=(len(pngs)+cols-1)//cols; sheet=Image.new('RGB',(cols*(t+10)+10,rows*(t+10)+10),(18,18,18))
for i,p in enumerate(pngs): sheet.paste(Image.open(p).resize((t,t),Image.LANCZOS),(10+(i%cols)*(t+10),10+(i//cols)*(t+10)))
sheet.save(f'{O}/contact_sheet.png')
for tag in ['listening','thinking','pages','sb_enter','sb_exit']:
    fr=sorted(glob.glob(f'{O}/gif_{tag}_*.ppm'))
    if not fr: continue
    ims=[Image.open(x).resize((300,300),Image.LANCZOS).convert('P',palette=Image.ADAPTIVE) for x in fr]
    ims[0].save(f'{O}/{tag}.gif',save_all=True,append_images=ims[1:],duration=(40 if tag.startswith('sb_') else 33),loop=0)
    for x in fr: os.remove(x)
print(len(pngs))
