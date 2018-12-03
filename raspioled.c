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

static int fd = -1;		// fd for /dev/i2c-{0,1}

static uint8_t *disp_buf;
static int disp_buf_size;
static int updating;

static pthread_t thread_id;
static int thread_quit;
static int thread_buf_modified;
static pthread_mutex_t mutex_modified;
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
  return 1;
}

static int oled_i2c_close(void)
{
  if(fd >= 0)
    close(fd);
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
    pthread_mutex_lock(&mutex_modified);
    pthread_cond_wait(&cond_modified, &mutex_modified);
    pthread_mutex_unlock(&mutex_modified);

    if(thread_quit) break;
    if(thread_buf_modified){
      updating = 1;
      thread_buf_modified = 0;
      oled_i2c_display();
      updating = 0;
      pthread_cond_broadcast(&cond_disp_done); 
    }
  }
  return NULL;
}

static int  wait_update(float timeout_sec)
{
  if(thread_buf_modified || updating){
    int err;
    unsigned int ms; 
    struct timespec t,abstime;

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
      PyErr_Format(PyExc_TimeoutError,"vsync timeout");
#else
      PyErr_Format(OledErr,"vsync timeout");
#endif
      return 0;
    }
  }
  return 1;
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
  if(!disp_buf){
    PyErr_Format(OledErr,"malloc %d byte failed", disp_buf_size + 4);
    return NULL;
  }
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
  
  if(pthread_mutex_init(&mutex_modified, NULL) ||
     pthread_mutex_init(&mutex_disp_done, NULL) ||
     pthread_cond_init(&cond_modified, NULL) ||
     pthread_cond_init(&cond_disp_done, NULL)){
    PyErr_Format(OledErr,"init cond/mutex  failed");
    return 0;
  }
  thread_buf_modified = thread_quit = 0;

  // thread起動
  
  if(pthread_create(&thread_id, NULL, oled_display_thread, NULL)){
    PyErr_Format(OledErr,"pthread_create failed");
    return 0;
  }
  
  Py_RETURN_NONE;
}

static PyObject *
oled_end_method(PyObject *self, PyObject *args)
{
  if(!PyArg_ParseTuple(args, ""))
    return NULL;
  
  // 後処理

  thread_quit = 1;
  pthread_cond_broadcast(&cond_modified);
  pthread_join(thread_id, NULL);
  
  if(!oled_i2c_close())
    return NULL;
  
  Py_RETURN_NONE;
}

static PyObject *
oled_clear_method(PyObject *self, PyObject *args, PyObject *keywds)
{
  int sync = 0;
  float timeout = DEFAULT_TIMEOUT;
  static char *kwlist[] = {"sync","timeout",NULL};
  
  if(!PyArg_ParseTupleAndKeywords(args, keywds,"|if",kwlist, &sync, &timeout))
    return NULL;

  memset(disp_buf, 0, disp_buf_size);
  thread_buf_modified = 1;
  pthread_cond_broadcast(&cond_modified);

  if(sync){
    if(!wait_update(timeout))
      return NULL;
  }
  
  Py_RETURN_NONE;
}


static PyObject *
oled_image_bytes_method(PyObject *self, PyObject *args, PyObject *keywds)
{
  PyObject *bytes;
  int w = SSD1306_WIDTH;
  int h = SSD1306_HEIGHT;
  int sync = 0;
  float timeout = DEFAULT_TIMEOUT;
  static char *kwlist[] = {"bytes","sync","timeout",NULL};

  if(!PyArg_ParseTupleAndKeywords(args, keywds,"O!|if",kwlist,
                                  &PyBytes_Type, &bytes, &sync, &timeout))
    return NULL;

  if(PyBytes_Size(bytes) != disp_buf_size){
    PyErr_Format(OledErr,"bad length %u, should be %d",
                 PyBytes_Size(bytes),disp_buf_size);
    return NULL;
  }
  char *b = PyBytes_AsString(bytes);
  if(!b) return NULL;

  for(int y=0; y<h; y++){
    uint8_t wmask = 1 << (y & 7);
    char *p = b + y * w / 8;
    uint8_t *q = disp_buf + (y/8)*w;
    for(int x=0;x<w;x++){
      uint8_t rmask = 0x80 >> (x & 7);
      if(p[x/8] & rmask){
        *q |= wmask;
      } else {
        *q &= ~wmask;
      }
      q++;
    }
  }

  thread_buf_modified = 1;
  pthread_cond_broadcast(&cond_modified);

  if(sync){
    if(!wait_update(timeout))
      return NULL;
  }
  
  Py_RETURN_NONE;
}

static PyObject *
oled_vsync_method(PyObject *self, PyObject *args, PyObject *keywds)
{
  float timeout = DEFAULT_TIMEOUT;
  static char *kwlist[] = {"timeout", NULL};
  if(!PyArg_ParseTupleAndKeywords(args, keywds, "|f",kwlist, &timeout))
    return NULL;
  if(!wait_update(timeout))
    return NULL;
  Py_RETURN_NONE;
}

static PyMethodDef OledMethods[] = {
  {"begin",  (PyCFunction)oled_begin_method,
   METH_VARARGS|METH_KEYWORDS,   "begin oled display"},
  {"end",    oled_end_method,   METH_VARARGS,
   "end oled display"},
  {"clear",  (PyCFunction)oled_clear_method,
   METH_VARARGS|METH_KEYWORDS,  "clear display"},
  {"image_bytes",  (PyCFunction)oled_image_bytes_method,
   METH_VARARGS|METH_KEYWORDS, "display PILLOW image"},
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
PyInit_raspioled(void)
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
initraspioled(void)
{
  PyObject *m = Py_InitModule("raspioled", OledMethods);
  if(!m) return;
  
  OledErr = PyErr_NewException("raspioled.error", NULL, NULL);
  Py_INCREF(OledErr);
  PyModule_AddObject(m, "error", OledErr);

  PyObject *size = Py_BuildValue("(ii)", SSD1306_WIDTH, SSD1306_HEIGHT);
  Py_INCREF(size);
  PyModule_AddObject(m, "size", size);
}
#endif
