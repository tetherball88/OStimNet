import React, { CSSProperties, useEffect } from 'react'
import { TooltipProvider } from '@/components/ui/tooltip'
import { ModalHost } from '@/components/modals/ModalHost'
import '@/components/modals/ExternalThreadSetupModal'
import { sendOStimNetAction } from '@/store/uiBridge'
import { useModalStore } from '@/store/useModalStore'
import { isInGame } from '@/lib/utils'

const DEV_PREVIEW_BACKGROUND: CSSProperties = {
  backgroundImage:
    'radial-gradient(circle at 50% 35%, rgba(62, 42, 22, 0.92), rgba(14, 10, 6, 0.98))',
}

export const App: React.FC = () => {
  const hasActiveModal = useModalStore((s) => s.activeModal !== null)
  const showModal = useModalStore((s) => s.showModal)
  const inGame = isInGame()

  useEffect(() => {
    window.log?.('OStimNet UI View mounted successfully')
    if (inGame) {
      sendOStimNetAction('uiReady')
    }
  }, [inGame])

  const showDevBar = !import.meta.env.PROD && !inGame

  if (!hasActiveModal && !showDevBar) {
    return null
  }

  return (
    <TooltipProvider delayDuration={250}>
      {showDevBar && !hasActiveModal && (
        <div
          className="fixed inset-0 p-[1.25rem] flex flex-col items-start gap-[0.75rem] font-ui text-foreground-primary"
          style={DEV_PREVIEW_BACKGROUND}
        >
          <div className="bg-panel-raised border border-border-strong rounded-lg p-[0.875rem] shadow-raised flex flex-wrap items-center gap-[0.5rem]">
            <span className="text-xs font-bold uppercase tracking-wide text-accent-bright mr-[0.5rem]">
              OStimNet Modal Preview
            </span>
            <button
              type="button"
              onClick={() =>
                showModal({
                  modalId: 'preview_consent',
                  title: 'Confirm Intimate Encounter',
                  subtitle: 'Lydia & Dragonborn',
                  tone: 'default',
                  size: 'md',
                  message: 'Lydia wants to start a sexual encounter with Dragonborn.',
                  controls: [
                    {
                      type: 'infobox',
                      id: 'scene_details',
                      label: 'Proposed Details',
                      description: 'Furniture: Bed\nIntent: romantic',
                      tone: 'default',
                    },
                  ],
                  buttons: [
                    { id: 'accept', label: 'Accept', variant: 'default' },
                    {
                      id: 'decline_explain',
                      label: 'Decline (Explain)',
                      variant: 'outline',
                      tooltip: 'Decline and type a reason for SkyrimNet',
                    },
                    {
                      id: 'cancel',
                      label: 'Cancel',
                      variant: 'secondary',
                      tooltip: 'Silently cancel without recording a SkyrimNet event',
                    },
                  ],
                })
              }
              className="px-[0.75rem] py-[0.375rem] rounded-md bg-accent text-foreground-onAccent text-xs font-semibold cursor-pointer"
            >
              Consent Modal
            </button>

            <button
              type="button"
              onClick={() =>
                showModal({
                  modalId: 'preview_posteval',
                  title: 'Participants Adjusted',
                  subtitle: 'Lydia & Dragonborn',
                  tone: 'warning',
                  size: 'md',
                  message:
                    'After evaluation, the participant list has been adjusted.\nLydia will proceed with Dragonborn.\nDo you want to go ahead?',
                  controls: [
                    {
                      type: 'infobox',
                      id: 'scene_details',
                      label: 'Proposed Details',
                      description:
                        'Excluded: Serana\nActivity: Missionary\nPosition: missionary\nIntent: romantic',
                      tone: 'warning',
                    },
                  ],
                  buttons: [
                    { id: 'accept', label: 'Accept', variant: 'default' },
                    { id: 'decline_explain', label: 'Decline (Explain)', variant: 'outline' },
                    { id: 'cancel', label: 'Cancel', variant: 'secondary' },
                  ],
                })
              }
              className="px-[0.75rem] py-[0.375rem] rounded-md bg-panel border border-border-strong text-xs font-semibold cursor-pointer"
            >
              Post-Eval Modal
            </button>

            <button
              type="button"
              onClick={() =>
                showModal({
                  modalId: 'preview_retry',
                  title: 'LLM Request Failed',
                  subtitle: 'PreStartSexualScene',
                  tone: 'danger',
                  size: 'sm',
                  message: 'Choose Retry to try the request again, or Ignore to skip this evaluation.',
                  controls: [
                    {
                      type: 'infobox',
                      id: 'error_info',
                      label: 'Error Details',
                      description: 'HTTP 504: Upstream model request timed out after 30s.',
                      tone: 'danger',
                    },
                  ],
                  buttons: [
                    { id: 'retry', label: 'Retry', variant: 'default' },
                    { id: 'ignore', label: 'Ignore', variant: 'secondary' },
                  ],
                })
              }
              className="px-[0.75rem] py-[0.375rem] rounded-md bg-panel border border-border-strong text-xs font-semibold cursor-pointer"
            >
              LLM Retry Modal
            </button>

            <button
              type="button"
              onClick={() =>
                showModal({
                  modalId: 'preview_ext_setup',
                  modalType: 'external_thread_setup',
                  title: 'Configure Scene',
                  subtitle: 'Dragonborn, Lydia and Serana',
                  size: 'md',
                  customData: {
                    threadId: 0,
                    actorCount: 3,
                    actors: [
                      { id: '00000014', name: 'Dragonborn' },
                      { id: '000A2C94', name: 'Lydia' },
                      { id: '02002B74', name: 'Serana' },
                    ],
                  },
                  buttons: [
                    { id: 'confirm', label: 'Confirm', variant: 'default' },
                    { id: 'cancel', label: 'Cancel', variant: 'secondary' },
                  ],
                })
              }
              className="px-[0.75rem] py-[0.375rem] rounded-md bg-panel border border-border-strong text-xs font-semibold cursor-pointer"
            >
              External Thread Setup
            </button>

            <button
              type="button"
              onClick={() =>
                showModal({
                  modalId: 'preview_roles',
                  title: 'Change Actor Roles',
                  subtitle: 'Dragonborn, Lydia and Serana',
                  tone: 'default',
                  size: 'md',
                  controls: [
                    {
                      type: 'checklist',
                      id: 'mainActors',
                      label: 'Select Initiators',
                      description:
                        'Select 1 to 2 participant(s) as Initiators. Unchecked participants will take secondary roles.',
                      showSelectAllToggle: false,
                      items: [
                        { id: '00000014', label: 'Dragonborn', checked: true },
                        { id: '000A2C94', label: 'Lydia', checked: false },
                        { id: '02002B74', label: 'Serana', checked: false },
                      ],
                    },
                  ],
                  buttons: [
                    {
                      id: 'apply',
                      label: 'Apply',
                      variant: 'default',
                      requireChecklistId: 'mainActors',
                      minCheckedCount: 1,
                      maxCheckedCount: 2,
                    },
                    { id: 'cancel', label: 'Cancel', variant: 'secondary' },
                  ],
                })
              }
              className="px-[0.75rem] py-[0.375rem] rounded-md bg-panel border border-border-strong text-xs font-semibold cursor-pointer"
            >
              Change Roles Checklist
            </button>
          </div>
        </div>
      )}

      <ModalHost style={!inGame ? DEV_PREVIEW_BACKGROUND : undefined} />
    </TooltipProvider>
  )
}
