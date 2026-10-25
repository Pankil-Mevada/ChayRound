// Run against tests/browser_fixture.py only. NODE_PATH must include Playwright.
const {chromium}=require('playwright');
const {execFileSync}=require('node:child_process');
const fs=require('node:fs');
const path=require('node:path');
const assert=require('node:assert/strict');
async function main(){
  const mailbox=process.argv[2];assert(mailbox?.startsWith('/tmp/chayround-browser-'));
  const browser=await chromium.launch({channel:'msedge',headless:true});
  const ownerContext=await browser.newContext({viewport:{width:1280,height:900}});
  const officeContext=await browser.newContext({viewport:{width:375,height:900}});
  const owner=await ownerContext.newPage(),office=await officeContext.newPage();const errors=[];
  for(const page of [owner,office]){page.on('pageerror',e=>errors.push(e.message));page.on('console',m=>{if(m.type()==='error')errors.push(m.text()+' '+m.location().url)});}
  const output=path.join(__dirname,'../.run-qa/screenshots');fs.mkdirSync(output,{recursive:true});
  async function submit(page,id){const button=page.locator('#'+id+' button[type=submit], #'+id+' button:not([type])').first();await button.click();while(await button.isDisabled())await page.waitForTimeout(25);}
  async function waitMessage(page,text){await page.locator('#message').filter({hasText:text}).waitFor();}
  try{
    await owner.goto('http://localhost:9280/#owner');
    await owner.locator('#stallName').fill('Browser QA Tea');await owner.locator('#newEmail').fill('browser-owner@example.test');await owner.locator('#newPassword').fill('isolated-browser-password-123');await submit(owner,'register');await waitMessage(owner,'Account created');
    await owner.locator('#email').fill('browser-owner@example.test');await owner.locator('#password').fill('isolated-browser-password-123');await submit(owner,'login');await owner.locator('#seller').waitFor();
    await owner.locator('#sendCode').click();await waitMessage(owner,'Verification code sent');
    const codes=JSON.parse(execFileSync('wsl',['--exec','cat',mailbox],{encoding:'utf8'}));
    await owner.locator('#code').fill(codes['browser-owner@example.test']);await submit(owner,'verifyForm');await waitMessage(owner,'Contact email verified');
    await owner.locator('#area').fill('QA Area');await owner.locator('#opens').fill('00:00');await owner.locator('#closes').fill('00:00');await owner.locator('#paused').uncheck();await submit(owner,'stallForm');await waitMessage(owner,'service settings saved');
    for(const [name,price] of [['Regular Tea','15'],['Special Tea','20']]){await owner.locator('#itemName').fill(name);await owner.locator('#price').fill(price);await submit(owner,'menuForm');await waitMessage(owner,'Menu saved');}
    await owner.locator('#officeName').fill('Browser Office');await owner.locator('#address').fill('Synthetic reception, QA building');await owner.locator('#policy').selectOption('MONTHLY');await submit(owner,'officeForm');await waitMessage(owner,'Office saved');
    await office.goto('http://localhost:9280/?stall=1&source=browser-qa');await office.locator('#publicStall').waitFor();await office.getByRole('button',{name:'Request office access',exact:true}).click();await office.locator('#joinName').fill('Browser Office');await submit(office,'joinOffice');await office.locator('#accessPending').waitFor();
    await owner.locator('#refresh').click();await owner.locator('#accessRequests select').selectOption('1');await owner.getByRole('button',{name:'Approve office',exact:true}).click();await waitMessage(owner,'Office connected');
    await office.locator('#checkAccess').click();await office.locator('#office').waitFor();
    await office.locator('#officeMessages').click();const literal='<img src=x onerror=alert(1)> Reception please';await office.locator('#messageBody').fill(literal);await submit(office,'messageForm');await office.locator('#conversationMessages').getByText(literal,{exact:true}).waitFor();assert.equal(await office.locator('#conversationMessages img').count(),0);
    await owner.locator('#refresh').click();await owner.locator('#conversationOffices').getByRole('button',{name:/Browser Office/}).click();await owner.locator('#conversationMessages').getByText(literal,{exact:true}).waitFor();await owner.locator('#messageBody').fill('Reception confirmed');await submit(owner,'messageForm');
    await office.locator('#refreshConversation').click();await office.locator('#conversationMessages').getByText('Reception confirmed',{exact:true}).waitFor();
    await office.locator('#closeConversation').click();await office.locator('#qty-1').fill('1');await submit(office,'orderForm');await waitMessage(office,'Order request saved');
    for(const button of ['Accept','Mark ready','Confirm fulfilment']){await owner.locator('#refresh').click();await owner.locator('#sellerOrders').getByRole('button',{name:button,exact:true}).click();await waitMessage(owner,'Order updated');}
    await office.locator('#officeRefresh').click();await office.locator('#officeOrders').getByText('FULFILLED',{exact:true}).waitFor();await office.locator('#qty-2').fill('4');await office.getByRole('button',{name:'Repeat this order',exact:true}).click();await waitMessage(office,'Review current prices');assert.equal(await office.locator('#qty-1').inputValue(),'1');assert.equal(await office.locator('#qty-2').inputValue(),'0');
    await owner.locator('#ownerBusinessRefresh').click();await owner.getByRole('heading',{name:'Referral reward budget',exact:true}).waitFor();await owner.getByRole('button',{name:'View business report',exact:true}).click();await owner.locator('#businessReport').getByText(/Fulfilled sales/).waitFor();
    await owner.locator('#sharePublicStall').click();assert.equal(await owner.locator('#publicInvitation canvas').count(),1);
    await owner.locator('#publicInvitation').screenshot({path:path.join(output,'public-invitation.png')});
    await office.locator('#officeExtras').click();await office.locator('#advanceSummary').getByText(/money controls disabled/).waitFor();
    await office.locator('#office').scrollIntoViewIfNeeded();assert(await office.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth));
    await office.screenshot({path:path.join(output,'office-mobile.png')});
    await owner.locator('#conversation').scrollIntoViewIfNeeded();await owner.screenshot({path:path.join(output,'owner-conversation.png')});
    assert.deepEqual(errors,[]);console.log('Browser QA passed: onboarding, private messages/text safety, order fulfilment, repeat basket reset, reports, QR invitation, advance gate and 375px layout.');
  }finally{await browser.close();}
}
main().catch(e=>{console.error(e);process.exitCode=1});
