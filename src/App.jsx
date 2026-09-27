import { useEffect, useState } from 'react'
import Navbar from './components/Navbar'
import Hero from './components/Hero'
import Philosophy from './components/Philosophy'
import Features from './components/Features'
import Platforms from './components/Platforms'
import DownloadCta from './components/DownloadCta'
import Community from './components/Community'
import Footer from './components/Footer'
import FloatingIsland from './components/FloatingIsland'
import './App.css'
import { LANGUAGES, defaultLanguage, languageLabels, translations } from './translations'

const routeNames = ['home', 'caracteristicas', 'plataformas', 'comunidad']

function getRouteFromHash() {
  if (typeof window === 'undefined') return 'home'
  const hash = window.location.hash.slice(1)
  return routeNames.includes(hash) ? hash : 'home'
}

function App() {
  const [route, setRoute] = useState(getRouteFromHash)
  const [language, setLanguage] = useState(() => {
    if (typeof window === 'undefined') return defaultLanguage
    const saved = window.localStorage.getItem('proyecthorLanguage')
    return saved && LANGUAGES.includes(saved) ? saved : defaultLanguage
  })
  const strings = translations[language]

  useEffect(() => {
    const onHashChange = () => setRoute(getRouteFromHash())
    window.addEventListener('hashchange', onHashChange)
    return () => window.removeEventListener('hashchange', onHashChange)
  }, [])

  useEffect(() => {
    if (typeof window !== 'undefined') {
      window.localStorage.setItem('proyecthorLanguage', language)
    }
  }, [language])

  return (
    <>
      <Navbar
        strings={strings}
        language={language}
        setLanguage={setLanguage}
        labels={languageLabels}
      />
      <main>
        {route === 'home' && (
          <>
            <Hero strings={strings} />
            <section className="section video-demo">
              <div className="container video-demo-copy">
                <div className="video-demo-wrapper">
                  <iframe
                    src="https://www.youtube.com/embed/8mxghhCancw"
                    title="ProyecThor demo 0.4.0"
                    frameBorder="0"
                    allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture"
                    allowFullScreen
                  />
                </div>
              </div>
            </section>
            <Philosophy strings={strings} />
            <DownloadCta strings={strings} />
          </>
        )}

        {route === 'caracteristicas' && <Features strings={strings} />}
        {route === 'plataformas' && <Platforms strings={strings} />}
        {route === 'comunidad' && <Community strings={strings} />}
      </main>
      <Footer strings={strings} />
      <FloatingIsland strings={strings} />
    </>
  )
}

export default App
