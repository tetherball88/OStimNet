export type ModalTone = 'default' | 'warning' | 'danger' | 'success' | 'ai'

export type ModalSize = 'sm' | 'md' | 'lg' | 'xl'

export type ModalButtonVariant = 'default' | 'secondary' | 'outline' | 'ghost' | 'danger'

export type ModalControlType = 'checkbox' | 'select' | 'text' | 'checklist' | 'infobox'

export interface ModalSelectOption {
  value: string
  label: string
  disabled?: boolean
  tooltip?: string
}

export interface ModalChecklistItem {
  id: string
  label: string
  subtitle?: string
  description?: string
  badgeText?: string
  badgeTone?: 'default' | 'accent' | 'warning' | 'danger' | 'success'
  checked?: boolean
  checkboxDisabled?: boolean
  selectKey?: string
  selectLabel?: string
  selectValue?: string
  selectOptions?: ModalSelectOption[]
  meta?: Record<string, any>
}

export interface ModalControl {
  type: ModalControlType
  id: string
  label?: string
  description?: string
  required?: boolean
  disabled?: boolean
  defaultValue?: boolean | string | number | null
  placeholder?: string
  options?: ModalSelectOption[]
  items?: ModalChecklistItem[]
  showSelectAllToggle?: boolean
  emptyMessage?: string
  tone?: ModalTone
}

export interface ModalButton {
  id: string
  label: string
  variant?: ModalButtonVariant
  disabled?: boolean
  tooltip?: string
  requireChecklistId?: string
  minCheckedCount?: number
  maxCheckedCount?: number
}

export interface ModalPayload {
  modalId: string
  modalType?: string
  title: string
  subtitle?: string
  message?: string
  tone?: ModalTone
  size?: ModalSize
  controls?: ModalControl[]
  buttons: ModalButton[]
  customData?: Record<string, any>
}

export interface ModalChecklistItemState {
  checked: boolean
  selectValue?: string
}

export interface ModalChecklistValue {
  checkedIds: string[]
  uncheckedIds: string[]
  items: Record<string, ModalChecklistItemState>
}

export interface ModalResponsePayload {
  modalId: string
  buttonId: string
  buttonIndex: number
  values: Record<string, any>
}
