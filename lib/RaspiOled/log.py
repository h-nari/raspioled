import inspect,os

def printf(fmt="", *args):
    frame = inspect.currentframe().f_back
    filename = os.path.basename(frame.f_code.co_filename)
    function = frame.f_code.co_name
    line     = frame.f_lineno
    print(("%s:%d(%s) " % (filename,line,function)) + (fmt % args))
    
