import React, { useEffect, useState } from 'react'
import { Bot, Info } from 'lucide-react'
import {
  CustomModalTemplateProps,
  registerModalTemplate,
} from './modalRegistry'
import {
  infoBoxVariants,
  itemBadgeVariants,
  renderToneIcon,
  toneIconContainerVariants,
} from './ModalHost'
import { Button } from '@/components/ui/button'
import { Checkbox } from '@/components/ui/checkbox'
import { Separator } from '@/components/ui/separator'
import { SelectInput, SelectOption } from '@/components/shared/SelectInput'
import { useModalStore } from '@/store/useModalStore'
import { ModalTone } from '@/types/modal'
import { cn } from '@/lib/utils'

interface ExternalActorSpec {
  id: string
  formId?: number
  name: string
}

const SCENE_TYPE_OPTIONS: SelectOption[] = [
  { value: 'sexual', label: 'Sexual Encounter' },
  { value: 'non_sexual', label: 'Non-Sexual / Affectionate' },
]

const SEXUAL_INTENT_OPTIONS: SelectOption[] = [
  { value: 'romantic', label: 'Romantic' },
  { value: 'lustful', label: 'Lustful' },
  { value: 'transactional', label: 'Transactional' },
  { value: 'dom', label: 'Dom' },
  { value: 'aggressive', label: 'Aggressive' },
  {
    value: 'ai',
    label: 'Let AI Decide',
    icon: <Bot className="w-[0.875em] h-[0.875em] text-accent-bright" />,
  },
]

const NON_SEXUAL_INTENT_OPTIONS: SelectOption[] = [
  { value: 'platonic', label: 'Platonic' },
  { value: 'romantic', label: 'Romantic' },
  { value: 'lustful', label: 'Lustful' },
  { value: 'transactional', label: 'Transactional' },
]

function getRoleLabel(intent: string): string {
  switch (intent) {
    case 'transactional':
      return 'Service Receivers'
    case 'dom':
      return 'Dominant Actors'
    case 'aggressive':
      return 'Aggressors'
    default:
      return 'Initiators'
  }
}

