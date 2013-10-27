#ifndef COMMON_H
#define COMMON_H

#include <stdexcept>
#include <cassert>

template<class Interface>
inline void SafeRelease(
    Interface **ppInterfaceToRelease
    )
{
    if (*ppInterfaceToRelease != NULL)
    {
        (*ppInterfaceToRelease)->Release();

        (*ppInterfaceToRelease) = NULL;
    }
}

#define CORt(x) \
	do \
	{ \
	HRESULT hr = (x); \
	if(!SUCCEEDED(hr)) throw std::runtime_error("HRESULT failure"); \
	} while(0)

#endif