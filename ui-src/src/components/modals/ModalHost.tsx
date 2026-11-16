import React, { useEffect, useRef, useState, useCallback } from 'react'
import { cva } from 'class-variance-authority'
import {
  Sparkles,
  AlertTriangle,
  ShieldAlert,
  CheckCircle2,
  Bot,
  Info,
} from 'lucide-react'
import { useModalStore } from '@/store/useModalStore'
import {
  ModalPayload,
  ModalControl,
  ModalChecklistItem,
  ModalChecklistValue,
  ModalTone,
} from '@/types/modal'
import { getModalTemplate } from './modalRegistry'
import { Button } from '@/components/ui/button'
import { Checkbox } from '@/components/ui/checkbox'
import { Input } from '@/components/ui/input'
import { Separator } from '@/components/ui/separator'
import { SelectInput } from '@/components/shared/SelectInput'
import { SearchInput } from '@/components/shared/SearchInput'
import { AppTooltip } from '@/components/shared/AppTooltip'
import { isEnterKey, isEscapeKey } from '@/utils/keys'
import { cn } from '@/lib/utils'

export const modalContainerVariants = cva(
  'w-full max-h-[86vh] rounded-lg border border-border-strong bg-window shadow-window flex flex-col overflow-hidden relative text-foreground-primary font-ui focus:outline-none',
  {
    variants: {
      size: {
        sm: 'max-w-[28rem]',
        md: 'max-w-[36rem]',
        lg: 'max-w-[48rem]',
        xl: 'max-w-[58rem]',
      },
    },
    defaultVariants: {
      size: 'md',
    },
  },
)

export const toneIconContainerVariants = cva(
  'p-[0.5rem] rounded-full border flex items-center justify-center shrink-0',
  {
    variants: {
      tone: {
        default: 'bg-accent-soft text-accent border-border-accent',
        ai: 'bg-accent-soft text-accent-bright border-border-accent',
        warning: 'bg-status-warning-soft text-status-warning border-status-warning',
        danger: 'bg-status-danger-soft text-status-danger border-status-danger',
        success: 'bg-status-success-soft text-status-success border-status-success',
      },
    },
    defaultVariants: {
      tone: 'default',
    },
  },
)

export const infoBoxVariants = cva(
  'p-[0.75rem] rounded-md border flex items-start gap-[0.625rem] text-xs',
  {
    variants: {
      tone: {
        default: 'bg-panel-inset border-border-subtle text-foreground-secondary',
        ai: 'bg-accent-faint border-border-accent text-foreground-primary',
        warning: 'bg-status-warning-soft border-status-warning text-foreground-primary',
        danger: 'bg-status-danger-soft border-status-danger text-foreground-primary',
        success: 'bg-status-success-soft border-status-success text-foreground-primary',
      },
    },
    defaultVariants: {
      tone: 'default',
    },
  },
)

export const itemBadgeVariants = cva(
  'px-[0.5rem] py-[0.125rem] rounded-sm text-xs font-semibold border shrink-0',
  {
    variants: {
      tone: {
        default: 'bg-panel-inset text-foreground-secondary border-border-subtle',
        accent: 'bg-accent-soft text-accent-bright border-border-accent',
        warning: 'bg-status-warning-soft text-status-warning border-status-warning',
        danger: 'bg-status-danger-soft text-status-danger border-status-danger',
        success: 'bg-status-success-soft text-status-success border-status-success',
      },
    },
    defaultVariants: {
      tone: 'default',
    },
  },
)

export function renderToneIcon(tone: ModalTone = 'default', className = 'w-[1.25em] h-[1.25em]') {
  switch (tone) {
    case 'warning':
      return <AlertTriangle className={className} />
    case 'danger':
      return <ShieldAlert className={className} />
    case 'success':
      return <CheckCircle2 className={className} />
    case 'ai':
      return <Bot className={className} />
    case 'default':
    default:
      return <Sparkles className={className} />
  }
}

interface ModalChecklistRowProps {
  controlId: string
  item: ModalChecklistItem
  checked: boolean
  selectValue?: string
  onToggle: (controlId: string, itemId: string) => void
  onSelectChange: (controlId: string, itemId: string, val: string) => void
}

