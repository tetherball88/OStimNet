import React from 'react'
import {
  Tooltip,
  TooltipTrigger,
  TooltipContent,
} from '@/components/ui/tooltip'

interface AppTooltipProps {
  content?: string | React.ReactNode
  children: React.ReactElement
  side?: 'top' | 'right' | 'bottom' | 'left'
}

export const AppTooltip: React.FC<AppTooltipProps> = ({
  content,
  children,
  side = 'top',
}) => {
  if (!content) return children

  return (
    <Tooltip>
      <TooltipTrigger asChild>{children}</TooltipTrigger>
      <TooltipContent side={side}>{content}</TooltipContent>
    </Tooltip>
  )
}
