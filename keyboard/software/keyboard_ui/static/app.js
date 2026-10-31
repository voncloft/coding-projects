'use strict';
const $=id=>document.getElementById(id);let config,token,state={},pageIndex=0,chunk=0,editing=false,editId=null,editSteps=[],lastEvent=0,focusIndex=-1,entities=[],savedRemotes=[];
const clone=v=>JSON.parse(JSON.stringify(v));
function message(text){$('message').textContent=text;}
async function api(path,data={}){const r=await fetch('/api/'+path,{method:'POST',headers:{'Content-Type':'application/json','X-Keyboard-Token':token},body:JSON.stringify(data)});const result=await r.json();if(!r.ok)throw Error(result.error||'Action failed');return result;}
function safe(fn){return async e=>{try{await fn(e);}catch(error){message(error.message);}}}
function el(tag,text,attrs={}){const node=document.createElement(tag);if(text!==null)node.textContent=text;Object.assign(node,attrs);return node;}
function button(text,fn,attrs={}){const b=el('button',text,attrs);b.type='button';b.addEventListener('click',safe(fn));return b;}
function theme(){document.documentElement.style.setProperty('--bg',config.background);document.documentElement.style.setProperty('--accent',config.accent);}
function render(){
 theme();$('page').replaceChildren(...config.pages.map((p,i)=>el('option',p.name,{value:i})));$('page').value=pageIndex;
 const page=config.pages[pageIndex];chunk=Math.max(0,Math.min(chunk,Math.floor((page.buttons.length+(editing?1:0)-1)/9)));
 const entries=page.buttons.slice(chunk*9,chunk*9+9);$('buttons').replaceChildren();
 for(const item of entries){const b=button('',()=>editing?openEditor(item):api('macro',{id:item.id}),{className:'macro'});b.style.background=item.color;b.append(el('span',item.name),el('small',editing?'Edit button':`${item.steps.length} ${item.steps.length===1?'action':'actions'}`));$('buttons').append(b);}
 if(editing&&page.buttons.length<24&&entries.length<9)$('buttons').append(button('+ Button',()=>openEditor(),{className:'macro add'}));
 if(!entries.length&&!editing)$('buttons').append(button('+ Add your buttons',()=>{editing=true;render();},{className:'macro add'}));
 $('editmode').textContent=editing?'Done':'Edit';$('editmode').classList.toggle('active',editing);$('pagination').textContent=`${chunk+1} / ${Math.max(1,Math.ceil((page.buttons.length+(editing&&page.buttons.length<24?1:0))/9))}`;
 $('background').value=config.background;$('accent').value=config.accent;$('keysounds').checked=config.key_sounds;
 focusIndex=-1;
}
async function save(){await api('config',config);render();message('Saved');}
function showView(name){document.querySelectorAll('.view').forEach(e=>e.hidden=e.id!==name);document.querySelectorAll('[data-view]').forEach(e=>e.classList.toggle('active',e.dataset.view===name));focusIndex=-1;if(name==='music'&&$('plexamp').getAttribute('src')==='about:blank')$('plexamp').src='http://127.0.0.1:32500';window.scrollTo(0,0);}
document.querySelectorAll('[data-view]').forEach(b=>b.onclick=()=>showView(b.dataset.view));
$('page').onchange=()=>{pageIndex=+$('page').value;chunk=0;render();};
$('editmode').onclick=()=>{editing=!editing;render();};
$('addpage').onclick=safe(async()=>{if(config.pages.length>=12)throw Error('Maximum 12 pages');const name=prompt('Page name');if(!name)return;config.pages.push({name,buttons:[]});pageIndex=config.pages.length-1;chunk=0;editing=true;await save();});
$('previous').onclick=()=>{if(chunk>0){chunk--;render();}};$('next').onclick=()=>{if((chunk+1)*9<config.pages[pageIndex].buttons.length+(editing?1:0)){chunk++;render();}};
function field(label,node){const l=el('label',label);l.append(node);return l;}
function textField(value,change,multiline=false){const n=el(multiline?'textarea':'input',null);n.value=value??'';if(multiline)n.rows=3;n.oninput=()=>change(n.value);return n;}
function selectField(options,value,change){const n=el('select',null);for(const [v,label] of options)n.append(el('option',label,{value:v}));n.value=value;n.onchange=()=>change(n.value);return n;}
function defaultStep(type){return ({shortcut:{type,value:'CTRL+C'},text:{type,value:''},delay:{type,ms:250},home:{type,entity:'light.',service:'toggle'},ir:{type,name:savedRemotes[0]||''},bank:{type,bank:0}})[type];}
function renderSteps(){
 $('steps').replaceChildren();editSteps.forEach((step,index)=>{
 const row=el('div',null,{className:'step'}),head=el('div',null,{className:'row'});head.append(el('span',`Step ${index+1}`,{className:'step-number'}),button('↑',()=>{if(index){[editSteps[index-1],editSteps[index]]=[step,editSteps[index-1]];renderSteps();}}),button('×',()=>{editSteps.splice(index,1);renderSteps();}));row.append(head);
 row.append(field('Action',selectField([['shortcut','Keyboard shortcut'],['text','Type text'],['delay','Wait'],['home','Wi-Fi device / scene'],['ir','Remote command'],['bank','Central macro bank']],step.type,type=>{editSteps[index]=defaultStep(type);renderSteps();})));
 if(['shortcut','text'].includes(step.type))row.append(field(step.type==='shortcut'?'Shortcut':'Text',textField(step.value,v=>step.value=v,step.type==='text')));
 if(step.type==='delay'){const input=textField(step.ms,v=>step.ms=+v);input.type='number';input.min=0;input.max=10000;row.append(field('Milliseconds',input));}
 if(step.type==='home'){
 row.append(field('Entity',textField(step.entity,v=>step.entity=v)));
 if(entities.length)row.append(field('Choose discovered device',selectField([['','Select…'],...entities.map(e=>[e.entity,e.name])],'',v=>{if(v){step.entity=v;renderSteps();}})));
 row.append(field('Command',selectField([['toggle','Toggle'],['turn_on','Turn on / activate'],['turn_off','Turn off']],step.service,v=>step.service=v)));}
 if(step.type==='ir')row.append(field('Saved command',selectField(savedRemotes.map(n=>[n,n]),step.name,v=>step.name=v)));
 if(step.type==='bank')row.append(field('Bank',selectField([0,1,2,3].map(i=>[i,`Bank ${i+1}`]),step.bank,v=>step.bank=+v)));
 $('steps').append(row);
 });
}
function openEditor(item){editId=item?.id||null;editSteps=clone(item?.steps||[{type:'shortcut',value:'CTRL+C'}]);$('buttonname').value=item?.name||'';$('buttoncolor').value=item?.color||'#215f78';$('deletebutton').hidden=!editId;renderSteps();$('editor').showModal();}
$('addstep').onclick=()=>{if(editSteps.length<64){editSteps.push(defaultStep('shortcut'));renderSteps();}};
const editKeyboard=button('On-screen keyboard',()=>{if(!typingTarget||!$('editor').contains(typingTarget))typingTarget=$('buttonname');rememberTyping();drawSoft();$('softkeys').showModal();});$('editform').insertBefore(editKeyboard,$('steps'));
$('closeeditor').onclick=()=>$('editor').close();$('editform').onsubmit=safe(async e=>{e.preventDefault();const page=config.pages[pageIndex],item={id:editId||'button_'+crypto.randomUUID().replaceAll('-',''),name:$('buttonname').value,color:$('buttoncolor').value,steps:clone(editSteps)};const next=clone(config),buttons=next.pages[pageIndex].buttons,index=buttons.findIndex(b=>b.id===editId);if(index<0)buttons.push(item);else buttons[index]=item;await api('config',next);config=next;$('editor').close();render();message('Button saved');});
$('deletebutton').onclick=safe(async()=>{const next=clone(config);next.pages[pageIndex].buttons=next.pages[pageIndex].buttons.filter(b=>b.id!==editId);await api('config',next);config=next;$('editor').close();render();});
$('cancel').onclick=safe(()=>api('cancel'));document.querySelectorAll('[data-bank]').forEach(b=>b.onclick=safe(()=>api('bank',{value:+b.dataset.bank})));document.querySelectorAll('[data-host]').forEach(b=>b.onclick=safe(()=>api('host',{slot:+b.dataset.host})));
$('pair').onclick=safe(async()=>{if(confirm('Replace the saved device in this Bluetooth slot?'))await api('host',{slot:+$('pairslot').value,pair:true});});
$('rotarymode').onchange=safe(()=>api('mode',{value:+$('rotarymode').value}));
$('savetheme').onclick=safe(async()=>{config.background=$('background').value;config.accent=$('accent').value;config.key_sounds=$('keysounds').checked;await save();});
$('displaybrightness').onchange=safe(()=>api('brightness',{value:+$('displaybrightness').value}));
$('rgbapply').onclick=safe(()=>api('rgb',{color:$('rgbcolor').value,brightness:+$('rgbbrightness').value,enabled:$('rgbenabled').checked}));
$('applyloads').onclick=safe(()=>api('loads',{mask:[...document.querySelectorAll('[data-load]:checked')].reduce((n,e)=>n|+e.dataset.load,0)}));
$('savehome').onclick=safe(async()=>{await api('home-config',{url:$('homeurl').value,token:$('hometoken').value});$('hometoken').value='';message('Connection saved');});
$('refreshhome').onclick=safe(async()=>{entities=await api('home-list');$('entities').replaceChildren(...entities.map(e=>{const row=el('div',null,{className:'device'}),name=el('span',e.name);name.append(el('small',e.entity+' · '+e.state));row.append(name,button('Add macro',()=>{showView('macros');editing=true;openEditor({name:e.name.slice(0,40),color:'#2a7259',steps:[{type:'home',entity:e.entity,service:'toggle'}]});}));return row;}));message(`${entities.length} devices found`);});
$('learn').onclick=safe(async()=>{await api('ir-learn');message(state.simulation?'Simulation: import a code to test the screen flow':'Point the remote at the keyboard and press its button within 10 seconds');});
$('importir').onclick=safe(()=>api('ir-import',{pronto:$('pronto').value}));$('nec').onclick=safe(()=>api('ir-import',{nec:{address:parseInt($('necaddress').value,16),command:parseInt($('neccommand').value,16),extended:$('necextended').checked}}));
$('testir').onclick=safe(()=>api('ir-test'));$('saveir').onclick=safe(()=>api('ir-save',{name:$('irname').value,worked:true}));
$('reloadmusic').onclick=()=>$('plexamp').src='http://127.0.0.1:32500';
$('export').onclick=()=>{const a=el('a',null,{href:URL.createObjectURL(new Blob([JSON.stringify(config,null,2)],{type:'application/json'})),download:'keyboard-macros.json'});a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000);};
$('importfile').onchange=safe(async()=>{const file=$('importfile').files[0];if(!file)return;if(file.size>200000)throw Error('File is too large');const next=JSON.parse(await file.text());await api('config',next);config=next;pageIndex=0;chunk=0;render();message('Pages imported');});
function rotary(event){if(event.mode!==1||$('editor').open)return;const nodes=[...document.querySelectorAll('.view:not([hidden]) button,.view:not([hidden]) select')].filter(e=>!e.disabled&&e.offsetParent!==null);if(!nodes.length)return;if(event.delta){document.querySelectorAll('.rotary-focus').forEach(e=>e.classList.remove('rotary-focus'));focusIndex=(focusIndex+event.delta+nodes.length)%nodes.length;nodes[focusIndex].classList.add('rotary-focus');nodes[focusIndex].scrollIntoView({block:'nearest'});}if(event.activate&&focusIndex>=0)nodes[focusIndex].click();}
async function poll(){try{const r=await fetch('/api/state');const data=await r.json();token=data.token;state=data.state;if(!config){config=data.config;$('homeurl').value=data.home_url;render();}const host=state.host===0?'USB':`Bluetooth ${state.host}`;$('connection').textContent=(state.simulation?'Preview · ':state.connected?'':'Controller offline · ')+host;$('battery').textContent=state.battery===null?'Battery —':state.battery+'%';$('battery').style.color=state.battery===null?'':state.battery>=80?'#76db8e':state.battery>=30?'#eed269':'#ff8a84';$('banklabel').textContent=`${state.bank+1} / 4`;$('rotarymode').value=state.mode;document.querySelectorAll('[data-bank]').forEach(b=>b.classList.toggle('active',+b.dataset.bank===state.bank));document.querySelectorAll('[data-host]').forEach(b=>b.classList.toggle('active',+b.dataset.host===state.host));$('busy').textContent=state.busy?'Running…':'';if(state.message!=='Ready'&&state.message!=='Done')message(state.message);$('powerdetail').textContent=state.battery_mv?`Battery ${(state.battery_mv/1000).toFixed(2)} V · ${state.battery_ma} mA · ${state.power_flags&4?'USB-C PD power':'Battery / low-power input'}`:'Power readings appear when connected to the keyboard.';
 $('irinfo').textContent=state.candidate?`${state.candidate.pulses.length} pulse durations · ${state.candidate.frequency/1000} kHz · ready to test`:'No command loaded.';
 if(JSON.stringify(savedRemotes)!==JSON.stringify(data.remotes)){savedRemotes=data.remotes;$('remotes').replaceChildren(...savedRemotes.map(name=>{const row=el('div',null,{className:'device'});row.append(el('span',name),button('Send',()=>api('ir-send',{name})),button('Add macro',()=>{showView('macros');editing=true;openEditor({name,color:'#76543c',steps:[{type:'ir',name}]});}));return row;}));}
 for(const event of data.events){if(event.id<=lastEvent)continue;lastEvent=event.id;if(event.kind==='encoder')rotary(event);if(event.kind==='activate')rotary({...event,activate:true});}
 }catch(error){message('Screen connection lost. Reconnecting…');}finally{setTimeout(poll,400);}}
