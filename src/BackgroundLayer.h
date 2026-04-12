#pragma once
#include "VLCBasePlayer.h"
#include <GL/glew.h>

namespace ProyecThor::Core {
    class BackgroundLayer {
    private:
        VLCBasePlayer m_Player;
        bool m_IsVideo = false;
        float m_BgColor[3] = {0.0f, 0.0f, 0.0f};

    public:
        void Update() { m_Player.UpdateTexture(); }
        // --- AÑADE ESTA FUNCIÓN ---
        void* GetTextureID() { return m_Player.GetTextureID(); }
        VLCBasePlayer* GetPlayer() { return &m_Player; }
        void SetVideo(const std::string& path) {
            m_IsVideo = true;
            m_Player.Play(path, true); // loop = true
        }

        void SetSolidColor(float r, float g, float b) {
            m_IsVideo = false;
            m_Player.Stop();
            m_BgColor[0] = r; m_BgColor[1] = g; m_BgColor[2] = b;
        }

        void Render(int vpW, int vpH) {
            if (m_IsVideo) {
                // ... (tu código de renderizado de quad)
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)m_Player.GetTextureID());
                // ... resto del render
                glBegin(GL_QUADS);
                    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
                    glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
                    glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
                    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
                glEnd();
                glBindTexture(GL_TEXTURE_2D, 0);
            } else {
                glClearColor(m_BgColor[0], m_BgColor[1], m_BgColor[2], 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
            }
        }
    };
}