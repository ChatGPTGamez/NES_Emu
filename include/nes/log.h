#pragma once
#include <stdio.h>

#ifndef NES_LOG_ENABLED
#define NES_LOG_ENABLED 1
#endif

#if NES_LOG_ENABLED
#define NES_LOGI(...) do { fprintf(stdout, "[I] "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); } while(0)
#define NES_LOGW(...) do { fprintf(stdout, "[W] "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); } while(0)
#define NES_LOGE(...) do { fprintf(stderr, "[E] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#else
#define NES_LOGI(...) do{}while(0)
#define NES_LOGW(...) do{}while(0)
#define NES_LOGE(...) do{}while(0)
#endif
