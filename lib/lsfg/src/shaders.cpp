// Lossless Scaling's frame generation shaders, from the player's Lossless.dll:
// read out of its resources, translated from DXBC to SPIR-V and cached.
//
// The resource numbers per shader and the binding renumbering after
// translation are lsfg-vk's (src/extract/extract.cpp and trans.cpp, v1.0.0, MIT).
#include "lsfg/lsfg.hpp"

#include <dxbc/dxbc_modinfo.h>
#include <dxbc/dxbc_module.h>
#include <dxbc/dxbc_reader.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <system_error>
#include <unordered_map>

namespace lsfg {

    namespace {
        // lsfg-vk's shader name -> Lossless.dll RCDATA resource number.
        const std::pair<const char*, uint32_t> kShaderResources[] = {
            { "mipmaps", 255 },
            { "alpha[0]", 267 }, { "alpha[1]", 268 }, { "alpha[2]", 269 }, { "alpha[3]", 270 },
            { "beta[0]", 275 }, { "beta[1]", 276 }, { "beta[2]", 277 }, { "beta[3]", 278 }, { "beta[4]", 279 },
            { "gamma[0]", 257 }, { "gamma[1]", 259 }, { "gamma[2]", 260 }, { "gamma[3]", 261 }, { "gamma[4]", 262 },
            { "delta[0]", 257 }, { "delta[1]", 263 }, { "delta[2]", 264 }, { "delta[3]", 265 }, { "delta[4]", 266 },
            { "delta[5]", 258 }, { "delta[6]", 271 }, { "delta[7]", 272 }, { "delta[8]", 273 }, { "delta[9]", 274 },
            { "generate", 256 },
            { "p_mipmaps", 255 },
            { "p_alpha[0]", 290 }, { "p_alpha[1]", 291 }, { "p_alpha[2]", 292 }, { "p_alpha[3]", 293 },
            { "p_beta[0]", 298 }, { "p_beta[1]", 299 }, { "p_beta[2]", 300 }, { "p_beta[3]", 301 }, { "p_beta[4]", 302 },
            { "p_gamma[0]", 280 }, { "p_gamma[1]", 282 }, { "p_gamma[2]", 283 }, { "p_gamma[3]", 284 }, { "p_gamma[4]", 285 },
            { "p_delta[0]", 280 }, { "p_delta[1]", 286 }, { "p_delta[2]", 287 }, { "p_delta[3]", 288 }, { "p_delta[4]", 289 },
            { "p_delta[5]", 281 }, { "p_delta[6]", 294 }, { "p_delta[7]", 295 }, { "p_delta[8]", 296 }, { "p_delta[9]", 297 },
            { "p_generate", 256 },
        };

        constexpr char kCacheMagic[8] = { 'L', 'S', 'F', 'G', 'S', 'P', 'V', '1' };

        uint32_t read32(const std::vector<uint8_t>& d, size_t o) {
            uint32_t v = 0;
            if (o + 4 <= d.size()) {
                std::memcpy(&v, d.data() + o, 4);
            }
            return v;
        }

        uint16_t read16(const std::vector<uint8_t>& d, size_t o) {
            uint16_t v = 0;
            if (o + 2 <= d.size()) {
                std::memcpy(&v, d.data() + o, 2);
            }
            return v;
        }