poll();

let typingTarget=null,typingStart=0,typingEnd=0,softShift=false;
function rememberTyping(){if(typingTarget){typingStart=typingTarget.selectionStart??typingTarget.value.length;typingEnd=typingTarget.selectionEnd??typingStart;}}
document.addEventListener('focusin',e=>{if(e.target.matches('textarea,input:not([type=color]):not([type=checkbox]):not([type=range]):not([type=file])')&&!$('softkeys').contains(e.target)){typingTarget=e.target;rememberTyping();}});
document.addEventListener('selectionchange',()=>{if(document.activeElement===typingTarget)rememberTyping();});
function typeSoft(text){if(!typingTarget)return;const before=typingTarget.value;typingTarget.value=before.slice(0,typingStart)+text+before.slice(typingEnd);typingStart+=text.length;typingEnd=typingStart;typingTarget.dispatchEvent(new Event('input',{bubbles:true}));}
function drawSoft(){const lower=['1234567890','qwertyuiop','asdfghjkl','zxcvbnm','-=[];\\,./'],upper=['!@#$%^&*()','QWERTYUIOP','ASDFGHJKL','ZXCVBNM','_+{}:\"|<>?'];$('softrows').replaceChildren(...(softShift?upper:lower).map(chars=>{const row=el('div',null,{className:'row'});for(const char of chars)row.append(button(char,()=>typeSoft(char)));return row;}));$('softshift').classList.toggle('active',softShift);}
$('showkeyboard').onclick=()=>{if(!typingTarget||!typingTarget.isConnected){message('Tap a text field first');return;}rememberTyping();drawSoft();$('softkeys').showModal();};
$('softshift').onclick=()=>{softShift=!softShift;drawSoft();};$('softspace').onclick=()=>typeSoft(' ');$('softback').onclick=()=>{if(typingStart===typingEnd&&typingStart)typingStart--;typeSoft('');};
$('softenter').onclick=()=>{if(typingTarget?.tagName==='TEXTAREA')typeSoft('\n');else $('softkeys').close();};$('softhide').onclick=()=>$('softkeys').close();

