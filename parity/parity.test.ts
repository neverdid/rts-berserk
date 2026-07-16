import { execFileSync, spawnSync } from 'node:child_process'
import { existsSync } from 'node:fs'
import { resolve } from 'node:path'
import { describe, expect, it } from 'vitest'
import {
  PARITY_SCENARIOS,
  executeTypeScriptScenario,
  serializeScenario,
  typeScriptCatalog,
  type ParitySnapshot,
} from './scenarios'

const runner = locateRunner()

describe('TypeScript and C++ authoritative parity', () => {
  it('keeps the shared faction and entity catalog synchronized', () => {
    const native = JSON.parse(execFileSync(runner, ['--catalog'], { encoding: 'utf8' }))
    expect(native).toEqual(typeScriptCatalog())
  })

  for (const scenario of PARITY_SCENARIOS) {
    it(scenario.name, () => {
      const expected = executeTypeScriptScenario(scenario)
      const process = spawnSync(runner, [], {
        input: serializeScenario(scenario),
        encoding: 'utf8',
      })
      expect(process.status, process.stderr).toBe(0)
      const native = JSON.parse(process.stdout) as ParitySnapshot
      const diagnostic = `native=${JSON.stringify(native)}\nTypeScript=${JSON.stringify(expected)}`

      scenario.checkpoints.forEach((checkpoint) => {
        const expectedValue = valueAtPath(expected, checkpoint.path)
        const nativeValue = valueAtPath(native, checkpoint.path)
        if (checkpoint.tolerance !== undefined) {
          expect(typeof expectedValue, `${checkpoint.path} should be numeric`).toBe('number')
          expect(typeof nativeValue, `${checkpoint.path} should be numeric`).toBe('number')
          expect(
            Math.abs((nativeValue as number) - (expectedValue as number)),
            `${checkpoint.path}: native=${String(nativeValue)}, TypeScript=${String(expectedValue)}\n${diagnostic}`,
          ).toBeLessThanOrEqual(checkpoint.tolerance)
        } else {
          expect(nativeValue, `${checkpoint.path}\n${diagnostic}`).toEqual(expectedValue)
        }
      })
    })
  }
})

function locateRunner(): string {
  const configured = process.env.ASHEN_PARITY_RUNNER
  const candidates = [
    configured,
    'build/native/native/Debug/ashen_parity_runner.exe',
    'build/native/native/Release/ashen_parity_runner.exe',
    'build/native/native/ashen_parity_runner',
    'build/parity/native/Release/ashen_parity_runner.exe',
    'build/parity/native/ashen_parity_runner',
  ]
    .filter((candidate): candidate is string => Boolean(candidate))
    .map((candidate) => resolve(candidate))
  const found = candidates.find((candidate) => existsSync(candidate))
  if (!found) {
    throw new Error(
      `Parity runner was not found. Build it with "cmake --preset dev" and "cmake --build --preset dev --config Debug". Checked:\n${candidates.join('\n')}`,
    )
  }
  return found
}

function valueAtPath(value: unknown, path: string): unknown {
  return path.split('.').reduce<unknown>((current, segment) => {
    if (typeof current !== 'object' || current === null || !(segment in current)) {
      throw new Error(`Checkpoint path does not exist: ${path}`)
    }
    return (current as Record<string, unknown>)[segment]
  }, value)
}
