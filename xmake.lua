-- CommonLibSSE-NG (alandtse fork, ng branch) as a git submodule in lib/commonlibsse-ng
includes("lib/commonlibsse-ng")

set_project("DynamicOcclusion")
set_version("0.1.0")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

target("DynamicOcclusion")
    add_deps("commonlibsse-ng")

    -- Generates the SKSE plugin metadata (Address Library v5 / struct-independent
    -- by default, which is what Skyrim 1.7.x needs).
    add_rules("commonlibsse-ng.plugin", {
        name = "DynamicOcclusion",
        author = "nannernick",
        description = "Hides geometry above the player between an ARPG-style camera and the character."
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
