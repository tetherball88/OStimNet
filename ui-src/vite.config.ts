import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import { resolve, dirname } from 'path'
import { fileURLToPath } from 'url'

const __dirname = dirname(fileURLToPath(import.meta.url))

export default defineConfig({
  base: './',
  resolve: {
    alias: {
      '@': resolve(__dirname, './src'),
    },
  },
  plugins: [
    react(),
    {
      name: 'remove-module-attrs',
      apply: 'build',
      transformIndexHtml(html) {
        const scripts: string[] = []
        html = html
          .replace(/ type="module"/g, '')
          .replace(/ crossorigin/g, '')
          .replace(/<script\b[^>]*src="[^"]*"[^>]*><\/script>/g, (match) => {
            scripts.push(match)
            return ''
          })
        return html.replace('</body>', `  ${scripts.join('\n  ')}\n</body>`)
      },
    },
  ],
  build: {
    outDir: resolve(__dirname, '../Magelight/OStimNet/views/OStimNet'),
    emptyOutDir: true,
    minify: false,
    cssCodeSplit: false,
    rollupOptions: {
      output: {
        format: 'iife',
      },
    },
  },
})