const ModalChecklistRow = React.memo<ModalChecklistRowProps>(
  ({ controlId, item, checked, selectValue, onToggle, onSelectChange }) => {
    const handleRowClick = () => {
      if (!item.checkboxDisabled) {
        onToggle(controlId, item.id)
      }
    }

    const handleCheckboxChange = () => {
      if (!item.checkboxDisabled) {
        onToggle(controlId, item.id)
      }
    }

    const handleSelectValueChange = (val: string) => {
      onSelectChange(controlId, item.id, val)
    }

    const hasSelect = Boolean(item.selectOptions && item.selectOptions.length > 0)

    return (
      <div
        onClick={handleRowClick}
        className={cn(
          'flex items-start justify-between gap-[0.75rem] p-[0.75rem] rounded-md border transition-colors focus-within:border-border-accent focus-within:shadow-glow',
          checked
            ? 'bg-accent-faint border-border-strong'
            : 'bg-panel-inset border-border-divider hover:border-border-subtle',
          !item.checkboxDisabled && 'cursor-pointer',
        )}
      >
        <div className="flex items-start gap-[0.75rem] min-w-0 flex-1">
          <div className="pt-[0.125rem]" onClick={(e) => e.stopPropagation()}>
            <Checkbox
              checked={checked}
              disabled={item.checkboxDisabled}
              onCheckedChange={handleCheckboxChange}
            />
          </div>

          <div className="flex flex-col gap-[0.25rem] min-w-0 flex-1">
            <div className="flex items-center gap-[0.5rem] flex-wrap">
              <span
                className={cn(
                  'text-sm font-semibold truncate',
                  checked ? 'text-foreground-primary' : 'text-foreground-secondary',
                )}
              >
                {item.label}
              </span>
              {item.badgeText && (
                <span
                  className={itemBadgeVariants({
                    tone: item.badgeTone || 'default',
                  })}
                >
                  {item.badgeText}
                </span>
              )}
            </div>

            {item.subtitle && (
              <span className="text-xs text-foreground-muted leading-tight">
                {item.subtitle}
              </span>
            )}

            {item.description && (
              <p className="text-xs text-foreground-secondary leading-normal mt-[0.125rem]">
                {item.description}
              </p>
            )}
          </div>
        </div>

        {hasSelect && item.selectOptions && (
          <div
            className="flex items-center gap-[0.5rem] shrink-0 self-center"
            onClick={(e) => e.stopPropagation()}
          >
            {item.selectLabel && (
              <span className="text-xs text-foreground-muted">{item.selectLabel}</span>
            )}
            <SelectInput
              value={selectValue ?? item.selectValue ?? item.selectOptions[0]?.value ?? ''}
              options={item.selectOptions}
              onChange={handleSelectValueChange}
              className="min-w-[8rem]"
            />
          </div>
        )}
      </div>
    )
  },
)
ModalChecklistRow.displayName = 'ModalChecklistRow'

interface ChecklistControlSectionProps {
  control: ModalControl
}

