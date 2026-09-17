import struct,sys
class R:
    def __init__(s,d): s.d=d; s.p=0
    def b(s): v=s.d[s.p]; s.p+=1; return v
    def rd(s,n): v=s.d[s.p:s.p+n]; s.p+=n; return v
    def sstr(s): n=s.b(); return s.rd(n).decode('latin1')
    def i32(s): v=struct.unpack_from('<i',s.d,s.p)[0]; s.p+=4; return v
    def i16(s): v=struct.unpack_from('<h',s.d,s.p)[0]; s.p+=2; return v
def ext(b):
    m=int.from_bytes(b[:8],'little'); se=int.from_bytes(b[8:],'little')
    sign=-1 if se&0x8000 else 1; e=se&0x7fff
    if e==0 and m==0: return 0.0
    return sign*m*2.0**(e-16383-63)
def val(r):
    t=r.b()
    if t==0: return None
    if t==1:
        out=[]
        while r.d[r.p]!=0: out.append(val(r))
        r.b(); return '('+', '.join(map(repr,out))+')'
    if t==2: return struct.unpack('<b',r.rd(1))[0]
    if t==3: return r.i16()
    if t==4: return r.i32()
    if t==5: return ext(r.rd(10))
    if t==6: return repr(r.sstr())
    if t==7: return r.sstr()
    if t==8: return 'False'
    if t==9: return 'True'
    if t==10: n=r.i32(); r.rd(n); return f'<binary {n} bytes>'
    if t==11:
        out=[]
        while True:
            x=r.sstr()
            if x=='': break
            out.append(x)
        return '['+', '.join(out)+']'
    if t==12: n=r.i32(); return repr(r.rd(n).decode('latin1'))
    if t==13: return 'nil'
    if t==14:
        items=[]
        while r.d[r.p]!=0:
            if r.d[r.p]==4: r.b(); r.i32()
            props={}
            while r.d[r.p]!=0:
                k=r.sstr(); props[k]=val(r)
            r.b(); items.append(props)
        r.b(); return '<'+' '.join('item '+' '.join(f'{k}={v}' for k,v in it.items()) for it in items)+'>'
    if t==15: return struct.unpack('<f',r.rd(4))[0]
    if t==16: return r.i32()
    if t==18: n=r.i32(); return repr(r.rd(n*2).decode('utf-16le'))
    if t==19: v=struct.unpack('<q',r.rd(8))[0]; return v
    if t==20: n=r.i32(); return repr(r.rd(n).decode('utf8','replace'))
    raise Exception(f'unknown type {t} at {r.p}')
def obj(r,ind,out):
    pre=r.d[r.p]
    if pre&0xf0==0xf0:
        r.b()
        if pre&2: r.b()  # childpos
    cls=r.sstr(); name=r.sstr()
    out.append('  '*ind+f'object {name}: {cls}')
    while r.d[r.p]!=0:
        k=r.sstr(); v=val(r)
        if k in ('Font.Height','Font.Name','Font.Charset','Font.Color','Font.Style','ParentFont','TabOrder','Color','ParentColor','Ctl3D','ParentCtl3D','PixelsPerInch','TextHeight','Glyph.Data','NumGlyphs','Picture.Data','OldCreateOrder','Ctl3D','ParentShowHint','ShowHint','Cursor'): continue
        out.append('  '*ind+f'  {k} = {v}')
    r.b()
    while r.d[r.p]!=0: obj(r,ind+1,out)
    r.b()
for f in sys.argv[1:]:
    d=open(f,'rb').read(); assert d[:4]==b'TPF0'
    r=R(d); r.p=4; out=[]; obj(r,0,out)
    open(f[:-4]+'.txt','w',encoding='utf8').write('\n'.join(out))
    print(f, len(out),'lines')
