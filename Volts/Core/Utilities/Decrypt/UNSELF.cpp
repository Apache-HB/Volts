#include "UNSELF.h"

#include "Core/Utilities/Logger/Logger.h"

#include "Core/Utilities/Endian.h"
#include "Core/Utilities/Convert.h"

using namespace Cthulhu;

namespace Volts
{
    // SELF files are big, complicated files that were designed to not be easy to decrypt
    // this code is going to reflect the many stages required to bypass all of sonys security
    namespace SCE
    {
        struct Header
        {
            // should always be 4539219ULL or "SCE\0"
            U32 Magic;
            
            // will be 2 for a PS3 game and 3 for a PSVita game
            Big<U32> HeaderVersion;

            Big<U16> KeyType;
            Big<U16> FileCategory;
            Big<U32> MetadataOffset;
            Big<U64> HeaderLength;
            Big<U64> DataLength;
        };

        struct VersionInfo
        {
            Big<U32> SubType;
            Big<U32> Present;
            Big<U32> Size;
            U8 Padding[4];
        };
    }

    namespace SELF
    {
        struct Header
        {
            U64 Type;
            Big<U64> InfoOffset;
            Big<U64> ELFOffset;
            Big<U64> ProgramHeaderOffset;
            Big<U64> SectionHeaderOffset;
            Big<U64> SectionInfoOffset;
            Big<U64> VersionOffset;
            Big<U64> ControlOffset;
            Big<U64> ControlLength;
            U8 Padding[8];
        };
    }

    namespace ELF
    {
        struct SmallHeader
        {
            Big<U32> Magic;
            U8 Class;
            U8 Endianness;
            U8 Version;
            U8 ABI;
            U8 ABIVersion;
            U8 Padding[7];
        };

        template<typename T>
        struct BigHeader
        {
            Big<U16> Type;
            Big<U16> Machine;
            Big<U32> Version;
            
            Big<T> Entry;
            Big<T> PHOffset;
            Big<T> SHOffset;
            
            Big<U32> Flags;
            Big<U16> HeaderSize;
            
            Big<U16> PHEntrySize;
            Big<U16> PHCount;
            
            Big<U16> SHEntrySize;
            Big<U16> SHCount;
        };

        template<typename T>
        struct ProgramHeader {};

        template<> struct ProgramHeader<U32> 
        {
            Big<U32> Type;
            Big<U32> Flags;

            Big<U32> Offset;
            Big<U32> VirtualAddress;
            Big<U32> PhysicalAddress;
            Big<U32> FileSize;
            Big<U32> MemorySize;

            Big<U32> Align;
        };

        template<> struct ProgramHeader<U64> 
        {
            Big<U32> Type;

            Big<U32> Offset;
            Big<U32> VirtualAddress;
            Big<U32> PhysicalAddress;
            Big<U32> FileSize;
            Big<U32> MemorySize;
            
            Big<U32> Flags;
            Big<U32> Align;
        };

        template<typename T>
        struct SectionHeader
        {
            Big<U32> NameOffset;
            Big<U32> Type;

            Big<T> Flags;
            Big<T> VirtualAddress;
            Big<T> Offset;
            Big<T> Size;

            Big<U32> Link;
            Big<U32> Info;

            Big<T> Align;
            Big<T> EntrySize;
        };
    }

    struct AppInfo
    {
        Big<U64> AuthID;
        Big<U32> VendorID;
        Big<U32> Type;
        Big<U32> Version;
        U8 Padding[12];
    };

    struct SectionInfo
    {
        Big<U64> Offset;
        Big<U64> Size;
        Big<U32> Compressed; // 1 if uncompressed, 2 if compressed
        U8 Padding[8]; // should always be 0
        Big<U32> Encrypted; // 1 if encrypted, 2 if unecrypted
    };

    struct ControlInfo
    {
        U32 Type;
        U32 Size;
        U64 HasNext;

        union
        {
            struct
            {
                Big<U32> ControlFlag;
                U8 Padding[28];
            } ControlFlags;

            struct
            {
                U8 Constant[20];
                U8 Digest[20];
                Big<U64> RequiredSystemVersion;
            } ELFDigest40;

            struct
            {
                U8 ConstantOrDigest[20];
                U8 Padding[12];
            } ELFDigest30;

            struct
            {
                Big<U32> Magic;
                Big<U32> LicenseVersion;
                Big<U32> DRMType;
                Big<U32> AppType;
                U8 ContentID[48];
                U8 Digest[16];
                U8 INVDigest[16];
                U8 XORDigest[16];
                U8 Padding[16];
            } NPDRMInfo;
        };
    };

    struct Decryptor
    {
        Decryptor(FS::BufferedFile& F)
            : File(F)
        {}

        ~Decryptor()
        {
            if(Is32)
            {
                delete PHeaders32;
                delete SHeaders32;
            }
            else
            {
                delete PHeaders64;
                delete SHeaders64;
            }
        }

    public:

