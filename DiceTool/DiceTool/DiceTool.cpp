// DiceTool.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Windows.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "STDFUPRTINC.h"
#include "STDFUFilesInc.h"
#include "DiceCore.h"
#include "DiceEcc.h"

#define diceDeviceVid                         0x0483
#define diceDevicePid                         0xDF11
#define diceDeviceVer                         0x0200

#define WSTR_TO_LOWER(__str) for (UINT32 n = 0; n < __str.size(); n++) __str[n] = tolower(__str[n]);
#define WSTR_TO_UPPER(__str) for (UINT32 n = 0; n < __str.size(); n++) __str[n] = toupper(__str[n]);

void PrintHex(std::vector<BYTE> data)
{
    for (uint32_t n = 0; n < data.size(); n++)
    {
        wprintf(L"%02x", data[n]);
    }
}

void WriteToFile(
    std::wstring fileName,
    std::vector<BYTE> data
)
{
    // http://stackoverflow.com/questions/14841396/stdunique-ptr-deleters-and-the-win32-api
    std::unique_ptr<std::remove_pointer<HANDLE>::type, decltype(&::CloseHandle)> hFile(::CreateFile(fileName.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL), &::CloseHandle);
    DWORD bytesWritten = 0;
    if (!WriteFile(hFile.get(), data.data(), data.size(), &bytesWritten, NULL))
    {
        throw GetLastError();
    }
}

std::vector<BYTE> ReadFromFile(
    std::wstring fileName
)
{
    // http://stackoverflow.com/questions/14841396/stdunique-ptr-deleters-and-the-win32-api
    std::unique_ptr<std::remove_pointer<HANDLE>::type, decltype(&::CloseHandle)> hFile(::CreateFile(fileName.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL), &::CloseHandle);
    DWORD bytesRead = 0;
    std::vector<BYTE> data(GetFileSize(hFile.get(), NULL));
    if (!ReadFile(hFile.get(), data.data(), data.size(), &bytesRead, NULL))
    {
        throw GetLastError();
    }
    return data;
}

std::vector<BYTE> ReadHex(
    std::wstring strIn
)
{
    std::vector<BYTE> dataOut(strIn.size() / 2);

    for (uint32_t cursor = 0; cursor < dataOut.size(); cursor++)
    {
        UINT32 scannedDigit = 0;
        if (swscanf_s(strIn.substr(cursor * 2, 2).c_str(), L"%x", &scannedDigit) != 1)
        {
            throw;
        }
        dataOut[cursor] = (BYTE)(scannedDigit & 0x000000FF);
    }
    return dataOut;
}

uint32_t GetTimeStamp(
    void
)
{
    FILETIME now = { 0 };
    LARGE_INTEGER convert = { 0 };

    // Get the current timestamp
    GetSystemTimeAsFileTime(&now);
    convert.LowPart = now.dwLowDateTime;
    convert.HighPart = now.dwHighDateTime;
    convert.QuadPart = (convert.QuadPart - (UINT64)(11644473600000 * 10000)) / 10000000;
    return convert.LowPart;
}

