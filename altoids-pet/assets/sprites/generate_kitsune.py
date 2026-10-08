#!/usr/bin/env python3
"""Author native 48x48 role maps and separate 1-bit gear; standard library only.
Run from any directory. Writes the dedicated firmware asset modules.
Roles: . transparent, O outline, F fur, A accent, D detail. No resampling.
"""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2] / 'firmware' / 'altoids_pet'
W = 48

def grid(): return [[0]*W for _ in range(W)]
def poly(g, points, role):
    # Rasterize integer pixel centers; no antialiasing or interpolated pixels.
    for y in range(W):
        for x in range(W):
            px,py=x+.5,y+.5;inside=False
            for i,(ax,ay) in enumerate(points):
                bx,by=points[(i+1)%len(points)]
                if (ay>py)!=(by>py) and px < (bx-ax)*(py-ay)/(by-ay)+ax:inside=not inside
            if inside:g[y][x]=role

def rect(g,x,y,w,h,r):
    for Y in range(y,y+h):
        for X in range(x,x+w):
            if 0<=X<W and 0<=Y<W:g[Y][X]=r

def line(g,x0,y0,x1,y1,r):
    dx=abs(x1-x0);sx=1 if x0<x1 else -1;dy=-abs(y1-y0);sy=1 if y0<y1 else -1;err=dx+dy
    while True:
        if 0<=x0<W and 0<=y0<W:g[y0][x0]=r
        if x0==x1 and y0==y1:break
        e=2*err
        if e>=dy:err+=dy;x0+=sx
        if e<=dx:err+=dx;y0+=sy

def shape(g,points,fill=2,outline=1):
    mask=grid();poly(mask,points,1)
    for y in range(W):
        for x in range(W):
            if mask[y][x]:
                edge=any(not (0<=x+dx<W and 0<=y+dy<W) or not mask[y+dy][x+dx] for dx,dy in ((-1,0),(1,0),(0,-1),(0,1)))
                g[y][x]=outline if edge else fill

def kitsune(state):
    g=grid()
    # Tail curls up behind the body, with one stepped, light tip and curl crease.
    shape(g,[(27,34),(33,33),(39,29),(40,25),(37,22),(39,18),(44,20),(47,25),(47,31),(44,37),(39,41),(30,43),(26,39)])
    poly(g,[(39,20),(43,22),(45,26),(43,29),(40,27),(40,24),(38,22)],3)
    line(g,34,37,40,35,1);line(g,40,35,42,31,1)
    shape(g,[(13,28),(29,28),(33,33),(34,39),(31,44),(11,44),(8,40),(9,34)])
    feetY=42 if state=='BOUNCE' else 43
    shape(g,[(10,feetY),(19,feetY),(19,feetY+4),(10,feetY+4)],3)
    shape(g,[(24,feetY),(33,feetY),(33,feetY+4),(24,feetY+4)],3)
    # Large pointed ears, not infant proportions; open space remains between tips.
    shape(g,[(8,3),(11,2),(19,14),(9,19),(6,12)])
    shape(g,[(30,14),(34,2),(37,3),(40,13),(37,19)])
    poly(g,[(10,6),(15,13),(10,14),(9,10)],3)
    poly(g,[(35,6),(37,12),(35,15),(32,13)],3)
    shape(g,[(11,12),(19,11),(26,11),(34,13),(38,18),(38,23),(35,28),(30,31),(13,31),(8,28),(5,24),(5,19),(8,14)])
    poly(g,[(9,24),(15,23),(21,25),(27,23),(34,24),(32,28),(27,30),(15,30),(10,27)],3)
    poly(g,[(17,32),(25,32),(27,35),(23,38),(20,39),(16,35)],3)
    # Two quiet paws, with a slightly inward reading pose.
    if state=='FOCUS':
        line(g,12,34,16,37,1);line(g,16,37,18,37,1)
        line(g,30,34,26,37,1);line(g,26,37,24,37,1)
    else:
        line(g,12,34,12,37,1);line(g,12,37,15,38,1)
        line(g,30,34,30,37,1);line(g,30,37,27,38,1)
    eyeX=[13,28];eyeY=20
    if state=='LOOK_LEFT':eyeX=[11,26]
    if state=='LOOK_RIGHT':eyeX=[15,30]
    if state=='LOOK_UP':eyeY=18
    if state in ('BLINK','SLEEP'):
        for x in [12,27]:line(g,x,22,x+3,22,4)
    elif state=='HAPPY':
        for x in [12,27]:line(g,x,21,x+1,20,4);line(g,x+1,20,x+3,21,4)
    elif state in ('SLEEPY','FOCUS'):
        for x in [12,27]:line(g,x,21,x+3,21,4);rect(g,x+1,22,2,1,4)
    else:
        for x in eyeX:rect(g,x,eyeY,2,4 if state=='EXCITED' else 3,4)
    rect(g,21,25,2,1,4)
    if state=='EXCITED':shape(g,[(21,27),(24,27),(24,30),(21,30)],4,4)
    elif state=='SLEEP':line(g,21,28,23,28,4)
    else:line(g,20,27,21,28,4);line(g,21,28,23,27,4)
    return g

