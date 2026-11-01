#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace ProyecThor::Core::MuninDRM {

// Identificador mágico del contenedor protegido de ProyecThor (6 bytes)
constexpr char kMagic[6] = { 'P', 'T', 'E', 'N', 'C', 0x02 };

// Comprueba si los datos en memoria o archivo en disco tienen la cabecera PTENC
bool IsEncrypted(const uint8_t* data, size_t size);
bool IsEncryptedFile(const std::string& filePath);

// Cifra un buffer en memoria o un archivo a formato PTENC protegido
bool EncryptBuffer(const std::vector<uint8_t>& plainData, std::vector<uint8_t>& outEncrypted);
bool EncryptFile(const std::string& sourcePlainFile, const std::string& destEncryptedFile);

// Descifra un buffer PTENC o archivo a los bytes crudos (JPEG/PNG) en memoria
bool DecryptBuffer(const uint8_t* encData, size_t encSize, std::vector<uint8_t>& outPlainData);
bool DecryptFileToMemory(const std::string& encFilePath, std::vector<uint8_t>& outPlainData);

// Gestión y sincronización de clave con Firebase Firestore
void SetRemoteKey(const std::string& key);
std::string GetRemoteKey();
void SyncWithFirebaseAsync();

} // namespace ProyecThor::Core::MuninDRM
