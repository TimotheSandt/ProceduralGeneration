#include "suites/Suites.h"

#include "Graphics/Shader.h"

#include <filesystem>
#include <fstream>
#include <utility>

namespace fs = std::filesystem;

namespace tests {

TestSuite CreateShaderSuite() {
    TestSuite suite{"Shader"};

    AddTest(suite, "get_file_contents reads a file", [] {
        const fs::path tempFile = fs::temp_directory_path() / "proceduralgeneration_shader_read.glsl";
        std::ofstream(tempFile.string()) << "void main() {}";
        AssertEqual(get_file_contents(tempFile.string().c_str()), std::string("void main() {}"), "Shader file contents should match");
        std::error_code ec;
        fs::remove(tempFile, ec);
    });

    AddTest(suite, "get_file_contents throws on missing file", [] {
        bool thrown = false;
        try {
            (void)get_file_contents("__definitely_missing_shader__.glsl");
        } catch (int) {
            thrown = true;
        }
        Assert(thrown, "Reading a missing shader file should throw");
    });

    AddTest(suite, "default shader starts empty", [] {
        Shader shader;
        AssertEqual(shader.GetID(), static_cast<GLuint>(0), "Default shader id should be zero");
        Assert(!shader.IsCompiled(), "Default shader should not be compiled");
    });

    AddTest(suite, "moving default shader preserves zero id", [] {
        Shader source;
        Shader moved(std::move(source));
        AssertEqual(moved.GetID(), static_cast<GLuint>(0), "Moved default shader should still have zero id");
        AssertEqual(source.GetID(), static_cast<GLuint>(0), "Moved-from default shader should keep zero id");
    });

    return suite;
}

}  // namespace tests
