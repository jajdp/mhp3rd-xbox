// prueba_dozen.cpp — Etapa 2 del port de Yakumo a la Xbox: ¿corre en la consola el Vulkan de Dozen
// para UWP (SternXD/mesa-uwp 26.2.2)?
//
// App mínima: CoreApplication + CoreWindow, sin SDL. Carga vulkan_dzn.dll del paquete (en UWP no hay
// vulkan-1.dll, así que se habla con el controlador directamente por su interfaz de ICD), crea la
// superficie con vkCreateWin32SurfaceKHR (el WSI de este Dozen presenta en la CoreWindow del hilo y
// le pide el tamaño a la app con uwp_get_width/uwp_get_height, que se exportan aquí) y dibuja un
// triángulo que gira sobre un fondo que cambia de color. Todo queda en LocalState\dozen_prueba.log:
// el controlador, cada paso con su VkResult y los cuadros por segundo.

#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>

#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "triangulo_frag.h"
#include "triangulo_vert.h"

namespace wac = winrt::Windows::ApplicationModel::Core;
namespace wam = winrt::Windows::ApplicationModel;
namespace wf = winrt::Windows::Foundation;
namespace wgd = winrt::Windows::Graphics::Display;
namespace wst = winrt::Windows::Storage;
namespace wsy = winrt::Windows::System;
namespace wuc = winrt::Windows::UI::Core;

// --- Registro en LocalState ---
static FILE *g_log = nullptr;
static LARGE_INTEGER g_t0, g_frec;
static void Log(const char *fmt, ...) {
  if (!g_log) return;
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  fprintf(g_log, "[%8.3f] ", (double)(t.QuadPart - g_t0.QuadPart) / (double)g_frec.QuadPart);
  va_list a;
  va_start(a, fmt);
  vfprintf(g_log, fmt, a);
  va_end(a);
  fputc('\n', g_log);
  fflush(g_log);
}

// --- Sondas: en qué paso va el hilo principal, un vigía que lo escribe cada segundo desde otro hilo
//     (distingue un cuelgue de un fallo) y las excepciones graves con su módulo ---
static const char *volatile g_paso = "arranque";
static volatile unsigned g_cuadro = 0;
static void Paso(const char *p) {
  g_paso = p;
  if (g_cuadro < 3) Log("  cuadro %u: %s", g_cuadro, p);
}
static DWORD WINAPI Vigia(void *) {
  unsigned ultimo = ~0u;
  for (int s = 1; s <= 120; ++s) {
    Sleep(1000);
    if (g_cuadro != ultimo || s % 10 == 0) Log("vigía %3d s: cuadro %u, paso «%s»", s, g_cuadro, g_paso);
    ultimo = g_cuadro;
  }
  return 0;
}
static volatile LONG g_excepciones = 0;
static LONG CALLBACK Excepcion(EXCEPTION_POINTERS *e) {
  const EXCEPTION_RECORD *r = e->ExceptionRecord;
  if (r->ExceptionCode == 0xE06D7363u || r->ExceptionCode < 0x80000000u) return EXCEPTION_CONTINUE_SEARCH;
  if (InterlockedIncrement(&g_excepciones) > 20) return EXCEPTION_CONTINUE_SEARCH;
  HMODULE mod = nullptr;
  wchar_t nombre[MAX_PATH] = L"?";
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     (LPCWSTR)r->ExceptionAddress, &mod);
  if (mod) GetModuleFileNameW(mod, nombre, MAX_PATH);
  const wchar_t *corto = wcsrchr(nombre, L'\\');
  Log("EXCEPCIÓN 0x%08lX en %p (%ls+0x%llX), paso «%s», cuadro %u", r->ExceptionCode, r->ExceptionAddress,
      corto ? corto + 1 : nombre, (unsigned long long)((const char *)r->ExceptionAddress - (const char *)mod), g_paso,
      g_cuadro);
  return EXCEPTION_CONTINUE_SEARCH;
}

// --- Lo que el WSI de este Dozen le pide a la app ---
static int g_ancho = 1920, g_alto = 1080;
static void *g_ventana = nullptr;

// Sonda de pilas (0.0.7.0): el port necesita 64 MiB de pila en el hilo que corre el juego. Se anota
// el hilo de wWinMain y se compara con el de IFrameworkView::Run, que es donde SDL3-uwp llama a la app.
static DWORD g_hilo_principal = 0;
static ULONG_PTR g_pila_principal_baja = 0, g_pila_principal_alta = 0;
static unsigned long long ReservaDePilaDelExe() {
  const auto *base = reinterpret_cast<const unsigned char *>(GetModuleHandleW(nullptr));
  const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
  const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(base + dos->e_lfanew);
  return nt->OptionalHeader.SizeOfStackReserve;
}
extern "C" __declspec(dllexport) int uwp_get_width(void) { return g_ancho; }
extern "C" __declspec(dllexport) int uwp_get_height(void) { return g_alto; }
extern "C" __declspec(dllexport) void *uwp_get_window_reference(void) { return g_ventana; }

// --- Funciones de Vulkan, resueltas a mano (no hay cargador) ---
#define VK_GLOBALES(X) X(vkCreateInstance) X(vkEnumerateInstanceExtensionProperties) X(vkEnumerateInstanceVersion)
#define VK_INSTANCIA(X)                                                                             \
  X(vkDestroyInstance) X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties)               \
  X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkGetPhysicalDeviceMemoryProperties)                \
  X(vkCreateDevice) X(vkGetDeviceProcAddr) X(vkEnumerateDeviceExtensionProperties)                 \
  X(vkCreateWin32SurfaceKHR) X(vkDestroySurfaceKHR) X(vkGetPhysicalDeviceSurfaceSupportKHR)        \
  X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) X(vkGetPhysicalDeviceSurfaceFormatsKHR)             \
  X(vkGetPhysicalDeviceSurfacePresentModesKHR) X(vkGetPhysicalDeviceFormatProperties)
