void clear_buff();
void wakeword_setup();
void wakeword_init();
bool wakeword_regist();
bool wakeword_compare();
extern int mode;   // 0: none, <0: REGIST, >0: COMPARE
