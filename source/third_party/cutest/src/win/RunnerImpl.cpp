﻿#include "RunnerImpl.h"

#include <set>
#include "gmock/gmock.h"

CUTEST_NS_BEGIN

void
initGoogleMock() {
    static bool init_once = true;
    if (init_once) {
        init_once = false;
        testing::InitGoogleMock(&__argc, __wargv);
    }
}

unsigned long long
tickCount64() {
    static LARGE_INTEGER ticks_per_second = { 0 };
    LARGE_INTEGER tick;
    if (!ticks_per_second.QuadPart) {
        ::QueryPerformanceFrequency(&ticks_per_second);
    }
    ::QueryPerformanceCounter(&tick);
    LONGLONG seconds = tick.QuadPart / ticks_per_second.QuadPart;
    LONGLONG leftPart = tick.QuadPart - (ticks_per_second.QuadPart * seconds);
    LONGLONG millSeconds = leftPart * 1000 / ticks_per_second.QuadPart;
    return (unsigned long long)seconds * 1000 + millSeconds;
}

thread_id
currentThreadId() {
    return ::GetCurrentThreadId();
}

Runner*
Runner::instance() {
    static RunnerImpl runner_impl;
    return &runner_impl;
}

HWND RunnerImpl::message_window = NULL;
std::set<Runnable*> RunnerImpl::auto_delete_runnables;

RunnerImpl::RunnerImpl() {
	initGoogleMock();
    this->listener_manager.add(&this->test_progress_logger);

    // Register message window class.
    WNDCLASSEX wcx;
    ::ZeroMemory(&wcx, sizeof(WNDCLASSEX));
    wcx.cbSize = sizeof(WNDCLASSEX);
    wcx.lpfnWndProc = messageWindowProc;
    wcx.hInstance = ::GetModuleHandle(NULL);
    wcx.lpszClassName = L"CUTEST_MAIN_TEST_RUNNER";
    ::RegisterClassEx(&wcx);

    // Create a message window.
    RunnerImpl::message_window = ::CreateWindow(
                                     wcx.lpszClassName,  // name of window class
                                     NULL,               // title-bar string
                                     0,                  // top-level window
                                     0,                  // default horizontal position
                                     0,                  // default vertical position
                                     0,                  // default width
                                     0,                  // default height
                                     HWND_MESSAGE,       // message window
                                     NULL,               // use class menu
                                     wcx.hInstance,      // handle to application instance
                                     NULL);              // no window-creation data

    // 通过这个异步方法给main_thread_id赋值
    asyncRunOnMainThread(this, false);
}

RunnerImpl::~RunnerImpl() {
    waitUntilAllTestEnd();

    if (RunnerImpl::message_window) {
        ::DestroyWindow(RunnerImpl::message_window);
        RunnerImpl::message_window = NULL;
    }

    std::set<Runnable*>::iterator it = RunnerImpl::auto_delete_runnables.begin();
    while (it != RunnerImpl::auto_delete_runnables.end()) {
        delete *it;
        ++it;
    }
}

void
RunnerImpl::asyncRunOnMainThread(Runnable* runnable, bool is_auto_delete) {
    ::PostMessage(RunnerImpl::message_window, WM_RUN, (WPARAM)runnable, is_auto_delete);
}

void
RunnerImpl::delayRunOnMainThread(unsigned int delay_ms, Runnable* runnable, bool is_auto_delete) {
    if (is_auto_delete) {
        ::PostMessage(RunnerImpl::message_window, WM_DELAY_RUN_AUTO_DELETE, (WPARAM)runnable, delay_ms);
    } else {
        ::PostMessage(RunnerImpl::message_window, WM_DELAY_RUN, (WPARAM)runnable, delay_ms);
    }
}

LRESULT CALLBACK
RunnerImpl::messageWindowProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    LRESULT ret = 0;
    switch (msg) {
    case WM_RUN: {
            Runnable* runnable = (Runnable*)wparam;
            runnable->run();
            if (lparam) {
                delete runnable;
            }
        }
        break;
    case WM_DELAY_RUN_AUTO_DELETE: {
            RunnerImpl::auto_delete_runnables.insert((Runnable*)wparam);
            ::SetTimer(RunnerImpl::message_window, (UINT_PTR)wparam, (UINT)lparam, &RunnerImpl::onTimer4DelayRunAutoDelete);
        }
        break;
    case WM_DELAY_RUN: {
            ::SetTimer(RunnerImpl::message_window, (UINT_PTR)wparam, (UINT)lparam, &RunnerImpl::onTimer4DelayRun);
        }
        break;
    default: {
            ret = DefWindowProc(wnd, msg, wparam, lparam);
        }
        break;
    }
    return ret;
}

VOID CALLBACK
RunnerImpl::onTimer4DelayRunAutoDelete(HWND wnd, UINT msg, UINT_PTR id_event, DWORD elapse_ms) {
    ::KillTimer(wnd, id_event);

    Runnable* runnable = (Runnable*)id_event;
    RunnerImpl::auto_delete_runnables.erase(runnable);
    runnable->run();
    delete runnable;
}

VOID CALLBACK
RunnerImpl::onTimer4DelayRun(HWND wnd, UINT msg, UINT_PTR id_event, DWORD elapse_ms) {
    ::KillTimer(wnd, id_event);

    Runnable* runnable = (Runnable*)id_event;
    runnable->run();
}

void
RunnerImpl::waitUntilAllTestEnd() {
    MSG msg = { 0 };
    while (STATE_NONE != this->state
           && ::GetMessage(&msg, NULL, 0, 0)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
}

CUTEST_NS_END