#define VK_DISPOSITIVO(X)                                                                           \
  X(vkGetDeviceQueue) X(vkDeviceWaitIdle) X(vkCreateSwapchainKHR) X(vkGetSwapchainImagesKHR)        \
  X(vkAcquireNextImageKHR) X(vkQueuePresentKHR) X(vkCreateImageView) X(vkCreateRenderPass)          \
  X(vkCreateFramebuffer) X(vkCreateShaderModule) X(vkCreatePipelineLayout)                          \
  X(vkCreateGraphicsPipelines) X(vkCreateCommandPool) X(vkAllocateCommandBuffers)                  \
  X(vkBeginCommandBuffer) X(vkEndCommandBuffer) X(vkResetCommandBuffer) X(vkCmdBeginRenderPass)     \
  X(vkCmdEndRenderPass) X(vkCmdBindPipeline) X(vkCmdPushConstants) X(vkCmdDraw) X(vkCmdSetViewport) \
  X(vkCmdSetScissor) X(vkCreateSemaphore) X(vkCreateFence) X(vkWaitForFences) X(vkResetFences)      \
  X(vkQueueSubmit) X(vkCreateImage) X(vkGetImageMemoryRequirements) X(vkAllocateMemory)             \
  X(vkBindImageMemory) X(vkCmdPipelineBarrier) X(vkCmdCopyImage) X(vkCmdBlitImage) X(vkCmdClearColorImage)
#define DECLARA(f) static PFN_##f f = nullptr;
VK_GLOBALES(DECLARA)
VK_INSTANCIA(DECLARA)
VK_DISPOSITIVO(DECLARA)
static PFN_vkGetInstanceProcAddr gipa = nullptr;
typedef VkResult(VKAPI_PTR *PFN_negociar)(uint32_t *);

static const char *Nombre(VkResult r) {
  switch (r) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    default: return "otro";
  }
}

