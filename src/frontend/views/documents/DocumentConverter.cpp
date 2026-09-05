#include "DocumentConverter.h"

#include <fpdfview.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <vector>

#include "stb_image_write.h"

namespace fs = std::filesystem;

namespace ProyecThor::UI {

    namespace {
        struct PDFiumLibrary {
            PDFiumLibrary()  { FPDF_InitLibrary(); }
            ~PDFiumLibrary() { FPDF_DestroyLibrary(); }
        };

        PDFiumLibrary& GetPDFiumLibrary() {
            static PDFiumLibrary lib;
            return lib;
        }
    }

    DocumentType DocumentConverter::DetectType(const std::string& filePath) {
        std::string ext = fs::path(filePath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".pdf")  return DocumentType::PDF;
        if (ext == ".pptx") return DocumentType::PPTX;
        if (ext == ".ppt")  return DocumentType::PPTX;
        if (ext == ".odp")  return DocumentType::PPTX;

        return DocumentType::Unknown;
    }

    std::string DocumentConverter::BuildCacheKey(const std::string& filePath) {
        std::error_code ec;
        auto lastWrite = fs::last_write_time(filePath, ec);
        auto fileSize  = fs::file_size(filePath, ec);

        std::ostringstream oss;
        oss << filePath << "_" << fileSize << "_"
            << lastWrite.time_since_epoch().count();

        std::string raw = oss.str();
        uint64_t hash = 5381;
        for (unsigned char c : raw) {
            hash = ((hash << 5) + hash) + c;
        }

        std::ostringstream result;
        result << std::hex << hash;
        return result.str();
    }

    void DocumentConverter::ClearCache(const std::string& filePath, const std::string& cacheDir) {
        std::string key      = BuildCacheKey(filePath);
        fs::path    cacheKey = fs::path(cacheDir) / key;

        std::error_code ec;
        if (fs::exists(cacheKey, ec)) {
            fs::remove_all(cacheKey, ec);
        }
    }

    ConversionResult DocumentConverter::Convert(
        const std::string& filePath,
        const std::string& cacheDir,
        int                dpi,
        ProgressCallback   onProgress)
    {
        ConversionResult result;

        if (!fs::exists(filePath)) {
            result.success      = false;
            result.errorMessage = "Archivo no encontrado: " + filePath;
            return result;
        }

        DocumentType type = DetectType(filePath);
        if (type == DocumentType::Unknown) {
            result.success      = false;
            result.errorMessage = "Tipo de archivo no soportado: " + filePath;
            return result;
        }

        std::string cacheKey  = BuildCacheKey(filePath);
        fs::path    outputDir = fs::path(cacheDir) / cacheKey;

        if (fs::exists(outputDir)) {
            std::vector<fs::directory_entry> entries;
            for (auto& entry : fs::directory_iterator(outputDir)) {
                if (entry.path().extension() == ".png") {
                    entries.push_back(entry);
                }
            }

            std::sort(entries.begin(), entries.end(),
                [](const fs::directory_entry& a, const fs::directory_entry& b) {
                    return a.path().filename().string() < b.path().filename().string();
                });

            std::vector<std::string> cached;
            for (auto& entry : entries) {
                cached.push_back(entry.path().string());
            }

            if (!cached.empty()) {
                result.success   = true;
                result.pagePaths = cached;
                return result;
            }

            std::error_code ec;
            fs::remove_all(outputDir, ec);
        }

        std::error_code ec;
        fs::create_directories(outputDir, ec);
        if (ec) {
            result.success      = false;
            result.errorMessage = "No se pudo crear directorio de cache: " + ec.message();
            return result;
        }

        std::string outDir = outputDir.string();

        if (type == DocumentType::PDF) {
            return ConvertPDF(filePath, outDir, dpi, onProgress);
        } else {
            return ConvertPPTX(filePath, outDir, dpi, onProgress);
        }
    }