STATES=['IDLE','BLINK','LOOK_LEFT','LOOK_RIGHT','HAPPY','EXCITED','SLEEPY','SLEEP','BOUNCE','LOOK_UP','FOCUS']

def asset(name,g,roles=True):
    out=f'const uint8_t {name}[{"KITSUNE_FRAME_BYTES" if roles else "GEAR_BITMAP_BYTES"}] PROGMEM = {{\n'
    chars='.OFAD'
    for row in g:
        data=[]
        if roles:
            data=[row[x]<<4 | row[x+1] for x in range(0,W,2)]
        else:
            for x in range(0,W,8):data.append(sum((1 if row[x+b] else 0)<<(7-b) for b in range(8)))
        out+='  '+', '.join(f'0x{v:02X}' for v in data)+', // '+''.join(chars[v] if roles else '#' if v else '.' for v in row)+'\n'
    return out+'};\n\n'

text='#include "sprites.h"\n\n// Generated native 48x48 role maps. Two pixels per byte, left pixel in high nibble.\n// Edit a frame here, or use assets/sprites/generate_kitsune.py to regenerate.\n// Row key: . transparent, O outline, F primary fur, A accent, D facial detail.\n\n'
for state in STATES:text+=asset('KITSUNE_'+state,kitsune(state))
(ROOT/'kitsune_assets.cpp').write_text(text)

# Separate 48x48 accessory canvases; masks clear only covered geometry.
gears={}
def outlined(points):
    mask=grid();poly(mask,points,1);art=grid();shape(art,points,0,1);return art,mask
art,mask=outlined([(12,9),(29,9),(33,13),(33,15),(10,15),(10,12)])
rect(art,9,15,27,2,1);rect(mask,9,15,27,2,1);rect(art,20,11,3,2,1);gears['FIELD_CAP']=(art,mask)
art=grid()
for x in (9,24):
    line(art,x,18,x+10,18,1);line(art,x,25,x+10,25,1);line(art,x,18,x,25,1);line(art,x+10,18,x+10,25,1)
line(art,19,21,24,21,1);line(art,6,20,9,20,1);line(art,34,20,37,20,1);gears['SUNGLASSES']=(art,None)
art,mask=outlined([(0,13),(4,7),(10,2),(16,0),(23,3),(29,7),(33,13)])
line(art,0,12,32,12,1);line(art,16,13,16,41,1);line(art,16,41,13,44,1);line(art,13,44,11,41,1)
rect(mask,15,12,3,30,1);rect(mask,11,40,6,5,1);gears['UMBRELLA']=(art,mask)
for name in ['RAINCOAT','WINTER_COAT']:
    art,mask=outlined([(12,30),(29,30),(33,34),(34,39),(31,44),(11,44),(8,39),(9,34)])
    line(art,21,31,21,43,1);line(art,12,33,16,35,1);line(art,30,33,26,35,1)
    for y in [34,38,41]:rect(art,23,y,1,1,1)
    if name=='WINTER_COAT':
        line(art,12,31,29,31,1);line(art,11,42,30,42,1)
        rect(art,12,32,3,1,1);rect(art,27,32,3,1,1)
    gears[name]=(art,mask)
art,mask=outlined([(10,28),(33,28),(33,32),(10,32)])
rect(mask,28,31,5,9,1);line(art,28,32,28,39,1);line(art,32,32,32,39,1);line(art,28,39,32,39,1)
line(art,11,30,31,30,1);gears['WINTER_SCARF']=(art,mask)
art=grid();mask=grid()
for x in (10,24):
    a,m=outlined([(x,42),(x+9,42),(x+9,47),(x,47)])
    for y in range(W):
        for X in range(W):art[y][X]|=a[y][X];mask[y][X]|=m[y][X]
    line(art,x+1,44,x+7,44,1)
gears['BOOTS']=(art,mask)
text='#include "gear_sprites.h"\n\n// Separate native 48x48, MSB-first 1-bit clothing/prop layers.\n// Generated by assets/sprites/generate_kitsune.py; no copied pet pixels.\n\n'
for name,(art,mask) in gears.items():
    text+=asset('GEAR_'+name,art,False)
    if mask is not None:text+=asset('GEAR_'+name+'_MASK',mask,False)
(ROOT/'gear_sprites.cpp').write_text(text)
if __name__=='__main__':print('Wrote native 48x48 kitsune role frames and separate gear masks.')
