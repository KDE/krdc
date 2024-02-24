#include "rdpaadview.h"

RdpAADView::RdpAADView()
{
}

BOOL RdpAADView::getAccessToken(freerdp *instance, AccessTokenType tokenType, char **token, size_t count, ...)
{
    // TODO: Implement webview like in SDL client to get AzuerAD oauth2 token
    // TODO: Ensure the whole thing runs on ui thread
    return FALSE;
}
