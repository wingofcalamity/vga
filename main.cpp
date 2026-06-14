#include <windows.h>
#include <cstdint>
#include <atomic>
#include <iostream>
#include "font.h"

#define RGB_R(r) r << 16 
#define RGB_G(g) g << 8
#define RGB_B(b) b
#define MY_RGB(r, g, b) RGB_R(r) | RGB_G(g) | RGB_B(b)

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

class State {
public:
  HWND hwnd;
  CRITICAL_SECTION cs;
  BITMAPINFO bmi = {0};
  uint32_t* fbFront;
  uint32_t* fbBack;
  uint32_t* fb;
  BOOL front = TRUE;
  std::atomic<BOOL> running = TRUE;
  void drawBuffer(int x, int y, uint32_t color)
  {
    uint32_t* currentFb = front ? fbBack : fbFront;
    
    if (x >= 0 && x < 640 && y >= 0 && y < 480) {
      EnterCriticalSection(&cs);
      currentFb[y*640+x] = color;
      LeaveCriticalSection(&cs);
    }
  }
  void swapBuffer()
  {
    front = !front;
    PostMessageA(hwnd, WM_USER + 1, 0, 0);
  }
  void drawA(char c, int x, int y) {
    const uint8_t *glyph = font_get(c);

    for (int lY = 0; lY < 16; lY++) {
      for (int lX = 0; lX < 8; lX++) {
        if (glyph[lY] & (1 << (7 - lX))) {
          drawBuffer(x + lX, y + lY, 0x00FFFFFF);
        }
        else {
          drawBuffer(x + lX, y + lY, 0x00000000); // background
        }
      }
    }
  }
  uint32_t* getCurrentBuffer()
  {
    return front ? fbFront : fbBack;
  }
  State()
  {
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 640;
    bmi.bmiHeader.biHeight = -480;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = 0;

    fbFront = new uint32_t[640*480];
    fbBack = new uint32_t[640*480];
    
    memset(fbFront, 0, 640 * 480 * sizeof(uint32_t));
    memset(fbBack, 0, 640 * 480 * sizeof(uint32_t));
    InitializeCriticalSection(&cs);
  }
  ~State() {
    DeleteCriticalSection(&cs);
    delete[] fbFront;
    delete[] fbBack;
  }
};

BOOL registerWc(HINSTANCE hInstance)
{
  WNDCLASS wc = {};

  wc.lpfnWndProc   = WndProc;
  wc.hInstance     = hInstance;
  wc.lpszClassName = "BASE";
  wc.hbrBackground = NULL;

  if(!RegisterClassA(&wc)) return FALSE;
  return TRUE;
}
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch(uMsg)
  {
    case WM_CREATE:
      {
        CREATESTRUCT* cs = (CREATESTRUCT*) lParam;
        State* state = (State*)cs->lpCreateParams;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)state);
      }
      break;
    case WM_PAINT:
      {
        HDC hdc;
        State* state = (State*) GetWindowLongPtrA(hwnd, GWLP_USERDATA);
        if(!state) break;
        PAINTSTRUCT ps;
        hdc = BeginPaint(hwnd, &ps);
        EnterCriticalSection(&state->cs);
        StretchDIBits(
          hdc,
          0, 0, 640, 480, 
          0, 0, 640, 480, 
          state->getCurrentBuffer(), &state->bmi, DIB_RGB_COLORS, SRCCOPY
        );
        LeaveCriticalSection(&state->cs);
        EndPaint(hwnd, &ps);
      }
      break;
    case WM_DESTROY:
      {
        State* state = (State*) GetWindowLongPtrA(hwnd, GWLP_USERDATA);
        if (!state) state->running = false;
        PostQuitMessage(0);
        return 0;
      }
      
    case WM_ERASEBKGND:
      return 1;
    case WM_USER + 1:
      InvalidateRect(hwnd, NULL, FALSE);
      break;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

DWORD WINAPI CpuThread(LPVOID lParam)
{
  State* state = (State*) lParam;
    // fill back
    for (int x = 0; x < 640; x++) {
      for (int y = 0; y < 480; y++) {
        state->drawBuffer(x, y, MY_RGB(0, 0, 0));
      }
    }
    // fill front
    state->swapBuffer();
    for (int x = 0; x < 640; x++) {
      for (int y = 0; y < 480; y++) {
        state->drawBuffer(x, y, MY_RGB(0, 0, 0));
      }
    }
    char* hw = "Deine Mutter";
    for (int i = 0; hw[i] != 0; i++) {
      state->drawA(hw[i], 10 + i * 8, 10);
    }
    state->swapBuffer();
  while(state->running) {
    // state->swapBuffer();
    Sleep(1000);
  }
  return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nShowCmd)
{
  MSG Msg;

  if(!registerWc(hInstance)) return 1;

  State* state = new State();

  HWND hwnd = CreateWindowA(
    "BASE", "Test", 
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX , //WS_OVERLAPPEDWINDOW, 
    CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, hInstance, (LPVOID)state
  );

  state->hwnd = hwnd;
  
  ShowWindow(hwnd, nShowCmd);
  UpdateWindow(hwnd);
  
  HANDLE hThread = CreateThread(NULL, 0, CpuThread, state, 0, NULL);

  while(GetMessage(&Msg, NULL, 0, 0))
  {
    TranslateMessage(&Msg);
    DispatchMessageA(&Msg);
  }
  state->running = false;

  WaitForSingleObject(hThread, INFINITE);
  CloseHandle(hThread);

  delete state;
  return 0;
}