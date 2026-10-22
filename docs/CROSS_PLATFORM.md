# Cross-Platform Integration Guide: "Amar Bangla Kotha" (আমার বাংলা কথা)

This guide explains how to connect the C++ core engine (`libabk`) to the native input method frameworks of macOS, Linux, and Windows.

The engine exposes a clean, zero-dependency C ABI in `include/abk/abk.h` and C++ API in `include/abk/engine.h`.

---

## 1. macOS Integration (InputMethodKit)

On macOS, custom keyboards and IMEs use Apple's **InputMethodKit (IMKit)** framework.

### Architecture:
- Framework: `InputMethodKit.framework` + `Carbon.framework` (for virtual keycodes)
- Language: Objective-C++ (`.mm`) or Swift bridging to C ABI.
- Target: `.app` bundle placed in `/Library/Input Methods/` or `~/Library/Input Methods/`.

### Implementation Outline (`ABKInputController.mm`):

```objc
#import <InputMethodKit/InputMethodKit.h>
#import "abk/abk.h"

@interface ABKInputController : IMKInputController {
    abk_engine_t _engine;
    NSMutableString* _compositionBuffer;
}
@end

@implementation ABKInputController

- (id)initWithServer:(IMKServer*)server delegate:(id)delegate client:(id)client {
    if (self = [super initWithServer:server delegate:delegate client:client]) {
        _engine = abk_engine_create();
        _compositionBuffer = [[NSMutableString alloc] init];
    }
    return self;
}

- (void)dealloc {
    if (_engine) {
        abk_engine_destroy(_engine);
    }
    [_compositionBuffer release];
    [super dealloc];
}

- (BOOL)handleEvent:(NSEvent*)event client:(id)sender {
    if ([event type] == NSEventTypeKeyDown) {
        NSString* chars = [event characters];
        unichar ch = [chars characterAtIndex:0];

        // 1. If Space or Return is pressed, commit current composition
        if (ch == ' ' || ch == '\r' || ch == '\n') {
            if ([_compositionBuffer length] > 0) {
                abk_suggestion_list_t* list = abk_get_suggestions(_engine, [_compositionBuffer UTF8String]);
                if (list && list->candidate_count > 0) {
                    NSString* commitText = [NSString stringWithUTF8String:list->candidates[0].word];
                    [sender insertText:commitText replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
                }
                abk_free_suggestions(list);
                [_compositionBuffer setString:@""];
                return (ch == ' '); // Let space pass or consume
            }
            return NO;
        }

        // 2. Handle Backspace
        if (ch == 0x7F) { // Backspace
            if ([_compositionBuffer length] > 0) {
                [_compositionBuffer deleteCharactersInRange:NSMakeRange([_compositionBuffer length] - 1, 1)];
                [self updateMarkedText:sender];
                return YES;
            }
            return NO;
        }

        // 3. Regular Latin typing (a-z, A-Z, etc.)
        if (isalnum(ch) || ispunct(ch)) {
            [_compositionBuffer appendString:chars];
            [self updateMarkedText:sender];
            return YES;
        }
    }
    return NO;
}

- (void)updateMarkedText:(id)sender {
    if ([_compositionBuffer length] > 0) {
        char out_buf[512];
        int len = abk_transliterate_fast(_engine, [_compositionBuffer UTF8String], out_buf, sizeof(out_buf));
        if (len > 0) {
            NSString* preview = [NSString stringWithUTF8String:out_buf];
            [sender setMarkedText:preview
                   selectionRange:NSMakeRange([preview length], 0)
                 replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
        }
    } else {
        [sender setMarkedText:@"" selectionRange:NSMakeRange(0, 0) replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
    }
}

@end
```

---

## 2. Linux Integration (IBus & Fcitx5)

On Linux, modern desktop environments (GNOME, KDE Plasma, XFCE) utilize either **IBus** or **Fcitx5**.

### Option A: IBus Engine (`ibus-amar-bangla-kotha`)
- Framework: `libibus-1.0`
- Language: C or C++

