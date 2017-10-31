#include <Windows.h>
#include <tchar.h>
#include "common\debug.h"

#include "Service.h"

#define SERVICE_NAME "kvm-ivshmem-host"

//=============================================================================

struct App
{
  SERVICE_STATUS        serviceStatus;
  SERVICE_STATUS_HANDLE statusHandle;
  HANDLE                serviceStopEvent;
};

struct App app =
{
  {0},
  NULL,
  INVALID_HANDLE_VALUE
};

//=============================================================================

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);

//=============================================================================

int main(int argc, TCHAR *argv[])
{
  SERVICE_TABLE_ENTRY ServiceTable[] =
  {
    {_T(SERVICE_NAME), (LPSERVICE_MAIN_FUNCTION)ServiceMain},
    {NULL, NULL}
  };

  if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
  {
    DWORD lastError = GetLastError();
    if (lastError == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
    {
      ServiceWorkerThread(INVALID_HANDLE_VALUE);
      getc(stdin);
      return 0;
    }

    return lastError;
  }

  return 0;
}

//=============================================================================

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
  app.statusHandle = RegisterServiceCtrlHandler(_T(SERVICE_NAME), ServiceCtrlHandler);
  if (!app.statusHandle)
    return;

  ZeroMemory(&app.serviceStatus, sizeof(app.serviceStatus));
  app.serviceStatus.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
  app.serviceStatus.dwControlsAccepted        = 0;
  app.serviceStatus.dwCurrentState            = SERVICE_START_PENDING;
  app.serviceStatus.dwWin32ExitCode           = 0;
  app.serviceStatus.dwServiceSpecificExitCode = 0;
  app.serviceStatus.dwCheckPoint              = 0;

  if (SetServiceStatus(app.statusHandle, &app.serviceStatus) == FALSE)
  {
    DEBUG_ERROR("SetServiceStatus failed");
    return;
  }

  app.serviceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (app.serviceStopEvent == INVALID_HANDLE_VALUE)
  {
    app.serviceStatus.dwControlsAccepted = 0;
    app.serviceStatus.dwCurrentState     = SERVICE_STOPPED;
    app.serviceStatus.dwWin32ExitCode    = GetLastError();
    app.serviceStatus.dwCheckPoint       = 1;
    if (SetServiceStatus(app.statusHandle, &app.serviceStatus) == FALSE)
    {
      DEBUG_ERROR("SetServiceStatus failed");
      return;
    }
  }

  app.serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
  app.serviceStatus.dwCurrentState     = SERVICE_RUNNING;
  app.serviceStatus.dwWin32ExitCode    = 0;
  app.serviceStatus.dwCheckPoint       = 0;
  if (SetServiceStatus(app.statusHandle, &app.serviceStatus) == FALSE)
  {
    DEBUG_ERROR("SetServiceStatus failed");
    return;
  }

  HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);
  WaitForSingleObject(hThread, INFINITE);

  CloseHandle(app.serviceStopEvent);
  app.serviceStatus.dwControlsAccepted = 0;
  app.serviceStatus.dwCurrentState     = SERVICE_STOPPED;
  app.serviceStatus.dwWin32ExitCode    = 0;
  app.serviceStatus.dwCheckPoint       = 3;

  if (SetServiceStatus(app.statusHandle, &app.serviceStatus) == FALSE)
  {
    DEBUG_ERROR("SetServiceStatus failed");
    return;
  }
}

//=============================================================================

VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
  switch (CtrlCode)
  {
    case SERVICE_CONTROL_STOP:
      if (app.serviceStatus.dwCurrentState != SERVICE_RUNNING)
        break;

      app.serviceStatus.dwControlsAccepted = 0;
      app.serviceStatus.dwCurrentState     = SERVICE_STOP_PENDING;
      app.serviceStatus.dwWin32ExitCode    = 0;
      app.serviceStatus.dwCheckPoint       = 4;

      if (SetServiceStatus(app.statusHandle, &app.serviceStatus) == FALSE)
        DEBUG_ERROR("SetServiceStatus failed");

      SetEvent(app.serviceStopEvent);
      break;

    default:
      break;
  }
}

//=============================================================================

DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
  Service *svc = svc->Get();
  if (!svc->Initialize())
  {
    DEBUG_ERROR("Failed to initialize service");
    return ERROR;
  }

  while (WaitForSingleObject(app.serviceStopEvent, 0) != WAIT_OBJECT_0)
    if (!svc->Process(app.serviceStopEvent))
      break;

  svc->DeInitialize();
  return ERROR_SUCCESS;
}

//=============================================================================