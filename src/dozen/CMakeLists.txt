# Etapa 2 del port de Yakumo a la Xbox: app UWP mínima que dibuja con el Vulkan de Dozen para UWP
# (SternXD/mesa-uwp 26.2.2). Sin SDL: CoreApplication + CoreWindow, como el host de los DKC.
#
# Configurar y compilar con el entorno «store» de MSVC (el CRT _app que la consola trae de serie),
# en una ruta corta por MAX_PATH; lo hace construir.ps1:
#   vcvarsall.bat x64 store
#   cmake -S <este dir> -B <carpeta de compilacion> -G Ninja -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0
#         -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
#   cmake --build <carpeta de compilacion>
cmake_minimum_required(VERSION 3.20)
project(dozen_prueba CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dozen_prueba WIN32 prueba_dozen.cpp)
target_include_directories(dozen_prueba PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
target_compile_definitions(dozen_prueba PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
target_compile_options(dozen_prueba PRIVATE /EHsc /utf-8 /W3)
# vulkan_dzn.dll se carga en tiempo de ejecución con LoadPackagedLibrary: no se enlaza.
target_link_libraries(dozen_prueba PRIVATE WindowsApp OneCore)
# El generador Ninja de WindowsStore no pone el bit APPCONTAINER: sin él la consola no activa la app.
target_link_options(dozen_prueba PRIVATE /APPCONTAINER)
# 64 MiB de pila reservada, como Yakumo (/STACK:67108864): la 0.0.7.0 mide qué hilo la recibe.
target_link_options(dozen_prueba PRIVATE /STACK:67108864)
set_target_properties(dozen_prueba PROPERTIES OUTPUT_NAME DozenPrueba)
