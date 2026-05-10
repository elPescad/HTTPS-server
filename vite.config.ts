import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'

// https://vitejs.dev/config/
export default defineConfig({
  server: {
    // Allows the ngrok URL to access the dev server
    allowedHosts: ['.ngrok-free.app'], 
    hmr: {
      // Connect HMR to the secure ngrok tunnel port
      clientPort: 443, 
    },
  },
  plugins: [
    react(),
    tailwindcss(),
  ],
})