const ChecklistControlSection: React.FC<ChecklistControlSectionProps> = ({ control }) => {
  const checklistValue = useModalStore(
    (s) => s.controlValues[control.id] as ModalChecklistValue | undefined,
  )
  const toggleChecklistItem = useModalStore((s) => s.toggleChecklistItem)
  const setAllChecklistItems = useModalStore((s) => s.setAllChecklistItems)
  const setChecklistItemSelectValue = useModalStore((s) => s.setChecklistItemSelectValue)

  const [filterQuery, setFilterQuery] = useState('')

  const items = control.items || []
  const checkedCount = checklistValue?.checkedIds.length ?? 0
  const totalCount = items.length
  const allChecked = totalCount > 0 && checkedCount === totalCount

  const filteredItems =
    filterQuery.trim().length === 0
      ? items
      : items.filter((item) => {
          const q = filterQuery.toLowerCase()
          return (
            item.label.toLowerCase().includes(q) ||
            (item.subtitle && item.subtitle.toLowerCase().includes(q)) ||
            (item.description && item.description.toLowerCase().includes(q)) ||
            (item.badgeText && item.badgeText.toLowerCase().includes(q))
          )
        })

  const handleToggleItem = useCallback(
    (ctrlId: string, itemId: string) => {
      toggleChecklistItem(ctrlId, itemId)
    },
    [toggleChecklistItem],
  )

  const handleSelectChange = useCallback(
    (ctrlId: string, itemId: string, val: string) => {
      setChecklistItemSelectValue(ctrlId, itemId, val)
    },
    [setChecklistItemSelectValue],
  )

  return (
    <div className="flex flex-col gap-[0.5rem]">
      <div className="flex items-center justify-between gap-[0.5rem]">
        <div className="flex items-center gap-[0.5rem]">
          {control.label && (
            <span className="text-xs font-semibold uppercase tracking-wide text-foreground-secondary">
              {control.label}
            </span>
          )}
          <span className="px-[0.5rem] py-[0.125rem] rounded-full bg-panel-inset border border-border-subtle text-xs text-accent-bright font-semibold">
            {checkedCount} / {totalCount} Selected
          </span>
        </div>

        {control.showSelectAllToggle !== false && totalCount > 1 && (
          <Button
            type="button"
            variant="ghost"
            size="sm"
            onClick={() => setAllChecklistItems(control.id, !allChecked)}
            className="text-xs h-[1.75rem] px-[0.625rem]"
          >
            {allChecked ? 'Deselect All' : 'Select All'}
          </Button>
        )}
      </div>

      {control.description && (
        <p className="text-xs text-foreground-muted leading-normal">{control.description}</p>
      )}

      {items.length > 8 && (
        <SearchInput
          value={filterQuery}
          onChange={setFilterQuery}
          placeholder="Filter items..."
        />
      )}

      <div className="max-h-[20rem] overflow-y-auto pr-[0.25rem] flex flex-col gap-[0.5rem]">
        {filteredItems.length === 0 ? (
          <div className="p-[1rem] rounded-md bg-panel-inset border border-border-divider text-center text-xs text-foreground-muted">
            {control.emptyMessage || 'No matching items found.'}
          </div>
        ) : (
          filteredItems.map((item) => {
            const itemState = checklistValue?.items[item.id]
            const isChecked = itemState?.checked ?? Boolean(item.checked)
            const currentSelectVal = itemState?.selectValue ?? item.selectValue
            return (
              <ModalChecklistRow
                key={item.id}
                controlId={control.id}
                item={item}
                checked={isChecked}
                selectValue={currentSelectVal}
                onToggle={handleToggleItem}
                onSelectChange={handleSelectChange}
              />
            )
          })
        )}
      </div>
    </div>
  )
}

interface ScalarControlSectionProps {
  control: ModalControl
}

