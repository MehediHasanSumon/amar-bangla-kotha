//
//  AmarBanglaKothaController.m
//  Amar Bangla Kotha (আমার বাংলা কথা) - macOS Input Method Controller
//

#import "AmarBanglaKothaController.h"

@implementation AmarBanglaKothaController

- (id)initWithServer:(IMKServer *)server delegate:(id)delegate client:(id)client {
    self = [super initWithServer:server delegate:delegate client:client];
    if (self) {
        _engine = abk_engine_create();
        _buffer = [[NSMutableString alloc] initWithCapacity:64];
        _primaryText = @"";
        _candidates = [[NSMutableArray alloc] initWithCapacity:10];
        _selectedIndex = 0;

        // Allocate candidates window
        _candidatesWindow = [[IMKCandidates alloc] initWithServer:server
                                                       panelType:kIMKSingleRowSteppingList
                                                       styleType:kIMKMain];

        // Ensure non-activating style for candidates panel so host application retains focus
        if ([_candidatesWindow respondsToSelector:@selector(panel)]) {
            NSPanel* panel = (NSPanel*)[_candidatesWindow performSelector:@selector(panel)];
            if (panel && [panel isKindOfClass:[NSPanel class]]) {
                [panel setStyleMask:[panel styleMask] | NSWindowStyleMaskNonactivatingPanel];
                [panel setBecomesKeyOnlyIfNeeded:YES];
                [panel setFloatingPanel:YES];
            }
        }

        // Locate and load phonetic dictionary
        NSBundle* bundle = [NSBundle mainBundle];
        NSString* dictPath = [bundle pathForResource:@"bangla_wordlist" ofType:@"txt"];
        if (!dictPath) {
            // Check fallback shared locations
            NSString* userShared = [NSHomeDirectory() stringByAppendingPathComponent:@".local/share/amar-bangla-kotha/bangla_wordlist.txt"];
            if ([[NSFileManager defaultManager] fileExistsAtPath:userShared]) {
                dictPath = userShared;
            }
        }
        if (dictPath && _engine) {
            abk_load_dictionary(_engine, [dictPath UTF8String]);
        }
    }
    return self;
}

- (void)dealloc {
    if (_engine) {
        abk_engine_destroy(_engine);
        _engine = NULL;
    }
    _buffer = nil;
    _primaryText = nil;
    _candidates = nil;
    _candidatesWindow = nil;
}

