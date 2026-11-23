from pathlib import Path
import json,hashlib,collections,argparse
from PIL import Image,ImageFont,ImageDraw
parser=argparse.ArgumentParser();parser.add_argument('--rom',type=Path,required=True);parser.add_argument('--font-dir',type=Path,required=True);parser.add_argument('--out',type=Path,required=True);args=parser.parse_args()
rom=args.rom.read_bytes();assert hashlib.sha1(rom).hexdigest()=='ff29faf0da37332e77524236f6966633ec51139d'
a='АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя«»—–№…'
faces=['NotoSans-Regular','NotoSans-SemiCondensed','NotoSans-Condensed','NotoSans-ExtraCondensed']
pins=json.loads((Path(__file__).resolve().parents[1]/'translation/readable-font-sources.json').read_text())
for face in faces:
 assert hashlib.sha256((args.font_dir/(face+'.ttf')).read_bytes()).hexdigest()==pins[face],face+' source checksum changed'
rows=[];allbytes=[];widths=[]
for profile in range(2):
 for i,ch in enumerate(a):
  advance=rom[0x1067b80+3175+profile*len(a)+i];allowed=advance-1
  if i >= 66:
   start=0x1000000+(3175+profile*len(a)+i)*128;data=rom[start:start+128]
   values=[(v>>s)&15 for v in data for s in (0,4)]
   bits=[sum(1<<x for x in range(16) if values[(x//8)*128+y*8+x%8]==3)for y in range(16)]
   allbytes.extend(data);widths.append(advance)
   rows.append({'character':ch,'profile':profile,'advance':advance,'face':'PT Sans punctuation retained','size':0,'body':bits})
   continue
  candidates=faces if profile==0 else faces[-1:]
  selected=None
  for size in [13,12,11]:
   for face in candidates:
    font=ImageFont.truetype(str(args.font_dir/(face+'.ttf')),size)
    im=Image.new('1',(20,20));ImageDraw.Draw(im).text((0,12-font.getmetrics()[0]),ch,font=font,fill=1)
    box=im.getbbox()
    if box and box[2]-box[0]<=allowed and box[3]<=15:
     selected=(im,box,face,size);break
   if selected:break
  assert selected,(ascii(ch),profile,advance)
  im,box,face,size=selected;body=Image.new('1',(16,16));body.paste(im.crop((box[0],0,box[2],16)),(0,0))
  bwidth=box[2]-box[0];dx=(advance-1-bwidth)//2
  if dx:body=Image.new('1',(16,16));body.paste(im.crop((box[0],0,box[2],16)),(dx,0))
  if size==11:
   # Keep the common cap/x-height when a narrow glyph needs the smaller hint.
   first,top,height=(4,3,9)if i<33 else(6,5,7)
   main=body.crop((0,first,16,12)).resize((16,height),Image.Resampling.NEAREST)
   tail=body.crop((0,12,16,16));body=Image.new('1',(16,16));body.paste(main,(0,top));body.paste(tail,(0,12))
  pixels=[[3 if body.getpixel((x,y)) else 2 if x and y and body.getpixel((x-1,y-1)) else 0 for x in range(16)]for y in range(16)]
  flat=[pixels[y][tile*8+x]for tile in range(2)for y in range(16)for x in range(8)]
  data=bytes(flat[j]|flat[j+1]<<4 for j in range(0,256,2));allbytes.extend(data);widths.append(advance)
  rows.append({'character':ch,'profile':profile,'advance':advance,'face':face,'size':size,'bbox':body.getbbox(),'body':[sum((1<<x)for x in range(16)if body.getpixel((x,y)))for y in range(16)]})
for profile in range(2):
 group=collections.defaultdict(list)
 for row in rows:
  if row['profile']==profile:group[tuple(row['body'])].append(row['character'])
 duplicates=[v for v in group.values()if len(v)>1]
 assert not duplicates,(profile,duplicates)
 print('duplicates',profile,duplicates)
p=args.out;p.mkdir(parents=True,exist_ok=True)
(p/'glyphs.bin').write_bytes(bytes(allbytes));(p/'audit.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf8')
sheet=Image.new('RGB',(16*18,40*8),'#e6e3c8');d=ImageDraw.Draw(sheet);label=ImageFont.truetype(str(args.font_dir/'NotoSans-Regular.ttf'),10)
for n,row in enumerate(rows):
 x=(n%18)*16;y=(n//18)*40
 for yy,bits in enumerate(row['body']):
  for xx in range(16):
   if bits&(1<<xx):d.point((x+xx,y+yy),fill='black')
 d.text((x,y+18),row['character'],font=label,fill='black')
sheet.resize((576,640),Image.Resampling.NEAREST).save(p/'alphabet-candidate.png')
print('sizes',collections.Counter((r['profile'],r['size'])for r in rows))


header = '#pragma once\n// Maksami UI Sans bitmap adaptation. Noto Project Authors, SIL OFL 1.1.\n// See android/payload/readability/OFL-Noto.txt. Punctuation retains PT Sans.\n#include <array>\n#include <cstdint>\nnamespace ffta::readable_font {\ninline constexpr std::uint32_t kAddress = 0x09063380u;\n'
header += 'inline constexpr std::array<std::uint8_t, 144> kWidths = {' + ','.join(map(str,widths)) + '};\n'
header += 'inline constexpr std::array<std::uint8_t, 18432> kPixels = {\n'
for start in range(0,len(allbytes),32): header += ','.join(str(v)for v in allbytes[start:start+32])+',\n'
header += '};\n} // namespace ffta::readable_font\n'
(p/'ffta_readable_glyphs.h').write_text(header,encoding='utf8')
(p/'font-sources.json').write_text(json.dumps({face:hashlib.sha256((args.font_dir/(face+'.ttf')).read_bytes()).hexdigest()for face in faces},indent=2),encoding='utf8')
