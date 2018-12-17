#include <Python.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h> 
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <pthread.h>

// #include <i2c/smbus.h>

#define SSD1306_WIDTH	128
#define SSD1306_HEIGHT	 64

#define DEFAULT_TIMEOUT		0.5		/* sec */
#define DEFAULT_I2C_DEV		"/dev/i2c-1"
#define DEFAULT_I2C_ADDR	0x3c

static PyObject *OledErr;

static int oled_opened; 
static int fd = -1;		// fd for /dev/i2c-{0,1}

static uint8_t *disp_buf;
static int disp_buf_size;
static int updating;

static pthread_t thread_id;
static int thread_quit;
static int buf_modified;
static pthread_mutex_t mutex_updating;
static pthread_cond_t  cond_modified;
static pthread_mutex_t mutex_disp_done;
static pthread_cond_t  cond_disp_done;

static int oled_i2c_open(const char *dev, int i2c_addr)
{
  fd = open(dev, O_RDWR);
  if(fd < 0){
    PyErr_Format(OledErr,"%s open failed", dev);
    return 0;
  }
  if(ioctl(fd, I2C_SLAVE, i2c_addr) < 0){
    PyErr_Format(OledErr,"set slave addr:%02x error",i2c_addr);
  }
  oled_opened = 1;
  return 1;
}

static int oled_i2c_close(void)
{
  if(fd >= 0)
    close(fd);
  oled_opened = 0;
  return 1;
}

static int oled_write1(int dc, uint8_t data)
{
  uint8_t buf[2];
  int n;
  
  buf[0] = dc ? 0xc0 : 0x80;
  buf[1] = data;
  n = write(fd, buf, 2);
  if(n < 0){
    PyErr_Format(OledErr,"i2c write failed");
    return 0;
  }
  return 1;
}

static inline int oled_cmd(uint8_t cmd)
{
  return oled_write1(0, cmd);
}

static const uint8_t oled_init_data[] = { 
  0xae, 	// Set Display OFF
  0xd5,		// set display clock div
  0x80,
  0xa8,		// set multiplex
  SSD1306_HEIGHT-1,
  0xd3,		// set display offset;
  0,
  0x40,		// set start line, 0
  0x8d, 	// set charge pump enable
  0x14, 	// charge pump ON
  0x20,		// set memory mode
  0,  		// page mode
  0xa0 | 1,	// seg remap
  0xc8,		// com scan dec
  0xda,		// set comp pins
  0x12,
  0x81, 	// set contrast
  0xcf,
  0xd9,		// set pre charge
  0xf1,
  0xdb,		// set vcom detect
  0x40,
  0xa4,		// display all on resume
  0xa6,		// normal display
  0x2e, 	//Deactivate scroll
  0xaf, 	//Entire display ON
};

static int oled_init(void)
{
  for(int i=0; i < sizeof(oled_init_data); i++){
    if(!oled_cmd(oled_init_data[i]))
      return 0;
  }
  return 1;
}

static int oled_i2c_display(void)
{
  int n;
  oled_cmd(0x21);	// set column adr
  oled_cmd(0);
  oled_cmd(SSD1306_WIDTH - 1);
  oled_cmd(0x22);	// set page addr
  oled_cmd(0);
  oled_cmd(SSD1306_HEIGHT/8 -1);

  disp_buf[-1] = 0x40;
  n = write(fd, disp_buf - 1, disp_buf_size + 1);
  if(n < 0){
    PyErr_Format(OledErr,"oled_display write error");
    return 0;
  }
  else if(n != disp_buf_size +1){
    PyErr_Format(OledErr,"oled_display write error n = %d, !=%d ",
                 n, disp_buf_size + 1);
    return 0;
  }
  return 1;
}

/// thread runner

