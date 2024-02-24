#ifndef RDPAADVIEW_H
#define RDPAADVIEW_H

#include <freerdp/freerdp.h>

class RdpAADView
{
public:
    RdpAADView();

    static BOOL getAccessToken(freerdp *instance, AccessTokenType tokenType, char **token, size_t count, ...);
};

#endif // RDPAADVIEW_H