```cpp
#include <ibus.h>
#include "abk/abk.h"

typedef struct {
    IBusEngine parent;
    abk_engine_t engine;
    GString* buffer;
} ABKIBusEngine;

static gboolean abk_engine_process_key_event(IBusEngine* engine,
                                             guint keyval,
                                             guint keycode,
                                             guint state) {
    ABKIBusEngine* abk = (ABKIBusEngine*)engine;
    if (state & IBUS_RELEASE_MASK) return FALSE;

    if (keyval == IBUS_KEY_space || keyval == IBUS_KEY_Return) {
        if (abk->buffer->len > 0) {
            abk_suggestion_list_t* list = abk_get_suggestions(abk->engine, abk->buffer->str);
            if (list && list->candidate_count > 0) {
                IBusText* text = ibus_text_new_from_string(list->candidates[0].word);
                ibus_engine_commit_text(engine, text);
            }
            abk_free_suggestions(list);
            g_string_truncate(abk->buffer, 0);
            ibus_engine_hide_preedit_text(engine);
            return (keyval == IBUS_KEY_Return); // commit and absorb enter
        }
        return FALSE;
    }

    if (keyval == IBUS_KEY_BackSpace) {
        if (abk->buffer->len > 0) {
            g_string_truncate(abk->buffer, abk->buffer->len - 1);
            // Update pre-edit
            char out_buf[256];
            abk_transliterate_fast(abk->engine, abk->buffer->str, out_buf, sizeof(out_buf));
            IBusText* text = ibus_text_new_from_string(out_buf);
            ibus_engine_update_preedit_text(engine, text, strlen(out_buf), TRUE);
            return TRUE;
        }
        return FALSE;
    }

    if (g_unichar_isprint(keyval)) {
        g_string_append_c(abk->buffer, (gchar)keyval);
        char out_buf[256];
        abk_transliterate_fast(abk->engine, abk->buffer->str, out_buf, sizeof(out_buf));
        IBusText* text = ibus_text_new_from_string(out_buf);
        ibus_engine_update_preedit_text(engine, text, strlen(out_buf), TRUE);
        return TRUE;
    }

    return FALSE;
}
```

### Option B: Standalone Input Overlay (X11 / Wayland)
You can also run ABK with `libxdo` or `wtype` (Wayland) as an autonomous background helper with a floating suggestion candidate window.

---

## 3. Windows Integration (TSF & Win32 Hook)

On Windows, keyboard input methods are implemented either through:
1. **Text Services Framework (TSF)** (Recommended for Windows 10 & 11 modern apps): Implements `ITfTextInputProcessorEx`, `ITfKeyEventSink`.
2. **Global Low-Level Keyboard Hook (`WH_KEYBOARD_LL`)**: Fast, lightweight system-tray utility that translates keystrokes and sends Unicode characters using `SendInput()`.

### Low-Level Keyboard Hook Example (`Win32 Hook`):

```cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "abk/abk.h"
#include <string>

static abk_engine_t g_engine = nullptr;
static std::string g_buffer = "";
static HHOOK g_hook = NULL;

void SendUnicodeString(const std::wstring& text) {
    std::vector<INPUT> inputs;
    for (wchar_t ch : text) {
        INPUT input = {0};
        input.type = INPUT_KEYBOARD;
        input.ki.wScan = ch;
        input.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(input);

        input.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs.push_back(input);
    }
    SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;

        if (pKey->vkCode == VK_SPACE || pKey->vkCode == VK_RETURN) {
            if (!g_buffer.empty()) {
                abk_suggestion_list_t* list = abk_get_suggestions(g_engine, g_buffer.c_str());
                if (list && list->candidate_count > 0) {
                    // Convert UTF-8 Bengali word to UTF-16 wchar_t
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, list->candidates[0].word, -1, NULL, 0);
                    std::wstring wstr(wlen, 0);
                    MultiByteToWideChar(CP_UTF8, 0, list->candidates[0].word, -1, &wstr[0], wlen);

                    // Erase typed Latin characters using backspaces
                    for (size_t i = 0; i < g_buffer.size(); i++) {
                        keybd_event(VK_BACK, 0, 0, 0);
                        keybd_event(VK_BACK, 0, KEYEVENTF_KEYUP, 0);
                    }

                    // Send Bengali Unicode string
                    SendUnicodeString(wstr.c_str());
                }
                abk_free_suggestions(list);
                g_buffer.clear();
                return (pKey->vkCode == VK_RETURN) ? 1 : 0;
            }
        } else if (pKey->vkCode >= 'A' && pKey->vkCode <= 'Z') {
            char ch = (char)(pKey->vkCode + 32); // lowercase
            g_buffer.push_back(ch);
        } else if (pKey->vkCode == VK_BACK) {
            if (!g_buffer.empty()) {
                g_buffer.pop_back();
            }
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}
```

---

## 4. Cross-Platform GUI Candidates Window

To render a floating suggestion box on all 3 platforms (like Avro, Google IME, or Gboard), you can use:
- **Qt / QML** or **Dear ImGui** or **FLTK**: Minimal footprint (~3MB), rendering the candidate list under the user's cursor.
- Connect to `abk_get_suggestions()` on every keystroke to update the list in < 0.5 milliseconds.