void *oled_display_thread(void *user_data)
{
  while(1){
    if(thread_quit)
      break;
    pthread_mutex_lock(&mutex_updating);
    updating = 0;
    pthread_cond_broadcast(&cond_disp_done);
    if(!buf_modified)
      pthread_cond_wait(&cond_modified, &mutex_updating);
    updating = 1;
    pthread_mutex_unlock(&mutex_updating);

    if(buf_modified){
      buf_modified = 0;
      oled_i2c_display();
    }
  }
  pthread_mutex_unlock(&mutex_updating);
  return NULL;
}

static int  wait_update(float timeout_sec)
{
  pthread_mutex_lock(&mutex_updating);
  if(updating){
    int err;
    unsigned int ms; 
    struct timespec t,abstime;

    pthread_mutex_unlock(&mutex_updating);

    clock_gettime(CLOCK_REALTIME, &t);
    ms = t.tv_nsec / 1000000 + timeout_sec * 1000;
    abstime.tv_sec = t.tv_sec + ms / 1000;
    abstime.tv_nsec = (ms % 1000) * 1000000;
    
    pthread_mutex_lock(&mutex_disp_done);
    Py_BEGIN_ALLOW_THREADS;
    err = pthread_cond_timedwait(&cond_disp_done,&mutex_disp_done,&abstime);
    Py_END_ALLOW_THREADS;
    pthread_mutex_unlock(&mutex_disp_done);

    if(err == ETIMEDOUT){
#if PY_MAJOR_VERSION >= 3
      return (int)PyErr_Format(PyExc_TimeoutError,"vsync timeout");
#else
      return (int)PyErr_Format(OledErr,"vsync timeout");
#endif
    }
  } else {
    pthread_mutex_unlock(&mutex_updating);
  }
    
  return 1;
}


static void write_image_to_buf(const char *b,  // image.tobytes()
                               int image_w,
                               int w, int h,
                               int src_x, int src_y,
                               int dst_x, int dst_y)
{
  int src_bpl = (image_w +7)/8;
  int dst_bpl = SSD1306_WIDTH;

  for(int iy=0; iy<h; iy++){
    int yd = dst_y + iy;
    int ys = src_y + iy;
    uint8_t wmask = 1 << (yd & 7);
    const char *p = b + ys * src_bpl;
    uint8_t *q = disp_buf + yd / 8*dst_bpl + dst_x;
    for(int ix=0;ix<w;ix++){
      int xs = src_x + ix;
      uint8_t rmask = 0x80 >> (xs & 7);
      if(p[xs/8] & rmask){
        *q |= wmask;
      } else {
        *q &= ~wmask;
      }
      q++;
    }
  }
}

static void oled_shift_left(int x0,int y0,int x1,int y1, int shift, int fill)
{
  int w = SSD1306_WIDTH;
  uint8_t *b = disp_buf;
  int i;

  for(int r = y0 /8; r*8 < y1; r++){
    uint8_t mask;
    if(r * 8 < y0)
      mask = 0xff << (y0 - r * 8);
    else
      mask = 0xff;
    if((r+1)*8 > y1)
      mask &= 0xff >> ((r+1)*8 - y1);

    if(shift > 0){
      for(i=x0; i< x1-shift;i++)
        b[w*r+i] = (b[w*r+i] & ~mask)|(b[w*r+i+shift] &mask);
      for(;i < x1;i++)
        b[w*r+i] = (b[w*r+i] & ~mask)|(fill ? mask : 0);
    }
    else if(shift < 0){
      for(i=x1-1;i>=x0-shift; i--)
        b[w*r+i]=(b[w*r+i] & ~mask)|(b[w*r+i+shift] & mask);
      for(;i>=x0; i--)
        b[w*r+i]=(b[w*r+i] & ~mask)|(fill ? mask : 0);
    }
  }
}

static uint8_t get_8bit_vertically(int x, int y)
{
  int r = y < 0 ? y/8-1 : y / 8;
  uint8_t b0 = r     < 0 ? 0 : disp_buf[r*SSD1306_WIDTH + x];
  uint8_t b1 = r + 1 < 0 ? 0 : disp_buf[(r+1)*SSD1306_WIDTH + x];
  uint16_t w = b0 | (b1 << 8);

  return (uint8_t)(w >> (y & 7));
}

