echo [NewRequest] > template.inf
echo Subject = "CN=AppAuthority, O=MSFT, C=US" >> template.inf
echo HashAlgorithm = sha256 >> template.inf
echo keyAlgorithm = ECDSA_P256 >> template.inf
echo KeyUsage = "CERT_DIGITAL_SIGNATURE_KEY_USAGE" >> template.inf
echo KeyUsageProperty = "NCRYPT_ALLOW_SIGNING_FLAG" >> template.inf
echo ProviderName = "Microsoft Software Key Storage Provider" >> template.inf
echo RequestType = Cert >> template.inf
echo Exportable = true >> template.inf
echo ExportableEncrypted = false >> template.inf
echo [EnhancedKeyUsageExtension] >> template.inf
echo OID=1.3.6.1.5.5.7.3.3 >> template.inf
certreq.exe -new template.inf AppAuthorityB64.cer
certutil.exe -decode AppAuthorityB64.cer AppAuthority.cer
echo [NewRequest] > template.inf
echo Subject = "CN=DiceAuthority, O=MSFT, C=US" >> template.inf
echo HashAlgorithm = sha256 >> template.inf
echo keyAlgorithm = ECDSA_P256 >> template.inf
echo KeyUsage = "CERT_DIGITAL_SIGNATURE_KEY_USAGE | CERT_KEY_CERT_SIGN_KEY_USAGE | CERT_CRL_SIGN_KEY_USAGE" >> template.inf
echo KeyUsageProperty = "NCRYPT_ALLOW_SIGNING_FLAG" >> template.inf
echo ProviderName = "Microsoft Software Key Storage Provider" >> template.inf
echo RequestType = Cert >> template.inf
echo Exportable = true >> template.inf
echo ExportableEncrypted = false >> template.inf
certreq.exe -new template.inf DiceAuthorityB64.cer
certutil.exe -decode DiceAuthorityB64.cer DiceAuthority.cer
certutil -addstore "Root" DiceAuthority.cer
del template.inf