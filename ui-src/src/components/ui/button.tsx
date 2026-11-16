import * as React from 'react'
import { Slot } from '@radix-ui/react-slot'
import { cva, type VariantProps } from 'class-variance-authority'
import { cn } from '@/lib/utils'

const buttonVariants = cva(
  'inline-flex items-center justify-center whitespace-nowrap rounded-md text-base font-medium transition-colors focus:outline-none focus-visible:outline-none focus:ring-2 focus:ring-accent-bright focus-visible:ring-2 focus-visible:ring-accent-bright focus:shadow-glow disabled:pointer-events-none disabled:opacity-50 border cursor-pointer',
  {
    variants: {
      variant: {
        default:
          'bg-accent text-foreground-onAccent hover:bg-accent-bright focus:bg-accent-bright border-accent-bright active:bg-accent-deep shadow-raised',
        secondary:
          'bg-panel-raised text-foreground-primary border-border-subtle hover:bg-accent-faint hover:border-border-strong hover:text-accent-bright focus:bg-accent-faint focus:border-border-accent focus:text-accent-bright',
        outline:
          'bg-transparent text-foreground-primary border-border-subtle hover:bg-accent-faint hover:border-border-accent hover:text-accent-bright focus:bg-accent-faint focus:border-border-accent focus:text-accent-bright',
        ghost:
          'bg-transparent text-foreground-secondary hover:bg-accent-faint hover:text-foreground-primary focus:bg-accent-faint focus:text-foreground-primary border-transparent',
        danger:
          'bg-status-danger-soft text-status-danger border-status-danger hover:bg-status-danger hover:text-foreground-primary focus:bg-status-danger focus:text-foreground-primary',
      },
      size: {
        default: 'h-[2.5rem] px-[1rem] py-[0.5rem]',
        sm: 'h-[2rem] px-[0.75rem] text-sm',
        lg: 'h-[3rem] px-[1.5rem] text-lg',
        icon: 'h-[2.25rem] w-[2.25rem]',
      },
    },
    defaultVariants: {
      variant: 'default',
      size: 'default',
    },
  },
)

export interface ButtonProps
  extends React.ButtonHTMLAttributes<HTMLButtonElement>,
    VariantProps<typeof buttonVariants> {
  asChild?: boolean
}

const Button = React.forwardRef<HTMLButtonElement, ButtonProps>(
  ({ className, variant, size, asChild = false, disabled, ...props }, ref) => {
    const Comp = asChild ? Slot : 'button'
    return (
      <Comp
        className={cn(buttonVariants({ variant, size, className }))}
        ref={ref}
        disabled={disabled}
        data-ml-sound={disabled ? undefined : 'click'}
        data-ml-sound-hover={disabled ? undefined : 'focus'}
        {...props}
      />
    )
  },
)
Button.displayName = 'Button'

export { Button, buttonVariants }
