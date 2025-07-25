#include <stdio.h>

#ifdef ESP_PLATFORM
extern "C" void app_main() {
    printf("ISO15118 port example\n");
}
#else
int main() {
    printf("ISO15118 port example\n");
    return 0;
}
#endif