const ScalarControlSection: React.FC<ScalarControlSectionProps> = ({ control }) => {
  const value = useModalStore((s) => s.controlValues[control.id])
  const setControlValue = useModalStore((s) => s.setControlValue)

  if (control.type === 'infobox') {
    const tone = control.tone || 'default'
    return (
      <div className={infoBoxVariants({ tone })}>
        <Info className="w-[1.25em] h-[1.25em] shrink-0 mt-[0.125rem] text-accent" />
        <div className="flex flex-col gap-[0.25rem]">
          {control.label && (
            <span className="font-semibold text-xs text-foreground-primary">
              {control.label}
            </span>
          )}
          {control.description && (
            <p className="text-xs leading-normal text-foreground-secondary whitespace-pre-line">
              {control.description}
            </p>
          )}
        </div>
      </div>
    )
  }

  if (control.type === 'checkbox') {
    const checked = Boolean(value)
    return (
      <div
        onClick={() => {
          if (!control.disabled) {
            setControlValue(control.id, !checked)
          }
        }}
        className={cn(
          'flex items-start gap-[0.75rem] p-[0.75rem] rounded-md border transition-colors focus-within:border-border-accent focus-within:shadow-glow',
          checked
            ? 'bg-accent-faint border-border-strong'
            : 'bg-panel-inset border-border-subtle hover:border-border-strong',
          !control.disabled && 'cursor-pointer',
        )}
      >
        <div className="pt-[0.125rem]" onClick={(e) => e.stopPropagation()}>
          <Checkbox
            checked={checked}
            disabled={control.disabled}
            onCheckedChange={(next) => setControlValue(control.id, next)}
          />
        </div>
        <div className="flex flex-col gap-[0.125rem]">
          {control.label && (
            <span className="text-sm font-medium text-foreground-primary">
              {control.label}
            </span>
          )}
          {control.description && (
            <span className="text-xs text-foreground-muted leading-normal">
              {control.description}
            </span>
          )}
        </div>
      </div>
    )
  }

  if (control.type === 'select') {
    const strVal = value !== undefined && value !== null ? String(value) : ''
    return (
      <div className="flex flex-col gap-[0.375rem]">
        {control.label && (
          <label className="text-xs font-semibold text-foreground-secondary">
            {control.label}
          </label>
        )}
        {control.description && (
          <span className="text-xs text-foreground-muted">{control.description}</span>
        )}
        <SelectInput
          value={strVal}
          options={control.options || []}
          disabled={control.disabled}
          onChange={(val) => setControlValue(control.id, val)}
          className="w-full"
        />
      </div>
    )
  }

  if (control.type === 'text') {
    const strVal = value !== undefined && value !== null ? String(value) : ''
    return (
      <div className="flex flex-col gap-[0.375rem]">
        {control.label && (
          <label className="text-xs font-semibold text-foreground-secondary">
            {control.label}
          </label>
        )}
        {control.description && (
          <span className="text-xs text-foreground-muted">{control.description}</span>
        )}
        <Input
          value={strVal}
          disabled={control.disabled}
          placeholder={control.placeholder}
          onChange={(e) => setControlValue(control.id, e.target.value)}
        />
      </div>
    )
  }

  return null
}

interface GenericModalContentProps {
  modal: ModalPayload
}

const GenericModalContent: React.FC<GenericModalContentProps> = ({ modal }) => {
  const controlValues = useModalStore((s) => s.controlValues)
  const isSubmitting = useModalStore((s) => s.isSubmitting)
  const submitModal = useModalStore((s) => s.submitModal)

  const tone = modal.tone || 'default'
  const paragraphs = modal.message
    ? modal.message.split('\n').filter((line) => line.trim().length > 0)
    : []

  return (
    <>
      <header className="px-[1.25rem] py-[1rem] bg-panel-raised flex items-center gap-[0.875rem]">
        <div className={toneIconContainerVariants({ tone })}>
          {renderToneIcon(tone, 'w-[1.25em] h-[1.25em]')}
        </div>
        <div className="flex flex-col min-w-0">
          <h2 className="text-lg font-bold text-accent-bright leading-tight truncate">
            {modal.title || 'OStimNet Confirmation'}
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
        {paragraphs.length > 0 && (
          <div className="flex flex-col gap-[0.5rem]">
            {paragraphs.map((p, idx) => (
              <p
                key={idx}
                className="text-base text-foreground-primary leading-normal"
              >
                {p}
              </p>
            ))}
          </div>
        )}

        {modal.controls && modal.controls.length > 0 && (
          <div className="flex flex-col gap-[0.875rem]">
            {modal.controls.map((ctrl) =>
              ctrl.type === 'checklist' ? (
                <ChecklistControlSection key={ctrl.id} control={ctrl} />
              ) : (
                <ScalarControlSection key={ctrl.id} control={ctrl} />
              ),
            )}
          </div>
        )}
      </div>

      <Separator />
      <footer className="px-[1.25rem] py-[0.875rem] bg-panel-raised flex items-center justify-between gap-[0.75rem] flex-wrap">
        <span className="text-xs text-foreground-muted select-none">
          Navigate: WASD / Arrows / Tab &bull; Select: Enter / Space / E
        </span>
        <div className="flex items-center justify-end gap-[0.625rem] flex-wrap ml-auto">
          {modal.buttons.map((btn, idx) => {
            let requirementUnmet = false
            if (btn.requireChecklistId) {
              const checklist = controlValues[btn.requireChecklistId] as
                | ModalChecklistValue
                | undefined
              const checkedCount = checklist?.checkedIds.length ?? 0
              if ((btn.minCheckedCount ?? 0) > 0 && checkedCount < (btn.minCheckedCount ?? 0)) {
                requirementUnmet = true
              }
              if ((btn.maxCheckedCount ?? 0) > 0 && checkedCount > (btn.maxCheckedCount ?? 0)) {
                requirementUnmet = true
              }
            }

            const isDisabled = Boolean(btn.disabled || isSubmitting || requirementUnmet)
            const buttonEl = (
              <Button
                key={btn.id || idx}
                type="button"
                data-modal-footer-btn="true"
                data-modal-btn-index={idx}
                variant={btn.variant || (idx === 0 ? 'default' : 'secondary')}
                disabled={isDisabled}
                onClick={() => submitModal(btn.id, idx)}
              >
                {btn.label}
              </Button>
            )

            if (btn.tooltip) {
              return (
                <AppTooltip key={btn.id || idx} content={btn.tooltip}>
                  <span className="inline-flex">{buttonEl}</span>
                </AppTooltip>
              )
            }

            return buttonEl
          })}
        </div>
      </footer>
    </>
  )
}

