#include "exampleappsupport.h"

#if defined(Q_OS_IOS)

#import <AVFAudio/AVFAudio.h>

#include <QDebug>

namespace RiveQtExampleSupport {

void configureAudioPlaybackSession()
{
    AVAudioSession* session = [AVAudioSession sharedInstance];
    NSError* error = nil;
    if (![session setCategory:AVAudioSessionCategoryPlayback
                        mode:AVAudioSessionModeDefault
                     options:0
                       error:&error])
    {
        qWarning() << "Failed to configure iOS audio playback session:"
                   << QString::fromNSString(error.localizedDescription);
        return;
    }

    error = nil;
    if (![session setActive:YES error:&error])
    {
        qWarning() << "Failed to activate iOS audio playback session:"
                   << QString::fromNSString(error.localizedDescription);
    }
}

} // namespace RiveQtExampleSupport

#endif
