import * as React from 'react'
import { cva, type VariantProps } from 'class-variance-authority'
import { Check } from 'lucide-react'
import { cn } from '@/lib/utils'

const checkboxVariants = cva(
  'inline-flex items-center justify-center shrink-0 rounded-sm border transition-colors focus:outline-none focus:ring-2 focus:ring-accent-bright focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent-bright disabled:cursor-not-allowed disabled:opacity-50 cursor-pointer',
  {
    variants: {
      state: {
        unchecked:
          'bg-panel-inset border-border-strong hover:border-border-accent hover:bg-accent-faint text-transparent',
        checked:
          'bg-accent border-accent-bright text-foreground-onAccent shadow-glow',
      },
      size: {
        default: 'w-[1.125rem] h-[1.125rem]',
        sm: 'w-[0.9375rem] h-[0.9375rem]',
        lg: 'w-[1.25rem] h-[1.25rem]',
      },
    },
    defaultVariants: {
      state: 'unchecked',
      size: 'default',
    },
  },
)

export interface CheckboxProps
  extends Omit<React.ButtonHTMLAttributes<HTMLButtonElement>, 'onChange'>,
    Omit<VariantProps<typeof checkboxVariants>, 'state'> {
  checked?: boolean
  onCheckedChange?: (checked: boolean) => void
}

const Checkbox = React.forwardRef<HTMLButtonElement, CheckboxProps>(
  ({ className, checked = false, size, disabled, onCheckedChange, onClick, ...props }, ref) => {
    const handleClick = (e: React.MouseEvent<HTMLButtonElement>) => {
      onClick?.(e)
      if (!e.defaultPrevented && !disabled) {
        onCheckedChange?.(!checked)
      }
    }

    return (
      <button
        type="button"
        role="checkbox"
        aria-checked={checked}
        disabled={disabled}
        ref={ref}
        onClick={handleClick}
        data-ml-sound={disabled ? undefined : 'click'}
        data-ml-sound-hover={disabled ? undefined : 'focus'}
        className={cn(
          checkboxVariants({
            state: checked ? 'checked' : 'unchecked',
            size,
            className,
          }),
        )}
        {...props}
      >
        <Check
          className={cn(
            'w-[0.8em] h-[0.8em] stroke-[3] transition-opacity',
            checked ? 'opacity-100' : 'opacity-0',
          )}
        />
      </button>
    )
  },
)
Checkbox.displayName = 'Checkbox'

export { Checkbox, checkboxVariants }
