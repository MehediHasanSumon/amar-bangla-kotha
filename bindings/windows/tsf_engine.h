#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <msctf.h>
#include <string>
#include <vector>
#include <memory>
#include "abk/abk.h"

// CLSID for Amar Bangla Kotha TSF Engine: {3F5C3B4E-5D8A-4F9A-8417-6B1A4B0C7291}
extern const GUID CLSID_AmarBanglaKotha;

// Profile GUID for Amar Bangla Kotha: {7A1E3B8F-9C2D-4E5F-B0A1-3C5E7A9B1D3F}
extern const GUID GUID_AmarBanglaKotha_Profile;

class CTsfEngine : public ITfTextInputProcessor,
                   public ITfKeyEventSink,
                   public ITfCompositionSink,
                   public ITfEditSession {
public:
    CTsfEngine();
    virtual ~CTsfEngine();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef(void) override;
    STDMETHODIMP_(ULONG) Release(void) override;

    // ITfTextInputProcessor methods
    STDMETHODIMP Activate(ITfThreadMgr *ptim, TfClientId tid) override;
    STDMETHODIMP Deactivate(void) override;

    // ITfKeyEventSink methods
    STDMETHODIMP OnSetFocus(BOOL fForeground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) override;
    STDMETHODIMP OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) override;
    STDMETHODIMP OnKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext *pic, REFGUID rguid, BOOL *pfEaten) override;

    // ITfCompositionSink methods
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition) override;

    // ITfEditSession methods
    STDMETHODIMP DoEditSession(TfEditCookie ec) override;

    // Factory helper
    static HRESULT CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObj);

private:
    // Internal composition & state helpers
    void _StartComposition(TfEditCookie ec, ITfContext *pic);
    void _EndComposition(TfEditCookie ec);
    void _UpdateComposition(TfEditCookie ec, ITfContext *pic);
    void _CommitComposition(TfEditCookie ec, ITfContext *pic, const std::wstring& text, bool appendSpace);
    void _ClearState();

    // Candidate window management
    void _ShowCandidateWindow(ITfContext *pic);
    void _HideCandidateWindow();
    static LRESULT CALLBACK CandidateWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Workstream 2: Safety & Password Field Detection
    bool _IsPasswordField(ITfContext* pic);

    LONG _refCount;
    ITfThreadMgr* _pThreadMgr;
    TfClientId _clientId;
    DWORD _dwKeyEventSinkCookie;

    // Active composition
    ITfComposition* _pComposition;
    ITfContext* _pContext;

    // Core C++ Engine instance
    abk_engine_t _engine;
    std::string _latinBuffer;
    std::wstring _primaryText;
    std::vector<std::wstring> _candidates;
    int _selectedIndex;
    HWND _hwndCandidate;

    // Pending edit session action
    enum EditAction {
        EDIT_NONE,
        EDIT_UPDATE_PREEDIT,
        EDIT_COMMIT_AND_END,
        EDIT_CANCEL,
        EDIT_UNDO_COMMIT
    };
    EditAction _pendingAction;
    std::wstring _textToCommit;
    std::wstring _undoCommittedText;
    bool _commitAppendSpace;
};
