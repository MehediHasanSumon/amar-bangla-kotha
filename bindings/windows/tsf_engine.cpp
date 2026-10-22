#include "tsf_engine.h"
#include <shlwapi.h>
#include <string>
#include <vector>

// {3F5C3B4E-5D8A-4F9A-8417-6B1A4B0C7291}
const GUID CLSID_AmarBanglaKotha = {
    0x3f5c3b4e, 0x5d8a, 0x4f9a, { 0x84, 0x17, 0x6b, 0x1a, 0x4b, 0x0c, 0x72, 0x91 }
};

// {7A1E3B8F-9C2D-4E5F-B0A1-3C5E7A9B1D3F}
const GUID GUID_AmarBanglaKotha_Profile = {
    0x7a1e3b8f, 0x9c2d, 0x4e5f, { 0xb0, 0xa1, 0x3c, 0x5e, 0x7a, 0x9b, 0x1d, 0x3f }
};

static std::wstring Utf8ToUtf16(const std::string& str) {
    if (str.empty()) return L"";
    int req = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0);
    if (req <= 0) return L"";
    std::wstring out(req, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &out[0], req);
    return out;
}

CTsfEngine::CTsfEngine()
    : _refCount(1),
      _pThreadMgr(nullptr),
      _clientId(TF_CLIENTID_NULL),
      _dwKeyEventSinkCookie(TF_INVALID_COOKIE),
      _pComposition(nullptr),
      _pContext(nullptr),
      _engine(nullptr),
      _pendingAction(EDIT_NONE),
      _commitAppendSpace(false) {
}

CTsfEngine::~CTsfEngine() {
    _ClearState();
    if (_engine) {
        abk_engine_destroy(_engine);
        _engine = nullptr;
    }
}

HRESULT CTsfEngine::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObj) {
    if (ppvObj == nullptr) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (pUnkOuter != nullptr) return CLASS_E_NOAGGREGATION;

    CTsfEngine* pEngine = new (std::nothrow) CTsfEngine();
    if (pEngine == nullptr) return E_OUTOFMEMORY;

    HRESULT hr = pEngine->QueryInterface(riid, ppvObj);
    pEngine->Release();
    return hr;
}

STDMETHODIMP CTsfEngine::QueryInterface(REFIID riid, void **ppvObj) {
    if (ppvObj == nullptr) return E_INVALIDARG;
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor)) {
        *ppvObj = static_cast<ITfTextInputProcessor*>(this);
    } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppvObj = static_cast<ITfKeyEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppvObj = static_cast<ITfCompositionSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfEditSession)) {
        *ppvObj = static_cast<ITfEditSession*>(this);
    }

    if (*ppvObj) {
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CTsfEngine::AddRef(void) {
    return InterlockedIncrement(&_refCount);
}

STDMETHODIMP_(ULONG) CTsfEngine::Release(void) {
    LONG count = InterlockedDecrement(&_refCount);
    if (count == 0) {
        delete this;
    }
    return count;
}

STDMETHODIMP CTsfEngine::Activate(ITfThreadMgr *ptim, TfClientId tid) {
    if (ptim == nullptr) return E_INVALIDARG;

    _pThreadMgr = ptim;
    _pThreadMgr->AddRef();
    _clientId = tid;

    // Advise Key Event Sink
    ITfSource* pSource = nullptr;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfSource, (void**)&pSource))) {
        pSource->AdviseKeyEventSink(_clientId, static_cast<ITfKeyEventSink*>(this), TRUE);
        pSource->Release();
    }

    // Initialize core C++ engine
    _engine = abk_engine_create();

    // Attempt to load dictionary
    char appData[MAX_PATH];
    if (ExpandEnvironmentStringsA("%APPDATA%\\AmarBanglaKotha\\bangla_wordlist.txt", appData, MAX_PATH) > 0) {
        abk_load_dictionary(_engine, appData);
    }

    return S_OK;
}

STDMETHODIMP CTsfEngine::Deactivate(void) {
    _ClearState();

    if (_pThreadMgr) {
        ITfSource* pSource = nullptr;
        if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfSource, (void**)&pSource))) {
            pSource->UnadviseKeyEventSink(_clientId);
            pSource->Release();
        }
        _pThreadMgr->Release();
        _pThreadMgr = nullptr;
    }

    if (_engine) {
        abk_engine_destroy(_engine);
        _engine = nullptr;
    }

    _clientId = TF_CLIENTID_NULL;
    return S_OK;
}

STDMETHODIMP CTsfEngine::OnSetFocus(BOOL /*fForeground*/) {
    return S_OK;
}

