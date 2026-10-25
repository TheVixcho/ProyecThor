# ProyecThor — sitio web

Landing page del proyecto (React + Vite), pensada para desplegarse en Firebase Hosting.

## Desarrollo local

```bash
npm install
npm run dev
```

## Compilar para producción

```bash
npm run build
```

Esto genera la carpeta `dist/` con los archivos estáticos listos para publicar. El
`firebase.json` de la raíz del repo ya apunta a `web/dist` como carpeta pública, así que no
hace falta tocar nada ahí.

## Desplegar a Firebase

Desde la **raíz del repositorio** (no desde `web/`):

```bash
npm run build --prefix web
firebase deploy --only hosting
```

(Requiere tener `firebase-tools` instalado y haber corrido `firebase login` una vez.)

## Estructura

- `src/components/` — una sección de la página por archivo (Navbar, Hero, Features, etc.)
- `src/links.js` — todas las URLs externas (GitHub, Discord, WhatsApp, Ko-fi) en un solo lugar
- `src/App.css` — estilos de toda la página (sin librerías de UI externas)
- `public/logo.png` — favicon y logo, tomado de `icon/iconweb.png` en la raíz del repo
