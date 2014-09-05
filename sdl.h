void backend_drawpixel(int x, int y, int nescolor);
void backend_update();
void backend_lock();
void backend_unlock();
void backend_init(int w, int h);
void backend_event();
void backend_delay(unsigned int ms);
void backend_readsavefile(char *name, unsigned char *memory);
void backend_writesavefile(char *name, unsigned char *memory);
