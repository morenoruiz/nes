void backend_drawpixel(int x, int y, int nescolor);
void backend_clear(int nescolor);
void backend_lock();
void backend_unlock();
void backend_init(int w, int h);
void backend_event();
void backend_readsavefile(char *name, unsigned char *memory);
void backend_writesavefile(char *name, unsigned char *memory);
