#include "windows.h"
#include "EterIndex.h"
#include "EterPack.h"
#include <string>
#include <iostream>
#include <vector>
#include <filesystem>
#pragma comment(lib, "liblzo")
namespace fs = std::filesystem;

#define PLIST EterIndexItem **
#define EINSTANCE UINT32
#define MODE_READ  0
#define MODE_WRITE 1

UINT32 g_index_key[4] = { 0x02b09eb9, 0x0581696f, 0x289b9863, 0x001a1879 };
UINT32 g_pack_key[4] = { 0x04b4b822, 0x1f6eb264, 0x0018eaae, 0x1cfbf6a6 };

char g_index_ext[5] = { '.', 'e', 'i', 'x', '\0' };
char g_pack_ext[5] = { '.', 'e', 'p', 'k', '\0' };

void GetFilesInDirectory(const std::string& dir, std::vector<std::string>& list)
{
    for (const auto& entry : fs::directory_iterator(dir))
    {
        if (entry.is_regular_file())
        {
            list.push_back(entry.path().string());
        }
    }
}

void GetAllSubDirectory(const std::string& dir, std::vector<std::string>& list)
{
    for (const auto& entry : fs::directory_iterator(dir))
    {
        if (entry.is_directory())
        {
            std::string subdir = entry.path().string();
            GetFilesInDirectory(subdir, list);
            GetAllSubDirectory(subdir, list);
        }
    }
}

std::vector<std::string> GetFilesListFromDirectory(const std::string& dir)
{
    std::vector<std::string> fileList;

    GetFilesInDirectory(dir, fileList);
    GetAllSubDirectory(dir, fileList);

    return fileList;
}

std::string GetDiffFromPaths(const std::string& fullPath, const std::string& diffPath)
{
    if (diffPath.length() >= fullPath.length())
        return fullPath;

    return fullPath.substr(diffPath.length() + 1);
}

std::string GetIntelVirtualPath(std::string& fileVirtualPath)
{
    // replace "\" with "/"
    std::replace(fileVirtualPath.begin(), fileVirtualPath.end(), '\\', '/');

    std::string prefix1 = "ymir work";
    if (fileVirtualPath.rfind(prefix1, 0) == 0) // starts with
        fileVirtualPath.insert(0, "d:/");

#ifdef ETER_NEXUS_UNIVERSAL_ELEMENTS
    std::string prefix2 = "universalelements work";
    if (fileVirtualPath.rfind(prefix2, 0) == 0) // starts with
        fileVirtualPath.insert(0, "d:/");
#endif

    return fileVirtualPath;
}

void CheckAndCreateDir(const std::string& fileName)
{
    std::filesystem::path p(fileName);

    // Get directory part of the path
    std::filesystem::path dir = p.parent_path();

    if (dir.empty())
        return;

    std::filesystem::create_directories(dir);
}

std::string GetFileNameFromPath(std::string path)
{
    if (path.find('\\') == std::string::npos)
        return path;

    if (!path.empty() && path.back() == '\\')
        path.pop_back();

    size_t pos = path.rfind('\\');
    if (pos == std::string::npos)
        return path;

    return path.substr(pos + 1);
}

int main(int argc, const char* argv[])
{
    if (argc < 2) {
        std::cout << "Usage:\n";
        std::cout << "  Extract: " << argv[0] << " e <index_file> <pack_file>\n";
        std::cout << "  Pack: " << argv[0] << " p <input_dir>\n";
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "e") {
        if (argc != 4) {
            std::cout << "Invalid arguments for extract.\n";
            return 1;
        }
        std::string indexFile = argv[2];
        std::string packFile = argv[3];


        // Load index
        EterIndex* pIndex = new EterIndex(indexFile.c_str(), g_index_key);
        if (!pIndex || !pIndex->LoadFile()) {
            std::cout << "Failed to load index: " << indexFile << "\n";
            delete pIndex;
            return 1;
        }

        // Load pack
        EterPack* pPack = new EterPack(packFile.c_str(), g_pack_key, true);
        if (!pPack) {
            std::cout << "Failed to load pack: " << packFile << "\n";
            delete pIndex;
            return 1;
        }

        // Get file list and extract
        EterIndexItem** files = pIndex->GetAllFiles();
        UINT32 fileCount = pIndex->GetFileCount();
        for (UINT32 i = 0; i < fileCount; ++i) {
            EterIndexItem* item = files[i];
            UINT8* data = pPack->GetStoredData(item, true);
            UINT32 size = pPack->GetStoredSize();
            char szFilePath[MAX_PATH], szMainDirectory[MAX_PATH];
            if (data != NULL && size != 0) {

                memcpy(szMainDirectory, packFile.data(), strlen(packFile.data()) - 4);
                szMainDirectory[strlen(packFile.data()) - 4] = 0;

                _snprintf(szFilePath, MAX_PATH, "%s\\%s", szMainDirectory, item->VirtualPath);
                CheckAndCreateDir(szFilePath);
                std::cout << szFilePath << "\n";
                _snprintf(szFilePath, MAX_PATH, "%s\\%s", szMainDirectory, item->VirtualPath);
                FastIO::FileWrite(szFilePath, "wb", data, size);
                std::cout << szFilePath << "\n";
            }
        }

        delete pPack;
        delete pIndex;
        std::cout << "Extraction complete.\n";

    }
    else if (mode == "p") {
        if (argc != 3) {
            std::cout << "Invalid arguments for pack.\n";
            return 1;
        }

        std::string inputDir = argv[2];

        auto szDirectoryPath = GetFileNameFromPath(inputDir);
        char szEixName[MAX_PATH] = {}, szEpkName[MAX_PATH] = {};


        std::filesystem::path p(inputDir);
        std::string result =
            (p.parent_path().string() + "\\") +
            szDirectoryPath +
            g_index_ext;

        snprintf(szEixName, MAX_PATH, "%s", result.c_str());

        std::cout << "Eix: " << szEixName << "\n";

        std::filesystem::path p2(inputDir);
        std::string result2 =
            (p2.parent_path().string() + "\\") +
            szDirectoryPath +
            g_pack_ext;

        snprintf(szEpkName, MAX_PATH, "%s", result2.c_str());
        std::cout << "EPK" << szEpkName << "\n";

        // Get files to pack
        std::vector<std::string> files = GetFilesListFromDirectory(inputDir);
        if (files.empty()) {
            std::cout << "No files found in: " << inputDir << "\n";
            return 1;
        }

        if (std::filesystem::exists(szEixName))
        {
            std::filesystem::remove(szEixName);
            std::cout << szEixName  << " deleted.";
        }
        if (std::filesystem::exists(szEpkName))
        {
            std::filesystem::remove(szEpkName);
            std::cout << szEpkName << " deleted.";
        }

        // Create index
        EterIndex* pIndex = new EterIndex(szEixName, g_index_key);
        pIndex->InitPack(files.size());

        // Create pack
        EterPack* pPack = new EterPack(szEpkName, g_pack_key, 1);

        for (const auto& file : files) {
            UINT32 outSize, hashCRC32;
            if (pPack->PutPack(file.c_str(), LZO_COMPRESSION_NO_ENCODING, &outSize, &hashCRC32)) {

                auto szFileVirtualPath = GetIntelVirtualPath(GetDiffFromPaths(file, inputDir));

                pIndex->PutFile(szFileVirtualPath.c_str(), LZO_COMPRESSION_NO_ENCODING, outSize, hashCRC32);
            }
        }

        pIndex->WritePack();
        // pPack->WritePack();  // Assuming WritePack exists or handle writing

        delete pPack;
        delete pIndex;
        std::cout << "Packing complete.\n";

    }
    else {
        std::cout << "Unknown mode: " << mode << "\n";
        return 1;
    }
}

