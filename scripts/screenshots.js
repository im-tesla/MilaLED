const puppeteer = require('puppeteer');
const path = require('path');

const OUT = path.join(__dirname, '..', 'assets', 'screenshots');
const URL = 'http://localhost:5299';
const WIDTH = 375;
const HEIGHT = 812;

const tabs = [
  { name: 'effects',  title: 'Effects tab' },
  { name: 'color',    title: 'Color tab' },
  { name: 'presets',  title: 'Presets tab' },
  { name: 'settings', title: 'Settings tab' },
];

(async () => {
  const browser = await puppeteer.launch({ headless: 'new' });
  const page = await browser.newPage();
  await page.setViewport({ width: WIDTH, height: HEIGHT, deviceScaleFactor: 2 });

  for (const { name, title } of tabs) {
    console.log(`Capturing ${name}...`);
    await page.goto(URL, { waitUntil: 'networkidle0', timeout: 15000 });

    // Click the tab button (4th nav button child for Settings, etc.)
    const idx = tabs.findIndex(t => t.name === name);
    await page.waitForSelector('nav button', { timeout: 5000 });
    const btns = await page.$$('nav button');
    if (btns[idx]) await btns[idx].click();

    // Wait for motion animation
    await new Promise(r => setTimeout(r, 400));

    await page.screenshot({
      path: path.join(OUT, `${name}.png`),
      fullPage: false,
    });
    console.log(`  -> ${name}.png saved`);
  }

  await browser.close();
  console.log('Done.');
})().catch(err => { console.error(err); process.exit(1); });
