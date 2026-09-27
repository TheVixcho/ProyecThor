import './FloatingIsland.css'

function FloatingIsland({ strings }) {
  return (
    <div className="floating-island floating-island-arrow">
      <a href="#home" className="floating-island-link" aria-label={strings.floating.home}>
        ↑
      </a>
    </div>
  )
}

export default FloatingIsland