const char* API_Copyright(void)
{
    return "EterPack API © 2013-2014, Crysus Technologies";
}

const char* API_Version(void)
{
    return "1.2.0 Alpha 2 (Full version)";
}

const char* API_Author(void)
{
    return "Crysus Technologies";
}

EINSTANCE LoadEterIndex(const char* szFilename, const UINT32* vKey, int iMode)
{
    sys_log(0, "====================== NEW INDEX CLASS ======================");

    EterIndex* pEixClass = new EterIndex(szFilename, vKey);

    if (!pEixClass) return 0;
    if (iMode == MODE_READ)
    {
        if (!pEixClass->LoadFile())
        {
            delete pEixClass;
            sys_log(1, "FAILED TO LOAD FILE %s", szFilename);
            return 0;
        }
    }

    sys_log(0, "ISTANCE AT 0x%p", pEixClass);
    sys_log(0, "ABSOLUTE PATH: %s", szFilename);

    return (EINSTANCE)pEixClass;
}

PLIST GetFilesList(EINSTANCE pInstance)
{
    return (pInstance != 0) ? ((EterIndex*)pInstance)->GetAllFiles() : 0;
}

UINT32 GetFileCount(EINSTANCE pInstance)
{
    return (pInstance != 0) ? ((EterIndex*)pInstance)->GetFileCount() : 0;
}

void InitPack(EINSTANCE pInstance, UINT32 iFileCount)
{
    if (pInstance == 0) return;

    ((EterIndex*)pInstance)->InitPack(iFileCount);
}

void PutFile(EINSTANCE pInstance, const char* szFilename, enum STORAGE_TYPE eType, UINT32 iFileSize, UINT32 iHashCRC32)
{
    if (pInstance == 0) return;

    ((EterIndex*)pInstance)->PutFile(szFilename, eType, iFileSize, iHashCRC32);
}

void WritePack(EINSTANCE pInstance)
{
    if (pInstance == 0) return;

    ((EterIndex*)pInstance)->WritePack();
}

void FreeEterIndex(EINSTANCE pInstance)
{
    if (pInstance == 0) return;

    delete ((EterIndex*)pInstance);
}

EINSTANCE LoadEterPack(const char* szFilename, const UINT32* vKey, int iMode)
{
    EterPack* pEpkClass = new EterPack(szFilename, vKey, iMode == MODE_READ);

    sys_log(0, "====================== NEW PACK CLASS ======================");
    sys_log(0, "ISTANCE AT 0x%p", pEpkClass);
    sys_log(0, "ABSOLUTE PATH: %s", szFilename);

    return (EINSTANCE)pEpkClass;
}

UINT8* GetFileData(EINSTANCE pInstance, EterIndexItem* pEixItem)
{
    return (pInstance != 0) ? ((EterPack*)pInstance)->GetStoredData(pEixItem, true) : 0;
}

UINT32 GetDataSize(EINSTANCE pInstance)
{
    return (pInstance != 0) ? ((EterPack*)pInstance)->GetStoredSize() : 0;
}

bool PutFileData(EINSTANCE pInstance, const char* szFilename, enum STORAGE_TYPE eType, UINT32* iOutSize, UINT32* iHashCRC32)
{
    return (pInstance != 0) ? ((EterPack*)pInstance)->PutPack(szFilename, eType, iOutSize, iHashCRC32) : false;
}

void FreeEterPack(EINSTANCE pInstance)
{
    if (pInstance == 0) return;

    delete ((EterPack*)pInstance);
}