void CreateDiceIdentity(
    std::wstring dfuFileName,
    ecc_publickey* authorityPub,
    ecc_publickey* devicePub,
    ecc_privatekey* devicePrv,
    DiceProperties_t properties,
    uint32_t rollBackProtection
)
{
    DWORD retVal = STDFUFILES_NOERROR;
    std::string fileName;
    HANDLE hDfu = INVALID_HANDLE_VALUE;
    HANDLE hDfuFile = INVALID_HANDLE_VALUE;
    DFUIMAGEELEMENT dfuImageElement = { 0 };
    std::vector<BYTE> image;
    PDiceFuse_t pDiceData = NULL;
    std::exception_ptr pendingException = NULL;

    fileName.resize(dfuFileName.size());
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, dfuFileName.c_str(), dfuFileName.size(), (LPSTR)fileName.c_str(), fileName.size(), NULL, NULL))
    {
        throw GetLastError();
    }
    fileName.append(".DFU");

    if ((retVal = STDFUFILES_CreateImage(&hDfu, 0)) != STDFUFILES_NOERROR)
    {
        throw retVal;
    }

    try
    {
        dfuImageElement.dwAddress = DICEDATAEEPROMSTART;
        dfuImageElement.dwDataLength = sizeof(DiceFuse_t);
        image.resize(dfuImageElement.dwDataLength);
        dfuImageElement.Data = image.data();
        pDiceData = (PDiceFuse_t)image.data();
        pDiceData->s.deviceInfo.magic = DICEPROVISIONEDID;
        pDiceData->s.deviceInfo.properties = properties;
        pDiceData->s.deviceInfo.rollBackProtection = rollBackProtection;
        if (authorityPub)
        {
            memcpy(&pDiceData->s.deviceInfo.authorityPub, authorityPub, sizeof(ecc_publickey));
        }
        if ((devicePub) && (devicePrv))
        {
            pDiceData->s.deviceInfo.magic = DICEMAGIC;
            memcpy(&pDiceData->s.deviceInfo.devicePub, devicePub, sizeof(ecc_publickey));
            memcpy(&pDiceData->s.devicePrv, devicePrv, sizeof(ecc_privatekey));
        }
        if ((retVal = STDFUFILES_SetImageElement(hDfu, 0, TRUE, dfuImageElement)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        if ((retVal = STDFUFILES_SetImageName(hDfu, "Dice")) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        if ((retVal = STDFUFILES_CreateNewDFUFile((PSTR)fileName.c_str(), &hDfuFile, diceDeviceVid, diceDevicePid, diceDeviceVer)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        try
        {
            if ((retVal = STDFUFILES_AppendImageToDFUFile(hDfuFile, hDfu)) != STDFUFILES_NOERROR)
            {
                throw retVal;
            }
        }
        catch (const std::exception& e)
        {
            pendingException = std::current_exception();
        }
        if ((retVal = STDFUFILES_CloseDFUFile(hDfuFile)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        if (pendingException != NULL)
        {
            std::rethrow_exception(pendingException);
        }
    }
    catch (const std::exception& e)
    {
        pendingException = std::current_exception();
    }

    if ((retVal = STDFUFILES_DestroyImage(&hDfu)) != STDFUFILES_NOERROR)
    {
        throw retVal;
    }

    if (pendingException != NULL)
    {
        std::rethrow_exception(pendingException);
    }
}

void GenerateEccKeypair(
    std::wstring keyFileName
)
{
    DWORD retVal = DICE_SUCCESS;
    std::wstring pubFileName(keyFileName);
    std::wstring prvFileName(keyFileName);
    ecc_publickey pub = { 0 };
    ecc_privatekey prv = { 0 };
    std::vector<BYTE> pubBlob(64);
    std::vector<BYTE> prvBlob(32);

    pubFileName.append(L".EPU");
    prvFileName.append(L".EPR");

    // Create a random key pair 
    if ((retVal = Dice_GenerateDSAKeyPair(&pub, &prv)) != DICE_SUCCESS)
    {
        throw retVal;
    }
    BigValToBigInt(prvBlob.data(), &prv);
    BigValToBigInt(pubBlob.data(), &pub.x);
    BigValToBigInt(&pubBlob.data()[32], &pub.y);
    wprintf(L"Public.x:\n");
    {
        std::vector<BYTE> num(32);
        BigValToBigInt(num.data(), &pub.x);
        PrintHex(num);
    }
    wprintf(L"\n");
    wprintf(L"Public.x:\n");
    {
        std::vector<BYTE> num(32);
        BigValToBigInt(num.data(), &pub.y);
        PrintHex(num);
    }
    wprintf(L"\n");

    // Write it to disk
    WriteToFile(prvFileName, prvBlob);
    WriteToFile(pubFileName, pubBlob);
}

void PrintAppInfo(PDiceEmbeddedSignature_t sigTrailer)
{
    wprintf(L"CodeSize:         %d\n", sigTrailer->signedData.codeSize);
    wprintf(L"CodeIssuanceDate: %d (0x%08x)\n", sigTrailer->signedData.issueDate, sigTrailer->signedData.issueDate);
    wprintf(L"CodeName:         ");
    {
        std::vector<BYTE> digest(sizeof(sigTrailer->signedData.codeDigest));
        memcpy(digest.data(), sigTrailer->signedData.codeDigest, digest.size());
        PrintHex(digest);
    }
    wprintf(L"\n");
    wprintf(L"AlternateName:    ");
    {
        std::vector<BYTE> digest(sizeof(sigTrailer->signedData.alternateDigest));
        memcpy(digest.data(), sigTrailer->signedData.alternateDigest, digest.size());
        PrintHex(digest);
    }
    wprintf(L"\n");
    wprintf(L"Signature.r:      ");
    {
        std::vector<BYTE> digest(32);
        BigValToBigInt(digest.data(), &sigTrailer->signature.r);
        PrintHex(digest);
    }
    wprintf(L"\n");
    wprintf(L"Signature.s:      ");
    {
        std::vector<BYTE> digest(32);
        BigValToBigInt(digest.data(), &sigTrailer->signature.s);
        PrintHex(digest);
    }
    wprintf(L"\n");
}

void CreateDiceApplication(
    std::wstring fileName,
    ecc_privatekey* authorityPrv,
    std::vector<BYTE> alternateDigest,
    uint32_t timeStamp
)
{
    DWORD retVal = STDFUFILES_NOERROR;
    HANDLE hDfu = INVALID_HANDLE_VALUE;
    HANDLE hDfuFile = INVALID_HANDLE_VALUE;
    DFUIMAGEELEMENT dfuImageElement = { 0 };
    std::vector<BYTE> image;
    PDiceEmbeddedSignature_t sigTrailer = NULL;
    std::exception_ptr pendingException = NULL;

    std::string hexFileName;
    hexFileName.resize(fileName.size());
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, fileName.c_str(), fileName.size(), (LPSTR)hexFileName.c_str(), hexFileName.size(), NULL, NULL))
    {
        throw GetLastError();
    }

    if ((retVal = STDFUFILES_ImageFromFile((PSTR)hexFileName.c_str(), &hDfu, 0)) != STDFUFILES_NOERROR)
    {
        throw retVal;
    }

    try
    {
        if ((retVal = STDFUFILES_GetImageElement(hDfu, 0, &dfuImageElement)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        image.resize(dfuImageElement.dwDataLength);
        dfuImageElement.Data = image.data();
        if ((retVal = STDFUFILES_GetImageElement(hDfu, 0, &dfuImageElement)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        image.resize(dfuImageElement.dwDataLength + sizeof(DiceEmbeddedSignature_t));
        dfuImageElement.Data = image.data();
        memset(&dfuImageElement.Data[dfuImageElement.dwDataLength], 0x00, sizeof(DiceEmbeddedSignature_t));
        sigTrailer = (PDiceEmbeddedSignature_t)&dfuImageElement.Data[dfuImageElement.dwDataLength];
        dfuImageElement.dwDataLength += sizeof(DiceEmbeddedSignature_t);
        sigTrailer->startMarker = DICEMARKER;
        sigTrailer->signedData.codeSize = dfuImageElement.dwDataLength - sizeof(DiceEmbeddedSignature_t);
        sigTrailer->signedData.issueDate = (timeStamp != 0) ? timeStamp : GetTimeStamp();
        Dice_SHA256_Block(dfuImageElement.Data, sigTrailer->signedData.codeSize, sigTrailer->signedData.codeDigest);
        if (alternateDigest.size() == sizeof(sigTrailer->signedData.alternateDigest))
        {
            memcpy(sigTrailer->signedData.alternateDigest, alternateDigest.data(), sizeof(sigTrailer->signedData.alternateDigest));
        }
        if ((retVal = Dice_DSASign((uint8_t*)&sigTrailer->signedData, sizeof(sigTrailer->signedData), authorityPrv, &sigTrailer->signature)) != DICE_SUCCESS)
        {
            throw retVal;
        }
        sigTrailer->endMarker = DICEMARKER;
        PrintAppInfo(sigTrailer);
        if ((retVal = STDFUFILES_SetImageElement(hDfu, 0, FALSE, dfuImageElement)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        //if ((retVal = STDFUFILES_ImageToFile((PSTR)hexFileName.c_str(), hDfu)) != STDFUFILES_NOERROR)
        //{
        //    throw retVal;
        //}
        if ((retVal = STDFUFILES_SetImageName(hDfu, "DiceApp")) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        std::string dfuFileName;
        dfuFileName = hexFileName.substr(0, hexFileName.size() - 4);
        dfuFileName.append(".DFU");
        if ((retVal = STDFUFILES_CreateNewDFUFile((PSTR)dfuFileName.c_str(), &hDfuFile, diceDeviceVid, diceDevicePid, diceDeviceVer)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }

        try
        {
            if ((retVal = STDFUFILES_AppendImageToDFUFile(hDfuFile, hDfu)) != STDFUFILES_NOERROR)
            {
                throw retVal;
            }
        }
        catch (const std::exception& e)
        {
            pendingException = std::current_exception();
        }

        if ((retVal = STDFUFILES_CloseDFUFile(hDfuFile)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }

        if (pendingException != NULL)
        {
            std::rethrow_exception(pendingException);
        }
    }
    catch (const std::exception& e)
    {
        pendingException = std::current_exception();
    }

    if ((retVal = STDFUFILES_DestroyImage(&hDfu)) != STDFUFILES_NOERROR)
    {
        throw retVal;
    }

    if (pendingException != NULL)
    {
        std::rethrow_exception(pendingException);
    }
}

void PrintDiceApplicationInfo(
    std::wstring fileName
)
{
    DWORD retVal = STDFUFILES_NOERROR;
    HANDLE hDfu = INVALID_HANDLE_VALUE;
    HANDLE hDfuFile = INVALID_HANDLE_VALUE;
    DFUIMAGEELEMENT dfuImageElement = { 0 };
    std::vector<BYTE> image;
    PDiceEmbeddedSignature_t sigTrailer = NULL;
    std::exception_ptr pendingException = NULL;

    std::string dfuFileName;
    dfuFileName.resize(fileName.size());
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, fileName.c_str(), fileName.size(), (LPSTR)dfuFileName.c_str(), dfuFileName.size(), NULL, NULL))
    {
        throw GetLastError();
    }

    WORD devVid;
    WORD devPid;
    WORD devBcd;
    BYTE devNbImages;
    if ((retVal = STDFUFILES_OpenExistingDFUFile((PSTR)dfuFileName.c_str(), &hDfuFile, &devVid, &devPid, &devBcd, &devNbImages)) != STDFUFILES_NOERROR)
    {
        throw retVal;
    }

    if ((retVal = STDFUFILES_ReadImageFromDFUFile(hDfuFile, 0, &hDfu)) != STDFUFILES_NOERROR)
    {
        throw retVal;
    }
    if ((retVal = STDFUFILES_GetImageElement(hDfu, 0, &dfuImageElement)) != STDFUFILES_NOERROR)
    {
        throw retVal;
    }
    image.resize(dfuImageElement.dwDataLength);
    dfuImageElement.Data = image.data();
    if ((retVal = STDFUFILES_GetImageElement(hDfu, 0, &dfuImageElement)) != STDFUFILES_NOERROR)
    {
        throw retVal;
    }
    sigTrailer = (PDiceEmbeddedSignature_t)&dfuImageElement.Data[dfuImageElement.dwDataLength - sizeof(DiceEmbeddedSignature_t)];
    if ((sigTrailer->startMarker == DICEMARKER) && (sigTrailer->endMarker == DICEMARKER))
    {
        PrintAppInfo(sigTrailer);
    }
    else
    {
        wprintf(L"No embedded signature trailer found.\n");
    }
    if ((retVal = STDFUFILES_DestroyImage(&hDfu)) != STDFUFILES_NOERROR)
    {
        throw retVal;
    }
    if ((retVal = STDFUFILES_CloseDFUFile(hDfuFile)) != STDFUFILES_NOERROR)
    {
        throw retVal;
    }
}

void RunGEK(std::unordered_map<std::wstring, std::wstring> param)
{
    std::wstring keyFileName(param.find(L"00")->second);
    keyFileName.resize(keyFileName.find('.', 0));
    GenerateEccKeypair(keyFileName);
}

void RunCIP(std::unordered_map<std::wstring, std::wstring> param)
{
    std::unordered_map<std::wstring, std::wstring>::iterator it;
    std::wstring dfuName(param.find(L"00")->second);
    std::vector<BYTE> ecc_pub;
    std::vector<BYTE> ecc_prv;
    std::vector<BYTE> ecc_authPub;
    DiceProperties_t properties = { 0 };
    UINT32 rollBack = 0;
    if ((it = param.find(L"-a")) != param.end())
    {
        std::vector<BYTE> eccdata = ReadFromFile(std::wstring(it->second).append(L".EPU"));
        ecc_authPub.resize(sizeof(ecc_publickey));
        ecc_publickey* pub = (ecc_publickey*)ecc_authPub.data();
        std::vector<BYTE> num(32);
        memcpy(num.data(), eccdata.data(), num.size());
        wprintf(L"Authority.x:    ");
        PrintHex(num);
        wprintf(L"\n");
        memcpy(num.data(), &eccdata[32], num.size());
        wprintf(L"Authority.y:    ");
        PrintHex(num);
        wprintf(L"\n");
        BigIntToBigVal(&pub->x, eccdata.data(), eccdata.size() / 2);
        BigIntToBigVal(&pub->y, &eccdata.data()[eccdata.size() / 2], eccdata.size() / 2);
    }
    if ((it = param.find(L"-d")) != param.end())
    {
        std::vector<BYTE> eccdata = ReadFromFile(std::wstring(it->second).append(L".EPU"));
        ecc_pub.resize(sizeof(ecc_publickey));
        ecc_publickey* pub = (ecc_publickey*)ecc_pub.data();
        std::vector<BYTE> num(32);
        memcpy(num.data(), eccdata.data(), num.size());
        wprintf(L"Device.x:    ");
        PrintHex(num);
        wprintf(L"\n");
        memcpy(num.data(), &eccdata[32], num.size());
        wprintf(L"Device.y:    ");
        PrintHex(num);
        wprintf(L"\n");
        BigIntToBigVal(&pub->x, eccdata.data(), eccdata.size() / 2);
        BigIntToBigVal(&pub->y, &eccdata.data()[eccdata.size() / 2], eccdata.size() / 2);
        eccdata = ReadFromFile(std::wstring(it->second).append(L".EPR"));
        ecc_prv.resize(sizeof(ecc_privatekey));
        ecc_privatekey* prv = (ecc_privatekey*)ecc_prv.data();
        BigIntToBigVal(prv, eccdata.data(), eccdata.size());
        properties.importedSeed = 1;
    }
    if (param.find(L"-noclear") != param.end()) properties.noClear = 1;
    if (param.find(L"-nobootnonce") != param.end()) properties.noBootNonce = 1;
    if ((it = param.find(L"-rollback")) != param.end())
    {
        swscanf_s(it->second.c_str(), L"%d", &rollBack);
    }
    CreateDiceIdentity(dfuName, (ecc_publickey*)ecc_authPub.data(), (ecc_publickey*)ecc_pub.data(), (ecc_privatekey*)ecc_prv.data(), properties, rollBack);
}

void RunCAP(std::unordered_map<std::wstring, std::wstring> param)
{
    std::unordered_map<std::wstring, std::wstring>::iterator it;
    std::wstring hexName(param.find(L"00")->second);
    std::vector<BYTE> ecc_authPrv(sizeof(ecc_privatekey));
    std::vector<BYTE> alternateDigest;
    UINT32 timeStamp = 0;

    std::vector<BYTE> eccdata = ReadFromFile(std::wstring(param.find(L"01")->second).append(L".EPR"));
    ecc_privatekey* prv = (ecc_privatekey*)ecc_authPrv.data();
    BigIntToBigVal(prv, eccdata.data(), eccdata.size());

    if ((it = param.find(L"-a")) != param.end())
    {
        alternateDigest = ReadHex(it->second);
        if (alternateDigest.size() != SHA256_DIGEST_LENGTH)
        {
            throw DICE_INVALID_PARAMETER;
        }
    }
    if ((it = param.find(L"-timestamp")) != param.end())
    {
        swscanf_s(it->second.c_str(), L"%d", &timeStamp);
    }
    CreateDiceApplication(hexName, prv, alternateDigest, timeStamp);
}

void RunPAI(std::unordered_map<std::wstring, std::wstring> param)
{
    std::unordered_map<std::wstring, std::wstring>::iterator it;
    std::wstring dfuName(param.find(L"00")->second);
    PrintDiceApplicationInfo(dfuName);
}

void PrintHelp(void)
{
    wprintf(L"Generate ECC key:\nGEK [keyFileName]\n");
    wprintf(L"Create ID Package:\nCIP [dfuFileName] { -a=authKeyFile | -d=devKeyFile | -noClear | -noBootNonce | -rollBack=0 }\n");
    wprintf(L"Create APP Package:\nCAP [hexFileName] [authKeyFile] { -a=alternateDigest | -timeStamp=0 }\n");
    wprintf(L"Print APP Package Info:\nPAI [dfuFileName]\n");
}

int wmain(int argc, const wchar_t** argv)
{
    try
    {
        if (argc <= 1)
        {
            throw;
        }
        std::wstring cmd(argv[1]);
        WSTR_TO_LOWER(cmd);
        std::unordered_map<std::wstring, std::wstring> param;
        for (UINT32 n = 2; n < (unsigned int)argc; n++)
        {
            std::wstring element(argv[n]);
            size_t divider = element.find('=', 0);
            size_t option = element.find('-', 0);
            std::pair<std::wstring, std::wstring> newPair;
            if ((divider == std::string::npos) && (option == std::wstring::npos))
            {
                std::wstring pos(L"  ");
                wsprintf((LPWSTR)pos.c_str(), L"%02d", param.size());
                newPair = std::pair<std::wstring, std::wstring>(pos, element);
            }
            else
            {
                std::wstring tag(element.substr(0, divider));
                WSTR_TO_LOWER(tag);
                newPair = std::pair<std::wstring, std::wstring>(tag, element.substr(divider + 1));
            }
            param.insert(newPair);
        }

        if ((cmd == std::wstring(L"gek")) && (param.size() == 1))
        {
            RunGEK(param);
        }
        if ((cmd == std::wstring(L"pai")) && (param.size() == 1))
        {
            RunPAI(param);
        }
        else if ((cmd == std::wstring(L"cip")) && (param.size() >= 1))
        {
            RunCIP(param);
        }
        else if ((cmd == std::wstring(L"cap")) && (param.size() >= 2))
        {
            RunCAP(param);
        }
        else
        {
            PrintHelp();
        }
    }
    catch (...)
    {
        PrintHelp();
    }
}

