"""Aplica al árbol de Yakumo --el que compiló tools/compilar-pc.ps1-- los cambios del port UWP.

    py -3.13 parchar_uwp.py [árbol]

Todo el cambio va bajo MHP3RD_UWP: la compilación de escritorio sale igual. Cada parche comprueba el
texto original (del tag v0.6.0-alpha.4) antes de tocarlo y se salta si ya está aplicado, así que se
puede correr las veces que haga falta. Los archivos nuevos se copian desde fuentes\\. Lo que cambia y
por qué: técnico §4.6 y §13.3 del subproyecto.
"""
import os
import pathlib
import shutil
import sys

AQUI = pathlib.Path(__file__).resolve().parent
ARBOL = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else r"<MHP3RD_ARBOL>")
HOST = ARBOL / "profiles" / "mhp3rd" / "host"

NUEVOS = [(AQUI / "fuentes" / "uwp_app.cpp", HOST / "platform" / "uwp_app.cpp")]

PARCHES = [
    # main.cpp: en UWP el punto de entrada es WinMain (uwp_app.cpp), que llama a yakumo_main.
    (HOST / "main.cpp",
     "#if defined(MHP3RD_ANDROID_APP)\n"
     "// android_app.cpp owns the entry point SDL calls and runs this on a thread\n"
     "// with the stack the game needs; restarting the process to get one is not an\n"
     "// option inside an app.\n"
     "int yakumo_main(int argc, char **argv) {",
     "#if defined(MHP3RD_ANDROID_APP) || defined(MHP3RD_UWP)\n"
     "// android_app.cpp owns the entry point SDL calls and runs this on a thread\n"
     "// with the stack the game needs; restarting the process to get one is not an\n"
     "// option inside an app. On UWP, uwp_app.cpp calls it on the view's thread.\n"
     "int yakumo_main(int argc, char **argv) {"),
    # El renderer: declaraciones de uwp_app.cpp.
    (HOST / "gpu" / "vulkan_renderer.cpp",
     "#include <vulkan/vulkan.h>\n",
     "#include <vulkan/vulkan.h>\n"
     "#if defined(MHP3RD_UWP)\n"
     "// uwp_app.cpp: SDL3-uwp has no Vulkan support, so the port asks for the surface itself.\n"
     "const char *const *mhp3rd_uwp_instance_extensions(uint32_t *count);\n"
     "bool mhp3rd_uwp_create_surface(VkInstance instance, VkSurfaceKHR *surface);\n"
     "#endif\n"),
    # La ventana: sin SDL_WINDOW_VULKAN (SDL3-uwp la rechazaría) y siempre a pantalla completa.
    (HOST / "gpu" / "vulkan_renderer.cpp",
     "    SDL_WindowFlags window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;\n",
     "#if defined(MHP3RD_UWP)\n"
     "    // On the console the window is the view's CoreWindow, always full screen.\n"
     "    SDL_WindowFlags window_flags = SDL_WINDOW_FULLSCREEN;\n"
     "#else\n"
     "    SDL_WindowFlags window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;\n"
     "#endif\n"),
    (HOST / "gpu" / "vulkan_renderer.cpp",
     "    const char *const *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);\n",
     "#if defined(MHP3RD_UWP)\n"
     "    const char *const *sdl_extensions = mhp3rd_uwp_instance_extensions(&extension_count);\n"
     "#else\n"
     "    const char *const *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);\n"
     "#endif\n"),
    (HOST / "gpu" / "vulkan_renderer.cpp",
     "    if (!SDL_Vulkan_CreateSurface(impl.window, impl.instance, nullptr, &impl.surface)) {\n",
     "#if defined(MHP3RD_UWP)\n"
     "    if (!mhp3rd_uwp_create_surface(impl.instance, &impl.surface)) {\n"
     "#else\n"
     "    if (!SDL_Vulkan_CreateSurface(impl.window, impl.instance, nullptr, &impl.surface)) {\n"
     "#endif\n"),
    (HOST / "gpu" / "vulkan_renderer.cpp",
     "    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {\n",
     "#if defined(MHP3RD_UWP)\n"
     "    if (!mhp3rd_uwp_create_surface(instance, &surface)) {\n"
     "#else\n"
     "    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {\n"
     "#endif\n"),
    # Un render target que no se puede crear se calla, y cada draw lo reintenta: en la consola se dice
    # una vez por qué (diagnóstico del negro del 2026-09-28).
    (HOST / "gpu" / "vulkan_renderer.cpp",
     "    Target target{};\n"
     "    if (!create_target(target, error)) {\n"
     "        destroy_target(target);\n"
     "        return nullptr;\n"
     "    }\n",
     "    Target target{};\n"
     "    if (!create_target(target, error)) {\n"
     "        destroy_target(target);\n"
     "#if defined(MHP3RD_UWP)\n"
     "        static bool reported = false;\n"
     "        if (!reported) std::cout << \"[render] cannot make a render target: \" << error << std::endl;\n"
     "        reported = true;\n"
     "#endif\n"
     "        return nullptr;\n"
     "    }\n"),
    # Y por qué no se puede crear: la imagen, lo que admite y los tipos de memoria de la consola.
    (HOST / "gpu" / "vulkan_renderer.cpp",
     "        allocate.memoryTypeIndex = find_memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);\n"
     "        if (!check(vkAllocateMemory(device, &allocate, nullptr, &memory), \"vkAllocateMemory\", error)) return false;\n"
     "        vkBindImageMemory(device, image, memory, 0u);\n",
     "        allocate.memoryTypeIndex = find_memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);\n"
     "#if defined(MHP3RD_UWP)\n"
     "        if (!check(vkAllocateMemory(device, &allocate, nullptr, &memory), \"vkAllocateMemory\", error)) {\n"
     "            static int reported = 0;\n"
     "            if (reported++ < 4) {\n"
     "                VkPhysicalDeviceMemoryProperties types{};\n"
     "                vkGetPhysicalDeviceMemoryProperties(physical_device, &types);\n"
     "                std::cout << \"[render] image allocation failed: format \" << static_cast<int>(format) << \", usage 0x\"\n"
     "                          << std::hex << usage << std::dec << \", \" << width << \"x\" << height << \", \"\n"
     "                          << requirements.size << \" B, allowed types 0x\" << std::hex << requirements.memoryTypeBits\n"
     "                          << std::dec << \", chose type \" << allocate.memoryTypeIndex << std::endl;\n"
     "                for (std::uint32_t i = 0; i < types.memoryTypeCount; ++i)\n"
     "                    std::cout << \"[render]   memory type \" << i << \": flags 0x\" << std::hex\n"
     "                              << types.memoryTypes[i].propertyFlags << std::dec << \", heap \"\n"
     "                              << types.memoryTypes[i].heapIndex << std::endl;\n"
     "                for (std::uint32_t i = 0; i < types.memoryHeapCount; ++i)\n"
     "                    std::cout << \"[render]   memory heap \" << i << \": \" << (types.memoryHeaps[i].size >> 20u)\n"
     "                              << \" MiB, flags 0x\" << std::hex << types.memoryHeaps[i].flags << std::dec << std::endl;\n"
     "            }\n"
     "            return false;\n"
     "        }\n"
     "#else\n"
     "        if (!check(vkAllocateMemory(device, &allocate, nullptr, &memory), \"vkAllocateMemory\", error)) return false;\n"
     "#endif\n"
     "        vkBindImageMemory(device, image, memory, 0u);\n"),
    # La profundidad: el Direct3D 12 de la consola declara D32_SFLOAT apto y luego le da 0 bytes de
    # memoria a su imagen; en UWP se toma el primer formato cuya imagen tiene de verdad un tamaño.
    (HOST / "gpu" / "vulkan_renderer.cpp",
     "    void choose_depth_format() {\n",
     "#if defined(MHP3RD_UWP)\n"
     "    [[nodiscard]] bool depth_image_has_memory(VkFormat format) const {\n"
     "        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};\n"
     "        info.imageType = VK_IMAGE_TYPE_2D;\n"
     "        info.format = format;\n"
     "        info.extent = {std::max(target_extent.width, 1u), std::max(target_extent.height, 1u), 1u};\n"
     "        info.mipLevels = 1u;\n"
     "        info.arrayLayers = 1u;\n"
     "        info.samples = VK_SAMPLE_COUNT_1_BIT;\n"
     "        info.tiling = VK_IMAGE_TILING_OPTIMAL;\n"
     "        info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;\n"
     "        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;\n"
     "        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;\n"
     "        VkImage image = VK_NULL_HANDLE;\n"
     "        if (vkCreateImage(device, &info, nullptr, &image) != VK_SUCCESS) return false;\n"
     "        VkMemoryRequirements requirements{};\n"
     "        vkGetImageMemoryRequirements(device, image, &requirements);\n"
     "        vkDestroyImage(device, image, nullptr);\n"
     "        return requirements.size > 0u;\n"
     "    }\n"
     "#endif\n"
     "    void choose_depth_format() {\n"),
    (HOST / "gpu" / "vulkan_renderer.cpp",
     "            if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u) {\n"
     "                depth_format = format;\n",
     "            if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u) {\n"
     "#if defined(MHP3RD_UWP)\n"
     "                if (!depth_image_has_memory(format)) {\n"
     "                    std::cout << \"[render] depth format \" << static_cast<int>(format)\n"
     "                              << \" gets no memory on this device; trying the next one\" << std::endl;\n"
     "                    continue;\n"
     "                }\n"
     "#endif\n"
     "                depth_format = format;\n"),
    # Las overlays: un app carga sus bibliotecas por su ruta dentro del paquete.
    (HOST / "overlays.cpp",
     "void *load_library(const std::filesystem::path &path) {\n"
     "#if defined(_WIN32)\n",
     "void *load_library(const std::filesystem::path &path) {\n"
     "#if defined(MHP3RD_UWP)\n"
     "    // An app loads its own libraries by their path inside the package, which has to be relative.\n"
     "    // lexically_relative only looks at the text: relative() canonicalises through\n"
     "    // GetFinalPathNameByHandle, which the console's sandbox refuses.\n"
     "    const std::filesystem::path relative = path.lexically_relative(executable_directory());\n"
     "    return reinterpret_cast<void *>(LoadPackagedLibrary((relative.empty() ? path : relative).c_str(), 0));\n"
     "#elif defined(_WIN32)\n"),
    # La fuente: CreateFile2 y el mapeo «FromApp», los de las apps.
    (HOST / "fonts" / "game_font.cpp",
     "        close();\n"
     "#if defined(_WIN32)\n"
     "        file_ = CreateFileW(",
     "        close();\n"
     "#if defined(MHP3RD_UWP)\n"
     "        file_ = CreateFile2(path.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);\n"
     "        if (file_ == INVALID_HANDLE_VALUE) return false;\n"
     "        LARGE_INTEGER size{};\n"
     "        if (!GetFileSizeEx(file_, &size) || size.QuadPart <= 0) return close(), false;\n"
     "        mapping_ = CreateFileMappingFromApp(file_, nullptr, PAGE_READONLY, 0, nullptr);\n"
     "        if (mapping_ == nullptr) return close(), false;\n"
     "        data_ = static_cast<const std::uint8_t *>(MapViewOfFileFromApp(mapping_, FILE_MAP_READ, 0, 0));\n"
     "        size_ = static_cast<std::size_t>(size.QuadPart);\n"
     "#elif defined(_WIN32)\n"
     "        file_ = CreateFileW("),
    # CMake: la opción, el ejecutable WIN32 con uwp_app.cpp y el bit APPCONTAINER.
    (ARBOL / "profiles" / "mhp3rd" / "CMakeLists.txt",
     "option(MHP3RD_ANDROID_APP \"Build the Android app's libmain.so instead of an executable\" OFF)\n",
     "option(MHP3RD_ANDROID_APP \"Build the Android app's libmain.so instead of an executable\" OFF)\n"
     "# The UWP app for the Xbox in Dev Mode: WinMain through SDL3-uwp, the game on the view's thread,\n"
     "# and Vulkan from the Dozen driver the package carries (host/platform/uwp_app.cpp).\n"
     "option(MHP3RD_UWP \"Build the UWP app for the Xbox in Dev Mode\" OFF)\n"),
    (ARBOL / "profiles" / "mhp3rd" / "CMakeLists.txt",
     "else()\n"
     "    add_executable(Yakumo ${MHP3RD_PROGRAM_SOURCES})\n"
     "endif()\n",
     "elseif(MHP3RD_UWP)\n"
     "    add_executable(Yakumo WIN32 ${MHP3RD_PROGRAM_SOURCES} host/platform/uwp_app.cpp)\n"
     "    # ImGui's Win32 helpers (clipboard, IME, shell) are desktop-only; SDL does those.\n"
     "    target_compile_definitions(Yakumo PRIVATE MHP3RD_UWP=1 IMGUI_DISABLE_WIN32_FUNCTIONS)\n"
     "    target_link_options(Yakumo PRIVATE /APPCONTAINER)\n"
     "else()\n"
     "    add_executable(Yakumo ${MHP3RD_PROGRAM_SOURCES})\n"
     "endif()\n"),
    (ARBOL / "profiles" / "mhp3rd" / "CMakeLists.txt",
     "    if(MHP3RD_RELEASE_LINK_OPTIONS)\n"
     "        target_link_options(${target} PRIVATE ${MHP3RD_RELEASE_LINK_OPTIONS})\n"
     "    endif()\n",
     "    if(MHP3RD_RELEASE_LINK_OPTIONS)\n"
     "        target_link_options(${target} PRIVATE ${MHP3RD_RELEASE_LINK_OPTIONS})\n"
     "    endif()\n"
     "    if(MHP3RD_UWP)\n"
     "        target_link_options(${target} PRIVATE /APPCONTAINER)\n"
     "    endif()\n"),
    # FFmpeg (la música y las películas): en UWP, el que compila ffmpeg_uwp.ps1 para el app container.
    # FFmpeg.cmake se incluye antes de declarar MHP3RD_UWP: la rama mira CMAKE_SYSTEM_NAME.
    (ARBOL / "profiles" / "mhp3rd" / "cmake" / "FFmpeg.cmake",
     "elseif(WIN32)\n"
     "    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR CMAKE_SYSTEM_PROCESSOR MATCHES \"ARM|arm|aarch\")\n",
     "elseif(CMAKE_SYSTEM_NAME STREQUAL \"WindowsStore\")\n"
     "    # The UWP app for the Xbox: the same decoders as the bundled build, compiled with MSVC's\n"
     "    # \"store\" environment and linked with /APPCONTAINER, from MHP3RD_FFMPEG_UWP_PREFIX (bin, lib,\n"
     "    # include). The BtbN build is a desktop one, which the console will not load.\n"
     "    set(MHP3RD_FFMPEG_UWP_PREFIX \"\" CACHE PATH \"FFmpeg built for UWP: the prefix with bin, lib and include\")\n"
     "    if(NOT EXISTS \"${MHP3RD_FFMPEG_UWP_PREFIX}/include/libavcodec/avcodec.h\")\n"
     "        message(FATAL_ERROR \"mhp3rd: the UWP app needs an FFmpeg built for UWP in MHP3RD_FFMPEG_UWP_PREFIX, \"\n"
     "            \"or -DMHP3RD_FFMPEG=OFF (no music, no movies)\")\n"
     "    endif()\n"
     "    add_library(mhp3rd_ffmpeg INTERFACE)\n"
     "    foreach(pair IN ITEMS \"avcodec;61\" \"avutil;59\")\n"
     "        list(GET pair 0 lib)\n"
     "        list(GET pair 1 soversion)\n"
     "        configure_file(\"${MHP3RD_FFMPEG_UWP_PREFIX}/bin/${lib}-${soversion}.dll\"\n"
     "            \"${_mhp3rd_ffmpeg_bin}/${lib}-${soversion}.dll\" COPYONLY)\n"
     "        # FFmpeg's own install puts the import libraries next to the DLLs, in bin.\n"
     "        set(_mhp3rd_ffmpeg_implib \"${MHP3RD_FFMPEG_UWP_PREFIX}/lib/${lib}.lib\")\n"
     "        if(NOT EXISTS \"${_mhp3rd_ffmpeg_implib}\")\n"
     "            set(_mhp3rd_ffmpeg_implib \"${MHP3RD_FFMPEG_UWP_PREFIX}/bin/${lib}.lib\")\n"
     "        endif()\n"
     "        add_library(mhp3rd_ffmpeg_${lib} SHARED IMPORTED)\n"
     "        set_target_properties(mhp3rd_ffmpeg_${lib} PROPERTIES\n"
     "            IMPORTED_LOCATION \"${_mhp3rd_ffmpeg_bin}/${lib}-${soversion}.dll\"\n"
     "            IMPORTED_IMPLIB \"${_mhp3rd_ffmpeg_implib}\")\n"
     "        target_link_libraries(mhp3rd_ffmpeg INTERFACE mhp3rd_ffmpeg_${lib})\n"
     "    endforeach()\n"
     "    target_include_directories(mhp3rd_ffmpeg INTERFACE \"${MHP3RD_FFMPEG_UWP_PREFIX}/include\")\n"
     "    configure_file(\"${MHP3RD_FFMPEG_UWP_PREFIX}/COPYING.LGPLv2.1\" \"${_mhp3rd_ffmpeg_bin}/FFmpeg-LICENSE.txt\" COPYONLY)\n"
     "    _mhp3rd_ffmpeg_write_notice(\"${_mhp3rd_ffmpeg_bin}\"\n"
     "        \"The FFmpeg libraries here (avcodec, avutil) are FFmpeg ${MHP3RD_FFMPEG_VERSION} built for UWP from\\n\"\n"
     "        \"the unmodified release source, licensed under the GNU LGPL version 2.1 or later\\n\"\n"
     "        \"(FFmpeg-LICENSE.txt). They are dynamically linked.\\n\\n\"\n"
     "        \"Source: ${MHP3RD_FFMPEG_URL}\\n\")\n"
     "    set(MHP3RD_HAS_FFMPEG ON)\n"
     "    message(STATUS \"mhp3rd: FFmpeg ${MHP3RD_FFMPEG_VERSION} built for UWP; music and movies enabled\")\n"
     "\n"
     "elseif(WIN32)\n"
     "    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR CMAKE_SYSTEM_PROCESSOR MATCHES \"ARM|arm|aarch\")\n"),
]