static void oled_shift_down(int x0, int y0, int x1, int y1,
                            int shift, int fill)
{
  int w = SSD1306_WIDTH;
  uint8_t *b = disp_buf,mask;
  int r;

  for(int x=x0; x<x1; x++){
    if(shift > 0){
      for(r=(y1-1)/8; r*8+8 > y0+shift; r--){
        mask = 0xff;
        if(r * 8 < y0 + shift) mask  = 0xff << (y0 + shift - r * 8);
        if((r+1)*8 > y1)       mask &= 0xff >> ((r+1)*8 - y1);
        b[r*w+x] = (b[r*w+x]&~mask)|(get_8bit_vertically(x,r*8-shift) & mask);
      }
      for(r = (y0+shift)/8; r*8 >= y0; r--){
        mask = 0xff;
        if(r * 8 < y0)             mask  = 0xff << (y0 - r * 8);
        if((r + 1)*8 > y0 + shift) mask &= 0xff >> ((r+1)*8 - y0 - shift);
        if(fill) b[r*w+x] |= mask;
        else     b[r*w+x] &= ~mask;
      }
    }
    if(shift < 0){
      for(r=y0/8; r*8+8 < y1-shift; r++){
        mask = 0xff;
        if(r * 8 < y0)     mask = 0xff << (y0 - r * 8);
        if(r * 8 + 8 > y1) mask &= 0xff >> (r * 8 + 8 - y1);
        b[r*w+x] = (b[r*w+x]&~mask)|(get_8bit_vertically(x,r*8-shift)&mask);
      }
      for(r=(y1+shift)/8; r*8 < y1; r++){
        mask = 0xff;
        if(r*8 < y1+shift) mask  = 0xff << (y1+shift - r*8);
        if(r*8+8 > y1)     mask &= 0xff >> (r*8+8-y1);
        if(fill)
          b[r*w+x] |=  mask;
        else
          b[r*w+x] &= ~mask;
      }
    }
  }
}

  
static PyObject *
oled_begin_method(PyObject *self, PyObject *args, PyObject *keywds)
{
  static char *kwlist[] = {"dev","i2c_addr",NULL};
  char *dev    = DEFAULT_I2C_DEV;
  int i2c_addr = DEFAULT_I2C_ADDR;
  
  if(!PyArg_ParseTupleAndKeywords(args, keywds,"|sI",kwlist,&dev,&i2c_addr))
    return NULL;
  
  // disp_buf確保

  disp_buf_size = SSD1306_WIDTH * SSD1306_HEIGHT / 8;
  disp_buf = PyMem_Malloc(disp_buf_size + 4);
  if(!disp_buf)
    return PyErr_Format(OledErr,"malloc %d byte failed", disp_buf_size + 4);
  disp_buf += 4;
  memset(disp_buf, 0, disp_buf_size);
  
  // 初期化
  // i2c初期化
  if(!oled_i2c_open(dev, i2c_addr))
    return NULL;
    
  // oled初期化

  if(!oled_init())
    return NULL;

  // thread初期化
  
  if(pthread_mutex_init(&mutex_updating, NULL) ||
     pthread_mutex_init(&mutex_disp_done, NULL) ||
     pthread_cond_init(&cond_modified, NULL) ||
     pthread_cond_init(&cond_disp_done, NULL)){
    return PyErr_Format(OledErr,"init cond/mutex  failed");
  }
  buf_modified = thread_quit = 0;

  // thread起動
  
  if(pthread_create(&thread_id, NULL, oled_display_thread, NULL)){
    return PyErr_Format(OledErr,"pthread_create failed");
  }
  
  Py_RETURN_NONE;
}

