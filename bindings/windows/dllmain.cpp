#include <windows.h>
#include <msctf.h>
#include <shlwapi.h>
#include "tsf_engine.h"

static LONG g_serverLocks = 0;
static HINSTANCE g_hInst = NULL;

class CClassFactory : public IClassFactory {
public:
    CClassFactory() : _refCount(1) {}

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj) override {
        if (!ppvObj) return E_INVALIDARG;
        *ppvObj = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppvObj = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef(void) override {
        return InterlockedIncrement(&_refCount);
    }

    STDMETHODIMP_(ULONG) Release(void) override {
        LONG c = InterlockedDecrement(&_refCount);
        if (c == 0) delete this;
        return c;
    }

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObj) override {
        return CTsfEngine::CreateInstance(pUnkOuter, riid, ppvObj);
    }

    STDMETHODIMP LockServer(BOOL fLock) override {
        if (fLock) {
            InterlockedIncrement(&g_serverLocks);
        } else {
            InterlockedDecrement(&g_serverLocks);
        }
        return S_OK;
    }

private:
    LONG _refCount;
};

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID /*lpvReserved*/) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hInst = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv) {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;

    if (IsEqualCLSID(rclsid, CLSID_AmarBanglaKotha)) {
        CClassFactory* pFactory = new (std::nothrow) CClassFactory();
        if (!pFactory) return E_OUTOFMEMORY;
        HRESULT hr = pFactory->QueryInterface(riid, ppv);
        pFactory->Release();
        return hr;
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow(void) {
    return (g_serverLocks == 0) ? S_OK : S_FALSE;
}

static const WCHAR c_szClsidKey[] = L"CLSID\\{3F5C3B4E-5D8A-4F9A-8417-6B1A4B0C7291}";
static const WCHAR c_szDesc[] = L"Amar Bangla Kotha Phonetic Text Service";
static const WCHAR c_szModel[] = L"Apartment";

STDAPI DllRegisterServer(void) {
    WCHAR szModule[MAX_PATH];
    if (GetModuleFileNameW(g_hInst, szModule, MAX_PATH) == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // 1. Register COM CLSID in Registry
    HKEY hKey = NULL;
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, c_szClsidKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)c_szDesc, (DWORD)(wcslen(c_szDesc) + 1) * sizeof(WCHAR));

        HKEY hSubKey = NULL;
        if (RegCreateKeyExW(hKey, L"InprocServer32", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hSubKey, NULL) == ERROR_SUCCESS) {
            RegSetValueExW(hSubKey, NULL, 0, REG_SZ, (const BYTE*)szModule, (DWORD)(wcslen(szModule) + 1) * sizeof(WCHAR));
            RegSetValueExW(hSubKey, L"ThreadingModel", 0, REG_SZ, (const BYTE*)c_szModel, (DWORD)(wcslen(c_szModel) + 1) * sizeof(WCHAR));
            RegCloseKey(hSubKey);
        }
        RegCloseKey(hKey);
    }

    // 2. Register with TSF Input Processor Profiles
    ITfInputProcessorProfiles* pProfiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (void**)&pProfiles);
    if (SUCCEEDED(hr) && pProfiles) {
        hr = pProfiles->Register(CLSID_AmarBanglaKotha);
        if (SUCCEEDED(hr)) {
            // Bengali (India) = 0x0445, Bengali (Bangladesh) = 0x0845
            const WCHAR c_szProfileDesc[] = L"Amar Bangla Kotha (আমার বাংলা কথা)";
            pProfiles->AddLanguageProfile(CLSID_AmarBanglaKotha,
                                          0x0445,
                                          GUID_AmarBanglaKotha_Profile,
                                          c_szProfileDesc,
                                          (ULONG)wcslen(c_szProfileDesc),
                                          szModule,
                                          (ULONG)wcslen(szModule),
                                          0);

            pProfiles->AddLanguageProfile(CLSID_AmarBanglaKotha,
                                          0x0845,
                                          GUID_AmarBanglaKotha_Profile,
                                          c_szProfileDesc,
                                          (ULONG)wcslen(c_szProfileDesc),
                                          szModule,
                                          (ULONG)wcslen(szModule),
                                          0);
        }
        pProfiles->Release();
    }

    return S_OK;
}

STDAPI DllUnregisterServer(void) {
    // 1. Unregister TSF Profile
    ITfInputProcessorProfiles* pProfiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (void**)&pProfiles);
    if (SUCCEEDED(hr) && pProfiles) {
        pProfiles->Unregister(CLSID_AmarBanglaKotha);
        pProfiles->Release();
    }

    // 2. Remove Registry entries
    WCHAR szSubKey[MAX_PATH];
    wsprintfW(szSubKey, L"%s\\InprocServer32", c_szClsidKey);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, szSubKey);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, c_szClsidKey);

    return S_OK;
}
