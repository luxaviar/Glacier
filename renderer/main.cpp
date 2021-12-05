
#include "App.h"
#include "Common/Util.h"

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
    LPSTR lpCmdLine, int nCmdShow) 
{
    try {
        int ret = glacier::App{ lpCmdLine }.Go();
        return ret;
    } catch( const glacier::BaseException& e ) {
        MessageBox( nullptr, glacier::ToWide(e.what()).c_str(), e.type(),MB_OK | MB_ICONEXCLAMATION );
    } catch( const std::exception& e ) {
        MessageBox( nullptr, glacier::ToWide(e.what()).c_str(),TEXT("Standard Exception"),MB_OK | MB_ICONEXCLAMATION );
    } catch( ... ) {
        MessageBox( nullptr, TEXT("No details available"),TEXT("Unknown Exception"),MB_OK | MB_ICONEXCLAMATION );
    }
    return -1;
}
