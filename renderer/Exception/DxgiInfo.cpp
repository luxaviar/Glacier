#ifndef NDEBUG

#include "DxgiInfo.h"
#include <memory>
#include <sstream>
#include "Window.h"
#include "Exception.h"

namespace glacier {

DxgiInfo::DxgiInfo() {
    // define function signature of DXGIGetDebugInterface
    typedef HRESULT (WINAPI* DXGIGetDebugInterface)(REFIID,void **);

    // load the dll that contains the function DXGIGetDebugInterface
    const auto hModDxgiDebug = LoadLibraryEx( TEXT("dxgidebug.dll"),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32 );
    if (hModDxgiDebug == nullptr) {
        WinThrowLastExcept();
    }

    // get address of DXGIGetDebugInterface in dll
    const auto DxgiGetDebugInterface = reinterpret_cast<DXGIGetDebugInterface>(
        reinterpret_cast<void*>(GetProcAddress( hModDxgiDebug,"DXGIGetDebugInterface" ))
    );
    if (DxgiGetDebugInterface == nullptr) {
        WinThrowLastExcept();
    }

    GfxThrowIfFailedRaw( DxgiGetDebugInterface( __uuidof(IDXGIInfoQueue),&info_queue_ ) );

    info_queue_->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
    info_queue_->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
}

void DxgiInfo::Set() noexcept {
    // set the index (next) so that the next all to GetMessages()
    // will only get errors generated after this call
    next_ = info_queue_->GetNumStoredMessages(DXGI_DEBUG_ALL);
}

std::string DxgiInfo::GetMessages() const {
    std::ostringstream oss;
    const auto end = info_queue_->GetNumStoredMessages(DXGI_DEBUG_ALL);
    for (auto i = next_; i < end; i++) {
        SIZE_T messageLength;
        // get the size of message i in bytes
        GfxThrowIfFailedRaw( info_queue_->GetMessage( DXGI_DEBUG_ALL,i,nullptr,&messageLength ) );
        // allocate memory for message
        auto bytes = std::make_unique<byte[]>( messageLength );
        auto pMessage = reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE*>(bytes.get());
        // get the message and push its description into the vector
        GfxThrowIfFailedRaw( info_queue_->GetMessage( DXGI_DEBUG_ALL,i,pMessage,&messageLength ) );
        if (i < end - 1) {
            oss << pMessage->pDescription << std::endl;
        } else {
            oss << pMessage->pDescription;
        }
    }

    return oss.str();
}

}

#endif