    ConversionResult DocumentConverter::ConvertPDF(
        const std::string& filePath,
        const std::string& outputDir,
        int                dpi,
        ProgressCallback   onProgress)
    {
        ConversionResult result;

        GetPDFiumLibrary();

        FPDF_DOCUMENT doc = FPDF_LoadDocument(filePath.c_str(), nullptr);
        if (!doc) {
            unsigned long err = FPDF_GetLastError();
            result.success      = false;
            result.errorMessage = "PDFium no pudo abrir el PDF: " + filePath +
                                  " (código de error: " + std::to_string(err) + ")";
            return result;
        }

        int totalPages = FPDF_GetPageCount(doc);
        if (totalPages <= 0) {
            FPDF_CloseDocument(doc);
            result.success      = false;
            result.errorMessage = "El PDF no contiene páginas.";
            return result;
        }

        const float scale = static_cast<float>(dpi) / 72.0f;

        for (int i = 0; i < totalPages; i++) {
            if (onProgress) {
                onProgress(i + 1, totalPages);
            }

            FPDF_PAGE page = FPDF_LoadPage(doc, i);
            if (!page) {
                continue;
            }

            int pageW = static_cast<int>(FPDF_GetPageWidth(page)  * scale);
            int pageH = static_cast<int>(FPDF_GetPageHeight(page) * scale);

            std::vector<unsigned char> buffer(pageW * pageH * 4, 0xFF);

            FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(
                pageW, pageH,
                FPDFBitmap_BGRA,
                buffer.data(),
                pageW * 4
            );

            FPDFBitmap_FillRect(bitmap, 0, 0, pageW, pageH, 0xFFFFFFFF);

            FPDF_RenderPageBitmap(
                bitmap, page,
                0, 0, pageW, pageH,
                0,
                FPDF_ANNOT
            );

            FPDFBitmap_Destroy(bitmap);
            FPDF_ClosePage(page);

            for (int px = 0; px < pageW * pageH; px++) {
                std::swap(buffer[px * 4 + 0], buffer[px * 4 + 2]);
            }

            std::ostringstream oss;
            oss << outputDir << "/page_" << std::setw(5) << std::setfill('0') << (i + 1) << ".png";
            std::string outPath = oss.str();

            if (!stbi_write_png(outPath.c_str(), pageW, pageH, 4, buffer.data(), pageW * 4)) {
                FPDF_CloseDocument(doc);
                result.success      = false;
                result.errorMessage = "Error al escribir PNG: " + outPath;
                return result;
            }

            result.pagePaths.push_back(outPath);
        }

        FPDF_CloseDocument(doc);
        result.success = true;
        return result;
    }

    std::string DocumentConverter::FindLibreOfficeBinary() {
        const std::vector<std::string> candidates = {
            "soffice",
            "/usr/bin/soffice",
            "/usr/lib/libreoffice/program/soffice",
            "/opt/libreoffice/program/soffice",
            "/Applications/LibreOffice.app/Contents/MacOS/soffice",
            "C:\\Program Files\\LibreOffice\\program\\soffice.exe",
            "C:\\Program Files (x86)\\LibreOffice\\program\\soffice.exe"
        };

        for (const auto& candidate : candidates) {
            std::string testCmd = "\"" + candidate + "\" --version > /dev/null 2>&1";
            if (std::system(testCmd.c_str()) == 0) {
                return candidate;
            }
        }

        return "";
    }

    ConversionResult DocumentConverter::ConvertPPTX(
        const std::string& filePath,
        const std::string& outputDir,
        int                dpi,
        ProgressCallback   onProgress)
    {
        ConversionResult result;

        std::string libreOfficeBin = FindLibreOfficeBinary();
        if (libreOfficeBin.empty()) {
            result.success      = false;
            result.errorMessage = "LibreOffice no encontrado. Instala LibreOffice para abrir archivos PPTX.";
            return result;
        }

        std::ostringstream cmdStream;
        cmdStream << "\"" << libreOfficeBin << "\""
                  << " --headless"
                  << " --convert-to pdf"
                  << " --outdir \"" << outputDir << "\""
                  << " \"" << filePath << "\""
                  << " > /dev/null 2>&1";

        std::string cmd     = cmdStream.str();
        int         exitCode = std::system(cmd.c_str());

        if (exitCode != 0) {
            result.success      = false;
            result.errorMessage = "LibreOffice fallo al convertir el archivo. Código: " + std::to_string(exitCode);
            return result;
        }

        fs::path    inputPath(filePath);
        std::string pdfName = inputPath.stem().string() + ".pdf";
        std::string pdfPath = (fs::path(outputDir) / pdfName).string();

        if (!fs::exists(pdfPath)) {
            result.success      = false;
            result.errorMessage = "LibreOffice no genero el PDF esperado en: " + pdfPath;
            return result;
        }

        ConversionResult pdfResult = ConvertPDF(pdfPath, outputDir, dpi, onProgress);

        std::error_code ec;
        fs::remove(pdfPath, ec);

        return pdfResult;
    }

}
