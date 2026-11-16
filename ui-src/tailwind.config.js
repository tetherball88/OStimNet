/** @type {import('tailwindcss').Config} */
export default {
  content: [
    './index.html',
    './src/**/*.{js,ts,jsx,tsx}',
  ],
  theme: {
    extend: {
      colors: {
        window: 'var(--window-bg)',
        panel: {
          DEFAULT: 'var(--panel-bg)',
          raised: 'var(--panel-raised-bg)',
          inset: 'var(--inset-bg)',
        },
        accent: {
          DEFAULT: 'var(--accent)',
          bright: 'var(--accent-bright)',
          deep: 'var(--accent-deep)',
          soft: 'var(--accent-soft)',
          faint: 'var(--accent-faint)',
        },
        'border-divider': 'var(--border-divider)',
        'border-subtle': 'var(--border-subtle)',
        'border-strong': 'var(--border-strong)',
        'border-accent': 'var(--border-accent)',
        foreground: {
          DEFAULT: 'var(--text-primary)',
          primary: 'var(--text-primary)',
          secondary: 'var(--text-secondary)',
          muted: 'var(--text-muted)',
          disabled: 'var(--text-disabled)',
          onAccent: 'var(--text-on-accent)',
        },
        status: {
          danger: 'var(--status-danger)',
          'danger-soft': 'var(--status-danger-soft)',
          success: 'var(--status-success)',
          'success-soft': 'var(--status-success-soft)',
          warning: 'var(--status-warning)',
          'warning-soft': 'var(--status-warning-soft)',
        },
      },
      borderRadius: {
        sm: 'var(--radius-sm)',
        DEFAULT: 'var(--radius-md)',
        md: 'var(--radius-md)',
        lg: 'var(--radius-lg)',
        full: 'var(--radius-full)',
      },
      fontFamily: {
        ui: ['var(--font-ui)', 'sans-serif'],
      },
      fontSize: {
        xs: 'var(--text-xs)',
        sm: 'var(--text-sm)',
        base: 'var(--text-base)',
        lg: 'var(--text-lg)',
        xl: 'var(--text-xl)',
        '2xl': 'var(--text-2xl)',
      },
      boxShadow: {
        window: 'var(--shadow-window)',
        raised: 'var(--shadow-raised)',
        glow: 'var(--shadow-glow)',
      },
      zIndex: {
        overlay: 'var(--z-overlay)',
        window: 'var(--z-window)',
        modal: 'var(--z-modal)',
        dropdown: 'var(--z-dropdown)',
        tooltip: 'var(--z-tooltip)',
      },
    },
  },
  plugins: [],
}
