
logging = True
#logging = False
def log(*args, **kwargs):
    if logging:
        print(*args, **kwargs)

def wait_log(*args, **kwargs):
    if logging:
        print(*args, **kwargs)
        input()