        bool LoadSCE()
        {
            File.Seek(0);
            // Read in the SCE header
            // this is essentially metadata the ps3 uses internally
            // we need some, but not all of it
            auto SCE = File.Read<SCE::Header>();

            LOGF_INFO("UNSELF",
                "SCE::Header{"
                "Magic = %u, "
                "HeaderVersion = %u, "
                "KeyType = %u, "
                "FileCategory = %u, "
                "MetadataOffset = %u, "
                "HeaderLength = %llu, "
                "DataLength = %llu}",
                SCE.Magic,
                SCE.HeaderVersion.Get(),
                SCE.KeyType.Get(),
                SCE.FileCategory.Get(),
                SCE.MetadataOffset.Get(),
                SCE.HeaderLength.Get(),
                SCE.DataLength.Get()
            );

            if(SCE.Magic != "SCE\0"_U32)
            {
                LOG_INFO("UNSELF", "SCE Header magic isnt valid");
                return false;
            }

            return true;
        }

        bool LoadSELF()
        {
            SELFHead = File.Read<SELF::Header>();
            
            LOGF_INFO("UNSELF",
                "SELF::Header{"
                "Type = %llu, "
                "AppInfoOffset = %llu, "
                "ELFOffset = %llu, "
                "PHeadOffset = %llu, "
                "SHeadOffset = %llu, "
                "SInfoOffset = %llu, "
                "VersionOffset = %llu, "
                "ControlInfoOffset = %llu, "
                "ControlLength = %llu}",
                SELFHead.Type,
                SELFHead.InfoOffset.Get(),
                SELFHead.ELFOffset.Get(),
                SELFHead.ProgramHeaderOffset.Get(),
                SELFHead.SectionHeaderOffset.Get(),
                SELFHead.SectionInfoOffset.Get(),
                SELFHead.VersionOffset.Get(),
                SELFHead.ControlOffset.Get(),
                SELFHead.ControlLength.Get()
            );

            return true;
        }

        bool LoadAppInfo()
        {
            File.Seek(SELFHead.InfoOffset);
            Info = File.Read<AppInfo>();

            LOGF_INFO("UNSELF",
                "AppInfo{"
                "AuthID = %llu, "
                "VendorID = %u, "
                "Type = %u, "
                "Version = %u}",
                Info.AuthID.Get(),
                Info.VendorID.Get(),
                Info.Type.Get(),
                Info.Version.Get()
            );

            return true;
        }

        bool LoadELF()
        {
            File.Seek(SELFHead.ELFOffset);

            auto Small = File.Read<ELF::SmallHeader>();

            LOGF_INFO("UNSELF",
                "ELF::SmallHeader{"
                "Magic = %u, "
                "Class = %u, "
                "Endian = %u, "
                "Version = %u, "
                "ABI = %u, "
                "ABIVersion = %u}",
                Small.Magic.Get(),
                Small.Class,
                Small.Endianness,
                Small.Version,
                Small.ABI,
                Small.ABIVersion
            );

            if(Small.Magic != 0x7F454C46)
            {
                LOG_ERROR("UNSELF", "ELF Header had incorrect magic");
                return false;
            }

            if(Small.Class == 1)
            {
                Is32 = true;
                LOG_INFO("UNSELF", "ELF Header is 32 bit");
                // is 32 bit
                ELFHead32 = File.Read<ELF::BigHeader<U32>>();
            }
            else
            {
                Is32 = false;
                LOG_INFO("UNSELF", "ELF Header is 64 bit");
                // must be 64 bit otherwise
                ELFHead64 = File.Read<ELF::BigHeader<U64>>();
            }

            return true;
        }

        // load program and section headers
        bool LoadSubHeaders()
        {
            File.Seek(SELFHead.ProgramHeaderOffset);

            if(Is32)
            {
                // load program headers and then section headers
                PHeaders32 = new Array<ELF::ProgramHeader<U32>>();

                if(ELFHead32.PHOffset == 0 && ELFHead32.PHCount)
                {
                    LOG_ERROR("UNSELF", "ELF Program header offset is 0");
                    return false;
                }

                for(U32 I = 0; I < ELFHead32.PHCount; I++)
                {
                    PHeaders32->Append(File.Read<ELF::ProgramHeader<U32>>());
                }

                SHeaders32 = new Array<ELF::SectionHeader<U32>>();

                if(ELFHead32.SHOffset == 0 && ELFHead32.SHCount)
                {
                    LOG_ERROR("UNSELF", "ELF Section header offset is 0");
                    return false;
                }

                File.Seek(SELFHead.SectionHeaderOffset);

                for(U32 I = 0; I < ELFHead32.SHCount; I++)
                {
                    SHeaders32->Append(File.Read<ELF::SectionHeader<U32>>());
                }
            }
            else 
            {
                PHeaders64 = new Array<ELF::ProgramHeader<U64>>();

                if(ELFHead64.PHOffset == 0 && ELFHead64.PHCount)
                {
                    LOG_ERROR("UNSELF", "ELF Program header offset is 0");
                    return false;
                }

                for(U32 I = 0; I < ELFHead64.PHCount; I++)
                {
                    PHeaders64->Append(File.Read<ELF::ProgramHeader<U64>>());
                }

                SHeaders64 = new Array<ELF::SectionHeader<U64>>();

                if(ELFHead64.SHOffset == 0 && ELFHead64.SHCount)
                {
                    LOG_ERROR("UNSELF", "ELF Section header offset is 0");
                    return false;
                }

                File.Seek(SELFHead.SectionHeaderOffset);

                for(U32 I = 0; I < ELFHead64.SHCount; I++)
                {
                    SHeaders64->Append(File.Read<ELF::SectionHeader<U64>>());
                }
            }

            return true;
        }