STDMETHODIMP CTsfEngine::OnPreservedKey(ITfContext* /*pic*/, REFGUID /*rguid*/, BOOL *pfEaten) {
    if (pfEaten) *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTsfEngine::OnTestKeyDown(ITfContext* /*pic*/, WPARAM wParam, LPARAM /*lParam*/, BOOL *pfEaten) {
    if (!pfEaten) return E_INVALIDARG;

    if (_latinBuffer.empty()) {
        *pfEaten = FALSE;
        return S_OK;
    }

    if (wParam == VK_SPACE || wParam == VK_RETURN || wParam == VK_ESCAPE || wParam == VK_BACK) {
        *pfEaten = TRUE;
        return S_OK;
    }

    if (wParam >= '1' && wParam <= '9') {
        *pfEaten = TRUE;
        return S_OK;
    }

    if ((wParam >= 'A' && wParam <= 'Z') || (wParam >= '0' && wParam <= '9') || wParam == VK_OEM_3) {
        *pfEaten = TRUE;
        return S_OK;
    }

    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTsfEngine::OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;

    // Check modifiers: Ignore if Ctrl or Alt is held
    if (GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_MENU) < 0) {
        if (!_latinBuffer.empty()) {
            _pendingAction = EDIT_COMMIT_AND_END;
            _textToCommit = _primaryText;
            _commitAppendSpace = false;
            HRESULT hrSession = S_OK;
            pic->RequestEditSession(_clientId, this, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
        }
        return S_OK;
    }

    // 1. Escape key: Cancel composition
    if (wParam == VK_ESCAPE) {
        if (!_latinBuffer.empty()) {
            _pendingAction = EDIT_CANCEL;
            HRESULT hrSession = S_OK;
            pic->RequestEditSession(_clientId, this, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
            *pfEaten = TRUE;
            return S_OK;
        }
        return S_OK;
    }

    // 2. Space key: Commit primary/top candidate with space
    if (wParam == VK_SPACE) {
        if (!_latinBuffer.empty()) {
            _pendingAction = EDIT_COMMIT_AND_END;
            _textToCommit = !_candidates.empty() ? _candidates[0] : _primaryText;
            _commitAppendSpace = true;
            HRESULT hrSession = S_OK;
            pic->RequestEditSession(_clientId, this, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
            *pfEaten = TRUE;
            return S_OK;
        }
        return S_OK;
    }

    // 3. Return / Enter key: Commit without trailing space
    if (wParam == VK_RETURN) {
        if (!_latinBuffer.empty()) {
            _pendingAction = EDIT_COMMIT_AND_END;
            _textToCommit = !_candidates.empty() ? _candidates[0] : _primaryText;
            _commitAppendSpace = false;
            HRESULT hrSession = S_OK;
            pic->RequestEditSession(_clientId, this, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
            *pfEaten = TRUE;
            return S_OK;
        }
        return S_OK;
    }

    // 4. Backspace key: Remove last character or pop logical unit
    if (wParam == VK_BACK) {
        if (!_latinBuffer.empty()) {
            _latinBuffer.pop_back();
            if (_latinBuffer.empty()) {
                _pendingAction = EDIT_CANCEL;
            } else {
                // Query suggestions
                abk_suggestion_list_t* res = abk_get_suggestions(_engine, _latinBuffer.c_str());
                if (res) {
                    _primaryText = res->primary_transliteration ? Utf8ToUtf16(res->primary_transliteration) : Utf8ToUtf16(_latinBuffer);
                    _candidates.clear();
                    for (size_t i = 0; i < res->candidate_count; ++i) {
                        if (res->candidates[i].word) {
                            _candidates.push_back(Utf8ToUtf16(res->candidates[i].word));
                        }
                    }
                    abk_free_suggestions(res);
                }
                _pendingAction = EDIT_UPDATE_PREEDIT;
            }
            HRESULT hrSession = S_OK;
            pic->RequestEditSession(_clientId, this, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
            *pfEaten = TRUE;
            return S_OK;
        }
        return S_OK;
    }

    // 5. Candidate selection via 1-9
    if (!_latinBuffer.empty() && wParam >= '1' && wParam <= '9') {
        size_t idx = (size_t)(wParam - '1');
        if (idx < _candidates.size()) {
            _pendingAction = EDIT_COMMIT_AND_END;
            _textToCommit = _candidates[idx];
            _commitAppendSpace = true;
            HRESULT hrSession = S_OK;
            pic->RequestEditSession(_clientId, this, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
            *pfEaten = TRUE;
            return S_OK;
        }
    }

    // 6. Character input
    BYTE keyState[256];
    GetKeyboardState(keyState);
    WCHAR chBuf[4] = {0};
    int ret = ToUnicode((UINT)wParam, (UINT)((lParam >> 16) & 0xFF), keyState, chBuf, 3, 0);
    if (ret > 0 && chBuf[0] >= 32) {
        char ch = (char)chBuf[0];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == '^' || ch == ':' || ch == '`') {
            _latinBuffer.push_back(ch);
            abk_suggestion_list_t* res = abk_get_suggestions(_engine, _latinBuffer.c_str());
            if (res) {
                _primaryText = res->primary_transliteration ? Utf8ToUtf16(res->primary_transliteration) : Utf8ToUtf16(_latinBuffer);
                _candidates.clear();
                for (size_t i = 0; i < res->candidate_count; ++i) {
                    if (res->candidates[i].word) {
                        _candidates.push_back(Utf8ToUtf16(res->candidates[i].word));
                    }
                }
                abk_free_suggestions(res);
            }
            _pendingAction = EDIT_UPDATE_PREEDIT;
            HRESULT hrSession = S_OK;
            pic->RequestEditSession(_clientId, this, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
            *pfEaten = TRUE;
            return S_OK;
        }

        // Punctuation handling
        if (!_latinBuffer.empty() && (ch == '.' || ch == ',' || ch == '!' || ch == '?' ||
                                      ch == ';' || ch == ':' || ch == '-' || ch == '(' || ch == ')')) {
            std::wstring committed = !_candidates.empty() ? _candidates[0] : _primaryText;
            committed += (ch == '.') ? L"\u09F7" : std::wstring(1, chBuf[0]); // Bengali Dari (।)
            _pendingAction = EDIT_COMMIT_AND_END;
            _textToCommit = committed;
            _commitAppendSpace = false;
            HRESULT hrSession = S_OK;
            pic->RequestEditSession(_clientId, this, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
            *pfEaten = TRUE;
            return S_OK;
        }
    }

    return S_OK;
}

STDMETHODIMP CTsfEngine::OnTestKeyUp(ITfContext* /*pic*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL *pfEaten) {
    if (pfEaten) *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTsfEngine::OnKeyUp(ITfContext* /*pic*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL *pfEaten) {
    if (pfEaten) *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTsfEngine::OnCompositionTerminated(TfEditCookie /*ecWrite*/, ITfComposition* /*pComposition*/) {
    _ClearState();
    return S_OK;
}

STDMETHODIMP CTsfEngine::DoEditSession(TfEditCookie ec) {
    if (!_pContext) return S_OK;

    switch (_pendingAction) {
        case EDIT_UPDATE_PREEDIT:
            _UpdateComposition(ec, _pContext);
            break;
        case EDIT_COMMIT_AND_END:
            _CommitComposition(ec, _pContext, _textToCommit, _commitAppendSpace);
            break;
        case EDIT_CANCEL:
            _EndComposition(ec);
            _ClearState();
            break;
        default:
            break;
    }

    _pendingAction = EDIT_NONE;
    return S_OK;
}

void CTsfEngine::_StartComposition(TfEditCookie ec, ITfContext *pic) {
    if (_pComposition != nullptr) return;

    ITfInsertAtSelection* pInsertAtSelection = nullptr;
    if (SUCCEEDED(pic->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsertAtSelection))) {
        ITfRange* pRange = nullptr;
        if (SUCCEEDED(pInsertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, NULL, 0, &pRange))) {
            ITfContextComposition* pContextComp = nullptr;
            if (SUCCEEDED(pic->QueryInterface(IID_ITfContextComposition, (void**)&pContextComp))) {
                pContextComp->StartComposition(ec, pRange, static_cast<ITfCompositionSink*>(this), &_pComposition);
                pContextComp->Release();
            }
            pRange->Release();
        }
        pInsertAtSelection->Release();
    }
}

void CTsfEngine::_EndComposition(TfEditCookie ec) {
    if (_pComposition) {
        ITfRange* pRange = nullptr;
        if (SUCCEEDED(_pComposition->GetRange(&pRange))) {
            pRange->SetText(ec, 0, L"", 0);
            pRange->Release();
        }
        _pComposition->EndComposition(ec);
        _pComposition->Release();
        _pComposition = nullptr;
    }
}

void CTsfEngine::_UpdateComposition(TfEditCookie ec, ITfContext *pic) {
    if (!_pComposition) {
        _StartComposition(ec, pic);
    }

    if (_pComposition) {
        ITfRange* pRange = nullptr;
        if (SUCCEEDED(_pComposition->GetRange(&pRange))) {
            pRange->SetText(ec, 0, _primaryText.c_str(), (LONG)_primaryText.length());
            pRange->Release();
        }
    }
}

void CTsfEngine::_CommitComposition(TfEditCookie ec, ITfContext *pic, const std::wstring& text, bool appendSpace) {
    std::wstring finalCommit = appendSpace ? text + L" " : text;

    if (!_pComposition) {
        _StartComposition(ec, pic);
    }

    if (_pComposition) {
        ITfRange* pRange = nullptr;
        if (SUCCEEDED(_pComposition->GetRange(&pRange))) {
            pRange->SetText(ec, 0, finalCommit.c_str(), (LONG)finalCommit.length());
            pRange->Release();
        }
        _pComposition->EndComposition(ec);
        _pComposition->Release();
        _pComposition = nullptr;
    }

    _ClearState();
}

void CTsfEngine::_ClearState() {
    _latinBuffer.clear();
    _primaryText.clear();
    _candidates.clear();
    if (_pComposition) {
        _pComposition->Release();
        _pComposition = nullptr;
    }
}
