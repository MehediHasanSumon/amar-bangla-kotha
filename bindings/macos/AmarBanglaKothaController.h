//
//  AmarBanglaKothaController.h
//  Amar Bangla Kotha (আমার বাংলা কথা) - macOS Input Method Controller
//
//  Subclasses IMKInputController to provide system-wide Bengali phonetic typing
//  via libabk across macOS applications (Safari, Pages, TextEdit, Chrome, etc.).
//

#import <Cocoa/Cocoa.h>
#import <InputMethodKit/InputMethodKit.h>
#include "abk/abk.h"

@interface AmarBanglaKothaController : IMKInputController {
@private
    abk_engine_t _engine;
    NSMutableString* _buffer;
    NSString* _primaryText;
    NSMutableArray<NSString*>* _candidates;
    IMKCandidates* _candidatesWindow;
}

- (id)initWithServer:(IMKServer *)server delegate:(id)delegate client:(id)client;
- (void)dealloc;

// IMKInputController event handling
- (BOOL)handleEvent:(NSEvent *)event client:(id)sender;

// Composition management
- (void)updatePreedit:(id)sender;
- (void)commitText:(NSString *)text toClient:(id)sender appendSpace:(BOOL)appendSpace;
- (void)cancelComposition:(id)sender;

@end
