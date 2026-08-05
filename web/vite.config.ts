import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

export default defineConfig(({ mode }) => ({
  plugins: [react()],
  // The WiFi build is served from the device's own root ("/"). The BLE
  // build is deployed to a GitHub Pages project page subpath, and relative
  // asset paths work there regardless of the actual repo/org name (this
  // app has no client-side routing, so relative paths are safe).
  base: mode === 'ble' ? './' : '/',
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
}))
