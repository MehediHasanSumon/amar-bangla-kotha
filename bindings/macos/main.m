//
//  main.m
//  Amar Bangla Kotha (আমার বাংলা কথা) - macOS Input Method Server Entry Point
//

#import <Cocoa/Cocoa.h>
#import <InputMethodKit/InputMethodKit.h>

const NSString* kConnectionName = @"AmarBanglaKotha_1_Connection";
IMKServer* server = nil;

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSString* identifier = [[NSBundle mainBundle] bundleIdentifier];
        if (!identifier) {
            identifier = @"org.freedesktop.AmarBanglaKotha.macos";
        }
        server = [[IMKServer alloc] initWithName:(NSString*)kConnectionName bundleIdentifier:identifier];
        [[NSApplication sharedApplication] run];
    }
    return 0;
}
