import React from 'react'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import {
  Tooltip,
  TooltipContent,
  TooltipTrigger,
} from '@/components/ui/tooltip'

export interface SelectOption {
  value: string | number
  label: string
  icon?: React.ReactNode
  disabled?: boolean
  tooltip?: string
}

interface SelectInputProps {
  value: string | number
  options: SelectOption[] | string[]
  disabled?: boolean
  onChange: (val: string) => void
  className?: string
}

export const SelectInput: React.FC<SelectInputProps> = ({
  value,
  options,
  disabled = false,
  onChange,
  className = '',
}) => {
  const normalizedOptions: SelectOption[] = options.map((opt) =>
    typeof opt === 'string' ? { value: opt, label: opt } : opt,
  )

  return (
    <Select
      value={value.toString()}
      disabled={disabled}
      onValueChange={onChange}
    >
      <SelectTrigger className={`w-auto min-w-[6rem] ${className}`}>
        <SelectValue />
      </SelectTrigger>
      <SelectContent>
        {normalizedOptions.map((opt, idx) => {
          const content = (
            <div className="flex items-center justify-between w-full gap-[0.5rem]">
              <div className="flex items-center gap-[0.375rem] min-w-0">
                {opt.icon}
                <span className="truncate">{opt.label}</span>
              </div>
              {opt.disabled && opt.tooltip && (
                <span className="text-[0.625rem] text-status-warning font-normal italic truncate max-w-[10rem] ml-auto">
                  {opt.tooltip}
                </span>
              )}
            </div>
          )

          const item = (
            <SelectItem
              key={idx}
              value={opt.value.toString()}
              disabled={opt.disabled}
              title={opt.tooltip}
              className={opt.disabled ? 'cursor-not-allowed opacity-50 text-foreground-disabled' : ''}
            >
              {content}
            </SelectItem>
          )

          if (opt.tooltip) {
            return (
              <Tooltip key={idx}>
                <TooltipTrigger asChild>{item}</TooltipTrigger>
                <TooltipContent side="right" className="max-w-[18rem] z-tooltip text-xs">
                  <p>{opt.tooltip}</p>
                </TooltipContent>
              </Tooltip>
            )
          }

          return item
        })}
      </SelectContent>
    </Select>
  )
}
