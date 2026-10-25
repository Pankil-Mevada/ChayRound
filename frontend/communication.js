'use strict';
let conversationOffice=null,messageCursor=0,messageSending=null,conversationEpoch=0;const messageRefreshes=new Set();
async function openConversation(id=0){
  conversationOffice=id;conversationEpoch++;messageSending=null;messageCursor=0;$('messageBody').value='';$('conversationTitle').textContent=(officeToken?officeState?.office_name:sellerState?.offices.find(o=>o.office_id===id)?.office_name)||'Office conversation';$('conversationMessages').replaceChildren();
  $('conversation').hidden=false;await refreshConversation();$('conversation').scrollIntoView({behavior:'smooth'});
}
async function refreshConversation(){
  if(conversationOffice===null)return;const scope=conversationOffice,epoch=conversationEpoch;if(messageRefreshes.has(epoch))return;messageRefreshes.add(epoch);try{
  let result;
  do{
    result=await api('conversations/'+scope+'?after='+messageCursor);if(scope!==conversationOffice||epoch!==conversationEpoch)return;
    result.messages.forEach(m=>{
      const row=node('div','','row');row.dataset.message=m.id;
      row.append(node('strong',m.sender==='OWNER'?'Tea-stall owner':m.sender==='SYSTEM'?'ChayRound account update':'Office'),node('p',m.body),node('small',new Date(m.created*1000).toLocaleString('en-IN')));
      $('conversationMessages').append(row);
    });messageCursor=result.next_cursor;
  }while(result.has_more);
  $('conversationStatus').textContent='Saved in ChayRound. '+(result.other_read_through?'Other side has opened messages through #'+result.other_read_through+'.':'Not yet opened by the other side.');
  if(messageCursor&&epoch===conversationEpoch)await api('conversations/'+scope+'/read','POST',{last_id:messageCursor});
  }finally{messageRefreshes.delete(epoch)}
}
async function conversationList(){
  if(!sellerState?.verified||officeToken)return;
  const result=await api('conversations');const box=$('conversationOffices');box.replaceChildren();
  result.offices.forEach(o=>box.append(action(o.name+(o.unread?' · '+o.unread+' unread':''),()=>openConversation(o.office_id))));
  if($('publicProfileForm').dataset.stall!==String(sellerState.stall_id)){const sid=sellerState.stall_id;const profile=await api('public/stalls/'+sid);if(sellerState.stall_id===sid){$('publicDescription').value=profile.description;$('publicTerms').value=profile.terms;$('publicContact').value=profile.contact;$('publicPhoto').value=profile.photo_url;$('publicProfileForm').dataset.stall=String(sid);}}

}
async function showPublicStall(id){
  const s=await api('public/stalls/'+id);chosenStall=s.id;
  $('publicStall').hidden=false;const box=$('publicStallContent');
  box.replaceChildren(node('h2',s.name),node('p',s.description||'Your neighbourhood tea stall'),node('p',s.area+' · '+time(s.opens)+'–'+time(s.closes)+' India time · '+(s.ordering_open?'Taking orders':'Currently paused or closed')),node('p',s.terms),node('p',s.contact));
  if(s.photo_url){const image=document.createElement('img');image.src=s.photo_url;image.alt=s.name;image.loading='lazy';image.referrerPolicy='no-referrer';image.style.maxWidth='100%';box.append(image);}
  s.menu.forEach(m=>box.append(node('div',m.name+' · '+money(m.price_paise)+(m.available?'':' · unavailable'),'row')));
  box.append(action('Request office access',()=>{$('discovery').hidden=false;$('joinOffice').hidden=false;$('chosenStall').textContent=s.name;$('joinName').focus();},false));
}
document.addEventListener('DOMContentLoaded',()=>{
  $('officeMessages').onclick=()=>openConversation(0).catch(e=>message(e.message));
  $('refreshConversation').onclick=()=>refreshConversation().catch(e=>message(e.message));
  $('closeConversation').onclick=()=>{$('conversation').hidden=true;conversationOffice=null;};
  bind('messageForm',async()=>{
    const content=$('messageBody').value.trim();if(!content)throw Error('Enter a message.');
    if(!messageSending||messageSending.body!==content)messageSending={body:content,request_id:crypto.randomUUID()};
    await api('conversations/'+conversationOffice,'POST',messageSending);messageSending=null;$('messageBody').value='';await refreshConversation();
  });
  bind('publicProfileForm',async()=>{
    await api('public-profile','POST',{description:$('publicDescription').value,photo_url:$('publicPhoto').value,terms:$('publicTerms').value,contact:$('publicContact').value});message('Public stall information saved.');
  });
  $('sharePublicStall').onclick=()=>{
    if(!sellerState)return;
    const url=location.origin+'/?stall='+sellerState.stall_id+'&source=owner-invitation';
    const box=$('publicInvitation');box.replaceChildren(node('p',sellerState.stall_name+' · '+sellerState.area));
    const link=node('a',url);link.href=url;box.append(link,node('p','Order from our current menu, track your tea round and view a clear office account. Request access; the owner confirms delivery eligibility.'));
    const qr=qrcode(0,'M');qr.addData(url);qr.make();const canvas=document.createElement('canvas');const size=qr.getModuleCount(),scale=5;canvas.width=canvas.height=(size+8)*scale;canvas.setAttribute('aria-label','Public stall invitation QR code');const context=canvas.getContext('2d');context.fillStyle='white';context.fillRect(0,0,canvas.width,canvas.height);context.fillStyle='black';for(let y=0;y<size;y++)for(let x=0;x<size;x++)if(qr.isDark(y,x))context.fillRect((x+4)*scale,(y+4)*scale,scale,scale);box.append(canvas,node('small','Use a publicly reachable HTTPS address before printing. Localhost links only work on this computer.'));
    box.append(action('Print stall invitation',()=>{document.body.classList.add('printingInvitation');window.print();document.body.classList.remove('printingInvitation')}));
  };
  const stallId=Number(new URLSearchParams(location.search).get('stall'));
  if(Number.isSafeInteger(stallId)&&stallId>0)showPublicStall(stallId).catch(e=>message(e.message));
});
setInterval(()=>{
  if(document.hidden)return;
  if(conversationOffice!==null&&!$('conversation').hidden)refreshConversation().catch(e=>message(e.message));
  if(ownerToken&&!officeToken&&sellerState?.verified)conversationList().catch(e=>message(e.message));
  if(officeToken&&officeState)refreshOrders().catch(e=>message(e.message));
},10000);

