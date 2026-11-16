import { ModalPayload } from '../types/modal'
import { useModalStore } from './useModalStore'

declare global {
  interface Window {
    ostimnetAction?: (jsonStr: string) => void
    log?: (message: string) => void
    showOStimNetModal?: (modalData: ModalPayload | string) => void
    clearOStimNetModal?: () => void
  }
}

function parsePayload<T = any>(raw: T | string): T {
  if (typeof raw === 'string') {
    const trimmed = raw.trim()
    if (
      (trimmed.startsWith('{') && trimmed.endsWith('}')) ||
      (trimmed.startsWith('[') && trimmed.endsWith(']'))
    ) {
      try {
        return JSON.parse(trimmed) as T
      } catch (e) {
        console.error('[UIBridge] Failed to parse JSON payload:', e)
      }
    }
  }
  return raw as T
}

export function sendOStimNetAction(action: string, payload: Record<string, any> = {}) {
  const jsonStr = JSON.stringify({ action, payload })
  if (typeof window !== 'undefined' && window.ostimnetAction) {
    window.ostimnetAction(jsonStr)
  } else {
    console.log(`[Standalone Mock Action] ${action}:`, payload)
  }
}

export function initializeUIBridge() {
  if (typeof window === 'undefined') return

  window.showOStimNetModal = (rawModal: ModalPayload | string) => {
    const modal = parsePayload<ModalPayload>(rawModal)
    if (!modal || typeof modal !== 'object' || !modal.modalId) {
      console.error('[UIBridge] Invalid showOStimNetModal payload:', rawModal)
      return
    }
    console.log('[UIBridge] showOStimNetModal:', modal)
    useModalStore.getState().showModal(modal)
  }

  window.clearOStimNetModal = () => {
    console.log('[UIBridge] clearOStimNetModal called')
    useModalStore.getState().clearModal()
  }
}
