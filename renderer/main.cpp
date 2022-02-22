
#include "App.h"
#include "Common/Util.h"
#include "Jobs/JobSystem.h"

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
    LPSTR lpCmdLine, int nCmdShow) 
{
    using namespace glacier;

    try {
        jobs::JobSystem::Instance()->Initialize(4);

        int ret = App{ lpCmdLine }.Go();

        jobs::JobSystem::Instance()->WaitUntilFinish();

        return ret;
    } catch( const BaseException& e ) {
        MessageBox( nullptr, ToWide(e.what()).c_str(), e.type(),MB_OK | MB_ICONEXCLAMATION );
    } catch( const std::exception& e ) {
        MessageBox( nullptr, ToWide(e.what()).c_str(),TEXT("Standard Exception"),MB_OK | MB_ICONEXCLAMATION );
    } catch( ... ) {
        MessageBox( nullptr, TEXT("No details available"),TEXT("Unknown Exception"),MB_OK | MB_ICONEXCLAMATION );
    }
    return -1;
}