export interface ModalHostProps {
  style?: React.CSSProperties
}

function getFocusableModalElements(container: HTMLElement | null): HTMLElement[] {
  if (!container) return []
  const nodes = container.querySelectorAll<HTMLElement>(
    'button:not(:disabled), input:not(:disabled), textarea:not(:disabled), select:not(:disabled), [tabindex]:not([tabindex="-1"]):not(:disabled)',
  )
  return Array.from(nodes).filter(
    (el) => !el.hasAttribute('disabled') && el.getAttribute('aria-hidden') !== 'true',
  )
}

function getFooterModalButtons(container: HTMLElement | null): HTMLElement[] {
  if (!container) return []
  const nodes = container.querySelectorAll<HTMLElement>(
    'button[data-modal-footer-btn="true"]:not(:disabled)',
  )
  return Array.from(nodes).filter((el) => !el.hasAttribute('disabled'))
}

function focusAndScrollIntoView(el: HTMLElement | null | undefined) {
  if (!el) return
  el.focus()
  el.scrollIntoView({ block: 'nearest' })
}

export const ModalHost: React.FC<ModalHostProps> = ({ style }) => {
  const activeModal = useModalStore((s) => s.activeModal)
  const controlValues = useModalStore((s) => s.controlValues)
  const submitModal = useModalStore((s) => s.submitModal)
  const modalRef = useRef<HTMLDivElement>(null)

  useEffect(() => {
    if (!activeModal) return

    const focusInitialElement = () => {
      const container = modalRef.current
      if (!container) return
      const footerBtns = getFooterModalButtons(container)
      if (footerBtns.length > 0) {
        focusAndScrollIntoView(footerBtns[0])
        return
      }
      const focusables = getFocusableModalElements(container)
      if (focusables.length > 0) {
        focusAndScrollIntoView(focusables[0])
        return
      }
      container.focus()
    }

    focusInitialElement()
    const rafId = window.requestAnimationFrame(focusInitialElement)

    const handleKeyDownCapture = (e: KeyboardEvent) => {
      const isListboxOpen = Boolean(document.querySelector('[role="listbox"]'))

      if (isEscapeKey(e)) {
        // Allow Escape to close an open Select dropdown, but never dismiss or unfocus the modal itself
        if (!isListboxOpen) {
          e.preventDefault()
          e.stopPropagation()
        }
        return
      }

      // When a Radix Select dropdown is open, let Radix handle Arrow/Enter/Space selection natively
      if (isListboxOpen) {
        return
      }

      const container = modalRef.current
      if (!container) return

      const activeEl = document.activeElement as HTMLElement | null
      const isTextInput =
        activeEl !== null &&
        (activeEl.tagName === 'INPUT' ||
          activeEl.tagName === 'TEXTAREA' ||
          activeEl.isContentEditable)

      const focusables = getFocusableModalElements(container)
      const footerBtns = getFooterModalButtons(container)

      const cycleFocus = (list: HTMLElement[], step: 1 | -1) => {
        if (list.length === 0) return
        const currentIdx = activeEl ? list.indexOf(activeEl) : -1
        let nextIdx: number
        if (currentIdx === -1) {
          nextIdx = step > 0 ? 0 : list.length - 1
        } else {
          nextIdx = (currentIdx + step + list.length) % list.length
        }
        focusAndScrollIntoView(list[nextIdx])
      }

      if (isTextInput) {
        if (e.key === 'Tab') {
          e.preventDefault()
          e.stopPropagation()
          cycleFocus(focusables, e.shiftKey ? -1 : 1)
        } else if (e.key === 'ArrowDown') {
          e.preventDefault()
          e.stopPropagation()
          cycleFocus(focusables, 1)
        } else if (e.key === 'ArrowUp') {
          e.preventDefault()
          e.stopPropagation()
          cycleFocus(focusables, -1)
        } else if (isEnterKey(e) && footerBtns.length > 0) {
          e.preventDefault()
          e.stopPropagation()
          focusAndScrollIntoView(footerBtns[0])
        }
        return
      }

      if (e.ctrlKey || e.altKey || e.metaKey) {
        return
      }

      if (e.key === 'Tab') {
        e.preventDefault()
        e.stopPropagation()
        cycleFocus(focusables, e.shiftKey ? -1 : 1)
        return
      }

      if (e.key === 'ArrowRight' || e.key === 'd' || e.key === 'D') {
        e.preventDefault()
        e.stopPropagation()
        const inFooter = activeEl?.getAttribute('data-modal-footer-btn') === 'true'
        cycleFocus(inFooter && footerBtns.length > 0 ? footerBtns : focusables, 1)
        return
      }

      if (e.key === 'ArrowLeft' || e.key === 'a' || e.key === 'A') {
        e.preventDefault()
        e.stopPropagation()
        const inFooter = activeEl?.getAttribute('data-modal-footer-btn') === 'true'
        cycleFocus(inFooter && footerBtns.length > 0 ? footerBtns : focusables, -1)
        return
      }

      if (e.key === 'ArrowDown' || e.key === 's' || e.key === 'S') {
        e.preventDefault()
        e.stopPropagation()
        cycleFocus(focusables, 1)
        return
      }

      if (e.key === 'ArrowUp' || e.key === 'w' || e.key === 'W') {
        e.preventDefault()
        e.stopPropagation()
        cycleFocus(focusables, -1)
        return
      }

      if (e.key === 'e' || e.key === 'E') {
        e.preventDefault()
        e.stopPropagation()
        if (activeEl && container.contains(activeEl) && activeEl !== container) {
          activeEl.click()
        } else if (footerBtns.length > 0) {
          focusAndScrollIntoView(footerBtns[0])
          footerBtns[0].click()
        }
        return
      }

      if (isEnterKey(e) || e.key === ' ') {
        if (!activeEl || !container.contains(activeEl) || activeEl === container) {
          e.preventDefault()
          e.stopPropagation()
          if (footerBtns.length > 0) {
            focusAndScrollIntoView(footerBtns[0])
            footerBtns[0].click()
          }
        }
      }
    }

    window.addEventListener('keydown', handleKeyDownCapture, true)

    return () => {
      window.cancelAnimationFrame(rafId)
      window.removeEventListener('keydown', handleKeyDownCapture, true)
    }
  }, [activeModal?.modalId])

  if (!activeModal) return null

  const CustomTemplate = getModalTemplate(activeModal.modalType)

  return (
    <div
      className="fixed inset-0 z-modal bg-overlay flex items-center justify-center p-[1rem] select-none font-ui bg-cover bg-center bg-no-repeat overflow-hidden"
      style={style}
    >
      <div
        ref={modalRef}
        tabIndex={-1}
        role="dialog"
        aria-modal="true"
        aria-label={activeModal.title}
        className={modalContainerVariants({ size: activeModal.size || 'md' })}
        onClick={(e) => e.stopPropagation()}
      >
        {CustomTemplate ? (
          <CustomTemplate
            modal={activeModal}
            values={controlValues}
            onSubmit={submitModal}
          />
        ) : (
          <GenericModalContent modal={activeModal} />
        )}
      </div>
    </div>
  )
}