static PyObject *
oled_end_method(PyObject *self, PyObject *args, PyObject *keywds)
{
  float timeout = DEFAULT_TIMEOUT;
  static char *kwlist[] = {"timeout",NULL};

  if(!oled_opened)
    return PyErr_Format(OledErr,"oled not opended, begin() first");
  
  if(!PyArg_ParseTupleAndKeywords(args, keywds, "|f", kwlist, &timeout))
    return NULL;
  
  // 後処理

  if(!wait_update(timeout))
    return NULL;
  
  pthread_mutex_lock(&mutex_updating);
  thread_quit = 1;
  pthread_cond_broadcast(&cond_modified);
  pthread_mutex_unlock(&mutex_updating);
  pthread_join(thread_id, NULL);

  if(!oled_i2c_close())
    return NULL;
  Py_RETURN_NONE;
}

static PyObject *
oled_clear_method(PyObject *self, PyObject *args, PyObject *keywds)
{
  int update = 1;
  int sync = 0;
  float timeout = DEFAULT_TIMEOUT;
  int fill = 0;
  PyObject *area=NULL;
  static char *kwlist[] = {"update","sync","timeout","fill","area",NULL};

  if(!oled_opened)
    return PyErr_Format(OledErr,"oled not opended, begin() first");
  
  if(!PyArg_ParseTupleAndKeywords(args, keywds,"|iifiO",kwlist,
                                  &update,&sync, &timeout, &fill, &area))
    return NULL;

  if(!area){
    memset(disp_buf, fill ? 0xff: 0x00, disp_buf_size);
  } else {
    int dx,dy,dw,dh;
    int w = SSD1306_WIDTH;
    int h = SSD1306_HEIGHT;
    if(!PyArg_ParseTuple(area,"iiii",&dx,&dy,&dw,&dh)){
      PyErr_Clear();
      if(!PyArg_ParseTuple(area,"(ii)(ii)",&dx,&dy,&dw,&dh))
        return PyErr_Format(OledErr,"area must be (x,y,w,h) or((x,y),(w,h))");
    }
    if(dx < 0) { dw += dx; dx = 0; }
    if(dy < 0) { dh += dy; dy = 0; }
    if(dx + dw > w) dw = w - dx;
    if(dy + dh > h) dh = h - dy;
    if(dw <= 0 || dh <= 0) goto end;
    for(int y=dy; y<dy+dh; y++){
      uint8_t *q = disp_buf + (y/8) * w + dx;
      uint8_t wmask = 1 << (y & 7);
      for(int x=dx; x<dx+dw; x++){
        if(fill)
          *q++ |= wmask;
        else
          *q++ &= ~wmask;
      }
    }
  }

  if(update){
    buf_modified = 1;
    pthread_cond_broadcast(&cond_modified);
    if(sync){
      if(!wait_update(timeout))
        return NULL;
    }
  }
  end:
  Py_RETURN_NONE;
}


