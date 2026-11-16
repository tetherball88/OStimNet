/**
 * Keyboard & DirectInput scancode helpers for Magelight/Ultralight views.
 * Handles Ultralight 2.0 numpad quirks where NumpadEnter or empty `e.code`
 * can be emitted for numpad keys.
 */

const NUMPAD_CODE_TO_VK: Record<string, number> = {
  Numpad0: 96,
  Numpad1: 97,
  Numpad2: 98,
  Numpad3: 99,
  Numpad4: 100,
  Numpad5: 101,
  Numpad6: 102,
  Numpad7: 103,
  Numpad8: 104,
  Numpad9: 105,
  NumpadMultiply: 106,
  NumpadAdd: 107,
  NumpadSubtract: 109,
  NumpadDecimal: 110,
  NumpadDivide: 111,
  NumpadEnter: 13,
}

export function getNormalizedKeyCode(
  e: Pick<KeyboardEvent, 'keyCode' | 'which' | 'code' | 'key' | 'location'>,
): number {
  if (e.code === 'NumpadEnter' || (e.key === 'Enter' && e.location === 3)) {
    return 13
  }
  if (e.code && NUMPAD_CODE_TO_VK[e.code] !== undefined) {
    return NUMPAD_CODE_TO_VK[e.code]
  }
  return e.keyCode || e.which || 0
}

export function isEnterKey(e: KeyboardEvent): boolean {
  return e.key === 'Enter' || e.code === 'NumpadEnter' || getNormalizedKeyCode(e) === 13
}

export function isEscapeKey(e: KeyboardEvent): boolean {
  return e.key === 'Escape' || e.code === 'Escape' || getNormalizedKeyCode(e) === 27
}
