#define DEBUG_ON //comment this out to turn off debug mode

#define errorf(fmt, ...) fprintf(stderr, "\033[0;31m<error>\033[0m "fmt , ##__VA_ARGS__)
#ifdef DEBUG_ON
#define debugf(fmt, ...) printf("\033[0;32m<debug>\033[0m " fmt, ##__VA_ARGS__)
#else
#define debugf(fmt, ...)
#endif
#define colorf(color, fmt, ...) printf("\033[0;" color "m" fmt "\033[0m", ##__VA_ARGS__)
#define purplef(fmt, ...) colorf("35", fmt, ##__VA_ARGS__)
#define redf(fmt, ...) colorf("31", fmt, ##__VA_ARGS__)
#define greenf(fmt, ...) colorf("32", fmt, ##__VA_ARGS__)