        // The RCDATA resources of a PE file, by number (first language of each).
        bool read_rcdata(const std::vector<uint8_t>& d, std::unordered_map<uint32_t, std::vector<uint8_t>>& out) {
            if (d.size() < 0x40 || d[0] != 'M' || d[1] != 'Z') {
                return false;
            }
            const size_t pe = read32(d, 0x3C);
            if (pe + 24 > d.size() || std::memcmp(d.data() + pe, "PE\0\0", 4) != 0) {
                return false;
            }
            const uint16_t sections = read16(d, pe + 6);
            const uint16_t optional_size = read16(d, pe + 20);
            const size_t optional = pe + 24;
            const bool pe32_plus = read16(d, optional) == 0x20B;
            const size_t directories = optional + (pe32_plus ? 112 : 96);
            const uint32_t resource_rva = read32(d, directories + 2 * 8);
            struct Section { uint32_t va, size, raw; };
            std::vector<Section> table;
            for (size_t i = 0, s = optional + optional_size; i < sections; i++, s += 40) {
                table.push_back({ read32(d, s + 12), std::max(read32(d, s + 8), read32(d, s + 16)), read32(d, s + 20) });
            }
            auto offset = [&](uint32_t rva) -> size_t {
                for (const Section& s : table) {
                    if (rva >= s.va && rva < s.va + s.size) {
                        return size_t(rva - s.va) + s.raw;
                    }
                }
                return 0;
            };
            const size_t base = offset(resource_rva);
            if (base == 0) {
                return false;
            }
            auto entries = [&](size_t dir, auto&& callback) {
                const uint32_t count = uint32_t(read16(d, dir + 12)) + read16(d, dir + 14);
                for (uint32_t i = 0; i < count && dir + 16 + 8 * i + 8 <= d.size(); i++) {
                    callback(read32(d, dir + 16 + 8 * i), read32(d, dir + 20 + 8 * i));
                }
            };
            entries(base, [&](uint32_t type, uint32_t type_dir) {
                if (type != 10 || !(type_dir & 0x80000000u)) { // RT_RCDATA
                    return;
                }
                entries(base + (type_dir & 0x7FFFFFFFu), [&](uint32_t id, uint32_t id_dir) {
                    if ((id & 0x80000000u) || !(id_dir & 0x80000000u) || out.count(id)) {
                        return;
                    }
                    entries(base + (id_dir & 0x7FFFFFFFu), [&](uint32_t, uint32_t leaf) {
                        if (out.count(id) || (leaf & 0x80000000u)) {
                            return;
                        }
                        const size_t data = offset(read32(d, base + leaf));
                        const uint32_t size = read32(d, base + leaf + 4);
                        if (data != 0 && data + size <= d.size()) {
                            out[id].assign(d.begin() + data, d.begin() + data + size);
                        }
                    });
                });
            });
            return true;
        }

        // DXBC to SPIR-V, bindings renumbered 0, 1, 2... in declaration order (lsfg-vk).
        std::vector<uint8_t> translate(const std::vector<uint8_t>& dxbc) {
            dxvk::DxbcReader reader(reinterpret_cast<const char*>(dxbc.data()), dxbc.size());
            dxvk::DxbcModule module(reader);
            const dxvk::DxbcModuleInfo info{};
            dxvk::SpirvCodeBuffer code = module.compile(info, "CS");

            std::vector<uint32_t> binding_offsets; // by variable id; 0 = none
            std::vector<uint32_t> order;
            for (auto ins : code) {
                if (ins.opCode() == spv::OpDecorate && ins.arg(2) == spv::DecorationBinding) {
                    const uint32_t var = ins.arg(1);
                    if (binding_offsets.size() <= var) {
                        binding_offsets.resize(var + 1, 0);
                    }
                    binding_offsets[var] = ins.offset() + 3;
                    order.push_back(var);
                }
                if (ins.opCode() == spv::OpFunction) {
                    break;
                }
            }
            uint32_t next = 0;
            for (uint32_t var : order) {
                if (binding_offsets[var] != 0) {
                    code.data()[binding_offsets[var]] = next++;
                }
            }
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(code.data());
            return std::vector<uint8_t>(bytes, bytes + code.size());
        }

        uint64_t hash(const std::vector<uint8_t>& data) {
            uint64_t h = 0xcbf29ce484222325ull;
            for (uint8_t b : data) {
                h = (h ^ b) * 0x100000001b3ull;
            }
            return h;
        }

