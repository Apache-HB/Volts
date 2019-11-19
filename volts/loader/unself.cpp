#include "unself.h"

#include "elf.h"

#include "keys/keys.h"

#include <external/aes/aes.h>

#include <spdlog/spdlog.h>

#include "svl/stream.h"
#include "svl/convert.h"
#include "svl/endian.h"

#include <sstream>

using svl::endian::big;

using namespace svl;
namespace cvt = svl::convert;

namespace volts::loader::unself
{
    namespace sce
    {
        struct header
        {
            u32 magic;
            big<u32> version;
            big<u16> type;
            big<u16> category;
            big<u32> metadata_start;
            big<u64> metadata_end;
            big<u64> size;
        };

        static_assert(sizeof(header) == 32);
    }

    namespace self
    {
        struct header
        {
            big<u64> type;
            big<u64> app_info_offset;
            big<u64> elf_offset;
            big<u64> prog_offset;
            big<u64> sect_offset;
            big<u64> sect_info_offset;
            big<u64> version_offset;
            big<u64> control_offset;
            big<u64> control_length;
            pad padding[8];  
        };

        static_assert(sizeof(header) == 80);

        struct control_info
        {
            u32 type;
            u64 size;
            u64 next;

            union
            {
                struct 
                {
                    big<u32> flags;
                    pad padding[28];
                } control_flags;

                static_assert(sizeof(control_flags) == 32);

                struct 
                {
                    u8 constant[20];
                    u8 elf_digest[20];
                    big<u64> required_version;
                } elf_digest_48;

                static_assert(sizeof(elf_digest_48) == 48);

                struct
                {
                    u8 const_or_digest[20];
                    pad padding[12];
                } elf_digest_32;

                static_assert(sizeof(elf_digest_32) == 32);

                struct 
                {
                    u32 magic;
                    big<u32> version;
                    big<u32> drm_type;
                    big<u32> app_type;
                    byte content_id[48];
                    byte digest[16];
                    byte inv_digest[16];
                    byte xor_digest[16];
                    pad padding[16];
                } npdrm_info;

                static_assert(sizeof(npdrm_info) == 128);
            };
        };
    }

    namespace metadata
    {
        struct info
        {
            byte key[16];
            pad key_pad[16];

            byte iv[16];
            pad iv_pad[16];
        };

        static_assert(sizeof(info) == 64);

        struct header
        {
            big<u64> signature_length;
            big<u32> algorithm;
            big<u32> sect_count;
            big<u32> key_count;
            big<u32> header_size;
            pad padding[8];
        };

        static_assert(sizeof(header) == 32);

        struct section
        {
            big<u64> offset;
            big<u64> size;
            big<u32> type;
            big<u32> index;
            big<u32> hash_algo;
            big<u32> hash_index;
            big<u32> encrypted;

            big<u32> key_index;
            big<u32> iv_index;

            big<u32> compressed;
        };

        static_assert(sizeof(section) == 48);
    }

    struct app_info
    {
        big<u64> auth;
        big<u32> vendor;
        big<u32> type;
        big<u64> version;
        pad padding[8];
    };

    static_assert(sizeof(app_info) == 32);

    struct self_decryptor
    {
        self_decryptor(std::istream& file)
            : stream(file)
        {}

        bool load_headers()
        {
            sce_header = streams::read<sce::header>(stream);

            if(sce_header.magic != cvt::to_u32("SCE\0"))
            {
                spdlog::error("invalid sce magic");
                return false;
            }

            self_header = streams::read<self::header>(stream);

            info = streams::read<app_info>(stream);
            elf_header = streams::read<elf::header<u64>>(stream);

            if(elf_header.magic != cvt::to_u32("\177ELF"))
            {
                spdlog::error("invalid elf magic");
                return false;
            }

            prog_headers = streams::read_n<elf::program_header<u64>>(stream, elf_header.prog_count);

            stream.seekg(self_header.sect_offset.get());

            sect_headers = streams::read_n<elf::section_header<u64>>(stream, elf_header.sect_count);

            stream.seekg(self_header.control_offset.get());

            int c = 0;
            while(c < self_header.control_length)
            {
                auto ctrl = read_control_info();
                c += ctrl.size;

                controls.push_back(ctrl);
            }

            return true;
        }

