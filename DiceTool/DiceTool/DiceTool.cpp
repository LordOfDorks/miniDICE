// DiceTool.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

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
        dataOut[cursor]  = (BYTE)std::stoul(strIn.substr(cursor * 2, 2), NULL, 16);
        //if (swscanf_s(strIn.substr(cursor * 2, 2).c_str(), L"%x", &scannedDigit) != 1)
        //{
        //    throw;
        //}
        // dataOut[cursor] = (BYTE)(scannedDigit & 0x000000FF);
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

void PrintAppInfo(PDiceEmbeddedSignature_t sigHdr)
{
    printf("AppName:          %s\n", sigHdr->s.sign.name);
    wprintf(L"Version           %d.%d\n", sigHdr->s.sign.version >> 16, sigHdr->s.sign.version & 0x0000ffff);
    wprintf(L"Size:             %d\n", sigHdr->s.sign.codeSize);
    wprintf(L"IssuanceDate:     %d (0x%08x)\n", sigHdr->s.sign.issueDate, sigHdr->s.sign.issueDate);
    wprintf(L"AppDigest:        ");
    {
        std::vector<BYTE> digest(sizeof(sigHdr->s.sign.appDigest));
        memcpy(digest.data(), sigHdr->s.sign.appDigest, digest.size());
        PrintHex(digest);
    }
    wprintf(L"\n");
    wprintf(L"AlternateDigest:  ");
    {
        std::vector<BYTE> digest(sizeof(sigHdr->s.sign.alternateDigest));
        memcpy(digest.data(), sigHdr->s.sign.alternateDigest, digest.size());
        PrintHex(digest);
    }
    wprintf(L"\n");
    wprintf(L"Signature.r:      ");
    {
        std::vector<BYTE> digest(32);
        BigValToBigInt(digest.data(), &sigHdr->s.signature.r);
        PrintHex(digest);
    }
    wprintf(L"\n");
    wprintf(L"Signature.s:      ");
    {
        std::vector<BYTE> digest(32);
        BigValToBigInt(digest.data(), &sigHdr->s.signature.s);
        PrintHex(digest);
    }
    wprintf(L"\n");
}

PCCERT_CONTEXT CertFromFile(std::wstring fileName)
{
    uint32_t retVal = 0;
    std::vector<BYTE> rawCert = ReadFromFile(fileName);
    DWORD result;
    if (CryptStringToBinaryA((LPSTR)rawCert.data(), rawCert.size(), CRYPT_STRING_BASE64HEADER, NULL, &result, NULL, NULL))
    {
        std::vector<BYTE> derCert(result, 0);
        if (!CryptStringToBinaryA((LPSTR)rawCert.data(), rawCert.size(), CRYPT_STRING_BASE64HEADER, derCert.data(), &result, NULL, NULL))
        {
            throw GetLastError();
        }
        rawCert = derCert;
    }
    PCCERT_CONTEXT hCert = NULL;
    if ((hCert = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, rawCert.data(), rawCert.size())) == NULL)
    {
        throw retVal;
    }
    return hCert;
}

std::vector<BYTE> CertThumbPrint(PCCERT_CONTEXT hCert)
{
    uint32_t retVal = 0;
    BCRYPT_ALG_HANDLE hSha1 = NULL;
    if ((retVal = BCryptOpenAlgorithmProvider(&hSha1, BCRYPT_SHA1_ALGORITHM, NULL, 0)) != 0)
    {
        throw retVal;
    }
    std::vector<BYTE> digest(20, 0);
    if ((retVal = BCryptHash(hSha1, NULL, 0, hCert->pbCertEncoded, hCert->cbCertEncoded, digest.data(), digest.size())) != 0)
    {
        throw retVal;
    }
    BCryptCloseAlgorithmProvider(hSha1, 0);
    return digest;
}

