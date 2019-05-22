#pragma once

#include "Meta/Aliases.h"

namespace Volts
{
    // we only support 128 and 256 bit decryption for now
    enum class KeySize : Cthulhu::U32
    {
        S128 = 128,
        S256 = 256,
    };

    struct AES
    {
        Cthulhu::U8 Rounds;
        Cthulhu::U32* RoundKeys;
        Cthulhu::U32 Buffer[68];

        void SetKeyEnc(const Cthulhu::Byte* Key, KeySize Size);
        void SetKeyDec(const Cthulhu::Byte* Key, KeySize Size);

        void EncryptCBC(Cthulhu::U32 Length, Cthulhu::Byte IV[16], const Cthulhu::Byte* Input, Cthulhu::Byte* Output);
        void DecryptCBC(Cthulhu::U32 Length, Cthulhu::Byte IV[16], const Cthulhu::Byte* Input, Cthulhu::Byte* Output);

        void CryptCTR(Cthulhu::U32 Length, Cthulhu::U32* Offset, const Cthulhu::Byte* Input, Cthulhu::Byte* Output);
    };
}