static PyObject *
oled_image_method(PyObject *self, PyObject *args, PyObject *keywds)
{
  if(!oled_opened)
    return PyErr_Format(OledErr,"oled not opended, begin() first");

  static char *kwlist[] = {"image","dst_area","src_area",
                           "update","sync","timeout",
                           NULL};
  PyObject *image;
  PyObject *dst_area = NULL;
  PyObject *src_area = NULL;
  int update = 1;
  int sync = 0;
  float timeout = DEFAULT_TIMEOUT;
  int w = SSD1306_WIDTH;
  int h = SSD1306_HEIGHT;
  int dst_x=0, dst_y=0, dst_w=w, dst_h=h;
  int src_x=0, src_y=0, src_w, src_h;

  if(!PyArg_ParseTupleAndKeywords(args, keywds,"O|OOiif",kwlist,
                                  &image, &dst_area, &src_area,
                                  &update, &sync, &timeout))
    return NULL;

  // image.__class__.__name__ == "image"
  PyObject *class = PyObject_GetAttrString(image,"__class__");
  if(!class) return NULL;
  PyObject *name  = PyObject_GetAttrString(class,"__name__");
  if(!name) return NULL;
#if  PY_MAJOR_VERSION >= 3
  char *name_str = PyUnicode_AsUTF8(name);
#else
  char *name_str = PyString_AsString(name);
#endif
  if(!name_str) return NULL;
  if(strcmp(name_str,"Image")!=0)
    return PyErr_Format(OledErr,"Image class expected, %s found", name_str);

  // image.mode == '1'

  PyObject *mode = PyObject_GetAttrString(image,"mode");
  if(!mode) return NULL;
#if  PY_MAJOR_VERSION >= 3
  char *mode_str = PyUnicode_AsUTF8(mode);
#else
  char *mode_str = PyString_AsString(mode);
#endif
  if(strcmp(mode_str,"1") != 0)
    return PyErr_Format(OledErr,"image.mode must be 1, not %s",mode_str);
  
  // image.bytes()
  PyObject *tobytes = PyObject_GetAttrString(image,"tobytes");
  if(!tobytes) return NULL;
  PyObject *bytes = PyObject_CallObject(tobytes,NULL);
  if(!bytes) return NULL;
  char *b = PyBytes_AsString(bytes);
  if(!b) return NULL;

  // image.size
  PyObject *size = PyObject_GetAttrString(image,"size");
  if(!size) return NULL;

  int image_w, image_h;
  if(!PyArg_ParseTuple(size,"ii", &image_w, &image_h))
    return NULL;
  src_w = image_w;
  src_h = image_h;

  // parse dst_area
  if(dst_area){
    if(!PyArg_ParseTuple(dst_area,"ii|ii", &dst_x, &dst_y, &dst_w, &dst_h)){
      PyErr_Clear();
      if(!PyArg_ParseTuple(dst_area,"(ii)(ii)", &dst_x,&dst_y,&dst_w,&dst_h))
        return PyErr_Format(OledErr,
                            "dst_area must be (x,y),(x,y,w,h)or((x,y),(w,h))");
    }
  }
  if(dst_x >= w || dst_y >= h || dst_w <= 0 || dst_h <= 0)    goto end;
  if(dst_x < 0){ dst_w += dst_x; dst_x = 0;}
  if(dst_y < 0){ dst_h += dst_y; dst_y = 0;}
  if(dst_x + dst_w > w) dst_w = w - dst_x;
  if(dst_y + dst_h > h) dst_h = h - dst_y;
  
  // parse src_area
  if(src_area){
    if(!PyArg_ParseTuple(src_area,"ii|ii",&src_x,&src_y,&src_w,&src_h)){
      PyErr_Clear();
      if(!PyArg_ParseTuple(src_area,"(ii)(ii)",&src_x,&src_y,&src_w,&src_h)){
        return PyErr_Format(OledErr,
                            "src_area must be (x,y),(x,y,w,h)or((x,y),(w,h))");
      }
    }
  }

  if(src_x >= image_w || src_y >= image_h || src_w <= 0 || src_h <= 0)
    goto end;
  if(src_x < 0){ src_w += src_x; src_x = 0;}
  if(src_y < 0){ src_h += src_y; src_y = 0;}
  if(src_x + src_w > image_w) src_w = image_w - src_x;
  if(src_y + src_h > image_h) src_h = image_h - src_y;

  int ww = src_w < dst_w ? src_w : dst_w;
  int hh = src_h < dst_h ? src_h : dst_h;

  // write to buf
  
  write_image_to_buf(b, image_w, ww, hh, src_x, src_y, dst_x, dst_y);

  if(update){
    buf_modified = 1;
    pthread_cond_broadcast(&cond_modified); // signal to write_thread

    if(sync){
      if(!wait_update(timeout))
        return NULL;
    }
  }
  
 end:
  Py_RETURN_NONE;
}

