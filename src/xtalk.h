// Copyright (c) 2009-2012 Bitcoin Developers
// Copyright (c) 2014 Talkcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef TALKCOIN_XTALK_H
#define TALKCOIN_XTALK_H

#include <string>
using namespace std;

namespace XTALK
{
    string EncryptString(const char *instr, const char *passPhrase);
    string DecryptString(const char *instr, const char *passPhrase);
    string Decode(string str, string pwd);
}

#endif
