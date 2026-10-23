#!/usr/bin/env python3
"""Extract translatable UI strings (menu items, menu/window titles) from ZEsarUX."""
import os, re, sys, json
SRC=sys.argv[1]
FUNCS={
 'menu_add_item_menu_en_es_ca':(4,'item'),'menu_add_item_menu_en_es_ca_inicial':(4,'item'),
 'menu_add_item_menu_format':(4,'item'),'menu_add_item_menu_inicial_format':(4,'item'),
 'menu_add_item_menu':(1,'item'),'menu_add_item_menu_inicial':(1,'item'),
 'menu_dibuja_menu':(3,'menu title'),'menu_dibuja_menu_no_title_lang':(3,'menu title'),
 'menu_dibuja_menu_dialogo':(3,'menu title'),'menu_dibuja_menu_dialogo_no_title_lang':(3,'menu title'),
 'zxvision_new_window':(7,'window title'),'zxvision_new_window_nocheck_staticsize':(7,'window title'),
 'zxvision_new_window_gn_cim':(7,'window title'),
 'zxvision_generic_message_tooltip':(0,'window title'),'zxvision_generic_message':(0,'window title'),
 'menu_generic_message':(0,'window title'),'menu_generic_message_format':(0,'window title'),
 'menu_generic_message_splash':(0,'window title'),'menu_generic_message_warn':(0,'window title'),
 'menu_generic_message_tooltip':(0,'window title'),'zxvision_menu_generic_message_setting':(0,'window title'),
 'menu_confirm_yesno':(0,'question'),'menu_confirm_yesno_texto':(0,'window title'),
 'menu_ventana_scanf':(0,'window title'),'menu_ventana_scanf_numero_enhanced':(0,'window title'),
 'menu_get_string_language':(0,'text'),
}
name_re=re.compile(r'\b('+'|'.join(sorted(FUNCS,key=len,reverse=True))+r')\s*\(')

def strip_comments(s):
    out=[];i=0;n=len(s)
    while i<n:
        c=s[i]
        if c=='"' or c=="'":
            q=c;j=i+1
            while j<n and s[j]!=q:
                if s[j]=='\\': j+=1
                j+=1
            out.append(s[i:j+1]); i=j+1
        elif s.startswith('//',i):
            j=s.find('\n',i); j=n if j<0 else j; out.append(' '*(j-i)); i=j
        elif s.startswith('/*',i):
            j=s.find('*/',i+2); j=n if j<0 else j+2
            out.append(''.join('\n' if ch=='\n' else ' ' for ch in s[i:j])); i=j
        else: out.append(c); i+=1
    return ''.join(out)

def split_args(s,i):
    # s[i] is after '('; return list of args and end index
    depth=0;args=[];cur=[];n=len(s)
    while i<n:
        c=s[i]
        if c in '"\'':
            q=c;j=i+1
            while j<n and s[j]!=q:
                if s[j]=='\\': j+=1
                j+=1
            cur.append(s[i:j+1]); i=j+1; continue
        if c in '([{': depth+=1
        elif c in ')]}':
            if depth==0:
                args.append(''.join(cur)); return args,i
            depth-=1
        elif c==',' and depth==0:
            args.append(''.join(cur)); cur=[]; i+=1; continue
        cur.append(c); i+=1
    return args,i

lit_re=re.compile(r'^\s*((?:"(?:[^"\\]|\\.)*"\s*)+)$',re.S)
def literal(a):
    m=lit_re.match(a)
    if not m: return None
    parts=re.findall(r'"((?:[^"\\]|\\.)*)"',m.group(1))
    raw=''.join(parts)
    # decode C escapes (keep as python str of the bytes)
    b=bytearray();i=0
    while i<len(raw):
        c=raw[i]
        if c=='\\':
            n=raw[i+1]
            if n in 'nt\\"\'': b+={'n':b'\n','t':b'\t','\\':b'\\','"':b'"',"'":b"'"}[n]; i+=2
            elif n in '01234567':
                j=i+1;v=''
                while j<len(raw) and len(v)<3 and raw[j] in '01234567': v+=raw[j]; j+=1
                b.append(int(v,8)); i=j
            elif n=='x':
                j=i+2;v=''
                while j<len(raw) and raw[j] in '0123456789abcdefABCDEF': v+=raw[j]; j+=1
                b.append(int(v,16)); i=j
            else: b+=n.encode(); i+=2
        else:
            b+=c.encode('utf-8'); i+=1
    return b.decode('utf-8','replace')

found={}
for root,dirs,files in os.walk(SRC):
    if 'third_party' in root: continue
    for fn in files:
        if not fn.endswith('.c'): continue
        path=os.path.join(root,fn)
        s=strip_comments(open(path,encoding='utf-8',errors='replace').read())
        for m in name_re.finditer(s):
            # skip definitions/declarations
            pre=s[max(0,m.start()-30):m.start()]
            if re.search(r'(void|int|char \*|z80_byte)\s*$',pre): continue
            idx,kind=FUNCS[m.group(1)]
            args,_=split_args(s,m.end())
            if len(args)<=idx: continue
            t=literal(args[idx])
            if t is None: continue
            extra={}
            if m.group(1).startswith('menu_add_item_menu_en_es_ca') and len(args)>6:
                es=literal(args[5]); extra['es']=es or ''
            key=t
            if not re.search(r'[A-Za-z]{2}',key): continue
            line=s.count('\n',0,m.start())+1
            rel=os.path.relpath(path,SRC)
            e=found.setdefault(key,{'kind':kind,'where':[],'es':''})
            if len(e['where'])<3: e['where'].append('%s:%d'%(rel,line))
            if extra.get('es'): e['es']=extra['es']
# strings_language_list
s=open(os.path.join(SRC,'zxvision/zxvision.c'),encoding='utf-8').read()
a=s.index('struct s_strings_language_list strings_language_list[]={'); b=s.index('};',a)
for m in re.finditer(r'\{\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"',s[a:b]):
    e=found.setdefault(m.group(1),{'kind':'text','where':['zxvision/zxvision.c (strings_language_list)'],'es':''})
    e['es']=e['es'] or m.group(2)
json.dump(found,open(sys.argv[2],'w'),ensure_ascii=False,indent=0)
from collections import Counter
print(len(found), Counter(v['kind'] for v in found.values()))