static PyObject *
oled_shift_method(PyObject *self, PyObject *args, PyObject *keywds)
{
  if(!oled_opened)
    return PyErr_Format(OledErr,"oled not opended, begin() first");
  
  static char *kwlist[] = {"amount","area","fill","update","sync","timeout",
                           NULL};
  PyObject *amount = NULL;
  PyObject *area = NULL;
  int fill = 0;
  int update = 1;
  int sync = 0;
  float timeout = DEFAULT_TIMEOUT;
  int sx = -1;
  int sy = 0;
  int x = 0;
  int y = 0;
  int w = SSD1306_WIDTH;
  int h = SSD1306_HEIGHT;

  if(!PyArg_ParseTupleAndKeywords(args, keywds,"|O!O!iiif",kwlist,
                                  &PyTuple_Type, &amount,
                                  &PyTuple_Type, &area,
                                  &fill, &update, &sync, &timeout))
    return NULL;

  if(amount){
    if(!PyArg_ParseTuple(amount, "ii", &sx, &sy))
      return NULL;
  }

  if(area){
    if(!PyArg_ParseTuple(area,"iiii", &x,&y,&w,&h)){
      PyErr_Clear();
      if(!PyArg_ParseTuple(area,"(ii)(ii)",&x,&y,&w,&h))
        return PyErr_Format(OledErr,
                            "area must be (x,y,w,h)or((x,y),(w,h))");
    }
  }

  if(sx != 0) oled_shift_left(x, y, x+w, y+h, -sx, fill);
  if(sy != 0) oled_shift_down(x, y, x+w, y+h, sy, fill);

  if(update && (sx !=0 || sy != 0)){
    buf_modified = 1;
    pthread_cond_broadcast(&cond_modified); // signal to write_thread
    if(sync){
      if(!wait_update(timeout))
        return NULL;
    }
  }

  Py_RETURN_NONE;
}

static PyObject *
oled_vsync_method(PyObject *self, PyObject *args, PyObject *keywds)
{
  float timeout = DEFAULT_TIMEOUT;
  static char *kwlist[] = {"timeout", NULL};

  if(!oled_opened)
    return PyErr_Format(OledErr,"oled not opended, begin() first");
  
  if(!PyArg_ParseTupleAndKeywords(args, keywds, "|f",kwlist, &timeout))
    return NULL;
  if(!wait_update(timeout))
    return NULL;
  Py_RETURN_NONE;
}

static PyMethodDef OledMethods[] = {
  {"begin",  (PyCFunction)oled_begin_method,
   METH_VARARGS|METH_KEYWORDS,   "begin oled display"},
  {"end",    (PyCFunction)oled_end_method,
   METH_VARARGS|METH_KEYWORDS,   "end oled display"},
  {"clear",  (PyCFunction)oled_clear_method,
   METH_VARARGS|METH_KEYWORDS,  "clear display"},
  {"image",  (PyCFunction)oled_image_method,
   METH_VARARGS|METH_KEYWORDS, "display PILLOW image"},
  {"shift",  (PyCFunction)oled_shift_method,
   METH_VARARGS|METH_KEYWORDS, "shift"},
  {"vsync", (PyCFunction)oled_vsync_method,
   METH_VARARGS|METH_KEYWORDS, "wait oled display updated"},
  { NULL, NULL, 0, NULL}
};

#if  PY_MAJOR_VERSION >= 3
static struct PyModuleDef oled_module = {
  PyModuleDef_HEAD_INIT,
  "raspioled",
  NULL,
  -1,
  OledMethods
};

PyMODINIT_FUNC
PyInit_oled(void)
{
  PyObject *m = PyModule_Create(&oled_module);

  OledErr = PyErr_NewException("raspioled.error", NULL, NULL);
  Py_INCREF(OledErr);
  PyModule_AddObject(m, "error", OledErr);

  PyObject *size = Py_BuildValue("(ii)", SSD1306_WIDTH, SSD1306_HEIGHT);
  Py_INCREF(size);
  PyModule_AddObject(m, "size", size);
  
  return m;
}
#else
PyMODINIT_FUNC
initoled(void) 
{
  PyObject *m = Py_InitModule("oled", OledMethods);
  if(!m) return;
  
  OledErr = PyErr_NewException("raspioled.error", NULL, NULL);
  Py_INCREF(OledErr);
  PyModule_AddObject(m, "error", OledErr);

  PyObject *size = Py_BuildValue("(ii)", SSD1306_WIDTH, SSD1306_HEIGHT);
  Py_INCREF(size);
  PyModule_AddObject(m, "size", size);
}
#endif