async function loadRounds(){const result=await api('rounds');const box=$('roundsList');box.replaceChildren();result.rounds.forEach(r=>{const row=node('div',time(r.minute)+' India time · maximum '+money(r.maximum_total_paise)+' · '+(r.cancelled?'Cancelled':r.paused?'Paused':'Enabled')+' · '+r.last_result,'row');if(!r.cancelled)row.append(action('Cancel future rounds',async()=>{if(!confirm('Cancel future rounds? Existing orders are unchanged.'))return;await api('rounds/'+r.id,'POST',{cancelled:true});await loadRounds()}),action(r.paused?'Resume':'Pause',async()=>{await api('rounds/'+r.id,'POST',{paused:!r.paused});await loadRounds()}),action('Skip today',async()=>{await api('rounds/'+r.id,'POST',{skip_day:Math.floor((Date.now()/1000+19800)/86400)});message('Today skipped. Existing orders need separate cancellation.')}));box.append(row)})}
document.addEventListener('DOMContentLoaded',()=>{
 $('loadRounds').onclick=()=>loadRounds().catch(e=>message(e.message));
 bind('roundForm',async()=>{const items=[...$('basket').querySelectorAll('input')].filter(i=>Number(i.value)>0).map(i=>({item_id:Number(i.dataset.item),quantity:Number(i.value)}));if(!items.length)throw Error('Choose the recurring basket above.');await api('rounds','POST',{items,days:Number($('roundDays').value),minute:minutes($('roundTime').value),maximum_total_paise:Math.round(Number($('roundMaximum').value)*100),consent:$('roundConsent').checked});await loadRounds();message('Recurring round saved. Operator must enable the recurring worker.');});
});