        bool LoadSectionInfoHeaders()
        {
            File.Seek(SELFHead.SectionInfoOffset);

            const U32 InfoCount = (Is32 ? ELFHead32.PHCount : ELFHead64.PHCount);

            for(U32 I = 0; I < InfoCount; I++)
            {
                SInfoArray.Append(File.Read<SectionInfo>());
            }

            return true;
        }

        void LoadVersionInfo()
        {
            File.Seek(SELFHead.VersionOffset);
            LOGF_INFO("UNSELF",
                "Seeked to %llu",
                SELFHead.VersionOffset.Get()
            );
            SCEVer = File.Read<decltype(SCEVer)>();

            LOGF_INFO("UNSELF",
                "SCE::VersionInfo{"
                "SubType = %u, "
                "Present = %u, "
                "Size = %u"
                "}",
                SCEVer.SubType.Get(),
                SCEVer.Present.Get(),
                SCEVer.Size.Get()
            );
        }

        void LoadControlInfo()
        {
            File.Seek(SELFHead.ControlOffset);

            U32 I = 0;
            while(I < SELFHead.ControlLength)
            {
                auto CInfo = ReadControlInfo();
                CInfoArray.Append(CInfo);
                I += CInfo.Size;
            }
        }

        ControlInfo ReadControlInfo()
        {
            LOG_INFO("UNSELF", "Loading Control Info");
            
            ControlInfo CTRL;
            CTRL.Type = Math::ByteSwap(File.Read<U32>());
            CTRL.Size = Math::ByteSwap(File.Read<U32>());
            CTRL.HasNext = Math::ByteSwap(File.Read<U64>());

            if(CTRL.Type == 1)
            {   
                LOG_INFO("UNSELF", "Type 1 Control");
                CTRL.ControlFlags = File.Read<decltype(CTRL.ControlFlags)>();
            }
            else if(CTRL.Type == 2)
            {
                if(CTRL.Size == 48)
                {
                    LOG_INFO("UNSELF", "Type 2, Size 48 Digest");
                    CTRL.ELFDigest30 = File.Read<decltype(CTRL.ELFDigest30)>();
                }
                else if(CTRL.Size == 64)
                {
                    LOG_INFO("UNSELF", "Type 2, Size 64 Digest");
                    CTRL.ELFDigest40 = File.Read<decltype(CTRL.ELFDigest40)>();
                }
            }
            else if(CTRL.Type == 3)
            {
                LOG_INFO("UNSELF", "NPDRM Control Info");
                CTRL.NPDRMInfo = File.Read<decltype(CTRL.NPDRMInfo)>();
            }
            else
            {
                LOGF_INFO("UNSELF", "Invalid Control Type of %u", CTRL.Type);
            }

            return CTRL;
        }

        bool LoadHeaders() 
        {
            LoadSCE();
            LoadSELF();

            LoadAppInfo();
            
            LoadELF();
            LoadSubHeaders();
            
            LoadVersionInfo();
            LoadControlInfo();
            LoadSectionInfoHeaders();

            return true;
        }

        bool LoadData() 
        {
            return true;
        }

        ELF::Binary DecryptData() 
        { 
            return {}; 
        }

        bool Is32;

        FS::BufferedFile& File;

        SCE::Header SCEHead;
        SELF::Header SELFHead;
        AppInfo Info;

        Array<SectionInfo> SInfoArray;
        Array<ControlInfo> CInfoArray;

        SCE::VersionInfo SCEVer;

        union
        {
            ELF::BigHeader<U32> ELFHead32;
            ELF::BigHeader<U64> ELFHead64;
        };

        union
        {
            struct
            {
                Array<ELF::SectionHeader<U32>>* SHeaders32;
                Array<ELF::ProgramHeader<U32>>* PHeaders32;
            };

            struct
            {
                Array<ELF::SectionHeader<U64>>* SHeaders64;
                Array<ELF::ProgramHeader<U64>>* PHeaders64;
            };
        };
    };

    namespace UNSELF
    {
        // file format reference from https://www.psdevwiki.com/ps3/SELF_File_Format_and_Decryption#Extracting_an_ELF
        ELF::Binary DecryptSELF(FS::BufferedFile& File)
        {
            Decryptor Decrypt(File);

            if(!Decrypt.LoadHeaders())
            {
                return {};
            }

            if(!Decrypt.LoadData())
            {
                return {};
            }

            return Decrypt.DecryptData();
        }
    }
}

