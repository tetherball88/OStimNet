import { type ClassValue, clsx } from 'clsx'
import { twMerge } from 'tailwind-merge'

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs))
}

/** True when running inside the Magelight UI host with the C++ bridge attached. */
export const isInGame = (): boolean =>
  typeof window !== 'undefined' && Boolean(window.ostimnetAction)