        bool read_cache(const std::filesystem::path& path, uint64_t dll_hash, ShaderSet& shaders) {
            std::ifstream file(path, std::ios::binary);
            if (!file.good()) {
                return false;
            }
            std::vector<uint8_t> d((std::istreambuf_iterator<char>(file)), {});
            size_t o = 0;
            auto take = [&](void* out, size_t n) {
                if (o + n > d.size()) {
                    return false;
                }
                std::memcpy(out, d.data() + o, n);
                o += n;
                return true;
            };
            char magic[8];
            uint64_t stored_hash = 0;
            uint32_t count = 0;
            if (!take(magic, 8) || std::memcmp(magic, kCacheMagic, 8) != 0 || !take(&stored_hash, 8) ||
                stored_hash != dll_hash || !take(&count, 4)) {
                return false;
            }
            ShaderSet loaded;
            for (uint32_t i = 0; i < count; i++) {
                uint32_t name_size = 0, code_size = 0;
                if (!take(&name_size, 4) || o + name_size > d.size()) {
                    return false;
                }
                std::string name(reinterpret_cast<const char*>(d.data() + o), name_size);
                o += name_size;
                if (!take(&code_size, 4) || o + code_size > d.size()) {
                    return false;
                }
                loaded[name].assign(d.begin() + o, d.begin() + o + code_size);
                o += code_size;
            }
            if (loaded.size() != std::size(kShaderResources)) {
                return false;
            }
            shaders = std::move(loaded);
            return true;
        }

        void write_cache(const std::filesystem::path& path, uint64_t dll_hash, const ShaderSet& shaders) {
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            auto put = [&](const void* p, size_t n) { file.write(reinterpret_cast<const char*>(p), std::streamsize(n)); };
            put(kCacheMagic, 8);
            put(&dll_hash, 8);
            const uint32_t count = uint32_t(shaders.size());
            put(&count, 4);
            for (const auto& [name, code] : shaders) {
                const uint32_t name_size = uint32_t(name.size()), code_size = uint32_t(code.size());
                put(&name_size, 4);
                put(name.data(), name_size);
                put(&code_size, 4);
                put(code.data(), code_size);
            }
        }
    }

    bool translate_shaders(const std::vector<uint8_t>& dll, ShaderSet& shaders, std::string& error) {
        std::unordered_map<uint32_t, std::vector<uint8_t>> resources;
        if (!read_rcdata(dll, resources)) {
            error = "This isn't a Windows DLL.";
            return false;
        }
        ShaderSet out;
        std::unordered_map<uint32_t, std::vector<uint8_t>> translated;
        for (const auto& [name, id] : kShaderResources) {
            auto found = translated.find(id);
            if (found == translated.end()) {
                auto it = resources.find(id);
                if (it == resources.end() || it->second.size() < 4 || std::memcmp(it->second.data(), "DXBC", 4) != 0) {
                    error = "This DLL doesn't have Lossless Scaling's frame generation shaders. Choose Lossless.dll from Lossless Scaling's folder.";
                    return false;
                }
                try {
                    found = translated.emplace(id, translate(it->second)).first;
                }
                catch (...) {
                    error = "One of the DLL's shaders couldn't be translated (shader " + std::to_string(id) + ").";
                    return false;
                }
            }
            out[name] = found->second;
        }
        shaders = std::move(out);
        return true;
    }

    bool load_shaders(const std::filesystem::path& dll_path, const std::filesystem::path& cache_path,
            ShaderSet& shaders, std::string& error) {
        std::ifstream file(dll_path, std::ios::binary);
        if (!file.good()) {
            error = "Lossless.dll couldn't be opened.";
            return false;
        }
        const std::vector<uint8_t> dll((std::istreambuf_iterator<char>(file)), {});
        const uint64_t dll_hash = hash(dll);
        if (!cache_path.empty() && read_cache(cache_path, dll_hash, shaders)) {
            return true;
        }
        if (!translate_shaders(dll, shaders, error)) {
            return false;
        }
        if (!cache_path.empty()) {
            write_cache(cache_path, dll_hash, shaders);
        }
        return true;
    }

}
