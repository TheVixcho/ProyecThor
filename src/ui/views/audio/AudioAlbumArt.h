#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <GL/glew.h>

namespace ProyecThor::Audio {

struct AlbumArt {
    std::vector<uint8_t> pixels;
    int                  width  = 0;
    int                  height = 0;
    GLuint               texID  = 0;

    bool HasData()    const { return !pixels.empty(); }
    bool HasTexture() const { return texID != 0; }
};

AlbumArt ExtractAlbumArt(const std::string& utf8FilePath);

void UploadAlbumArtToGL(AlbumArt& art);

void FreeAlbumArtTexture(AlbumArt& art);

}