// Registra el resultado de cada llamada y corta la prueba si falla.
#define VK_OK(llamada)                                                            \
  do {                                                                            \
    VkResult r_ = (llamada);                                                      \
    Log("%-44s -> %s (%d)", #llamada, Nombre(r_), (int)r_);                      \
    if (r_ < 0) throw std::runtime_error(#llamada);                               \
  } while (0)
// Igual, pero sin escribir cuando va bien (para lo que se llama en cada cuadro).
#define VK_CADA(llamada)                                                          \
  do {                                                                            \
    VkResult r_ = (llamada);                                                      \
    if (r_ < 0) {                                                                 \
      Log("%s -> %s (%d)", #llamada, Nombre(r_), (int)r_);                       \
      throw std::runtime_error(#llamada);                                         \
    }                                                                             \
  } while (0)

struct App : winrt::implements<App, wac::IFrameworkViewSource, wac::IFrameworkView> {
  wuc::CoreWindow ventana_{nullptr};
  bool vivo_ = true;

  VkInstance inst_ = VK_NULL_HANDLE;
  VkSurfaceKHR sup_ = VK_NULL_HANDLE;
  VkPhysicalDevice fis_ = VK_NULL_HANDLE;
  VkDevice dev_ = VK_NULL_HANDLE;
  VkQueue cola_ = VK_NULL_HANDLE;
  uint32_t familia_ = 0;
  VkSwapchainKHR sc_ = VK_NULL_HANDLE;
  VkFormat formato_ = VK_FORMAT_UNDEFINED;
  VkExtent2D ext_{};
  std::vector<VkImage> imagenes_;
  std::vector<VkImageView> vistas_;
  std::vector<VkFramebuffer> fbs_;
  std::vector<VkSemaphore> sem_fin_;  // uno por imagen de la swapchain
  VkRenderPass rp_ = VK_NULL_HANDLE;
  VkPipelineLayout pl_ = VK_NULL_HANDLE;
  VkPipeline pipe_ = VK_NULL_HANDLE;
  VkCommandPool pool_ = VK_NULL_HANDLE;
  static const int kVuelo = 2;
  VkCommandBuffer cb_[kVuelo]{};
  VkSemaphore sem_disp_[kVuelo]{};
  VkFence valla_[kVuelo]{};

  // Variantes de la prueba, de LocalState\prueba.txt (clave=valor por línea):
  //   formato=44|37      B8G8R8A8_UNORM (por defecto) o R8G8B8A8_UNORM para la swapchain
  //   carga=clear|dontcare|load   el loadOp de la pasada
  //   destino=swapchain|propio    dibujar en la imagen de la swapchain o en una propia y copiarla
  //   propio=37|44       formato de la imagen propia (por defecto, el de la swapchain)
  //   tam=AxB            tamaño de la imagen propia (por defecto, el de la swapchain), p. ej. 960x544
  //   copia=copy|blit    pasarla con vkCmdCopyImage, o como Yakumo: limpiar la swapchain a negro con
  //                      vkCmdClearColorImage y hacer un vkCmdBlitImage lineal, escalado y centrado
  std::string cfg_formato_ = "44", cfg_carga_ = "clear", cfg_destino_ = "swapchain";
  std::string cfg_propio_, cfg_tam_, cfg_copia_ = "copy";
  bool propio_ = false;
  VkFormat formato_prop_ = VK_FORMAT_UNDEFINED;
  VkExtent2D ext_prop_{};
  VkImage prop_img_ = VK_NULL_HANDLE;
  VkDeviceMemory prop_mem_ = VK_NULL_HANDLE;
  VkImageView prop_vista_ = VK_NULL_HANDLE;
  VkFramebuffer prop_fb_ = VK_NULL_HANDLE;

  wac::IFrameworkView CreateView() { return *this; }
  void Initialize(wac::CoreApplicationView const &) {}
  void Load(winrt::hstring const &) {}
  void Uninitialize() {}

  void SetWindow(wuc::CoreWindow const &w) {
    ventana_ = w;
    QueryPerformanceFrequency(&g_frec);
    QueryPerformanceCounter(&g_t0);
    try {
      std::string datos = winrt::to_string(wst::ApplicationData::Current().LocalFolder().Path());
      g_log = fopen((datos + "\\dozen_prueba.log").c_str(), "w");
    } catch (...) {}
    Log("SetWindow: etapa 2, prueba de Dozen para UWP (SternXD/mesa-uwp 26.2.2)");
    AddVectoredExceptionHandler(1, Excepcion);
    HANDLE hv = CreateThread(nullptr, 0, Vigia, nullptr, 0, nullptr);
    if (hv) CloseHandle(hv);
    g_ventana = winrt::get_abi(w);
    try {
      auto b = w.Bounds();
      double escala = wgd::DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();
      g_ancho = (int)std::lround(b.Width * escala);
      g_alto = (int)std::lround(b.Height * escala);
      Log("SetWindow: ventana %.0f x %.0f (escala %.2f) -> %d x %d px", b.Width, b.Height, escala, g_ancho,
          g_alto);
    } catch (...) {
      Log("SetWindow: no se pudo leer el tamaño; se queda en %d x %d", g_ancho, g_alto);
    }
    w.Closed([this](auto &&, auto &&) {
      Log("CoreWindow.Closed");
      vivo_ = false;
    });
    // En la Xbox, B es «atrás»: sin atenderla, el shell cierra la app.
    w.KeyDown([](auto &&, wuc::KeyEventArgs const &a) {
      auto k = a.VirtualKey();
      if (k == wsy::VirtualKey::GamepadB || k == wsy::VirtualKey::GoBack || k == wsy::VirtualKey::Escape)
        a.Handled(true);
    });
    try {
      wuc::SystemNavigationManager::GetForCurrentView().BackRequested(
          [](wf::IInspectable const &, wuc::BackRequestedEventArgs const &a) { a.Handled(true); });
    } catch (...) {}
  }

  void Run() {
    ventana_.Activate();
    Log("Run: ventana activada");
    {
      ULONG_PTR baja = 0, alta = 0;
      GetCurrentThreadStackLimits(&baja, &alta);
      Log("pilas: exe reserva %llu KiB; wWinMain en el hilo %lu con %llu KiB; Run en el hilo %lu (%s) con %llu KiB",
          ReservaDePilaDelExe() >> 10, g_hilo_principal,
          (unsigned long long)(g_pila_principal_alta - g_pila_principal_baja) >> 10, GetCurrentThreadId(),
          GetCurrentThreadId() == g_hilo_principal ? "el mismo" : "otro", (unsigned long long)(alta - baja) >> 10);
    }
    // Espacio (0.0.8.0): el portal no deja subir la ISO de 1,3 GB al LocalState de Yakumo; ¿cabe?
    try {
      const std::wstring local(wst::ApplicationData::Current().LocalFolder().Path());
      ULARGE_INTEGER libre{}, total{}, libre_total{};
      if (GetDiskFreeSpaceExW(local.c_str(), &libre, &total, &libre_total))
        Log("espacio en %ls: %llu MB libres de %llu MB", local.c_str(), libre.QuadPart >> 20, total.QuadPart >> 20);
      else
        Log("GetDiskFreeSpaceExW(LocalState) falló: error %lu", GetLastError());
    } catch (...) {
      Log("espacio: sin LocalState");
    }
    // Dónde se instalan los paquetes y cuánto queda: el 2026-09-28 la instalación de la versión en español
    // falló con «There is not enough space on the disk» y el portal no dice el espacio de ninguna unidad.
    {
      std::vector<std::wstring> rutas;
      try {
        rutas.emplace_back(wam::Package::Current().InstalledLocation().Path());
      } catch (...) {
        Log("espacio: sin carpeta de instalación");
      }
      for (const wchar_t *r : {L"S:\\", L"S:\\Program Files\\WindowsApps\\", L"D:\\", L"D:\\DevelopmentFiles\\", L"E:\\", L"Q:\\"})
        rutas.emplace_back(r);
      for (const auto &r : rutas) {
        ULARGE_INTEGER libre{}, total{}, libre_total{};
        if (GetDiskFreeSpaceExW(r.c_str(), &libre, &total, &libre_total))
          Log("espacio en %ls: %llu MB libres de %llu MB", r.c_str(), libre.QuadPart >> 20, total.QuadPart >> 20);
        else
          Log("espacio en %ls: error %lu", r.c_str(), GetLastError());
      }
    }
    auto disp = ventana_.Dispatcher();
    bool dibuja = false;
    try {
      IniciaVulkan();
      dibuja = true;
    } catch (std::exception const &e) {
      Log("PRUEBA FALLIDA en: %s", e.what());
    } catch (...) {
      Log("PRUEBA FALLIDA: excepción desconocida");
    }
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    long long t_seg = t.QuadPart, t_ini = t.QuadPart;
    unsigned cuadros = 0, total = 0;
    bool anunciado = false;
    int f = 0;
    while (vivo_) {
      disp.ProcessEvents(wuc::CoreProcessEventsOption::ProcessAllIfPresent);
      if (!vivo_) break;
      if (!dibuja) {
        // La app sigue viva para que se pueda bajar el registro por el portal.
        disp.ProcessEvents(wuc::CoreProcessEventsOption::ProcessOneAndAllPending);
        continue;
      }
      QueryPerformanceCounter(&t);
      double seg = (double)(t.QuadPart - t_ini) / (double)g_frec.QuadPart;
      try {
        Cuadro(f, seg);
      } catch (std::exception const &e) {
        Log("PRUEBA FALLIDA al dibujar el cuadro %u: %s", total, e.what());
        dibuja = false;
        continue;
      }
      f = (f + 1) % kVuelo;
      ++cuadros;
      ++total;
      g_cuadro = total;
      if (t.QuadPart - t_seg >= g_frec.QuadPart) {
        Log("latido: %u cuadros en el último segundo, %u en total", cuadros, total);
        cuadros = 0;
        t_seg = t.QuadPart;
      }
      if (!anunciado && seg >= 20.0) {
        Log("PRUEBA OK: %u cuadros presentados en %.1f s con Dozen sobre Direct3D 12", total, seg);
        anunciado = true;
      }
    }
    if (dev_) vkDeviceWaitIdle(dev_);
    Log("Run: fin");
  }

  // Opciones de Dozen sin recompilar: LocalState\dzn_debug.txt con el valor de DZN_DEBUG (por ejemplo
  // «nobindless» o «sync,nobindless»). Mesa lo lee con getenv al crear la instancia; en UWP el entorno
  // del proceso está vacío, así que se pone con _putenv_s antes de cargar el controlador.
  void LeeOpcionesDozen() {
    try {
      std::string ruta = winrt::to_string(wst::ApplicationData::Current().LocalFolder().Path()) + "\\dzn_debug.txt";
      FILE *fo = fopen(ruta.c_str(), "r");
      if (!fo) {
        Log("DZN_DEBUG: sin dzn_debug.txt, Dozen con sus valores de fábrica");
        return;
      }
      char valor[256] = {0};
      if (fgets(valor, sizeof(valor), fo)) {
        size_t n = strlen(valor);
        while (n && (valor[n - 1] == '\n' || valor[n - 1] == '\r' || valor[n - 1] == ' ')) valor[--n] = 0;
        _putenv_s("DZN_DEBUG", valor);
        Log("DZN_DEBUG=%s (de dzn_debug.txt; getenv devuelve «%s»)", valor, getenv("DZN_DEBUG") ? getenv("DZN_DEBUG") : "NULL");
      }
      fclose(fo);
    } catch (...) {}
  }

  void LeePrueba() {
    try {
      std::string ruta = winrt::to_string(wst::ApplicationData::Current().LocalFolder().Path()) + "\\prueba.txt";
      FILE *fo = fopen(ruta.c_str(), "r");
      if (fo) {
        char linea[256];
        while (fgets(linea, sizeof(linea), fo)) {
          std::string l(linea);
          while (!l.empty() && (l.back() == '\n' || l.back() == '\r' || l.back() == ' ')) l.pop_back();
          size_t eq = l.find('=');
          if (eq == std::string::npos) continue;
          std::string k = l.substr(0, eq), v = l.substr(eq + 1);
          if (k == "formato") cfg_formato_ = v;
          else if (k == "carga") cfg_carga_ = v;
          else if (k == "destino") cfg_destino_ = v;
          else if (k == "propio") cfg_propio_ = v;
          else if (k == "tam") cfg_tam_ = v;
          else if (k == "copia") cfg_copia_ = v;
        }
        fclose(fo);
      }
    } catch (...) {}
    propio_ = (cfg_destino_ == "propio");
    Log("variante: formato=%s carga=%s destino=%s propio=%s tam=%s copia=%s", cfg_formato_.c_str(),
        cfg_carga_.c_str(), cfg_destino_.c_str(), cfg_propio_.empty() ? "(el de la swapchain)" : cfg_propio_.c_str(),
        cfg_tam_.empty() ? "(el de la swapchain)" : cfg_tam_.c_str(), cfg_copia_.c_str());
  }

  void IniciaVulkan() {
    LeePrueba();
    LeeOpcionesDozen();
    HMODULE m = LoadPackagedLibrary(L"vulkan_dzn.dll", 0);
    Log("LoadPackagedLibrary(vulkan_dzn.dll) -> %p (error %lu)", (void *)m, m ? 0ul : GetLastError());
    if (!m) throw std::runtime_error("LoadPackagedLibrary");
    auto negociar = (PFN_negociar)GetProcAddress(m, "vk_icdNegotiateLoaderICDInterfaceVersion");
    gipa = (PFN_vkGetInstanceProcAddr)GetProcAddress(m, "vk_icdGetInstanceProcAddr");
    if (!negociar || !gipa) throw std::runtime_error("faltan las funciones de ICD");
    uint32_t version_icd = 5;
    VK_OK(negociar(&version_icd));
    Log("interfaz de ICD negociada: %u", version_icd);
#define CARGA_GLOBAL(f) f = (PFN_##f)gipa(VK_NULL_HANDLE, #f);
    VK_GLOBALES(CARGA_GLOBAL)
    if (!vkCreateInstance || !vkEnumerateInstanceExtensionProperties) throw std::runtime_error("vkCreateInstance");
    uint32_t api = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion) vkEnumerateInstanceVersion(&api);
    Log("versión de la instancia: %u.%u.%u", VK_API_VERSION_MAJOR(api), VK_API_VERSION_MINOR(api),
        VK_API_VERSION_PATCH(api));

    uint32_t n = 0;
    VK_OK(vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr));
    std::vector<VkExtensionProperties> exts(n);
    VK_OK(vkEnumerateInstanceExtensionProperties(nullptr, &n, exts.data()));
    bool hay_sup = false, hay_win32 = false;
    for (auto &e : exts) {
      Log("  extensión de instancia: %s", e.extensionName);
      if (!strcmp(e.extensionName, VK_KHR_SURFACE_EXTENSION_NAME)) hay_sup = true;
      if (!strcmp(e.extensionName, VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) hay_win32 = true;
    }
    if (!hay_sup || !hay_win32) throw std::runtime_error("faltan VK_KHR_surface o VK_KHR_win32_surface");

    VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.pApplicationName = "Yakumo etapa 2";
    ai.apiVersion = VK_API_VERSION_1_1;
    const char *iexts[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &ai;
    ici.enabledExtensionCount = 2;
    ici.ppEnabledExtensionNames = iexts;
    VK_OK(vkCreateInstance(&ici, nullptr, &inst_));
#define CARGA_INSTANCIA(f) f = (PFN_##f)gipa(inst_, #f);
    VK_INSTANCIA(CARGA_INSTANCIA)

    // La superficie: el WSI de este Dozen toma la CoreWindow del hilo, así que la HWND no se usa.
    VkWin32SurfaceCreateInfoKHR sci{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    sci.hinstance = GetModuleHandleW(nullptr);
    sci.hwnd = (HWND)(uintptr_t)1;
    VK_OK(vkCreateWin32SurfaceKHR(inst_, &sci, nullptr, &sup_));

    uint32_t nf = 0;
    VK_OK(vkEnumeratePhysicalDevices(inst_, &nf, nullptr));
    if (nf == 0) throw std::runtime_error("ningún dispositivo físico");
    std::vector<VkPhysicalDevice> fisicos(nf);
    VK_OK(vkEnumeratePhysicalDevices(inst_, &nf, fisicos.data()));
    for (uint32_t i = 0; i < nf; ++i) {
      VkPhysicalDeviceProperties p;
      vkGetPhysicalDeviceProperties(fisicos[i], &p);
      Log("dispositivo %u: %s, Vulkan %u.%u.%u, controlador 0x%08X, vendedor 0x%04X, id 0x%04X, tipo %d", i,
          p.deviceName, VK_API_VERSION_MAJOR(p.apiVersion), VK_API_VERSION_MINOR(p.apiVersion),
          VK_API_VERSION_PATCH(p.apiVersion), p.driverVersion, p.vendorID, p.deviceID, (int)p.deviceType);
      VkPhysicalDeviceMemoryProperties mp;
      vkGetPhysicalDeviceMemoryProperties(fisicos[i], &mp);
      for (uint32_t h = 0; h < mp.memoryHeapCount; ++h)
        Log("  montón %u: %llu MB%s", h, (unsigned long long)(mp.memoryHeaps[h].size >> 20),
            (mp.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? " (del dispositivo)" : "");
    }
    fis_ = fisicos[0];

    uint32_t nq = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(fis_, &nq, nullptr);
    std::vector<VkQueueFamilyProperties> qs(nq);
    vkGetPhysicalDeviceQueueFamilyProperties(fis_, &nq, qs.data());
    bool hay_cola = false;
    for (uint32_t i = 0; i < nq && !hay_cola; ++i) {
      VkBool32 presenta = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(fis_, i, sup_, &presenta);
      Log("familia de colas %u: flags 0x%X, %u colas, presenta %s", i, qs[i].queueFlags, qs[i].queueCount,
          presenta ? "sí" : "no");
      if ((qs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presenta) {
        familia_ = i;
        hay_cola = true;
      }
    }
    if (!hay_cola) throw std::runtime_error("ninguna cola de gráficos que presente");

    float prioridad = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = familia_;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prioridad;
    const char *dexts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = dexts;
    VK_OK(vkCreateDevice(fis_, &dci, nullptr, &dev_));
#define CARGA_DISPOSITIVO(f) f = (PFN_##f)vkGetDeviceProcAddr(dev_, #f);
    VK_DISPOSITIVO(CARGA_DISPOSITIVO)
    vkGetDeviceQueue(dev_, familia_, 0, &cola_);

    CreaSwapchain();
    CreaTuberia();

    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = familia_;
    VK_OK(vkCreateCommandPool(dev_, &pci, nullptr, &pool_));
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = pool_;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = kVuelo;
    VK_OK(vkAllocateCommandBuffers(dev_, &cai, cb_));
    VkSemaphoreCreateInfo semi{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < kVuelo; ++i) {
      VK_OK(vkCreateSemaphore(dev_, &semi, nullptr, &sem_disp_[i]));
      VK_OK(vkCreateFence(dev_, &fi, nullptr, &valla_[i]));
    }
    Log("Vulkan listo: a dibujar");
  }

  void CreaSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    VK_OK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(fis_, sup_, &caps));
    Log("superficie: %u x %u (mín %u x %u, máx %u x %u), imágenes %u..%u", caps.currentExtent.width,
        caps.currentExtent.height, caps.minImageExtent.width, caps.minImageExtent.height, caps.maxImageExtent.width,
        caps.maxImageExtent.height, caps.minImageCount, caps.maxImageCount);
    uint32_t n = 0;
    VK_OK(vkGetPhysicalDeviceSurfaceFormatsKHR(fis_, sup_, &n, nullptr));
    std::vector<VkSurfaceFormatKHR> fmts(n);
    VK_OK(vkGetPhysicalDeviceSurfaceFormatsKHR(fis_, sup_, &n, fmts.data()));
    VkSurfaceFormatKHR elegido = fmts.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
                                              : fmts[0];
    const int pedido = atoi(cfg_formato_.c_str());
    for (auto &f : fmts) {
      Log("  formato %d, espacio %d", (int)f.format, (int)f.colorSpace);
      if ((int)f.format == pedido) elegido = f;
    }
    Log("  usos admitidos de la swapchain: 0x%X", caps.supportedUsageFlags);
    formato_ = elegido.format;
    uint32_t np = 0;
    VK_OK(vkGetPhysicalDeviceSurfacePresentModesKHR(fis_, sup_, &np, nullptr));
    std::vector<VkPresentModeKHR> modos(np);
    VK_OK(vkGetPhysicalDeviceSurfacePresentModesKHR(fis_, sup_, &np, modos.data()));
    for (auto m : modos) Log("  modo de presentación %d", (int)m);

    ext_ = caps.currentExtent;
    if (ext_.width == 0xFFFFFFFFu || ext_.width == 0) ext_ = {(uint32_t)g_ancho, (uint32_t)g_alto};
    formato_prop_ = cfg_propio_.empty() ? formato_ : (VkFormat)atoi(cfg_propio_.c_str());
    ext_prop_ = ext_;
    unsigned tw = 0, th = 0;
    if (sscanf(cfg_tam_.c_str(), "%ux%u", &tw, &th) == 2 && tw && th) ext_prop_ = {tw, th};
    for (VkFormat fm : {formato_, formato_prop_}) {
      VkFormatProperties fp{};
      vkGetPhysicalDeviceFormatProperties(fis_, fm, &fp);
      const VkFormatFeatureFlags o = fp.optimalTilingFeatures;
      Log("  formato %d en tiling óptimo: 0x%X (color %s, blit origen %s, blit destino %s, filtro lineal %s)", (int)fm,
          o, (o & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) ? "sí" : "no", (o & VK_FORMAT_FEATURE_BLIT_SRC_BIT) ? "sí" : "no",
          (o & VK_FORMAT_FEATURE_BLIT_DST_BIT) ? "sí" : "no",
          (o & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ? "sí" : "no");
    }
    uint32_t cuantas = caps.minImageCount < 2 ? 2 : caps.minImageCount;
    if (caps.maxImageCount && cuantas > caps.maxImageCount) cuantas = caps.maxImageCount;
    VkCompositeAlphaFlagBitsKHR alfa = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(caps.supportedCompositeAlpha & alfa)) {
      for (uint32_t b = 1; b; b <<= 1)
        if (caps.supportedCompositeAlpha & b) {
          alfa = (VkCompositeAlphaFlagBitsKHR)b;
          break;
        }
    }
    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = sup_;
    sci.minImageCount = cuantas;
    sci.imageFormat = elegido.format;
    sci.imageColorSpace = elegido.colorSpace;
    sci.imageExtent = ext_;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (propio_ ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0);
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = alfa;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;
    VK_OK(vkCreateSwapchainKHR(dev_, &sci, nullptr, &sc_));
    uint32_t ni = 0;
    VK_OK(vkGetSwapchainImagesKHR(dev_, sc_, &ni, nullptr));
    imagenes_.resize(ni);
    VK_OK(vkGetSwapchainImagesKHR(dev_, sc_, &ni, imagenes_.data()));
    Log("swapchain: %u imágenes de %u x %u, formato %d, FIFO", ni, ext_.width, ext_.height, (int)formato_);

    // Con destino=propio la pasada dibuja en la imagen propia, que puede tener otro formato.
    VkAttachmentDescription at{};
    at.format = propio_ ? formato_prop_ : formato_;
    at.samples = VK_SAMPLE_COUNT_1_BIT;
    at.loadOp = cfg_carga_ == "dontcare" ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
                : cfg_carga_ == "load"   ? VK_ATTACHMENT_LOAD_OP_LOAD
                                         : VK_ATTACHMENT_LOAD_OP_CLEAR;
    at.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    at.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    at.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    at.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    at.finalLayout = propio_ ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo rpi{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpi.attachmentCount = 1;
    rpi.pAttachments = &at;
    rpi.subpassCount = 1;
    rpi.pSubpasses = &sub;
    rpi.dependencyCount = 1;
    rpi.pDependencies = &dep;
    VK_OK(vkCreateRenderPass(dev_, &rpi, nullptr, &rp_));

    vistas_.resize(ni);
    fbs_.resize(ni);
    sem_fin_.resize(ni);
    VkSemaphoreCreateInfo semi{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (uint32_t i = 0; i < ni; ++i) {
      // Con destino=propio la swapchain no es destino de ninguna pasada: sin vistas ni framebuffers.
      if (!propio_) {
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = imagenes_[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = formato_;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VK_OK(vkCreateImageView(dev_, &vi, nullptr, &vistas_[i]));
        VkFramebufferCreateInfo fbi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbi.renderPass = rp_;
        fbi.attachmentCount = 1;
        fbi.pAttachments = &vistas_[i];
        fbi.width = ext_.width;
        fbi.height = ext_.height;
        fbi.layers = 1;
        VK_OK(vkCreateFramebuffer(dev_, &fbi, nullptr, &fbs_[i]));
      }
      VK_OK(vkCreateSemaphore(dev_, &semi, nullptr, &sem_fin_[i]));
    }
    if (propio_) CreaImagenPropia();
  }

  // destino=propio: una imagen de color propia, por defecto del mismo formato y tamaño que la de la
  // swapchain, que luego se le copia o se le pasa con un blit. Separa «dibujar» de «dibujar en una
  // imagen de DXGI»; con propio=37 tam=960x544 copia=blit es el camino con que presenta Yakumo.
  void CreaImagenPropia() {
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = formato_prop_;
    ii.extent = {ext_prop_.width, ext_prop_.height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_OK(vkCreateImage(dev_, &ii, nullptr, &prop_img_));
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(dev_, prop_img_, &mr);
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(fis_, &mp);
    uint32_t tipo = UINT32_MAX;
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
      if ((mr.memoryTypeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        tipo = i;
        break;
      }
    if (tipo == UINT32_MAX) throw std::runtime_error("sin memoria del dispositivo para la imagen propia");
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = tipo;
    VK_OK(vkAllocateMemory(dev_, &mai, nullptr, &prop_mem_));
    VK_OK(vkBindImageMemory(dev_, prop_img_, prop_mem_, 0));
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = prop_img_;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = formato_prop_;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VK_OK(vkCreateImageView(dev_, &vi, nullptr, &prop_vista_));
    VkFramebufferCreateInfo fbi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbi.renderPass = rp_;
    fbi.attachmentCount = 1;
    fbi.pAttachments = &prop_vista_;
    fbi.width = ext_prop_.width;
    fbi.height = ext_prop_.height;
    fbi.layers = 1;
    VK_OK(vkCreateFramebuffer(dev_, &fbi, nullptr, &prop_fb_));
    Log("imagen propia: %u x %u, formato %d, %llu KB en el tipo de memoria %u", ext_prop_.width, ext_prop_.height,
        (int)formato_prop_, (unsigned long long)(mr.size >> 10), tipo);
  }

  void Barrera(VkCommandBuffer cb, VkImage img, VkImageLayout de, VkImageLayout a, VkAccessFlags acc_de,
               VkAccessFlags acc_a, VkPipelineStageFlags et_de, VkPipelineStageFlags et_a) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask = acc_de;
    b.dstAccessMask = acc_a;
    b.oldLayout = de;
    b.newLayout = a;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cb, et_de, et_a, 0, 0, nullptr, 0, nullptr, 1, &b);
  }

  VkShaderModule Modulo(const uint32_t *codigo, size_t bytes) {
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = bytes;
    ci.pCode = codigo;
    VkShaderModule m = VK_NULL_HANDLE;
    VK_OK(vkCreateShaderModule(dev_, &ci, nullptr, &m));
    return m;
  }

  void CreaTuberia() {
    VkShaderModule vs = Modulo(g_spv_vert, sizeof(g_spv_vert));
    VkShaderModule fs = Modulo(g_spv_frag, sizeof(g_spv_frag));
    VkPipelineShaderStageCreateInfo etapas[2]{};
    etapas[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    etapas[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    etapas[0].module = vs;
    etapas[0].pName = "main";
    etapas[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    etapas[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    etapas[1].module = fs;
    etapas[1].pName = "main";
    VkPipelineVertexInputStateCreateInfo vin{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                         VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;
    VkDynamicState din[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    ds.dynamicStateCount = 2;
    ds.pDynamicStates = din;
    VkPushConstantRange pcr{VK_SHADER_STAGE_VERTEX_BIT, 0, 2 * sizeof(float)};
    VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges = &pcr;
    VK_OK(vkCreatePipelineLayout(dev_, &pli, nullptr, &pl_));
    VkGraphicsPipelineCreateInfo gpi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gpi.stageCount = 2;
    gpi.pStages = etapas;
    gpi.pVertexInputState = &vin;
    gpi.pInputAssemblyState = &ia;
    gpi.pViewportState = &vp;
    gpi.pRasterizationState = &rs;
    gpi.pMultisampleState = &ms;
    gpi.pColorBlendState = &cb;
    gpi.pDynamicState = &ds;
    gpi.layout = pl_;
    gpi.renderPass = rp_;
    gpi.subpass = 0;
    // Aquí Dozen traduce los shaders de SPIR-V a DXIL: si esto pasa, pasan también los de Yakumo.
    VK_OK(vkCreateGraphicsPipelines(dev_, VK_NULL_HANDLE, 1, &gpi, nullptr, &pipe_));
  }

  void Cuadro(int f, double seg) {
    Paso("vkWaitForFences");
    VK_CADA(vkWaitForFences(dev_, 1, &valla_[f], VK_TRUE, UINT64_MAX));
    uint32_t idx = 0;
    Paso("vkAcquireNextImageKHR");
    VkResult r = vkAcquireNextImageKHR(dev_, sc_, UINT64_MAX, sem_disp_[f], VK_NULL_HANDLE, &idx);
    if (g_cuadro < 3) Log("  cuadro %u: adquirida la imagen %u (%s)", g_cuadro, idx, Nombre(r));
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
      Log("vkAcquireNextImageKHR: la swapchain quedó vieja");
      return;
    }
    if (r < 0) {
      Log("vkAcquireNextImageKHR -> %s (%d)", Nombre(r), (int)r);
      throw std::runtime_error("vkAcquireNextImageKHR");
    }
    Paso("vkResetFences");
    VK_CADA(vkResetFences(dev_, 1, &valla_[f]));
    Paso("vkResetCommandBuffer");
    VK_CADA(vkResetCommandBuffer(cb_[f], 0));
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    Paso("vkBeginCommandBuffer");
    VK_CADA(vkBeginCommandBuffer(cb_[f], &bi));
    // Fondo que recorre los colores en 6 segundos, para ver a simple vista que se presenta.
    float h = (float)std::fmod(seg / 6.0, 1.0) * 6.2831853f;
    VkClearValue fondo{};
    fondo.color.float32[0] = 0.12f + 0.10f * std::cos(h);
    fondo.color.float32[1] = 0.12f + 0.10f * std::cos(h + 2.094f);
    fondo.color.float32[2] = 0.16f + 0.10f * std::cos(h + 4.189f);
    fondo.color.float32[3] = 1.0f;
    const VkExtent2D dibujo = propio_ ? ext_prop_ : ext_;
    VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rbi.renderPass = rp_;
    rbi.framebuffer = propio_ ? prop_fb_ : fbs_[idx];
    rbi.renderArea = {{0, 0}, dibujo};
    rbi.clearValueCount = 1;
    rbi.pClearValues = &fondo;
    Paso("vkCmdBeginRenderPass");
    vkCmdBeginRenderPass(cb_[f], &rbi, VK_SUBPASS_CONTENTS_INLINE);
    Paso("vkCmdBindPipeline");
    vkCmdBindPipeline(cb_[f], VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_);
    VkViewport v{0, 0, (float)dibujo.width, (float)dibujo.height, 0, 1};
    VkRect2D sc{{0, 0}, dibujo};
    Paso("vkCmdSetViewport y vkCmdSetScissor");
    vkCmdSetViewport(cb_[f], 0, 1, &v);
    vkCmdSetScissor(cb_[f], 0, 1, &sc);
    float empuje[2] = {(float)std::fmod(seg * 1.2, 6.2831853), (float)dibujo.width / (float)dibujo.height};
    Paso("vkCmdPushConstants");
    vkCmdPushConstants(cb_[f], pl_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(empuje), empuje);
    Paso("vkCmdDraw");
    vkCmdDraw(cb_[f], 3, 1, 0, 0);
    Paso("vkCmdEndRenderPass");
    vkCmdEndRenderPass(cb_[f]);
    if (propio_) {
      Paso("barrera de la imagen propia");
      Barrera(cb_[f], prop_img_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
      Paso("barrera de la swapchain a TRANSFER_DST");
      Barrera(cb_[f], imagenes_[idx], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
              VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
      if (cfg_copia_ == "blit") {
        // Como record_game_blit de Yakumo: negro en toda la imagen, una barrera entre la limpieza y el
        // blit, y el cuadro escalado con filtro lineal, centrado y con su proporción.
        const VkClearColorValue negro{{0.0f, 0.0f, 0.0f, 1.0f}};
        const VkImageSubresourceRange todo{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        Paso("vkCmdClearColorImage");
        vkCmdClearColorImage(cb_[f], imagenes_[idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &negro, 1, &todo);
        Barrera(cb_[f], imagenes_[idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);
        const double esc = (std::min)((double)ext_.width / ext_prop_.width, (double)ext_.height / ext_prop_.height);
        const int32_t w = (int32_t)(ext_prop_.width * esc), hh = (int32_t)(ext_prop_.height * esc);
        const int32_t x0 = ((int32_t)ext_.width - w) / 2, y0 = ((int32_t)ext_.height - hh) / 2;
        VkImageBlit bl{};
        bl.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        bl.srcOffsets[1] = {(int32_t)ext_prop_.width, (int32_t)ext_prop_.height, 1};
        bl.dstSubresource = bl.srcSubresource;
        bl.dstOffsets[0] = {x0, y0, 0};
        bl.dstOffsets[1] = {x0 + w, y0 + hh, 1};
        Paso("vkCmdBlitImage");
        vkCmdBlitImage(cb_[f], prop_img_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imagenes_[idx],
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bl, VK_FILTER_LINEAR);
      } else {
        VkImageCopy cp{};
        cp.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        cp.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        cp.extent = {(std::min)(ext_prop_.width, ext_.width), (std::min)(ext_prop_.height, ext_.height), 1};
        Paso("vkCmdCopyImage");
        vkCmdCopyImage(cb_[f], prop_img_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imagenes_[idx],
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);
      }
      Paso("barrera de la swapchain a PRESENT_SRC");
      Barrera(cb_[f], imagenes_[idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
              VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }
    Paso("vkEndCommandBuffer");
    VK_CADA(vkEndCommandBuffer(cb_[f]));
    VkPipelineStageFlags espera = propio_ ? VK_PIPELINE_STAGE_ALL_COMMANDS_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &sem_disp_[f];
    si.pWaitDstStageMask = &espera;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb_[f];
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &sem_fin_[idx];
    Paso("vkQueueSubmit");
    VK_CADA(vkQueueSubmit(cola_, 1, &si, valla_[f]));
    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &sem_fin_[idx];
    pi.swapchainCount = 1;
    pi.pSwapchains = &sc_;
    pi.pImageIndices = &idx;
    Paso("vkQueuePresentKHR");
    r = vkQueuePresentKHR(cola_, &pi);
    if (g_cuadro < 3) Log("  cuadro %u: presentado (%s)", g_cuadro, Nombre(r));
    Paso("fin del cuadro");
    if (r < 0 && r != VK_ERROR_OUT_OF_DATE_KHR) {
      Log("vkQueuePresentKHR -> %s (%d)", Nombre(r), (int)r);
      throw std::runtime_error("vkQueuePresentKHR");
    }
  }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  g_hilo_principal = GetCurrentThreadId();
  GetCurrentThreadStackLimits(&g_pila_principal_baja, &g_pila_principal_alta);
  wac::CoreApplication::Run(winrt::make<App>());
  return 0;
}
