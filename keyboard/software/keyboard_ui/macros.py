"""Validated, ordered touchscreen macros. No shell evaluation."""
import re
KEYS={chr(65+i):4+i for i in range(26)}
KEYS.update({str(i):29+i for i in range(1,10)});KEYS['0']=39
KEYS.update({'ENTER':40,'ESC':41,'BACKSPACE':42,'TAB':43,'SPACE':44,'MINUS':45,
'EQUAL':46,'LEFTBRACKET':47,'RIGHTBRACKET':48,'BACKSLASH':49,'SEMICOLON':51,
'QUOTE':52,'GRAVE':53,'COMMA':54,'DOT':55,'SLASH':56,'CAPSLOCK':57,
'PRINTSCREEN':70,'SCROLLLOCK':71,'PAUSE':72,'INSERT':73,'HOME':74,'PAGEUP':75,
'DELETE':76,'END':77,'PAGEDOWN':78,'RIGHT':79,'LEFT':80,'DOWN':81,'UP':82,
'NUMLOCK':83,'MENU':101})
KEYS.update({f'F{i}':57+i if i<=12 else 104+i-13 for i in range(1,25)})
MODS={'CTRL':1,'CONTROL':1,'SHIFT':2,'ALT':4,'GUI':8,'SUPER':8,'WIN':8,'CMD':8}
BASE="1234567890-=[]\\;'`,./"
SHIFT="!@#$%^&*()_+{}|:\"~<>?"
PUNCT=dict(zip(BASE,[30,31,32,33,34,35,36,37,38,39,45,46,47,48,49,51,52,53,54,55,56]))

def chord(value):
    if not isinstance(value,str):raise ValueError('Shortcut must be text')
    parts=value.upper().replace(' ','').split('+');mods=0
    for part in parts[:-1]:
        if part not in MODS:raise ValueError(f'Unknown modifier: {part}')
        mods|=MODS[part]
    if parts[-1] not in KEYS:raise ValueError(f'Unknown key: {parts[-1]}')
    return (mods<<8)|KEYS[parts[-1]]

def text_codes(value):
    result=[]
    for char in value:
        if 'a'<=char<='z':code=ord(char)-97+4
        elif 'A'<=char<='Z':code=0x200|ord(char)-65+4
        elif char in PUNCT:code=PUNCT[char]
        elif char in SHIFT:code=0x200|PUNCT[BASE[SHIFT.index(char)]]
        elif char in ' \n\t':code={' ':44,'\n':40,'\t':43}[char]
        else:raise ValueError('Text macros use the US keyboard layout and printable ASCII')
        result.append(code)
    return result

def validate_steps(steps):
    if not isinstance(steps,list) or not 1<=len(steps)<=64:raise ValueError('Use 1–64 macro steps')
    budget=0
    for step in steps:
        if not isinstance(step,dict):raise ValueError('Invalid macro step')
        kind=step.get('type')
        if kind=='shortcut':chord(step.get('value',''));budget+=1
        elif kind=='text':
            value=step.get('value','')
            if not isinstance(value,str) or len(value)>1000:raise ValueError('Text step too long')
            text_codes(value);budget+=len(value)
        elif kind=='delay':
            ms=step.get('ms')
            if not isinstance(ms,int) or not 0<=ms<=10000:raise ValueError('Delay must be 0–10000 ms')
            budget+=ms/25
        elif kind=='home':
            if not re.fullmatch(r'(light|switch|scene|script|media_player)\.[a-z0-9_]+',step.get('entity','')):raise ValueError('Choose a Home Assistant entity')
            if step.get('service') not in ['turn_on','turn_off','toggle']:raise ValueError('Unsupported device action')
        elif kind=='ir':
            if not isinstance(step.get('name'),str) or not step['name']:raise ValueError('Choose a saved remote command')
        elif kind=='bank':
            if step.get('bank') not in range(4):raise ValueError('Bank must be 0–3')
        else:raise ValueError(f'Unknown step: {kind}')
    if budget>2400:raise ValueError('Macro exceeds one minute of input/delay')
    return steps

def validate_config(config):
    if not isinstance(config,dict):raise ValueError('Configuration must be an object')
    pages=config.get('pages')
    if not isinstance(pages,list) or not 1<=len(pages)<=12:raise ValueError('Use 1–12 pages')
    ids=set()
    for page in pages:
        if not isinstance(page,dict) or not isinstance(page.get('name'),str) or not 1<=len(page['name'])<=32:raise ValueError('Page needs a short name')
        buttons=page.get('buttons')
        if not isinstance(buttons,list) or len(buttons)>24:raise ValueError('Maximum 24 buttons per page')
        for button in buttons:
            if not isinstance(button,dict):raise ValueError('Invalid button')
            ident=button.get('id','');name=button.get('name','');color=button.get('color','')
            if not re.fullmatch(r'[a-zA-Z0-9_-]{1,48}',ident) or ident in ids:raise ValueError('Button IDs must be unique')
            ids.add(ident)
            if not isinstance(name,str) or not 1<=len(name)<=40:raise ValueError('Button needs a short name')
            if not re.fullmatch(r'#[0-9a-fA-F]{6}',color):raise ValueError('Invalid button color')
            validate_steps(button.get('steps'))
    for key in ['background','accent']:
        if not re.fullmatch(r'#[0-9a-fA-F]{6}',config.get(key,'')):raise ValueError('Invalid screen color')
    if not isinstance(config.get('key_sounds',False),bool):raise ValueError('Invalid key sound preference')
    return config

DEFAULT={
 'background':'#101820','accent':'#63d8bd','key_sounds':False,
 'pages':[{'name':'Everyday','buttons':[
 {'id':'copy','name':'Copy','color':'#215f78','steps':[{'type':'shortcut','value':'CTRL+C'}]},
 {'id':'paste','name':'Paste','color':'#215f78','steps':[{'type':'shortcut','value':'CTRL+V'}]},
 {'id':'undo','name':'Undo','color':'#76543c','steps':[{'type':'shortcut','value':'CTRL+Z'}]},
 {'id':'save','name':'Save','color':'#2a7259','steps':[{'type':'shortcut','value':'CTRL+S'}]},
 {'id':'shot','name':'Screenshot','color':'#614989','steps':[{'type':'shortcut','value':'PRINTSCREEN'}]},
 {'id':'search','name':'Find','color':'#614989','steps':[{'type':'shortcut','value':'CTRL+F'}]},
 {'id':'bank1','name':'Macro bank 1','color':'#37435b','steps':[{'type':'bank','bank':0}]},
 {'id':'bank2','name':'Macro bank 2','color':'#37435b','steps':[{'type':'bank','bank':1}]},
 {'id':'bank3','name':'Macro bank 3','color':'#37435b','steps':[{'type':'bank','bank':2}]}]},
 {'name':'My buttons','buttons':[]}]
}