def escribe(ruta, texto):
    tmp = ruta.with_name(ruta.name + ".tmp")
    tmp.write_bytes(texto.encode("utf-8"))
    os.replace(tmp, ruta)


def main():
    if not (ARBOL / "CMakeLists.txt").is_file():
        sys.exit(f"no es el árbol de Yakumo: {ARBOL}")
    for origen, destino in NUEVOS:
        if destino.exists() and destino.read_bytes() == origen.read_bytes():
            print("ya está:", destino.relative_to(ARBOL))
        else:
            shutil.copyfile(origen, destino)
            print("copiado:", destino.relative_to(ARBOL))
    textos = {}
    for ruta, viejo, nuevo in PARCHES:
        if ruta not in textos:
            datos = ruta.read_bytes()
            if b"\r\n" in datos:
                sys.exit(f"{ruta} tiene CRLF: el parche espera LF")
            textos[ruta] = datos.decode("utf-8")
        t = textos[ruta]
        if nuevo in t:
            print("ya aplicado:", ruta.relative_to(ARBOL), "·", viejo.strip().splitlines()[0][:60])
            continue
        n = t.count(viejo)
        if n != 1:
            sys.exit(f"{ruta.relative_to(ARBOL)}: el texto original aparece {n} veces: {viejo[:80]!r}")
        textos[ruta] = t.replace(viejo, nuevo)
        print("aplicado:", ruta.relative_to(ARBOL), "·", viejo.strip().splitlines()[0][:60])
    for ruta, t in textos.items():
        if ruta.read_bytes().decode("utf-8") != t:
            escribe(ruta, t)
    print("listo")


if __name__ == "__main__":
    main()