void SignDiceIdentityPackage(
    std::wstring fileName,
    PCCERT_CONTEXT deviceAuthCert,
    DWORD dwKeySpec,
    HCRYPTPROV_OR_NCRYPT_KEY_HANDLE deviceAuth,
    BCRYPT_KEY_HANDLE codeAuth,
    BOOL noClear,
    UINT32 rollBack
)
{
    std::exception_ptr pendingException = NULL;
    DWORD retVal = STDFUFILES_NOERROR;
    BCRYPT_ALG_HANDLE hRng = NULL;
    std::string dfuFileName(fileName.size(), '\0');
    HANDLE hDfu = INVALID_HANDLE_VALUE;
    HANDLE hImage = INVALID_HANDLE_VALUE;
    HANDLE hDfuFile = INVALID_HANDLE_VALUE;
    DFUIMAGEELEMENT dfuImageElement = { 0 };
    PCCERT_CONTEXT devCert = NULL;
    BCRYPT_KEY_HANDLE devPub = NULL;
    DWORD result;
    PBYTE pbEncAuthorityKeyInfo = NULL;
    DWORD cbEncAuthorityKeyInfo = 0;

    try
    { 
        if((retVal = BCryptOpenAlgorithmProvider(&hRng, BCRYPT_RNG_ALGORITHM, NULL, 0)) != 0)
        {
            throw retVal;
        }

        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, fileName.c_str(), fileName.size(), (LPSTR)dfuFileName.c_str(), dfuFileName.size(), NULL, NULL))
        {
            throw GetLastError();
        }
        WORD vid;
        WORD pid;
        WORD bcd;
        BYTE images;
        if ((retVal = STDFUFILES_OpenExistingDFUFile((PSTR)dfuFileName.c_str(), &hDfu, &vid, &pid, &bcd, &images)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        if ((retVal = STDFUFILES_ReadImageFromDFUFile(hDfu, 0, &hImage)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        if ((retVal = STDFUFILES_GetImageElement(hImage, 0, &dfuImageElement)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        std::vector<BYTE> image(dfuImageElement.dwDataLength, 0);
        dfuImageElement.Data = image.data();
        if ((retVal = STDFUFILES_GetImageElement(hImage, 0, &dfuImageElement)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        STDFUFILES_DestroyImage(&hImage);
        hImage = INVALID_HANDLE_VALUE;
        STDFUFILES_CloseDFUFile(&hDfu);
        hDfu = INVALID_HANDLE_VALUE;
        wprintf(L"Image in %d bytes\n", dfuImageElement.dwDataLength);

        // Update the policy in the header
        PDicePublicInfo_t dicePublic = (PDicePublicInfo_t)image.data();
        dicePublic->properties.noClear = noClear ? 1 : 0;
        if(rollBack != -1) dicePublic->rollBackProtection = rollBack;
        if(codeAuth)
        {
            if ((retVal = BCryptExportKey(codeAuth, NULL, BCRYPT_ECCPUBLIC_BLOB, NULL, 0, &result, 0)) != 0)
            {
                throw retVal;
            }
            std::vector<BYTE> codeAuthPub(result, 0);
            if ((retVal = BCryptExportKey(codeAuth, NULL, BCRYPT_ECCPUBLIC_BLOB, codeAuthPub.data(), codeAuthPub.size(), &result, 0)) != 0)
            {
                throw retVal;
            }
            BCRYPT_ECCKEY_BLOB* keyHdr = (BCRYPT_ECCKEY_BLOB*)codeAuthPub.data();
            BigIntToBigVal(&dicePublic->authorityPub.x, &codeAuthPub[sizeof(BCRYPT_ECCKEY_BLOB)], keyHdr->cbKey);
            BigIntToBigVal(&dicePublic->authorityPub.y, &codeAuthPub[sizeof(BCRYPT_ECCKEY_BLOB) + keyHdr->cbKey], keyHdr->cbKey);
        }

        // Open the selfsigned device certificate and verify it
        std::string certBag(dicePublic->certBag);
        result = 0;
        std::string encDevCert(certBag.substr(0, certBag.find("-----BEGIN CERTIFICATE-----", 29)));
        retVal = CryptStringToBinaryA(encDevCert.c_str(), encDevCert.size(), CRYPT_STRING_BASE64HEADER, NULL, &result, NULL, NULL);
        std::vector<BYTE> rawCert(result, 0);
        retVal = CryptStringToBinaryA(encDevCert.c_str(), encDevCert.size(), CRYPT_STRING_BASE64HEADER, rawCert.data(), &result, NULL, NULL);
        if ((devCert = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, rawCert.data(), rawCert.size())) == NULL)
        {
            throw retVal;
        }
        if (!CryptImportPublicKeyInfoEx2(X509_ASN_ENCODING, &devCert->pCertInfo->SubjectPublicKeyInfo, 0, NULL, &devPub))
        {
            throw DICE_INVALID_PARAMETER;
        }
        DWORD validityFlags = CERT_STORE_SIGNATURE_FLAG | CERT_STORE_TIME_VALIDITY_FLAG;
        if (!CertVerifySubjectCertificateContext(devCert, devCert, &validityFlags))
        {
            throw DICE_INVALID_PARAMETER;
        }

        // Create the new device certificate to issue
        CERT_INFO certInfo = { 0 };
        certInfo.dwVersion = CERT_V3;
        certInfo.SerialNumber.cbData = 16;
        std::vector<BYTE> certSerial(certInfo.SerialNumber.cbData, 0);
        certInfo.SerialNumber.pbData = certSerial.data();
        if((retVal = BCryptGenRandom(hRng, certSerial.data(), certSerial.size(), 0)) != 0)
        {
            throw retVal;
        }
        certInfo.SignatureAlgorithm.pszObjId = szOID_ECDSA_SHA256;
        certInfo.Issuer.cbData = deviceAuthCert->pCertInfo->Issuer.cbData;
        certInfo.Issuer.pbData = deviceAuthCert->pCertInfo->Issuer.pbData;
        certInfo.Subject.cbData = devCert->pCertInfo->Subject.cbData;
        certInfo.Subject.pbData = devCert->pCertInfo->Subject.pbData;
        SYSTEMTIME systemTime;
        GetSystemTime(&systemTime);
        SystemTimeToFileTime(&systemTime, &certInfo.NotBefore);
        systemTime.wYear += 20;
        SystemTimeToFileTime(&systemTime, &certInfo.NotAfter);
        certInfo.SubjectPublicKeyInfo = devCert->pCertInfo->SubjectPublicKeyInfo;
        certInfo.cExtension = devCert->pCertInfo->cExtension;
        certInfo.rgExtension = devCert->pCertInfo->rgExtension;
        for (UINT32 n = 0; n < certInfo.cExtension; n++)
        {
            if (!strcmp(certInfo.rgExtension[n].pszObjId, szOID_AUTHORITY_KEY_IDENTIFIER))
            {
                // Find the authority's key identifier
                std::vector<BYTE> authorityKeyId;
                CERT_AUTHORITY_KEY_ID_INFO keyIdInfo = { 0 };
                keyIdInfo.CertSerialNumber.cbData = deviceAuthCert->pCertInfo->SerialNumber.cbData;
                keyIdInfo.CertSerialNumber.pbData = deviceAuthCert->pCertInfo->SerialNumber.pbData;
                keyIdInfo.CertIssuer.cbData = deviceAuthCert->pCertInfo->Issuer.cbData;
                keyIdInfo.CertIssuer.pbData = deviceAuthCert->pCertInfo->Issuer.pbData;

                for (UINT32 m = 0; m < deviceAuthCert->pCertInfo->cExtension; m++)
                {
                    if (!strcmp(deviceAuthCert->pCertInfo->rgExtension[m].pszObjId, szOID_SUBJECT_KEY_IDENTIFIER))
                    {
                        CRYPT_DIGEST_BLOB keyId = {0};
                        DWORD keyidSize = sizeof(keyId);
                        if (!CryptDecodeObject(X509_ASN_ENCODING,
                                               deviceAuthCert->pCertInfo->rgExtension[m].pszObjId,
                                               deviceAuthCert->pCertInfo->rgExtension[m].Value.pbData,
                                               deviceAuthCert->pCertInfo->rgExtension[m].Value.cbData,
                                               CRYPT_DECODE_NOCOPY_FLAG,
                                               &keyId, &keyidSize))
                        {
                            throw GetLastError();
                        }
                        authorityKeyId.resize(keyId.cbData);
                        keyIdInfo.KeyId.cbData = (UINT32)authorityKeyId.size();
                        keyIdInfo.KeyId.pbData = authorityKeyId.data();
                        memcpy(keyIdInfo.KeyId.pbData, keyId.pbData, keyIdInfo.KeyId.cbData);
                        break;
                    }
                }
                if (!CryptEncodeObjectEx(X509_ASN_ENCODING,
                    X509_AUTHORITY_KEY_ID,
                    &keyIdInfo,
                    CRYPT_ENCODE_ALLOC_FLAG,
                    NULL,
                    &certInfo.rgExtension[n].Value.pbData,
                    &certInfo.rgExtension[n].Value.cbData))
                {
                    throw GetLastError();
                }
                break;
            }
        }

        // Issue the new certificate
        result = 0;
        CRYPT_ALGORITHM_IDENTIFIER certAlgId = { szOID_ECDSA_SHA256,{ 0, NULL } };
        if (!CryptSignAndEncodeCertificate(deviceAuth,
            dwKeySpec,
            X509_ASN_ENCODING,
            X509_CERT_TO_BE_SIGNED,
            &certInfo,
            &certAlgId,
            NULL,
            NULL,
            &result))
        {
            throw GetLastError();
        }
        std::vector<BYTE> newCert(result, 0);
        if (!CryptSignAndEncodeCertificate(deviceAuth,
            dwKeySpec,
            X509_ASN_ENCODING,
            X509_CERT_TO_BE_SIGNED,
            &certInfo,
            &certAlgId,
            NULL,
            newCert.data(),
            &result))
        {
            throw GetLastError();
        }

        // Store a copy of the new cert on the disk
        fileName = fileName.substr(0, fileName.size() - 4) + std::wstring(L"-Auth") + fileName.substr(dfuFileName.size() - 4);
        WriteToFile(fileName.substr(0, fileName.size() - 4).append(L".cer"), newCert);

        // PEM encode 
        result = 0;
        CryptBinaryToStringA(newCert.data(), newCert.size(), CRYPT_STRING_BASE64HEADER, NULL, &result);
        std::string newEncCert(result, '\0');
        CryptBinaryToStringA(newCert.data(), newCert.size(), CRYPT_STRING_BASE64HEADER, (LPSTR)newEncCert.c_str(), &result);
        newEncCert.resize(newEncCert.find('\0', 0));

        // Put the public structure together
        dicePublic->certBagLen = newEncCert.size();
        dicePublic->size = sizeof(DicePublicInfo_t) + dicePublic->certBagLen;
        image.resize(dicePublic->size);
        dfuImageElement.Data = image.data();
        dfuImageElement.dwDataLength = image.size();
        dicePublic = (PDicePublicInfo_t)image.data();
        memcpy(dicePublic->certBag, newEncCert.data(), dicePublic->certBagLen);
        wprintf(L"Image out %d bytes\n", dfuImageElement.dwDataLength);

        // Create the updated DFU package
        if ((retVal = STDFUFILES_CreateImage(&hImage, 0)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        if ((retVal = STDFUFILES_SetImageElement(hImage, 0, true, dfuImageElement)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        dfuFileName = dfuFileName.substr(0, dfuFileName.size() - 4) + std::string("-Auth") + dfuFileName.substr(dfuFileName.size() - 4);
        if ((retVal = STDFUFILES_CreateNewDFUFile((PSTR)dfuFileName.c_str(), &hDfu, vid, pid, bcd)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        if ((retVal = STDFUFILES_AppendImageToDFUFile(hDfu, hImage)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
    }
    catch (const std::exception& e)
    {
        pendingException = std::current_exception();
    }

    if (hImage != INVALID_HANDLE_VALUE)
    {
        STDFUFILES_DestroyImage(&hImage);
    }

    if (hDfu != INVALID_HANDLE_VALUE)
    {
        STDFUFILES_CloseDFUFile(&hDfu);
    }

    if (devPub)
    {
        BCryptDestroyKey(devPub);
    }

    if (hRng)
    {
        BCryptCloseAlgorithmProvider(hRng, 0);
    }

    if (pendingException != NULL)
    {
        std::rethrow_exception(pendingException);
    }
}

void ExportDiceCertificatePackage(
    std::wstring fileName
)
{
    std::exception_ptr pendingException = NULL;
    DWORD retVal = STDFUFILES_NOERROR;
    std::string dfuFileName(fileName.size(), '\0');
    HANDLE hDfu = INVALID_HANDLE_VALUE;
    HANDLE hImage = INVALID_HANDLE_VALUE;
    HANDLE hDfuFile = INVALID_HANDLE_VALUE;
    DFUIMAGEELEMENT dfuImageElement = { 0 };
    HCERTSTORE hPkcs7Store = NULL;
    PCCERT_CONTEXT hCert = NULL;

    try
    {
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, fileName.c_str(), fileName.size(), (LPSTR)dfuFileName.c_str(), dfuFileName.size(), NULL, NULL))
        {
            throw GetLastError();
        }
        WORD vid;
        WORD pid;
        WORD bcd;
        BYTE images;
        if ((retVal = STDFUFILES_OpenExistingDFUFile((PSTR)dfuFileName.c_str(), &hDfu, &vid, &pid, &bcd, &images)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        if ((retVal = STDFUFILES_ReadImageFromDFUFile(hDfu, 0, &hImage)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        if ((retVal = STDFUFILES_GetImageElement(hImage, 0, &dfuImageElement)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        std::vector<BYTE> image(dfuImageElement.dwDataLength, 0);
        dfuImageElement.Data = image.data();
        if ((retVal = STDFUFILES_GetImageElement(hImage, 0, &dfuImageElement)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        PDicePublicInfo_t dicePublic = (PDicePublicInfo_t)image.data();
        std::string certBag(dicePublic->certBag, dicePublic->certBagLen);
        const std::string startMarker("-----BEGIN CERTIFICATE-----\r\n");
        const std::string endMarker("-----END CERTIFICATE-----\r\n");
        uint32_t certNo = 0;
        std::string encDevCert;

        std::wstring pkcsFileName(fileName.substr(0, fileName.size() - 4));
        DeleteFile(std::wstring(pkcsFileName).append(L".p7b").c_str());
        DeleteFile(std::wstring(pkcsFileName).append(L".p7c").c_str());
        if ((hPkcs7Store = CertOpenStore(CERT_STORE_PROV_FILENAME_W, PKCS_7_ASN_ENCODING, NULL, CERT_STORE_CREATE_NEW_FLAG | CERT_FILE_STORE_COMMIT_ENABLE_FLAG, std::wstring(pkcsFileName).append(L".p7c").c_str())) == NULL)
        {
            throw GetLastError();
        }

        for (size_t cursor = certBag.find(startMarker, 0); ((cursor = certBag.find(startMarker, cursor)) != certBag.npos); cursor += encDevCert.size())
        {
            encDevCert = certBag.substr(cursor, certBag.find(endMarker, cursor) + endMarker.size());
            DWORD result;
            retVal = CryptStringToBinaryA(encDevCert.c_str(), encDevCert.size(), CRYPT_STRING_BASE64HEADER, NULL, &result, NULL, NULL);
            std::vector<BYTE> rawCert(result, 0);
            retVal = CryptStringToBinaryA(encDevCert.c_str(), encDevCert.size(), CRYPT_STRING_BASE64HEADER, rawCert.data(), &result, NULL, NULL);
            if ((hCert = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, rawCert.data(), rawCert.size())) == NULL)
            {
                throw retVal;
            }
            if (!CertAddCertificateContextToStore(hPkcs7Store, hCert, CERT_STORE_ADD_ALWAYS, NULL))
            {
                throw GetLastError();
            }
            CertFreeCertificateContext(hCert);
            hCert = NULL;
        }
        CertCloseStore(hPkcs7Store, 0);
        hPkcs7Store = NULL;
        MoveFile(std::wstring(pkcsFileName).append(L".p7c").c_str(), std::wstring(pkcsFileName).append(L".p7b").c_str());
    }
    catch (const std::exception& e)
    {
        pendingException = std::current_exception();
    }

    if (hCert)
    {
        CertFreeCertificateContext(hCert);
    }

    if (hPkcs7Store)
    {
        CertCloseStore(hPkcs7Store, 0);
    }

    if (hImage != INVALID_HANDLE_VALUE)
    {
        STDFUFILES_DestroyImage(&hImage);
    }

    if (hDfu != INVALID_HANDLE_VALUE)
    {
        STDFUFILES_CloseDFUFile(&hDfu);
    }

    if (pendingException != NULL)
    {
        std::rethrow_exception(pendingException);
    }
}

void CreateDiceApplication(
    std::wstring fileName,
    HCRYPTPROV_OR_NCRYPT_KEY_HANDLE codeAuth,
    std::vector<BYTE> alternateDigest,
    uint32_t version,
    uint32_t timeStamp
)
{
    DWORD retVal = STDFUFILES_NOERROR;
    HANDLE hDfu = INVALID_HANDLE_VALUE;
    HANDLE hDfuFile = INVALID_HANDLE_VALUE;
    DFUIMAGEELEMENT dfuImageElement = { 0 };
    BCRYPT_ALG_HANDLE hSha256 = NULL;
    std::exception_ptr pendingException = NULL;


    try
    {
        if ((retVal = BCryptOpenAlgorithmProvider(&hSha256, BCRYPT_SHA256_ALGORITHM, NULL, 0)) != 0)
        {
            throw retVal;
        }

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

        // Load the hex file and extend it with the header space
        std::vector<BYTE> image;
        PDiceEmbeddedSignature_t sigHdr = NULL;
        if ((retVal = STDFUFILES_GetImageElement(hDfu, 0, &dfuImageElement)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        image.resize(sizeof(DiceEmbeddedSignature_t) + dfuImageElement.dwDataLength);
        sigHdr = (PDiceEmbeddedSignature_t)image.data();
        dfuImageElement.Data = &image[sizeof(DiceEmbeddedSignature_t)];
        if ((retVal = STDFUFILES_GetImageElement(hDfu, 0, &dfuImageElement)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        dfuImageElement.Data = image.data();
        dfuImageElement.dwAddress -= sizeof(DiceEmbeddedSignature_t);
        dfuImageElement.dwDataLength += sizeof(DiceEmbeddedSignature_t);

        // Add the information about this image to the header
        sigHdr->s.sign.magic = DICEMAGIC;
        sigHdr->s.sign.codeSize = dfuImageElement.dwDataLength - sizeof(DiceEmbeddedSignature_t);
        sigHdr->s.sign.version = 0x00000000;
        sigHdr->s.sign.issueDate = GetTimeStamp();
        memcpy(sigHdr->s.sign.name, hexFileName.c_str(), hexFileName.length() - 4);
        if ((retVal = BCryptHash(hSha256, NULL, 0, &dfuImageElement.Data[sizeof(DiceEmbeddedSignature_t)], sigHdr->s.sign.codeSize, sigHdr->s.sign.appDigest, sizeof(sigHdr->s.sign.appDigest))) != 0)
        {
            throw retVal;
        }
        memset(sigHdr->s.sign.alternateDigest, 0x00, sizeof(sigHdr->s.sign.alternateDigest));

        // Pick up information from the previous built App
        WIN32_FIND_DATA fileData = { 0 };
        HANDLE search = INVALID_HANDLE_VALUE;
        std::wstring searchMask(fileName);
        searchMask.resize(searchMask.size() - 4);
        searchMask.append(L"-*-*-*.DFU");
        if ((search = FindFirstFile(searchMask.c_str(), &fileData)) != INVALID_HANDLE_VALUE)
        {
            while (FindNextFile(search, &fileData)); // Skip to the last one
            std::wstring lastDfuFile(fileData.cFileName);
            uint32_t lastVersion = std::stoul(lastDfuFile.substr(lastDfuFile.size() - 4 - 64 - 1 - 8, 8), NULL, 16);
            sigHdr->s.sign.version = lastVersion + 1;
            std::vector<BYTE> lastAlternateDigest = ReadHex(lastDfuFile.substr(lastDfuFile.size() - 4 - 64, 64));
            memcpy(sigHdr->s.sign.alternateDigest, lastAlternateDigest.data(), sizeof(sigHdr->s.sign.alternateDigest));
        }

        // Info override
        if (alternateDigest.size() == sizeof(sigHdr->s.sign.alternateDigest))
        {
            memcpy(sigHdr->s.sign.alternateDigest, alternateDigest.data(), sizeof(sigHdr->s.sign.alternateDigest));
        }
        if (version != -1)
        {
            sigHdr->s.sign.version = version;
        }
        if (timeStamp != -1)
        {
            sigHdr->s.sign.issueDate = timeStamp;
        }

        // Sign the header
        if (codeAuth != NULL)
        {
            std::vector<BYTE> hdrDigest(SHA256_DIGEST_LENGTH, 0);
            if ((retVal = BCryptHash(hSha256, NULL, 0, (PBYTE)&sigHdr->s.sign, sizeof(sigHdr->s.sign), hdrDigest.data(), hdrDigest.size())) != 0)
            {
                throw retVal;
            }
            std::vector<BYTE> sig(32 * 2, 0);
            DWORD result;
            if ((retVal = NCryptSignHash(codeAuth, NULL, hdrDigest.data(), hdrDigest.size(), sig.data(), sig.size(), &result, 0)) != 0)
            {
                throw retVal;
            }
            BigIntToBigVal(&sigHdr->s.signature.r, &sig[0], 32);
            BigIntToBigVal(&sigHdr->s.signature.s, &sig[32], 32);

            // Check the header signature
            if ((retVal = NCryptExportKey(codeAuth, NULL, BCRYPT_ECCPUBLIC_BLOB, NULL, NULL, 0, &result, 0)) != 0)
            {
                throw retVal;
            }
            std::vector<BYTE> codeAuthKeyData(result, 0);
            if ((retVal = NCryptExportKey(codeAuth, NULL, BCRYPT_ECCPUBLIC_BLOB, NULL, codeAuthKeyData.data(), codeAuthKeyData.size(), &result, 0)) != 0)
            {
                throw retVal;
            }
            BCRYPT_ECCKEY_BLOB* keyHdr = (BCRYPT_ECCKEY_BLOB*)codeAuthKeyData.data();
            ecc_publickey codeAuthPub = { 0 };
            BigIntToBigVal(&codeAuthPub.x, &codeAuthKeyData[sizeof(BCRYPT_ECCKEY_BLOB)], keyHdr->cbKey);
            BigIntToBigVal(&codeAuthPub.y, &codeAuthKeyData[sizeof(BCRYPT_ECCKEY_BLOB) + keyHdr->cbKey], keyHdr->cbKey);
            if ((retVal = Dice_DSAVerify((PBYTE)&sigHdr->s.sign, sizeof(sigHdr->s.sign), &sigHdr->s.signature, &codeAuthPub)) != DICE_SUCCESS)
            {
                throw retVal;
            }
        }
        else
        {
            // Unsigned Image
            memset(&sigHdr->s.signature, 0x00, sizeof(sigHdr->s.signature));
        }

        if ((retVal = STDFUFILES_SetImageElement(hDfu, 0, FALSE, dfuImageElement)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }
        if ((retVal = STDFUFILES_SetImageName(hDfu, (PSTR)hexFileName.c_str())) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }

        // Build the DFU filename
        std::wstring digestString(MAX_PATH, 0);
        DWORD cchDigestString = digestString.size();
        CryptBinaryToString(sigHdr->s.sign.appDigest, sizeof(sigHdr->s.sign.appDigest), CRYPT_STRING_HEXRAW, (LPWSTR)digestString.c_str(), &cchDigestString);
        digestString.resize(cchDigestString - 2);
        std::wstring versionString(MAX_PATH, 0);
        wsprintf((WCHAR*)versionString.c_str(), L"%08x", sigHdr->s.sign.version);
        versionString.resize(wcslen(versionString.c_str()));

        std::wstring wDfuFileName(fileName);
        wDfuFileName.resize(wDfuFileName.size() - 4);
        wDfuFileName.append(L"-");
        wDfuFileName.append(std::to_wstring(sigHdr->s.sign.issueDate));
        wDfuFileName.append(L"-");
        wDfuFileName.append(versionString);
        wDfuFileName.append(L"-");
        wDfuFileName.append(digestString);
        wDfuFileName.append(L".DFU");
        hexFileName.resize(wDfuFileName.size());
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wDfuFileName.c_str(), wDfuFileName.size(), (LPSTR)hexFileName.c_str(), hexFileName.size(), NULL, NULL))
        {
            throw GetLastError();
        }
        if ((retVal = STDFUFILES_CreateNewDFUFile((PSTR)hexFileName.c_str(), &hDfuFile, diceDeviceVid, diceDevicePid, diceDeviceVer)) != STDFUFILES_NOERROR)
        {
            throw retVal;
        }

        try
        {
            if ((retVal = STDFUFILES_AppendImageToDFUFile(hDfuFile, hDfu)) != STDFUFILES_NOERROR)
            {
                throw retVal;
            }
            PrintAppInfo(sigHdr);
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

    if (hSha256)
    {
        BCryptCloseAlgorithmProvider(hSha256, 0);
    }

    if (pendingException != NULL)
    {
        std::rethrow_exception(pendingException);
    }
}

//inline static void CloseBCryptAlgProv(BCRYPT_ALG_HANDLE hAlg)
//{
//    BCryptCloseAlgorithmProvider(hAlg, 0);
//}
//
//inline static BCRYPT_ALG_HANDLE OpenBcryptAlgProv(LPCWSTR pszAlgId, LPCWSTR pszImplementation, ULONG dwFlags)
//{
//    BCRYPT_ALG_HANDLE hAlg = NULL;
//    return (!BCryptOpenAlgorithmProvider(&hAlg, pszAlgId, pszImplementation, dwFlags)) ? hAlg : NULL;
//}
//
//inline static BCRYPT_KEY_HANDLE GenerateBcryptKeyPair(BCRYPT_ALG_HANDLE hAlgorithm, ULONG dwLength, ULONG dwFlags)
//{
//    BCRYPT_KEY_HANDLE hKey = NULL;
//    return (!BCryptGenerateKeyPair(hAlgorithm, &hKey, dwLength, dwFlags)) ? hKey : NULL;
//}
//
//inline static BCRYPT_KEY_HANDLE ImportBcryptKeyPair(BCRYPT_ALG_HANDLE hAlgorithm, BCRYPT_KEY_HANDLE hImportKey, LPCWSTR pszBlobType, PUCHAR pbInput, ULONG cbInput, ULONG dwFlags)
//{
//    BCRYPT_KEY_HANDLE hKey = NULL;
//    return (!BCryptImportKeyPair(hAlgorithm, hImportKey, pszBlobType, &hKey, pbInput, cbInput, dwFlags)) ? hKey : NULL;
//}
//
//void CheckCrypto()
//{
//    NTSTATUS status = 0;
//
//    // http://stackoverflow.com/questions/14841396/stdunique-ptr-deleters-and-the-win32-api
//    std::unique_ptr<std::remove_pointer<BCRYPT_ALG_HANDLE>::type, decltype(&::CloseBCryptAlgProv)> hAlg(::OpenBcryptAlgProv(BCRYPT_ECDSA_P256_ALGORITHM, NULL, 0), &::CloseBCryptAlgProv);
//
//    uint8_t digest1[32] = {
//        0xe3, 0x86, 0xbc, 0xee, 0x9d, 0xa3, 0xd8, 0x43, 0x60, 0x91, 0xb3, 0x9e, 0x35, 0x73, 0xc1, 0x88,
//        0x5e, 0x74, 0x9c, 0x27, 0x7a, 0x6d, 0x98, 0x6c, 0x8b, 0xc8, 0xff, 0xbe, 0x59, 0x1c, 0xbc, 0x72
//    };
//
//    uint8_t sig1[64] = {
//        0x4c, 0x84, 0xc4, 0x58, 0x27, 0xb9, 0x7e, 0x50, 0x61, 0xd7, 0x26, 0xa9, 0x61, 0xee, 0xf9, 0x67,
//        0x5f, 0x4a, 0x03, 0xac, 0x67, 0xb1, 0xa3, 0xd9, 0x76, 0x23, 0xfa, 0x55, 0xbe, 0x65, 0x6c, 0x2b,
//        0x60, 0x7a, 0xf9, 0x27, 0xe7, 0x40, 0xfb, 0x2b, 0x30, 0x09, 0x3b, 0xf7, 0xea, 0x44, 0x3a, 0x15,
//        0x99, 0x53, 0x86, 0x17, 0x91, 0xff, 0x38, 0x21, 0x60, 0x64, 0x0a, 0x92, 0x15, 0xdc, 0xcd, 0x29
//    };
//
//    uint8_t pub1[64] = {
//        0xdf, 0x73, 0xf3, 0xc5, 0xb1, 0x6e, 0x79, 0x33, 0x4a, 0x21, 0x57, 0xa9, 0xb5, 0x92, 0x75, 0x65,
//        0xff, 0xb3, 0x6f, 0x2f, 0x1e, 0x6f, 0xe7, 0xd2, 0xba, 0x2b, 0x76, 0x2d, 0xb8, 0xa0, 0xed, 0x05,
//        0x7b, 0xe6, 0xe3, 0xf7, 0x50, 0xf3, 0x55, 0xb4, 0xaf, 0xbb, 0xa7, 0xc6, 0xe9, 0x5e, 0x19, 0x3e,
//        0x6f, 0xc0, 0x6b, 0x28, 0xb8, 0x6d, 0xe8, 0x89, 0xe4, 0x78, 0xa2, 0xd1, 0x4c, 0xb5, 0x0d, 0x5b
//    };
//    std::vector<byte> pubKey(sizeof(BCRYPT_ECCKEY_BLOB) + 0x40, 0);
//    BCRYPT_ECCKEY_BLOB* keyHdr = (BCRYPT_ECCKEY_BLOB*)pubKey.data();
//    keyHdr->dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
//    keyHdr->cbKey = 0x20;
//    memcpy(&pubKey[sizeof(BCRYPT_ECCKEY_BLOB)], pub1, sizeof(pub1));
//
//    std::unique_ptr<std::remove_pointer<BCRYPT_KEY_HANDLE>::type, decltype(&::BCryptDestroyKey)> hPubKey(::ImportBcryptKeyPair(hAlg.get(), NULL, BCRYPT_ECCPUBLIC_BLOB, pubKey.data(), (ULONG)pubKey.size(), 0), &::BCryptDestroyKey);
//
//    status = BCryptVerifySignature(hPubKey.get(), NULL, digest1, sizeof(digest1), sig1, sizeof(sig1), 0);
//
//    std::unique_ptr<std::remove_pointer<BCRYPT_KEY_HANDLE>::type, decltype(&::BCryptDestroyKey)> hKey(::GenerateBcryptKeyPair(hAlg.get(), 256, 0), &::BCryptDestroyKey);
//    status = BCryptFinalizeKeyPair(hKey.get(), 0);
//
//    std::vector<byte> hash(32);
//    for (uint32_t n = 0; n < hash.size(); n++) hash[n] = n;
//    DWORD vectorSize;
//    
//    status = BCryptSignHash(hKey.get(), NULL, hash.data(), hash.size(), NULL, 0, &vectorSize, 0);
//    std::vector<byte> signature(vectorSize, 0);
//    status = BCryptSignHash(hKey.get(), NULL, hash.data(), hash.size(), signature.data(), signature.size(), &vectorSize, 0);
//
//    status = BCryptExportKey(hKey.get(), NULL, BCRYPT_ECCPRIVATE_BLOB, NULL, 0, &vectorSize, 0);
//    std::vector<byte> key(vectorSize, 0);
//    status = BCryptExportKey(hKey.get(), NULL, BCRYPT_ECCPRIVATE_BLOB, key.data(), key.size(), &vectorSize, 0);
//
//    keyHdr = (BCRYPT_ECCKEY_BLOB*)key.data();
//    uint32_t cursor = sizeof(BCRYPT_ECCKEY_BLOB);
//    ecc_publickey pub = {0};
//    BigIntToBigVal(&pub.x, &key.data()[cursor], keyHdr->cbKey);
//    cursor += keyHdr->cbKey;
//    BigIntToBigVal(&pub.y, &key.data()[cursor], keyHdr->cbKey);
//    cursor += keyHdr->cbKey;
//    ecc_privatekey prv = {0};
//    BigIntToBigVal(&prv, &key.data()[cursor], keyHdr->cbKey);
//    cursor += keyHdr->cbKey;
//
//    ecc_signature sig = {0};
//    BigIntToBigVal(&sig.r, &signature[0], keyHdr->cbKey);
//    BigIntToBigVal(&sig.s, &signature[keyHdr->cbKey], keyHdr->cbKey);
//
//    status = Dice_DSAVerifyDigest(hash.data(), &sig, &pub);
//    memset(&sig, 0x00, sizeof(sig));
//    status = Dice_DSASignDigest(hash.data(), &prv, &sig);
//
//    BigValToBigInt(&signature[0], &sig.r);
//    BigValToBigInt(&signature[keyHdr->cbKey], &sig.s);
//
//    status = BCryptVerifySignature(hKey.get(), NULL, hash.data(), hash.size(), signature.data(), signature.size(), 0);
//}
//
//void GenerateCertSample()
//{
//    ecc_privatekey devPrv = {0};
//    ecc_publickey devPub = {0};
//    ecc_privatekey comPrv = {0};
//    ecc_publickey comPub = {0};
//
//    Dice_GenerateDSAKeyPair(&devPub, &devPrv);
//    Dice_GenerateDSAKeyPair(&comPub, &comPrv);
//
//    std::vector<byte> pemBuffer1(DER_MAX_PEM, 0);
//    {
//        DICE_X509_TBS_DATA x509DeviceCertData = { { { 0 },
//            "DICECore", "MSFT", "US",
//            &devPub, &devPrv
//            },
//            1483228800, 2145830400,
//            { { 0 },
//            "DICECore", "MSFT", "US",
//            &devPub, &devPrv
//            } };
//        Dice_KDF_SHA256_Seed(x509DeviceCertData.Issuer.CertSerialNum,
//            sizeof(x509DeviceCertData.Issuer.CertSerialNum),
//            (const uint8_t*)&devPub,
//            sizeof(devPub),
//            NULL,
//            (const uint8_t*)x509DeviceCertData.Issuer.CommonName,
//            strlen(x509DeviceCertData.Issuer.CommonName));
//        x509DeviceCertData.Issuer.CertSerialNum[0] &= 0x7F; // Make sure the serial number is always positive
//        memcpy(x509DeviceCertData.Subject.CertSerialNum, x509DeviceCertData.Issuer.CertSerialNum, sizeof(x509DeviceCertData.Subject.CertSerialNum));
//
//        std::vector<byte> cerBuffer(DER_MAX_TBS, 0);
//        DERBuilderContext cerCtx = { 0 };
//        DERInitContext(&cerCtx, cerBuffer.data(), cerBuffer.size());
//        X509MakeCertBody(&cerCtx, &x509DeviceCertData);
//        X509MakeDeviceCert(&cerCtx, DICEVERSION, DICETIMESTAMP, 0, NULL);
//        X509CompleteCert(&cerCtx, &x509DeviceCertData);
//        pemBuffer1.resize(DERtoPEM(&cerCtx, 0, (char*)pemBuffer1.data(), pemBuffer1.size()));
//    }
//    std::vector<byte> pemBuffer2(DER_MAX_PEM, 0);
//    {
//        DicePublicInfo_t info = {0};
//        DiceEmbeddedSignature_t appHdr = { { { DICEMAGIC,
//            1234,
//            0x00010003,
//            1492473712,
//            "AppName",
//            { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f },
//            { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f }
//            },
//            { 0 } } };
//        DICE_X509_TBS_DATA x509DeviceCertData = { { { 0 },
//            "DICECore", "MSFT", "US",
//            &devPub, &devPrv
//            },
//            1483228800, 2145830400,
//            { { 0 },
//            appHdr.s.sign.name, "OEM", "US",
//            &comPub, &comPrv
//            } };
//        Dice_KDF_SHA256_Seed(x509DeviceCertData.Issuer.CertSerialNum,
//            sizeof(x509DeviceCertData.Issuer.CertSerialNum),
//            (const uint8_t*)&devPub,
//            sizeof(devPub),
//            NULL,
//            (const uint8_t*)x509DeviceCertData.Issuer.CommonName,
//            strlen(x509DeviceCertData.Issuer.CommonName));
//        x509DeviceCertData.Issuer.CertSerialNum[0] &= 0x7F; // Make sure the serial number is always positive
//        Dice_KDF_SHA256_Seed(x509DeviceCertData.Subject.CertSerialNum,
//            sizeof(x509DeviceCertData.Subject.CertSerialNum),
//            (const uint8_t*)&comPub,
//            sizeof(comPrv),
//            NULL,
//            (const uint8_t*)x509DeviceCertData.Subject.CommonName,
//            strlen(x509DeviceCertData.Subject.CommonName));
//        x509DeviceCertData.Subject.CertSerialNum[0] &= 0x7F; // Make sure the serial number is always positive
//
//        std::vector<byte> cerBuffer(DER_MAX_TBS, 0);
//        DERBuilderContext cerCtx = { 0 };
//        DERInitContext(&cerCtx, cerBuffer.data(), cerBuffer.size());
//        X509MakeCertBody(&cerCtx, &x509DeviceCertData);
//        X509MakeCompoundCert(&cerCtx, &info, &appHdr);
//        X509CompleteCert(&cerCtx, &x509DeviceCertData);
//        pemBuffer2.resize(DERtoPEM(&cerCtx, 0, (char*)pemBuffer2.data(), pemBuffer2.size()));
//    }
//    printf("%s%s\n", pemBuffer1.data(), pemBuffer2.data());
//}

void RunSIP(std::unordered_map<std::wstring, std::wstring> param)
{
    std::exception_ptr pendingException = NULL;
    std::unordered_map<std::wstring, std::wstring>::iterator it;
    std::wstring fileName(param.find(L"00")->second);
    HCERTSTORE hStore = NULL;
    PCCERT_CONTEXT hDevAuthCert = NULL;
    PCCERT_CONTEXT hCert = NULL;
    DWORD dwKeySpec;
    BOOL pfCallerFreeProvOrCryptKey;
    HCRYPTPROV_OR_NCRYPT_KEY_HANDLE deviceAuth = NULL;
    BCRYPT_KEY_HANDLE codeAuth = NULL;
    BOOL noClear = false;
    UINT32 rollBack = -1;

    try
    {
        std::vector<BYTE> certThumbPrint;
        if ((it = param.find(L"-at")) != param.end())
        {
            // Get NCrypt Handle to certificate private key pointed to by CertThumbPrint
            certThumbPrint = ReadHex(it->second);
        }
        else if ((it = param.find(L"-af")) != param.end())
        {
            // Get NCrypt Handle to certificate private key pointed to by Certificate file
            PCCERT_CONTEXT hCert = CertFromFile(it->second);
            certThumbPrint = CertThumbPrint(hCert);
            CertFreeCertificateContext(hCert);
        }
        else
        {
            throw DICE_INVALID_PARAMETER;
        }
        CRYPT_HASH_BLOB findTP = { certThumbPrint.size(), certThumbPrint.data() };
        if ((hStore = CertOpenSystemStore(NULL, L"MY")) == NULL)
        {
            throw DICE_INVALID_PARAMETER;
        }
        if ((hDevAuthCert = CertFindCertificateInStore(hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HASH, &findTP, NULL)) == NULL)
        {
            throw DICE_INVALID_PARAMETER;
        }
        if (!CryptAcquireCertificatePrivateKey(hDevAuthCert, CRYPT_ACQUIRE_SILENT_FLAG | CRYPT_ACQUIRE_ONLY_NCRYPT_KEY_FLAG, NULL, &deviceAuth, &dwKeySpec, &pfCallerFreeProvOrCryptKey))
        {
            throw DICE_INVALID_PARAMETER;
        }

        if (((it = param.find(L"-ct")) != param.end()) || ((it = param.find(L"-cf")) != param.end()))
        {
            std::vector<BYTE> certThumbPrint;
            if ((it = param.find(L"-ct")) != param.end())
            {
                // Get NCrypt Handle to certificate private key pointed to by CertThumbPrint
                certThumbPrint = ReadHex(it->second);
            }
            if ((it = param.find(L"-cf")) != param.end())
            {
                // Get NCrypt Handle to certificate private key pointed to by Certificate file
                PCCERT_CONTEXT hCert = CertFromFile(it->second);
                certThumbPrint = CertThumbPrint(hCert);
                CertFreeCertificateContext(hCert);
            }
            CRYPT_HASH_BLOB findTP = { certThumbPrint.size(), certThumbPrint.data() };
            if ((hCert = CertFindCertificateInStore(hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HASH, &findTP, NULL)) == NULL)
            {
                throw DICE_INVALID_PARAMETER;
            }
            if (!CryptImportPublicKeyInfoEx2(X509_ASN_ENCODING, &hCert->pCertInfo->SubjectPublicKeyInfo, 0, NULL, &codeAuth))
            {
                throw DICE_INVALID_PARAMETER;
            }
            CertCloseStore(hStore, 0);
            hStore = NULL;
            CertFreeCertificateContext(hCert);
            hCert = NULL;
        }
        if (param.find(L"-noclear") != param.end())
        {
            noClear = true;
        }
        if ((it = param.find(L"-rollback")) != param.end())
        {
            swscanf_s(it->second.c_str(), L"%d", &rollBack);
        }
        SignDiceIdentityPackage(fileName, hDevAuthCert, dwKeySpec, deviceAuth, codeAuth, noClear, rollBack);
    }

    catch (const std::exception& e)
    {
        pendingException = std::current_exception();
    }

    if (hStore)
    {
        CertCloseStore(hStore, 0);
    }

    if (hCert)
    {
        CertFreeCertificateContext(hCert);
    }

    if (hDevAuthCert)
    {
        CertFreeCertificateContext(hDevAuthCert);
    }

    if (codeAuth)
    {
        BCryptDestroyKey(codeAuth);
    }

    if ((deviceAuth) && (pfCallerFreeProvOrCryptKey))
    {
        if (dwKeySpec == CERT_NCRYPT_KEY_SPEC)
        {
            NCryptFreeObject(deviceAuth);
        }
        else
        {
            CryptReleaseContext(deviceAuth, 0);
        }
    }

    if (pendingException != NULL)
    {
        std::rethrow_exception(pendingException);
    }
}

void RunECP(std::unordered_map<std::wstring, std::wstring> param)
{
    std::unordered_map<std::wstring, std::wstring>::iterator it;
    std::wstring fileName(param.find(L"00")->second);

    ExportDiceCertificatePackage(fileName);
}

void RunCCT(std::unordered_map<std::wstring, std::wstring> param)
{
    std::exception_ptr pendingException = NULL;
    std::unordered_map<std::wstring, std::wstring>::iterator it;
    std::wstring fileName(param.find(L"00")->second);

    PCCERT_CONTEXT hCert = CertFromFile(fileName);
    std::vector<BYTE> digest = CertThumbPrint(hCert);
    PrintHex(digest);

    if (hCert)
    {
        CertFreeCertificateContext(hCert);
    }

    if (pendingException != NULL)
    {
        std::rethrow_exception(pendingException);
    }
}

void RunCAP(std::unordered_map<std::wstring, std::wstring> param)
{
    std::exception_ptr pendingException = NULL;
    std::unordered_map<std::wstring, std::wstring>::iterator it;
    std::wstring hexName(param.find(L"00")->second);
    HCERTSTORE hStore = NULL;
    PCCERT_CONTEXT hCert = NULL;
    DWORD dwKeySpec;
    BOOL pfCallerFreeProvOrCryptKey;
    HCRYPTPROV_OR_NCRYPT_KEY_HANDLE codeAuth = NULL;
    std::vector<BYTE> alternateDigest;
    UINT32 version = -1;
    UINT32 timeStamp = -1;

    try
    {
        if (((it = param.find(L"-ct")) != param.end()) || ((it = param.find(L"-cf")) != param.end()))
        {
            std::vector<BYTE> certThumbPrint;
            if ((it = param.find(L"-ct")) != param.end())
            {
                // Get NCrypt Handle to certificate private key pointed to by CertThumbPrint
                certThumbPrint = ReadHex(it->second);
            }
            if ((it = param.find(L"-cf")) != param.end())
            {
                // Get NCrypt Handle to certificate private key pointed to by Certificate file
                PCCERT_CONTEXT hCert = CertFromFile(it->second);
                certThumbPrint = CertThumbPrint(hCert);
                CertFreeCertificateContext(hCert);
            }
            CRYPT_HASH_BLOB findTP = { certThumbPrint.size(), certThumbPrint.data() };
            if((hStore = CertOpenSystemStore(NULL, L"MY")) == NULL)
            {
                throw DICE_INVALID_PARAMETER;
            }
            if((hCert = CertFindCertificateInStore(hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HASH, &findTP, NULL)) == NULL)
            {
                throw DICE_INVALID_PARAMETER;
            }
            if (!CryptAcquireCertificatePrivateKey(hCert, CRYPT_ACQUIRE_SILENT_FLAG | CRYPT_ACQUIRE_ONLY_NCRYPT_KEY_FLAG, NULL, &codeAuth, &dwKeySpec, &pfCallerFreeProvOrCryptKey))
            {
                throw DICE_INVALID_PARAMETER;
            }
        }
        if ((it = param.find(L"-d")) != param.end())
        {
            alternateDigest = ReadHex(it->second);
            if (alternateDigest.size() != SHA256_DIGEST_LENGTH)
            {
                throw DICE_INVALID_PARAMETER;
            }
        }
        if ((it = param.find(L"-version")) != param.end())
        {
            swscanf_s(it->second.c_str(), L"%d", &version);
        }
        if ((it = param.find(L"-timestamp")) != param.end())
        {
            swscanf_s(it->second.c_str(), L"%d", &timeStamp);
        }
        CreateDiceApplication(hexName, codeAuth, alternateDigest, version, timeStamp);
    }

    catch (const std::exception& e)
    {
        pendingException = std::current_exception();
    }

    if (hStore)
    {
        CertCloseStore(hStore, 0);
    }

    if (hCert)
    {
        CertFreeCertificateContext(hCert);
    }

    if ((codeAuth) && (pfCallerFreeProvOrCryptKey))
    {
        if (dwKeySpec == CERT_NCRYPT_KEY_SPEC)
        {
            NCryptFreeObject(codeAuth);
        }
        else
        {
            CryptReleaseContext(codeAuth, 0);
        }
    }

    if (pendingException != NULL)
    {
        std::rethrow_exception(pendingException);
    }
}

// Debug parameter:
// SIP diceID.dfu -at=9252851cf0e10901d5538f2fb92adb5ef47b7fb0 -ct=9d0d05f53c5ea8260264c584862ee2de56eaf974 -noClear
// ECP diceID2.dfu
// CAP payload.hex -ct=9d0d05f53c5ea8260264c584862ee2de56eaf974

void PrintHelp(void)
{
    wprintf(L"Calculate Certificate Thumbprint:\nCCT certFile.cer\n");
    wprintf(L"Sign DeviceID Package:\nSIP [dfuFileName] [-at=DevAuthCertTP | -af=DevAuthCertFile] { -ct=CodeAuthCertTP | -cf=CodeAuthCertFile | -noClear | -rollBack=0 }\n");
    wprintf(L"Export Certificate Package:\nECP [dfuFileName]\n");
    wprintf(L"Create APP Package:\nCAP [hexFileName] { -ct=CodeAuthCertTP | -cf=CodeAuthCertFile | -d=alternateDigest | -version=0 | -timeStamp=0 }\n");
}

int wmain(int argc, const wchar_t** argv)
{
    try
    {
        if (argc <= 2)
        {
            throw ERROR_INVALID_PARAMETER;
        }
        std::wstring cmd(argv[1]);
        WSTR_TO_LOWER(cmd);
        std::unordered_map<std::wstring, std::wstring> param;
        for (UINT32 n = 2; n < (unsigned int)argc; n++)
        {
            std::wstring element(argv[n]);
            size_t divider = element.find('=', 0);
            std::pair<std::wstring, std::wstring> newPair;
            if ((element[0] != '-') && (divider == std::string::npos))
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

        if ((cmd == std::wstring(L"sip")) && (param.size() >= 2))
        {
            RunSIP(param);
        }
        else if ((cmd == std::wstring(L"ecp")) && (param.size() == 1))
        {
            RunECP(param);
        }
        else if ((cmd == std::wstring(L"cct")) && (param.size() == 1))
        {
            RunCCT(param);
        }
        else if ((cmd == std::wstring(L"cap")) && (param.size() >= 1))
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

