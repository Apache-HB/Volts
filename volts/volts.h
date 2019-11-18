#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "files/sfo.h"

#include "svl/stream.h"

#include <filesystem>
#include <fstream>

#define CXXOPTS_NO_EXCEPTIONS
#define CXXOPTS_NO_RTTI

#include <external/cxxopts.hpp>

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace volts
{
    namespace fs = std::filesystem;
    namespace json = rapidjson;
    namespace opts = cxxopts;

    using namespace files;

    struct cmd
    {
        static cmd& get()
        {
            static cmd* singleton = new cmd();
            return *singleton;
        }

        void parse(int argc, char** argv)
        {
            auto opts = build_options(argc, argv);

            if(opts.count("output"))
            {
                auto path = opts["output"].as<std::string>();
                auto log = spdlog::basic_logger_mt("volts", path);
                spdlog::set_default_logger(log);
                output_path = path;
            }

            if(opts.count("sfo"))
            {
                if(auto path = opts["sfo"].as<std::string>(); fs::exists(path))
                {
                    std::ifstream stream(path, std::ios::binary | std::ios::in);
                    auto obj = files::sfo::load(stream);

                    json::StringBuffer s;
                    json::Writer w(s);

                    w.StartObject();
                
                    if(!obj)
                    {
                        w.Key("success");
                        w.Bool(false);
                    }
                    else
                    {
                        w.Key("success");
                        w.Bool(true);

                        for(auto& [key, val] : *obj)
                        {
                            w.Key(key.c_str());
                            switch(val.type)
                            {
                            case sfo::format::array:
                                w.StartArray();
                                
                                for(auto byte : val.data)
                                    w.Int(byte);

                                w.EndArray();
                                break;
                            case sfo::format::string:
                                w.String((char*)val.data.data());
                                break;
                            case sfo::format::integer:
                                w.Int(*(int*)val.data.data());
                                break;
                            default:
                                w.StartObject();
                                
                                w.Key("error");
                                w.String("unknown_type");

                                w.EndObject();
                                break;
                            }
                        }
                    }
                
                    w.EndObject();

                    auto out = output_file("sfo.json");
                    svl::streams::write_utf8(out, s.GetString());
                    spdlog::info("wrote parsed sfo file to {}", out);
                }
            }
        }

    private:

        opts::ParseResult build_options(int argc, char** argv)
        {
            opts::Options options("volts", "c++ ps3 emulator");

            options.add_options()
                ("output", "set logging output file", cxxopts::value<std::string>())
                ("sfo", "parse .sfo file", cxxopts::value<std::string>())
            ;

            return options.parse(argc, argv);
        }

        std::string output_file(const std::string& name)
        {
            if(output_path)
            {
                auto ret = *output_path;
                ret.replace_extension("." + name);
                return ret.string();
            }
            
            return name;
        }

        std::optional<fs::path> output_path = std::nullopt;
    };
}