        bool load_metadata(const std::vector<byte>& key)
        {
            stream.seekg(sce_header.metadata_start + sizeof(sce::header));

            auto meta_info = streams::read<metadata::info>(stream);

            const int header_size = sce_header.metadata_end
                - sizeof(sce::header)
                - sce_header.metadata_start
                - sizeof(metadata::info);

            stream.seekg(sce_header.metadata_start + sizeof(sce::header) + sizeof(metadata::info));

            auto headers = streams::read_n(stream, header_size);

            aes_context aes;

            auto k = keys::get_self_key((key_type)info.type.get(), sce_header.type, info.version);

            if(!k)
            {
                spdlog::error("couldnt find key");
                return false;
            }

            if((sce_header.type & 0x8000) != 0x8000)
            {
                if(!decrypt_npdrm(cvt::as_bytes(meta_info), key))
                    return false;

                aes_setkey_dec(&aes, k->erk, 256);
                aes_crypt_cbc(
                    &aes, 
                    AES_DECRYPT, 
                    sizeof(metadata::info), 
                    k->riv, 
                    (byte*)&meta_info, 
                    (byte*)&meta_info
                );
            }

            for(int i = 0; i < 16; i++)
            {
                if(meta_info.key_pad[i] | meta_info.iv_pad[i])
                {
                    spdlog::error("failed to decrypt metadata info");
                    return false;
                }
            }

            size_t offset = 0;
            byte stream_buffer[16] = {};
            aes_setkey_enc(&aes, meta_info.key, 128);
            aes_crypt_ctr(
                &aes,
                headers.size(),
                &offset,
                meta_info.iv,
                stream_buffer,
                headers.data(),
                headers.data()
            );

            meta_header = *(metadata::header*)headers.data();

            int len = meta_header.key_count * 16;

            for(int i = 0; i < meta_header.sect_count; i++)
            {
                auto sect = *(metadata::section*)(headers.data() + sizeof(metadata::header) + sizeof(metadata::section) * i);

                if(sect.encrypted == 3)
                    len += sect.size;

                meta_sections.push_back(sect);
            }

            auto* front = headers.data() + sizeof(metadata::header) + meta_header.sect_count * sizeof(metadata::section);
            auto* back = front + len;

            data_keys = { front, back };

            return true;
        }

        void decrypt()
        {

        }

        std::vector<svl::byte> elf()
        {
            std::vector<svl::byte> ret;
            streams::vectorbuf buf(&ret);
            std::ostream out(&buf);

            return ret;
        }

    private:
        bool decrypt_npdrm(std::vector<byte> data, std::vector<byte> meta_key)
        {
            auto ctrl = std::find_if(std::begin(controls), std::end(controls), [](auto& val) { return val.type == 3; });

            if(ctrl == std::end(controls))
                return true;

            byte key[16];

            switch(ctrl->npdrm_info.version)
            {
            case 1: 
                spdlog::error("cant decrypt network license");
                return false;
            case 2: 
                spdlog::error("cant decrypt local lisence yet");
                return false;
            case 3: 
                std::memcpy(key, meta_key.empty() ? meta_key.data() : keys::free_klic, sizeof(key));
                return true;
            default: 
                spdlog::error("invalid license type");
                return false;
            }
        }

        self::control_info read_control_info()
        {
            self::control_info ret;

            ret.type = endian::byte_swap(streams::read<u32>(stream));
            ret.size = endian::byte_swap(streams::read<u32>(stream));
            ret.next = endian::byte_swap(streams::read<u64>(stream));

            if(ret.type == 1)
            {
                ret.control_flags = streams::read<decltype(self::control_info::control_flags)>(stream);
            }
            else if(ret.type == 2)
            {
                if(ret.size == 48)
                {
                    ret.elf_digest_48 = streams::read<decltype(self::control_info::elf_digest_48)>(stream);
                }
                else if(ret.size == 64)
                {
                    ret.elf_digest_32 = streams::read<decltype(self::control_info::elf_digest_32)>(stream);
                }
            }
            else if(ret.type == 3)
            {
                ret.npdrm_info = streams::read<decltype(self::control_info::npdrm_info)>(stream);
            }
            else
            {
                spdlog::error("invalid control info type");
            }

            return ret;
        }

        // headers
        sce::header sce_header;
        self::header self_header;
        elf::header<u64> elf_header;
        app_info info;

        std::vector<elf::program_header<u64>> prog_headers;
        std::vector<elf::section_header<u64>> sect_headers;

        // metadata
        metadata::header meta_header;
        std::vector<self::control_info> controls;
        std::vector<byte> data_keys;
        std::vector<metadata::section> meta_sections;

        std::istream& stream;
    };

    std::vector<svl::byte> load(std::istream& file, std::vector<byte> key)
    {
        self_decryptor dec(file);

        if(!dec.load_headers())
        {
            return {};
        }

        if(!dec.load_metadata(key))
        {
            return {};
        }

        dec.decrypt();

        return dec.elf();
    }
}