export const ExternalThreadSetupModal: React.FC<CustomModalTemplateProps> = ({
  modal,
  onSubmit,
}) => {
  const isSubmitting = useModalStore((s) => s.isSubmitting)

  const actors: ExternalActorSpec[] = Array.isArray(modal.customData?.actors)
    ? modal.customData!.actors
    : []

  const [sceneType, setSceneType] = useState<'sexual' | 'non_sexual'>('sexual')
  const [intent, setIntent] = useState<string>('romantic')
  const [checkedIds, setCheckedIds] = useState<string[]>(() =>
    actors.length >= 2 ? [actors[0].id] : actors.map((a) => a.id),
  )

  useEffect(() => {
    setSceneType('sexual')
    setIntent('romantic')
    setCheckedIds(actors.length >= 2 ? [actors[0].id] : actors.map((a) => a.id))
  }, [modal.modalId])

  const handleSceneTypeChange = (nextType: string) => {
    const normalized = nextType === 'non_sexual' ? 'non_sexual' : 'sexual'
    setSceneType(normalized)
    if (normalized === 'non_sexual') {
      if (
        intent !== 'platonic' &&
        intent !== 'romantic' &&
        intent !== 'lustful' &&
        intent !== 'transactional'
      ) {
        setIntent('platonic')
      }
    } else if (intent === 'platonic') {
      setIntent('romantic')
    }
  }

  const toggleActor = (id: string) => {
    setCheckedIds((prev) =>
      prev.includes(id) ? prev.filter((item) => item !== id) : [...prev, id],
    )
  }

  const headerTone: ModalTone =
    intent === 'ai'
      ? 'ai'
      : intent === 'aggressive'
        ? 'danger'
        : intent === 'dom'
          ? 'warning'
          : sceneType === 'non_sexual'
            ? 'success'
            : 'default'

  const intentOptions =
    sceneType === 'sexual' ? SEXUAL_INTENT_OPTIONS : NON_SEXUAL_INTENT_OPTIONS
  const roleLabel = getRoleLabel(intent)
  const needsRoleSelection = intent !== 'ai' && actors.length >= 2
  const maxPrimary = Math.max(1, actors.length - 1)
  const roleCountInvalid =
    needsRoleSelection && (checkedIds.length < 1 || checkedIds.length >= actors.length)

  const confirmDisabled = isSubmitting || roleCountInvalid

  const handleConfirm = () => {
    const uncheckedIds = actors
      .map((a) => a.id)
      .filter((id) => !checkedIds.includes(id))

    onSubmit('confirm', 0, {
      sceneType,
      intent,
      mainActors: {
        checkedIds,
        uncheckedIds,
        items: {},
      },
    })
  }

  return (
    <>
      <header className="px-[1.25rem] py-[1rem] bg-panel-raised flex items-center gap-[0.875rem]">
        <div className={toneIconContainerVariants({ tone: headerTone })}>
          {renderToneIcon(headerTone, 'w-[1.25em] h-[1.25em]')}
        </div>
        <div className="flex flex-col min-w-0">
          <h2 className="text-lg font-bold text-accent-bright leading-tight truncate">
            {modal.title || 'Configure Scene'}
          </h2>
          {modal.subtitle && (
            <span className="text-xs text-foreground-muted truncate mt-[0.125rem]">
              {modal.subtitle}
            </span>
          )}
        </div>
      </header>
      <Separator />

      <div className="p-[1.25rem] flex-1 min-h-0 overflow-y-auto flex flex-col gap-[1rem]">
        <div className="grid grid-cols-1 sm:grid-cols-2 gap-[0.75rem]">
          <div className="flex flex-col gap-[0.375rem]">
            <label className="text-xs font-semibold uppercase tracking-wide text-foreground-secondary">
              Scene Type
            </label>
            <SelectInput
              value={sceneType}
              options={SCENE_TYPE_OPTIONS}
              onChange={handleSceneTypeChange}
              className="w-full"
            />
          </div>

          <div className="flex flex-col gap-[0.375rem]">
            <label className="text-xs font-semibold uppercase tracking-wide text-foreground-secondary">
              Scene Intent
            </label>
            <SelectInput
              value={intent}
              options={intentOptions}
              onChange={setIntent}
              className="w-full"
            />
          </div>
        </div>

        {intent === 'ai' && (
          <div className={infoBoxVariants({ tone: 'ai' })}>
            <Bot className="w-[1.25em] h-[1.25em] shrink-0 mt-[0.125rem] text-accent-bright" />
            <div className="flex flex-col gap-[0.25rem]">
              <span className="font-semibold text-xs text-foreground-primary">
                Delegate to SkyrimNet AI
              </span>
              <p className="text-xs leading-normal text-foreground-secondary">
                SkyrimNet will evaluate the scene context and assign participant intent and primary/secondary roles automatically.
              </p>
            </div>
          </div>
        )}

        {intent !== 'ai' && actors.length === 1 && (
          <div className={infoBoxVariants({ tone: headerTone })}>
            <Info className="w-[1.25em] h-[1.25em] shrink-0 mt-[0.125rem] text-accent" />
            <p className="text-xs leading-normal text-foreground-secondary">
              {actors[0].name} will be configured as the sole participant with{' '}
              <span className="font-semibold text-foreground-primary">{intent}</span> intent.
            </p>
          </div>
        )}

        {needsRoleSelection && (
          <div className="flex flex-col gap-[0.5rem]">
            <div className="flex items-center justify-between gap-[0.5rem]">
              <span className="text-xs font-semibold uppercase tracking-wide text-foreground-secondary">
                Select {roleLabel}
              </span>
              <span
                className={cn(
                  'px-[0.5rem] py-[0.125rem] rounded-full bg-panel-inset border text-xs font-semibold',
                  roleCountInvalid
                    ? 'border-status-danger text-status-danger'
                    : 'border-border-subtle text-accent-bright',
                )}
              >
                {checkedIds.length} / {maxPrimary} Max Primary
              </span>
            </div>
            <p className="text-xs text-foreground-muted leading-normal">
              Choose 1 to {maxPrimary} participant(s) as {roleLabel}. Unchecked participants take secondary roles.
            </p>

            <div className="max-h-[16rem] overflow-y-auto pr-[0.25rem] flex flex-col gap-[0.5rem]">
              {actors.map((actor) => {
                const isPrimary = checkedIds.includes(actor.id)
                return (
                  <div
                    key={actor.id}
                    onClick={() => toggleActor(actor.id)}
                    className={cn(
                      'flex items-center justify-between gap-[0.75rem] p-[0.75rem] rounded-md border transition-colors cursor-pointer focus-within:border-border-accent focus-within:shadow-glow',
                      isPrimary
                        ? 'bg-accent-faint border-border-strong'
                        : 'bg-panel-inset border-border-divider hover:border-border-subtle',
                    )}
                  >
                    <div className="flex items-center gap-[0.75rem] min-w-0 flex-1">
                      <div onClick={(e) => e.stopPropagation()}>
                        <Checkbox
                          checked={isPrimary}
                          onCheckedChange={() => toggleActor(actor.id)}
                        />
                      </div>
                      <span
                        className={cn(
                          'text-sm font-semibold truncate',
                          isPrimary ? 'text-foreground-primary' : 'text-foreground-secondary',
                        )}
                      >
                        {actor.name}
                      </span>
                    </div>
                    <span
                      className={itemBadgeVariants({
                        tone: isPrimary ? 'accent' : 'default',
                      })}
                    >
                      {isPrimary ? 'Primary' : 'Secondary'}
                    </span>
                  </div>
                )
              })}
            </div>
          </div>
        )}
      </div>

      <Separator />
      <footer className="px-[1.25rem] py-[0.875rem] bg-panel-raised flex items-center justify-between gap-[0.75rem] flex-wrap">
        <span className="text-xs text-foreground-muted select-none">
          Navigate: WASD / Arrows / Tab &bull; Select: Enter / Space / E
        </span>
        <div className="flex items-center justify-end gap-[0.625rem] flex-wrap ml-auto">
          <Button
            type="button"
            data-modal-footer-btn="true"
            data-modal-btn-index={0}
            variant="default"
            disabled={confirmDisabled}
            onClick={handleConfirm}
          >
            Confirm
          </Button>
          <Button
            type="button"
            data-modal-footer-btn="true"
            data-modal-btn-index={1}
            variant="secondary"
            disabled={isSubmitting}
            onClick={() => onSubmit('cancel', 1)}
          >
            Cancel
          </Button>
        </div>
      </footer>
    </>
  )
}

registerModalTemplate('external_thread_setup', ExternalThreadSetupModal)
