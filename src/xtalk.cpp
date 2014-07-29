// Copyright (c) 2014 Talkcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define _CRT_SECURE_NO_DEPRECATE
#define CRYPTOPP_DEFAULT_NO_DLL
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1

#include "cryptopp/dll.h"
#include "cryptopp/default.h"

USING_NAMESPACE(CryptoPP)
USING_NAMESPACE(std)

namespace XTALK
{
    string EncryptString(const char *instr, const char *passPhrase)
    {
        string outstr;

        DefaultEncryptorWithMAC encryptor(passPhrase, new HexEncoder(new StringSink(outstr)));
        encryptor.Put((byte *)instr, strlen(instr));
        encryptor.MessageEnd();

        return outstr;
    }

    string DecryptString(const char *instr, const char *passPhrase)
    {
        string outstr;

        HexDecoder decryptor(new DefaultDecryptorWithMAC(passPhrase, new StringSink(outstr)));
        decryptor.Put((byte *)instr, strlen(instr));
        decryptor.MessageEnd();

        return outstr;
    }

    string Decode(string str, string pwd)
    {
        try
        {
            return XTALK::DecryptString(str.c_str(), pwd.c_str());
        }
        catch (...)
        {
            return "";
        }
    }
}