let audioPollBusy=false;
async function refreshAudio(){
 if(audioPollBusy||!token)return;audioPollBusy=true;
 try{
  const a=await api('audio-status');$('audiostatus').textContent=a.message||'Choose headphones or keyboard speakers';
  $('audioscan').disabled=a.busy;
  const current=$('audiooutput').value;
  $('audiooutput').replaceChildren(...a.outputs.map(o=>el('option',o.label,{value:o.id})));
  if([...$('audiooutput').options].some(o=>o.value===current))$('audiooutput').value=current;
  $('headphones').replaceChildren(...a.devices.map(d=>{
   const row=el('div',null,{className:'device'}),label=el('span',d.name);label.append(el('small',d.connected?'Connected':d.paired?'Saved':'Available'));
   row.append(label,button(d.connected?'Disconnect':d.paired?'Connect':'Pair and connect',async()=>{await api(d.connected?'audio-disconnect':'audio-connect',{address:d.address});await refreshAudio();},{disabled:a.busy}));
   if(d.paired)row.append(button('Forget',async()=>{if(confirm('Forget '+d.name+'?')){await api('audio-forget',{address:d.address});await refreshAudio();}},{disabled:a.busy}));
   return row;
  }));
 }finally{audioPollBusy=false;}
}
$('audiorefresh').onclick=safe(refreshAudio);
$('audioscan').onclick=safe(async()=>{await api('audio-scan');await refreshAudio();});
$('audioselect').onclick=safe(async()=>{if(!$('audiooutput').value)throw Error('No audio output is available yet');await api('audio-output',{id:+$('audiooutput').value});message('Audio output selected');});
$('audiovolume').onchange=safe(()=>api('audio-volume',{value:+$('audiovolume').value}));
setInterval(()=>{if(!$('settings').hidden)refreshAudio().catch(e=>{$('audiostatus').textContent=e.message;});},2000);
