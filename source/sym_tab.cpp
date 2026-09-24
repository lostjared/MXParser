#include "MXParser/sym_tab.hpp"
#include<algorithm>
#include<print>
#include<optional>
namespace mx {

    void SymbolTable::enter(const std::string &vname, const Symbol &symbol) {
            auto keyval = symbol;
            if(is_keyword(vname)) {
                keyval.sym_type = SYMBOL_TYPE::SYMBOL_KEYWORD;
                return;
            } else {
                if(exists(vname)) {
                    return;
                }
                keyval.sym_type = SYMBOL_TYPE::SYMBOL_IDENTIFER;
            }
            keyval.sym_variable_name = vname;
            if(scope.empty()) {
                push_scope();
            }
            keyval.depth = static_cast<size_t>(scope.size());
            std::println("symbol: {}:{} -> {}",keyval.depth,  vname, sym_to_string(keyval.sym_type));
            scope.back().push_back(keyval);
    }

    bool SymbolTable::exists(const std::string &vname) {
        std::optional<Symbol> sym = lookup(vname);
        if (!sym)
            return false;
        return true;
    }
    std::optional<Symbol> SymbolTable::lookup(const std::string &vname) {
        Symbol sym;
        for(auto scope_i = scope.rbegin(); scope_i != scope.rend(); ++scope_i) {
            for(auto key = scope_i->rbegin(); key != scope_i->rend(); ++key) {
                if(key->sym_variable_name == vname)
                    return *key;
            }
        }
        return std::nullopt;
    }

    void  SymbolTable::print() {
    }

    void SymbolTable::push_scope() {
        scope.emplace_back();
        indent += "\t";
    }

    void SymbolTable::pop_scope() {
        if(!scope.empty())
            scope.pop_back();

        if(!indent.empty())
            indent.pop_back();
    }

    std::string SymbolTable::sym_to_string(SYMBOL_TYPE symbol) {
        switch(symbol) {
            case SYMBOL_TYPE::SYMBOL_NOT_FOUND:
                break;
            case SYMBOL_TYPE::SYMBOL_IDENTIFER:
                return "id";
            case SYMBOL_TYPE::SYMBOL_FLOAT_VARIABLE:
                return "floating point";
            case SYMBOL_TYPE::SYMBOL_INTEGER_VARIABLE:
                return "integer";
            case SYMBOL_TYPE::SYMBOL_FUNCTION:
                return "functoin";
            case SYMBOL_TYPE::SYMBOL_KEYWORD:
                return "keyword";
        }
        return "unknown";
    }

    bool SymbolTable::is_keyword(const std::string &key) {
        static const char *keywords[] = {"const",
            "uniform",
            "buffer",
            "shared",
            "attribute",
            "varying",
            "coherent",
            "volatile",
            "restrict",
            "readonly",
            "writeonly",
            "atomic_uint",
            "layout",
            "centroid",
            "flat",
            "smooth",
            "noperspective",
            "patch",
            "sample",
            "invariant",
            "precise",
            "break",
            "continue",
            "do",
            "for",
            "while",
            "switch",
            "case",
            "default",
            "if",
            "else",
            "subroutine",
            "in",
            "out",
            "inout",
            "int",
            "void",
            "bool",
            "true",
            "false",
            "float",
            "double",
            "discard",
            "return",
            "vec2",
            "vec3",
            "vec4",
            "ivec2",
            "ivec3",
            "ivec4",
            "bvec2",
            "bvec3",
            "bvec4",
            "uint",
            "uvec2",
            "uvec3",
            "uvec4",
            "dvec2",
            "dvec3",
            "dvec4",
            "mat2",
            "mat3",
            "mat4",
            "mat2x2",
            "mat2x3",
            "mat2x4",
            "mat3x2",
            "mat3x3",
            "mat3x4",
            "mat4x2",
            "mat4x3",
            "mat4x4",
            "dmat2",
            "dmat3",
            "dmat4",
            "dmat2x2",
            "dmat2x3",
            "dmat2x4",
            "dmat3x2",
            "dmat3x3",
            "dmat3x4",
            "dmat4x2",
            "dmat4x3",
            "dmat4x4",
            "lowp",
            "mediump",
            "highp",
            "precision",
            "sampler1D",
            "sampler1DShadow",
            "sampler1DArray",
            "sampler1DArrayShadow",
            "isampler1D",
            "isampler1DArray",
            "usampler1D",
            "usampler1DArray",
            "sampler2D",
            "sampler2DShadow",
            "sampler2DArray",
            "sampler2DArrayShadow",
            "isampler2D",
            "isampler2DArray",
            "usampler2D",
            "usampler2DArray",
            "sampler2DRect",
            "sampler2DRectShadow",
            "isampler2DRect",
            "usampler2DRect",
            "sampler2DMS",
            "isampler2DMS",
            "usampler2DMS",
            "sampler2DMSArray",
            "isampler2DMSArray",
            "usampler2DMSArray",
            "sampler3D",
            "isampler3D",
            "usampler3D",
            "samplerCube",
            "samplerCubeShadow",
            "isamplerCube",
            "usamplerCube",
            "samplerCubeArray",
            "samplerCubeArrayShadow",
            "isamplerCubeArray",
            "usamplerCubeArray",
            "samplerBuffer",
            "isamplerBuffer",
            "usamplerBuffer",
            "image1D",
            "iimage1D",
            "uimage1D",
            "image1DArray",
            "iimage1DArray",
            "uimage1DArray",
            "image2D",
            "iimage2D",
            "uimage2D",
            "image2DArray",
            "iimage2DArray",
            "uimage2DArray",
            "image2DRect",
            "iimage2DRect",
            "uimage2DRect",
            "image2DMS",
            "iimage2DMS",
            "uimage2DMS",
            "image2DMSArray",
            "iimage2DMSArray",
            "uimage2DMSArray",
            "image3D",
            "iimage3D",
            "uimage3D",
            "imageCube",
            "iimageCube",
            "uimageCube",
            "imageCubeArray",
            "iimageCubeArray",
            "uimageCubeArray",
            "imageBuffer",
            "iimageBuffer",
            "uimageBuffer",
            "struct",
            nullptr};
            for (size_t i = 0; keywords[i] != nullptr; ++i) {
                if (std::string(keywords[i]) == key) {
                    return true;
                }
            }
            return false;
    }
} // namespace mx