- (void)updatePreedit:(id)sender {
    if ([_buffer length] == 0) {
        _primaryText = @"";
        _selectedIndex = 0;
        [_candidates removeAllObjects];
        if (_candidatesWindow) {
            [_candidatesWindow hide];
        }
        abk_suggestion_reset(_engine);
        [sender setMarkedText:@"" selectionRange:NSMakeRange(0, 0) replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
        return;
    }

    abk_suggestion_list_t* suggestions = abk_get_suggestions(_engine, [_buffer UTF8String]);
    if (!suggestions) {
        return;
    }

    if (suggestions->primary_transliteration) {
        _primaryText = [NSString stringWithUTF8String:suggestions->primary_transliteration];
    } else {
        _primaryText = [_buffer copy];
    }

    [_candidates removeAllObjects];
    for (size_t i = 0; i < suggestions->candidate_count; ++i) {
        if (suggestions->candidates[i].word) {
            NSString* word = [NSString stringWithUTF8String:suggestions->candidates[i].word];
            [_candidates addObject:word];
        }
    }

    _selectedIndex = abk_suggestion_get_selected_index(_engine);
    abk_free_suggestions(suggestions);

    // Update marked pre-edit text in client application
    NSAttributedString* attrText = [[NSAttributedString alloc] initWithString:_primaryText
                                                                   attributes:@{
        NSUnderlineStyleAttributeName: @(NSUnderlineStyleSingle)
    }];
    [sender setMarkedText:attrText
           selectionRange:NSMakeRange([_primaryText length], 0)
         replacementRange:NSMakeRange(NSNotFound, NSNotFound)];

    // Update candidate window
    if ([_candidates count] > 0 && _candidatesWindow) {
        [_candidatesWindow updateCandidates];
        [_candidatesWindow show:kIMKLocateCandidatesBelowHint];
    } else if (_candidatesWindow) {
        [_candidatesWindow hide];
    }
}

- (NSArray *)candidates:(id)sender {
    return _candidates;
}

- (void)candidateSelected:(NSAttributedString *)candidateString {
    if (!candidateString || [candidateString length] == 0) return;
    NSString* text = [candidateString string];
    abk_log_event(_engine, "CLICK", [text UTF8String]);
    [self commitText:text toClient:[self client] appendSpace:YES];
}

- (void)candidateSelectionChanged:(NSAttributedString *)candidateString {
    if (!candidateString) return;
    NSString* str = [candidateString string];
    NSUInteger idx = [_candidates indexOfObject:str];
    if (idx != NSNotFound) {
        _selectedIndex = (NSInteger)idx;
        abk_suggestion_select_index(_engine, (int)idx);
        abk_log_event(_engine, "SELECTION_CHANGED", [str UTF8String]);
    }
}

- (void)commitText:(NSString *)text toClient:(id)sender appendSpace:(BOOL)appendSpace {
    if (!text || [text length] == 0) {
        const char* sel = abk_suggestion_get_selected_word_ptr(_engine);
        if (sel && strlen(sel) > 0) {
            text = [NSString stringWithUTF8String:sel];
        } else {
            text = _primaryText.length > 0 ? _primaryText : _buffer;
        }
    }

    NSString* commitStr = appendSpace ? [text stringByAppendingString:@" "] : text;

    [_buffer setString:@""];
    _primaryText = @"";
    _selectedIndex = 0;
    [_candidates removeAllObjects];
    abk_suggestion_reset(_engine);

    if (_candidatesWindow) {
        [_candidatesWindow hide];
    }

    [sender insertText:commitStr replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
}

- (void)cancelComposition:(id)sender {
    [_buffer setString:@""];
    _primaryText = @"";
    _selectedIndex = 0;
    [_candidates removeAllObjects];
    abk_suggestion_reset(_engine);

    if (_candidatesWindow) {
        [_candidatesWindow hide];
    }

    [sender setMarkedText:@"" selectionRange:NSMakeRange(0, 0) replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
}

- (BOOL)handleEvent:(NSEvent *)event client:(id)sender {
    if ([event type] != NSEventTypeKeyDown) {
        return NO;
    }

    // Workstream 2: Auto-detect password / secure text fields (macOS SecureEventInput)
    if (IsSecureEventInputEnabled()) {
        abk_engine_set_secure_mode(_engine, 1);
        if ([_buffer length] > 0) {
            [self cancelComposition:sender];
        }
        return NO; // Pass through to password field untouched, nothing logged or learned
    } else {
        abk_engine_set_secure_mode(_engine, 0);
    }

    NSEventModifierFlags flags = [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;

    // Pass through Command and Control shortcuts to OS
    if (flags & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) {
        if ([_buffer length] > 0) {
            [self commitText:_primaryText toClient:sender appendSpace:NO];
        }
        return NO;
    }

    unsigned short keyCode = [event keyCode];
    NSString* chars = [event characters];

    // 1. Escape key (keyCode 53)
    if (keyCode == 53) {
        if ([_buffer length] > 0) {
            abk_log_event(_engine, "KEY_ESCAPE", "cancel composition");
            [self cancelComposition:sender];
            return YES;
        }
        return NO;
    }

    // 2. Navigation: Down arrow (keyCode 125)
    if (keyCode == 125) {
        if ([_buffer length] > 0 && [_candidates count] > 0) {
            abk_suggestion_select_next(_engine);
            _selectedIndex = abk_suggestion_get_selected_index(_engine);
            abk_log_event(_engine, "KEY_DOWN", "navigate next candidate");
            return YES; // Key consumed, candidate selection updated
        }
        return NO;
    }

    // 3. Navigation: Up arrow (keyCode 126)
    if (keyCode == 126) {
        if ([_buffer length] > 0 && [_candidates count] > 0) {
            abk_suggestion_select_prev(_engine);
            _selectedIndex = abk_suggestion_get_selected_index(_engine);
            abk_log_event(_engine, "KEY_UP", "navigate prev candidate");
            return YES; // Key consumed, candidate selection updated
        }
        return NO;
    }

    // 4. Navigation: Tab key (keyCode 48)
    if (keyCode == 48) {
        if ([_buffer length] > 0 && [_candidates count] > 0) {
            if (flags & NSEventModifierFlagShift) {
                abk_suggestion_select_prev(_engine);
                abk_log_event(_engine, "KEY_SHIFT_TAB", "navigate prev candidate");
            } else {
                abk_suggestion_select_next(_engine);
                abk_log_event(_engine, "KEY_TAB", "navigate next candidate");
            }
            _selectedIndex = abk_suggestion_get_selected_index(_engine);
            return YES; // Key consumed
        }
        return NO;
    }

    // 5. Space key (keyCode 49)
    if (keyCode == 49) {
        if ([_buffer length] > 0) {
            const char* sel = abk_suggestion_get_selected_word_ptr(_engine);
            NSString* target = (sel && strlen(sel) > 0) ? [NSString stringWithUTF8String:sel] :
                               ([_candidates count] > 0 ? _candidates[0] : _primaryText);
            abk_log_event(_engine, "KEY_SPACE", [target UTF8String]);
            [self commitText:target toClient:sender appendSpace:YES];
            return YES;
        }
        return NO;
    }

    // 6. Return / Enter key (keyCode 36, 76)
    if (keyCode == 36 || keyCode == 76) {
        if ([_buffer length] > 0) {
            const char* sel = abk_suggestion_get_selected_word_ptr(_engine);
            NSString* target = (sel && strlen(sel) > 0) ? [NSString stringWithUTF8String:sel] :
                               ([_candidates count] > 0 ? _candidates[0] : _primaryText);
            abk_log_event(_engine, "KEY_ENTER", [target UTF8String]);
            [self commitText:target toClient:sender appendSpace:NO];
            return YES;
        }
        return NO;
    }

    // 7. Backspace / Delete key (keyCode 51)
    if (keyCode == 51) {
        if ([_buffer length] > 0) {
            [_buffer deleteCharactersInRange:NSMakeRange([_buffer length] - 1, 1)];
            [self updatePreedit:sender];
            return YES;
        }
        return NO;
    }

    // 8. Candidate selection via 1-9 number keys
    if ([_buffer length] > 0 && [chars length] == 1) {
        unichar ch = [chars characterAtIndex:0];
        if (ch >= '1' && ch <= '9') {
            NSUInteger idx = (NSUInteger)(ch - '1');
            if (idx < [_candidates count]) {
                abk_log_event(_engine, "KEY_NUMBER", [_candidates[idx] UTF8String]);
                [self commitText:_candidates[idx] toClient:sender appendSpace:YES];
                return YES;
            }
        }
    }

    // 6. Latin letters & phonetic characters
    if ([chars length] == 1) {
        unichar ch = [chars characterAtIndex:0];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == '^' || ch == ':' || ch == '`') {
            [_buffer appendString:chars];
            [self updatePreedit:sender];
            return YES;
        }

        // Punctuation handling: commit pending composition + punctuation
        if ([_buffer length] > 0 && (ch == '.' || ch == ',' || ch == '!' || ch == '?' ||
                                     ch == ';' || ch == ':' || ch == '-' || ch == '(' || ch == ')')) {
            NSString* target = [_candidates count] > 0 ? _candidates[0] : _primaryText;
            NSString* punctStr = (ch == '.') ? @"।" : chars; // Bengali Dari mapping
            [self commitText:[target stringByAppendingString:punctStr] toClient:sender appendSpace:NO];
            return YES;
        }
    }

    // Any other key when composing: commit current candidate and let key pass through
    if ([_buffer length] > 0) {
        [self commitText:_primaryText toClient:sender appendSpace:NO];
    }

    return NO;
}

@end
