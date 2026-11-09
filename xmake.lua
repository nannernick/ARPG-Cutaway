-- CommonLibSSE-NG (alandtse fork, ng branch) as a git submodule in lib/commonlibsse-ng
includes("lib/commonlibsse-ng")

set_project("ARPGCutaway")
set_version("0.1.0")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

target("ARPGCutaway")
    add_deps("commonlibsse-ng")

    -- Generates the SKSE plugin metadata (Address Library v5 / struct-independent
    -- by default, which is what Skyrim 1.7.x needs).
    add_rules("commonlibsse-ng.plugin", {
        name = "ARPGCutaway",
        author = "nannernick",
        description = "ARPG-style interior cutaway: clips walls and roofs above the player for a high camera."
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
