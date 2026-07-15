import { mkdir, writeFile } from 'node:fs/promises'
import { fileURLToPath } from 'node:url'
import { chromium } from 'playwright'

const baseUrl = process.env.VOWFALL_URL ?? 'http://127.0.0.1:5173/'
const outputDir = new URL('../artifacts/visual-qa/', import.meta.url)
await mkdir(outputDir, { recursive: true })
const outputPath = (name) => fileURLToPath(new URL(name, outputDir))

const browser = await chromium.launch({ headless: true })
const report = { baseUrl, desktop: {}, mobile: {}, errors: [] }

async function preparePage(context, bucket) {
  const page = await context.newPage()
  page.on('console', (message) => {
    if (message.type() === 'error') report.errors.push(`${bucket} console: ${message.text()}`)
  })
  page.on('pageerror', (error) => report.errors.push(`${bucket} page: ${error.message}`))
  await page.addInitScript(() => {
    window.__vowfallSnapshot = null
    window.addEventListener('rts:state', (event) => {
      window.__vowfallSnapshot = event.detail
    })
  })
  await page.goto(baseUrl, { waitUntil: 'networkidle' })
  await page.locator('.main-menu').waitFor({ state: 'visible' })
  return page
}

async function canvasSignal(page) {
  const png = await page.locator('.three-canvas').screenshot()
  return { pngBytes: png.length, distinctByteValues: new Set(png).size }
}

async function visibleOverflow(page, rootSelector) {
  return page.locator(rootSelector).evaluate((root) => {
    const width = document.documentElement.clientWidth
    const height = document.documentElement.clientHeight
    const rootStyle = getComputedStyle(root)
    const allowsVerticalScroll = rootStyle.overflowY === 'auto' || rootStyle.overflowY === 'scroll'
    return [...root.querySelectorAll('*')]
      .filter((element) => {
        const style = getComputedStyle(element)
        const rect = element.getBoundingClientRect()
        return style.display !== 'none' && style.visibility !== 'hidden' && rect.width > 1 && rect.height > 1
      })
      .filter((element) => {
        const rect = element.getBoundingClientRect()
        return (
          rect.left < -2 ||
          rect.right > width + 2 ||
          (!allowsVerticalScroll && (rect.top < -2 || rect.bottom > height + 2))
        )
      })
      .slice(0, 12)
      .map((element) => ({
        tag: element.tagName.toLowerCase(),
        className: element.className?.toString().slice(0, 80) ?? '',
        text: element.textContent?.trim().replace(/\s+/g, ' ').slice(0, 80) ?? '',
        rect: element.getBoundingClientRect().toJSON(),
      }))
  })
}

const desktopContext = await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 })
const desktop = await preparePage(desktopContext, 'desktop')
await desktop.screenshot({ path: outputPath('desktop-menu.png') })
report.desktop.menuOverflow = await visibleOverflow(desktop, '.main-menu')
await desktop.locator('[data-launch="story"]').click()
await desktop.locator('#briefing').waitFor({ state: 'visible' })
await desktop.screenshot({ path: outputPath('desktop-briefing.png') })
report.desktop.briefingOverflow = await visibleOverflow(desktop, '#briefing')
await desktop.locator('#start-mission').click()
await desktop.waitForTimeout(2400)
report.desktop.canvas = await canvasSignal(desktop)
report.desktop.snapshot = await desktop.evaluate(() => ({
  mode: window.__vowfallSnapshot?.mode,
  missionStarted: window.__vowfallSnapshot?.missionStarted,
  mission: window.__vowfallSnapshot?.mission?.id,
  race: window.__vowfallSnapshot?.players?.[1]?.race,
  objective: window.__vowfallSnapshot?.objective,
}))
await desktop.screenshot({ path: outputPath('desktop-battle.png') })

const mobileContext = await browser.newContext({ viewport: { width: 390, height: 844 }, deviceScaleFactor: 1 })
const mobile = await preparePage(mobileContext, 'mobile')
await mobile.screenshot({ path: outputPath('mobile-menu.png'), fullPage: false })
report.mobile.menuOverflow = await visibleOverflow(mobile, '.main-menu')
await mobile.locator('[data-launch="skirmish"]').click()
await mobile.waitForTimeout(1900)
report.mobile.canvas = await canvasSignal(mobile)
await mobile.screenshot({ path: outputPath('mobile-battle.png') })
await mobile.locator('#panel-toggle').click()
await mobile.locator('#command-panel').waitFor({ state: 'visible' })
await mobile.screenshot({ path: outputPath('mobile-council.png') })
report.mobile.councilOverflow = await visibleOverflow(mobile, '#command-panel')

await browser.close()

await writeFile(new URL('report.json', outputDir), `${JSON.stringify(report, null, 2)}\n`)

const canvasFailed = [report.desktop.canvas, report.mobile.canvas].some(
  (canvas) => !canvas || canvas.pngBytes < 10000 || canvas.distinctByteValues < 32,
)
if (report.errors.length > 0 || canvasFailed) {
  console.error(JSON.stringify(report, null, 2))
  process.exitCode = 1
} else {
  console.log(JSON.stringify(report, null, 2))
}
